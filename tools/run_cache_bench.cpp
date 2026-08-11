// Run-cache benchmark: is caching shaped runs by content worth it?
//
// RAD's font cache keeps a run cache - a whole shaped string, reused by hash - layered on
// top of its per-glyph cache. This measures whether that pays off, on real source text.
//
// Links the real library so "shaping cost" is the actual TextProcessor, not a model of it.
// Deliberately not wired into CMake - delete tools/ and nothing notices:
//
//   c++ -O2 -std=c++20 tools/run_cache_bench.cpp -o /tmp/run_cache_bench (see link line below)
//     -Ilibamethyst/src -Ilibamethyst/include -Ilibamethyst/vendor/freetype-src/include \
//     -Ilibamethyst/vendor/glm-src -Ilibamethyst/vendor/spdlog-src/include \
//     build/libamethyst/libamethyst.a libamethyst/vendor/freetype-build/libfreetyped.a \
//     libamethyst/vendor/lunasvg-build/liblunasvg.a libamethyst/vendor/spdlog-build/libspdlogd.a \
//     libamethyst/vendor/tracy-build/libTracyClient.a -lpthread -ldl
//
//   /tmp/run_cache_bench --dir ~/dev/reference/neovim/src/nvim --font libamethyst/assets/fonts/Roboto-Regular.ttf
//
// Glyph rasterisation is warmed before measuring: it is already cached per (codepoint, size)
// and is not what a run cache would avoid.
//
// What is compared, per granularity and cache scope:
//   - glyphs shaped vs served from cache
//   - bytes hashed: the cost being added. Hashing a string reads every byte, which is
//     roughly what shaping a cheap run costs, so a run cache can merely relocate work.
//   - wall clock against the baseline of shaping each newly exposed line
//
// Access pattern is the dominant axis. The engine already skips re-shaping unchanged lines
// (TextLayoutState), so the question is not "re-shape the screen every frame" but "when a
// line is newly exposed, is its content already shaped somewhere":
//   sweep   scroll by one line; 1 newly exposed line per frame
//   page    jump a screenful; every line newly exposed
//   jitter  random jumps; every line newly exposed
//   redraw  same lines re-shaped every frame, modelling a full invalidation - the in-frame
//           reuse RAD's per-frame run cache is built for
//
#include "modules/glyph_atlas.h"
#include "modules/text_processor.h"
#include "parsers/freetype/font_loader.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <list>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using namespace Amethyst;
using Clock = std::chrono::steady_clock;

namespace {

constexpr uint32_t VISIBLE_LINES = 45;
constexpr size_t MAX_CORPUS_LINES = 200000;
constexpr size_t CHUNK_LEN = 16;
constexpr uint32_t FONT_PIXEL_SIZE = 14;

enum class Granularity { Line, Word, Token, Chunk };
enum class Scope { Frame, Lru };
enum class Pattern { Sweep, Page, Jitter, Redraw };

const char *name(Granularity g)
{
    switch (g) {
    case Granularity::Line: return "line";
    case Granularity::Word: return "word";
    case Granularity::Token: return "token";
    case Granularity::Chunk: return "chunk";
    }
    return "?";
}

const char *name(Scope s)
{
    return s == Scope::Frame ? "frame" : "lru";
}

const char *name(Pattern p)
{
    switch (p) {
    case Pattern::Sweep: return "sweep";
    case Pattern::Page: return "page";
    case Pattern::Jitter: return "jitter";
    case Pattern::Redraw: return "redraw";
    }
    return "?";
}

inline uint64_t hashBytes(const char *data, size_t size, uint64_t &bytesHashed)
{
    bytesHashed += size;
    uint64_t h = 1469598103934665603ull; // FNV-1a
    for (size_t i = 0; i < size; ++i) {
        h ^= static_cast<unsigned char>(data[i]);
        h *= 1099511628211ull;
    }
    return h;
}

inline bool isWordByte(char c)
{
    unsigned char u = static_cast<unsigned char>(c);
    return (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || u == '_' || u >= 0x80;
}

// Runs are non-overlapping slices covering the line, so total work is comparable across
// granularities.
void splitRuns(std::string_view line, Granularity gran, std::vector<std::string_view> &out)
{
    out.clear();
    if (line.empty()) {
        return;
    }

    switch (gran) {
    case Granularity::Line:
        out.push_back(line);
        return;

    case Granularity::Chunk:
        for (size_t i = 0; i < line.size(); i += CHUNK_LEN) {
            out.push_back(line.substr(i, std::min(CHUNK_LEN, line.size() - i)));
        }
        return;

    case Granularity::Word: {
        size_t i = 0;
        while (i < line.size()) {
            size_t start = i;
            bool space = line[i] == ' ' || line[i] == '\t';
            while (i < line.size() && ((line[i] == ' ' || line[i] == '\t') == space)) {
                ++i;
            }
            out.push_back(line.substr(start, i - start));
        }
        return;
    }

    case Granularity::Token: {
        size_t i = 0;
        while (i < line.size()) {
            size_t start = i;
            if (line[i] == ' ' || line[i] == '\t') {
                while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
                    ++i;
                }
            } else if (isWordByte(line[i])) {
                while (i < line.size() && isWordByte(line[i])) {
                    ++i;
                }
            } else {
                ++i; // punctuation: one token each
            }
            out.push_back(line.substr(start, i - start));
        }
        return;
    }
    }
}

struct CachedRun {
    std::vector<GlyphQuad> quads;
    std::list<uint64_t>::iterator order; // position in the LRU list, front = least recent
    size_t keyBytes = 0;
};

struct Stats {
    uint64_t glyphsShaped = 0;
    uint64_t glyphsReused = 0;
    uint64_t lookups = 0;
    uint64_t hits = 0;
    uint64_t bytesHashed = 0;
    size_t peakEntries = 0;
    size_t peakBytes = 0;
    double seconds = 0.0;
};

std::vector<std::string> loadCorpus(const std::vector<std::string> &dirs)
{
    std::vector<std::string> lines;
    for (const std::string &dir : dirs) {
        std::error_code ec;
        auto end = std::filesystem::recursive_directory_iterator();
        for (auto it = std::filesystem::recursive_directory_iterator(dir, ec); it != end; it.increment(ec)) {
            if (ec) {
                break;
            }
            if (!it->is_regular_file()) {
                continue;
            }
            std::string ext = it->path().extension().string();
            if (ext != ".c" && ext != ".h" && ext != ".cpp" && ext != ".hpp") {
                continue;
            }
            std::ifstream file(it->path());
            std::string line;
            while (std::getline(file, line)) {
                lines.push_back(line);
                if (lines.size() >= MAX_CORPUS_LINES) {
                    return lines;
                }
            }
        }
    }
    return lines;
}

std::vector<uint32_t> buildSchedule(Pattern pattern, size_t corpusLines, uint32_t frames, std::mt19937 &rng)
{
    std::vector<uint32_t> firstLine;
    firstLine.reserve(frames);
    uint32_t maxFirst = corpusLines > VISIBLE_LINES ? static_cast<uint32_t>(corpusLines - VISIBLE_LINES) : 1;

    for (uint32_t f = 0; f < frames; ++f) {
        switch (pattern) {
        case Pattern::Sweep: firstLine.push_back(f % maxFirst); break;
        case Pattern::Page: firstLine.push_back((f * VISIBLE_LINES) % maxFirst); break;
        case Pattern::Jitter: firstLine.push_back(rng() % maxFirst); break;
        case Pattern::Redraw: firstLine.push_back(0); break;
        }
    }
    return firstLine;
}

inline bool isExposed(Pattern pattern, uint32_t lineIdx, uint32_t prevFirst)
{
    if (pattern == Pattern::Redraw || prevFirst == UINT32_MAX) {
        return true;
    }
    return lineIdx < prevFirst || lineIdx >= prevFirst + VISIBLE_LINES;
}

} // namespace

int main(int argc, char **argv)
{
    std::vector<std::string> dirs;
    std::string fontPath;
    uint32_t frames = 2000;
    size_t lruCap = 4096;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--dir" && i + 1 < argc) {
            dirs.push_back(argv[++i]);
        } else if (arg == "--font" && i + 1 < argc) {
            fontPath = argv[++i];
        } else if (arg == "--frames" && i + 1 < argc) {
            frames = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--lru-cap" && i + 1 < argc) {
            lruCap = std::stoul(argv[++i]);
        } else {
            std::printf("usage: %s --dir <path> [--dir <path>] --font <ttf> [--frames n] [--lru-cap n]\n", argv[0]);
            return 1;
        }
    }
    if (dirs.empty() || fontPath.empty()) {
        std::printf("need --dir <path> and --font <ttf>\n");
        return 1;
    }

    std::vector<std::string> corpus = loadCorpus(dirs);
    if (corpus.size() <= VISIBLE_LINES) {
        std::printf("corpus too small (%zu lines)\n", corpus.size());
        return 1;
    }

    FontLoader fontLoader;
    if (!fontLoader.loadFont(fontPath)) {
        std::printf("failed to load font %s\n", fontPath.c_str());
        return 1;
    }
    GlyphAtlas atlas(&fontLoader);
    TextProcessor processor;
    processor.setGlyphAtlas(&atlas);

    TextLayoutParams params;
    params.fontSize = static_cast<float>(FONT_PIXEL_SIZE);
    params.bounds = {100000.0f, 100000.0f};

    // Warm the glyph cache: rasterisation is already cached per (codepoint, size) and is
    // not what a run cache would avoid, so it must not land in the measured window.
    for (size_t i = 0; i < std::min<size_t>(corpus.size(), 8000); ++i) {
        processor.layoutTextBatched(corpus[i], params);
    }

    auto shape = [&](std::string_view run, std::vector<GlyphQuad> &out) {
        BatchedText shaped = processor.layoutTextBatched(std::string(run), params);
        out = std::move(shaped.glyphs);
    };

    size_t totalChars = 0;
    for (const std::string &line : corpus) {
        totalChars += line.size();
    }
    std::printf("corpus %zu lines, %zu chars, avg %.1f | viewport %u | frames %u | real shaper, %upx | lru cap %zu\n\n",
                corpus.size(), totalChars, static_cast<double>(totalChars) / static_cast<double>(corpus.size()), VISIBLE_LINES,
                frames, FONT_PIXEL_SIZE, lruCap);

    const Pattern patterns[] = {Pattern::Sweep, Pattern::Page, Pattern::Jitter, Pattern::Redraw};
    const Granularity grans[] = {Granularity::Line, Granularity::Word, Granularity::Token, Granularity::Chunk};
    const Scope scopes[] = {Scope::Frame, Scope::Lru};

    std::vector<std::string_view> runs;
    std::vector<GlyphQuad> shapedQuads;
    std::vector<GlyphQuad> placed;

    for (Pattern pattern : patterns) {
        std::mt19937 rng(12345);
        std::vector<uint32_t> schedule = buildSchedule(pattern, corpus.size(), frames, rng);

        Stats base;
        {
            uint32_t prevFirst = UINT32_MAX;
            auto started = Clock::now();
            for (uint32_t first : schedule) {
                for (uint32_t row = 0; row < VISIBLE_LINES; ++row) {
                    uint32_t lineIdx = first + row;
                    if (!isExposed(pattern, lineIdx, prevFirst)) {
                        continue;
                    }
                    shape(corpus[lineIdx], shapedQuads);
                    base.glyphsShaped += shapedQuads.size();
                }
                prevFirst = first;
            }
            base.seconds = std::chrono::duration<double>(Clock::now() - started).count();
        }

        std::printf("=== %s ===\n", name(pattern));
        std::printf("%-7s%-7s%10s%10s%7s%9s%8s%10s%9s%9s\n", "gran", "scope", "shaped", "reused", "hit%", "entries", "memKB",
                    "hashMB", "ms", "vsBase");
        std::printf("%-7s%-7s%10llu%10s%7s%9s%8s%10s%9.1f%8.2fx\n", "-", "none", (unsigned long long)base.glyphsShaped, "-", "-",
                    "-", "-", "-", base.seconds * 1000.0, 1.0);

        for (Granularity gran : grans) {
            for (Scope scope : scopes) {
                Stats st;
                std::unordered_map<uint64_t, CachedRun> cache;
                std::list<uint64_t> order; // O(1) LRU: a linear scan per eviction dominates everything
                size_t liveBytes = 0;
                uint32_t prevFirst = UINT32_MAX;

                auto started = Clock::now();
                for (uint32_t first : schedule) {
                    if (scope == Scope::Frame) {
                        cache.clear();
                        order.clear();
                        liveBytes = 0;
                    }

                    for (uint32_t row = 0; row < VISIBLE_LINES; ++row) {
                        uint32_t lineIdx = first + row;
                        if (!isExposed(pattern, lineIdx, prevFirst)) {
                            continue;
                        }

                        splitRuns(corpus[lineIdx], gran, runs);
                        for (std::string_view run : runs) {
                            st.lookups++;
                            uint64_t key = hashBytes(run.data(), run.size(), st.bytesHashed);

                            auto it = cache.find(key);
                            if (it != cache.end()) {
                                st.hits++;
                                st.glyphsReused += it->second.quads.size();
                                order.splice(order.end(), order, it->second.order);
                                // Placing a cached run at a new origin is a copy plus a shift.
                                placed.assign(it->second.quads.begin(), it->second.quads.end());
                                continue;
                            }

                            shape(run, shapedQuads);
                            st.glyphsShaped += shapedQuads.size();

                            order.push_back(key);
                            CachedRun entry;
                            entry.quads = shapedQuads;
                            entry.order = std::prev(order.end());
                            entry.keyBytes = run.size();
                            liveBytes += entry.quads.size() * sizeof(GlyphQuad) + entry.keyBytes + 64;
                            cache.emplace(key, std::move(entry));

                            if (scope == Scope::Lru && cache.size() > lruCap) {
                                auto victim = cache.find(order.front());
                                liveBytes -= victim->second.quads.size() * sizeof(GlyphQuad) + victim->second.keyBytes + 64;
                                order.pop_front();
                                cache.erase(victim);
                            }
                        }
                        st.peakEntries = std::max(st.peakEntries, cache.size());
                        st.peakBytes = std::max(st.peakBytes, liveBytes);
                    }
                    prevFirst = first;
                }
                st.seconds = std::chrono::duration<double>(Clock::now() - started).count();

                double hitPct = st.lookups ? 100.0 * static_cast<double>(st.hits) / static_cast<double>(st.lookups) : 0.0;
                std::printf("%-7s%-7s%10llu%10llu%6.1f%%%9zu%8zu%10.1f%9.1f%8.2fx\n", name(gran), name(scope),
                            (unsigned long long)st.glyphsShaped, (unsigned long long)st.glyphsReused, hitPct, st.peakEntries,
                            st.peakBytes / 1024, static_cast<double>(st.bytesHashed) / (1024.0 * 1024.0), st.seconds * 1000.0,
                            base.seconds > 0.0 ? st.seconds / base.seconds : 0.0);
            }
        }
        std::printf("\n");
    }

    return 0;
}
