/*
 * Amethyst logging implementation
 */

#include "logging/log.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <format>

#ifdef _WIN32
#include <io.h>
#define AM_ISATTY(fd)       _isatty(fd)
#define AM_FILENO(f)        _fileno(f)
#define AM_LOCALTIME(tm, t) localtime_s(tm, t)
#else
#include <unistd.h>
#define AM_ISATTY(fd)       isatty(fd)
#define AM_FILENO(f)        fileno(f)
#define AM_LOCALTIME(tm, t) localtime_r(t, tm)
#endif

namespace Amethyst {

std::vector<LogMessage> Log::s_RecentLogs;
size_t Log::s_MaxRecentLogs = 1000;
LogLevel Log::s_Level = LogLevel::TRACE;

static FILE *s_LogFile = nullptr;
static bool s_StderrColor = false;

static constexpr size_t MAX_LOG_FILE_BYTES = 5 * 1024 * 1024;
static constexpr int LOG_ROTATE_COUNT = 3;
static constexpr const char *LOG_PATH = "logs/amethyst.log";

static const char *levelTag(LogLevel level)
{
    switch (level) {
    case LogLevel::TRACE:
        return "trace";
    case LogLevel::DEBUG:
        return "debug";
    case LogLevel::INFO:
        return "info";
    case LogLevel::WARN:
        return "warn";
    case LogLevel::ERR:
        return "error";
    case LogLevel::CRITICAL:
        return "critical";
    }
    return "?";
}

static const char *levelColor(LogLevel level)
{
    switch (level) {
    case LogLevel::TRACE:
        return "\033[37m";
    case LogLevel::DEBUG:
        return "\033[36m";
    case LogLevel::INFO:
        return "\033[32m";
    case LogLevel::WARN:
        return "\033[33m";
    case LogLevel::ERR:
        return "\033[31m";
    case LogLevel::CRITICAL:
        return "\033[1;31m";
    }
    return "";
}

static void rotateLogFile()
{
    std::error_code ec;
    auto sz = std::filesystem::file_size(LOG_PATH, ec);
    if (ec || sz <= MAX_LOG_FILE_BYTES) {
        return;
    }

    for (int i = LOG_ROTATE_COUNT - 1; i >= 1; --i) {
        auto from = std::format("{}.{}", LOG_PATH, i);
        auto to = std::format("{}.{}", LOG_PATH, i + 1);
        std::filesystem::rename(from, to, ec);
    }
    std::filesystem::rename(LOG_PATH, std::format("{}.1", LOG_PATH), ec);
}

void Log::Init()
{
    std::filesystem::create_directories("logs");
    rotateLogFile();
    s_LogFile = fopen(LOG_PATH, "a");
    s_StderrColor = AM_ISATTY(AM_FILENO(stderr)) != 0;

#ifdef NDEBUG
    s_Level = LogLevel::INFO;
#else
    s_Level = LogLevel::TRACE;
#endif

    WriteV(LogLevel::INFO, "Log", "Logger initialized");
}

void Log::Shutdown()
{
    WriteV(LogLevel::INFO, "Log", "Shutting down logger");
    if (s_LogFile != nullptr) {
        fclose(s_LogFile);
        s_LogFile = nullptr;
    }
    s_RecentLogs.clear();
}

void Log::WriteV(LogLevel level, std::string_view tag, std::string msg)
{
    if (level < s_Level) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    struct tm timeinfo{};
    AM_LOCALTIME(&timeinfo, &tt);

    char shortTs[16];
    strftime(shortTs, sizeof(shortTs), "%H:%M:%S", &timeinfo);

    const char *tag_s = levelTag(level);

    if (s_StderrColor) {
        fprintf(stderr, "%s[%s.%03d] [%s] %s: %s\033[0m\n", levelColor(level), shortTs, (int)ms.count(), tag_s,
                std::string(tag).c_str(), msg.c_str());
    } else {
        fprintf(stderr, "[%s.%03d] [%s] %s: %s\n", shortTs, (int)ms.count(), tag_s, std::string(tag).c_str(), msg.c_str());
    }

    if (s_LogFile != nullptr) {
        char fullTs[32];
        strftime(fullTs, sizeof(fullTs), "%Y-%m-%d %H:%M:%S", &timeinfo);
        fprintf(s_LogFile, "[%s.%03d] [%s] %s: %s\n", fullTs, (int)ms.count(), tag_s, std::string(tag).c_str(), msg.c_str());
        if (level >= LogLevel::INFO) {
            fflush(s_LogFile);
        }
    }

    LogMessage logMsg;
    logMsg.message = std::format("[{}.{:03d}] [{}] {}: {}", shortTs, (int)ms.count(), tag_s, tag, msg);
    logMsg.level = level;
    logMsg.timestamp = std::format("{}.{:03d}", shortTs, (int)ms.count());

    s_RecentLogs.push_back(std::move(logMsg));
    if (s_RecentLogs.size() > s_MaxRecentLogs) {
        s_RecentLogs.erase(s_RecentLogs.begin());
    }
}

} // namespace Amethyst
