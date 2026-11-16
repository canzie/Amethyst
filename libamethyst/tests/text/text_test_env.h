#ifndef AMETHYST__TESTS_TEXT_TEST_ENV_H
#define AMETHYST__TESTS_TEXT_TEST_ENV_H

#include "common/ui_test_helpers.h"

#include "components/input_events.h"
#include "components/input_interface.h"
#include "modules/glyph_atlas.h"
#include "modules/text_processor.h"
#include "modules/text_source.h"
#include "parsers/freetype/font_registry.h"
#include "rendering/draw_context.h"
#include "rendering/geometry_registry.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace AmethystTest {

using namespace Amethyst;

inline std::string robotoPath()
{
    return std::string(AMETHYST_TEST_ASSETS) + "/fonts/Roboto-Regular.ttf";
}

inline std::string openSansPath()
{
    return std::string(AMETHYST_TEST_ASSETS) + "/fonts/OpenSans-Regular.ttf";
}

/**
 * @brief The CPU half of text rendering: a font, a glyph atlas with fake page textures, a
 * text processor and a geometry registry nobody uploads. Enough to call draw() headless.
 */
struct TextEnv {
    GlyphAtlas atlas;
    TextProcessor processor;
    std::unique_ptr<GeometryRegistry> geometry;
    DrawContext ctx;
    FontId roboto;
    FontId openSans;

    TextEnv()
    {
        // the registry is process-wide; the first font loaded is the default
        roboto = FontRegistry::instance().loadFont("Roboto", robotoPath());
        openSans = FontRegistry::instance().loadFont("Open Sans", openSansPath());

        uint32_t nextTexture = 0;
        atlas.init([nextTexture]() mutable { return AmTextureId{nextTexture++}; });
        processor.setGlyphAtlas(&atlas);
        geometry = GeometryRegistry::create(nullptr);

        ctx.geometry = geometry.get();
        ctx.textProcessor = &processor;
        ctx.glyphAtlas = &atlas;
    }
};

/** @brief Feed one key press the way a backend would. */
inline void pressKey(int key, int mods = 0)
{
    InputInterface::onKey(key, 0, KEY_ACTION_PRESS, mods);
}

inline void typeText(std::string_view ascii)
{
    for (char ch : ascii) {
        InputInterface::onChar(static_cast<uint32_t>(static_cast<unsigned char>(ch)));
    }
}

/** @brief A plain line-vector TextSourceBase, the same shape the test app uses. */
class LineSource : public TextSourceBase {
  public:
    explicit LineSource(std::vector<std::string> lines = {""}) : m_lines(std::move(lines))
    {
        if (m_lines.empty()) {
            m_lines.emplace_back();
        }
    }

    uint64_t lineCount() const override { return m_lines.size(); }

    std::string_view line(uint64_t index) const override
    {
        return index < m_lines.size() ? std::string_view(m_lines[index]) : std::string_view{};
    }

    void replace(TextRange range, std::string_view with) override
    {
        std::string head = m_lines[range.start.line].substr(0, range.start.column);
        std::string tail = m_lines[range.end.line].substr(range.end.column);
        m_lines.erase(m_lines.begin() + static_cast<std::ptrdiff_t>(range.start.line),
                      m_lines.begin() + static_cast<std::ptrdiff_t>(range.end.line) + 1);

        std::vector<std::string> inserted;
        std::string current = head;
        for (char ch : with) {
            if (ch == '\n') {
                inserted.push_back(current);
                current.clear();
            } else {
                current.push_back(ch);
            }
        }
        current += tail;
        inserted.push_back(current);
        m_lines.insert(m_lines.begin() + static_cast<std::ptrdiff_t>(range.start.line), inserted.begin(), inserted.end());
        ++m_revision;
    }

    uint64_t revision() const override { return m_revision; }

    std::string joined() const
    {
        std::string out;
        for (size_t i = 0; i < m_lines.size(); i++) {
            if (i > 0) {
                out.push_back('\n');
            }
            out += m_lines[i];
        }
        return out;
    }

  private:
    std::vector<std::string> m_lines;
    uint64_t m_revision = 0;
};

} // namespace AmethystTest

#endif // AMETHYST__TESTS_TEXT_TEST_ENV_H
