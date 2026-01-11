/*
 * Amethyst logging implementation
 */

#include "logging/log.h"

#include <spdlog/sinks/callback_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/pattern_formatter.h>

namespace Amethyst {

std::shared_ptr<spdlog::logger> Log::s_Logger;
std::vector<LogMessage> Log::s_RecentLogs;
size_t Log::s_MaxRecentLogs = 1000;

void Log::Init()
{
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("%^[%T] [%l] %v%$");

    auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "logs/amethyst.log", 5 * 1024 * 1024, 3);
    fileSink->set_pattern("[%Y-%m-%d %T.%e] [%l] %v");

    auto callbackSink = std::make_shared<spdlog::sinks::callback_sink_mt>(
        [](const spdlog::details::log_msg &msg) { Log::LogCallback(msg); });

    std::vector<spdlog::sink_ptr> sinks = {consoleSink, fileSink, callbackSink};

    s_Logger = std::make_shared<spdlog::logger>("AMETHYST", sinks.begin(), sinks.end());
    s_Logger->set_level(spdlog::level::trace);
    s_Logger->flush_on(spdlog::level::info);
    spdlog::register_logger(s_Logger);

#ifdef NDEBUG
    SetLogLevel(spdlog::level::info);
#else
    SetLogLevel(spdlog::level::trace);
#endif

    s_Logger->info("Logger initialized");
}

void Log::Shutdown()
{
    s_Logger->info("Shutting down logger");
    s_Logger.reset();
    s_RecentLogs.clear();
    spdlog::shutdown();
}

void Log::SetLogLevel(spdlog::level::level_enum level)
{
    s_Logger->set_level(level);
}

void Log::LogCallback(const spdlog::details::log_msg &msg)
{
    spdlog::memory_buf_t formatted;
    spdlog::pattern_formatter formatter("[%T.%e] [%^%l%$] %v");
    formatter.format(msg, formatted);

    LogMessage logMsg;
    logMsg.message = fmt::to_string(formatted);
    logMsg.level = msg.level;

    char timestamp[64];
    time_t rawtime;
    struct tm timeinfo;
    time(&rawtime);
#ifdef _WIN32
    localtime_s(&timeinfo, &rawtime);
#else
    localtime_r(&rawtime, &timeinfo);
#endif
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);
    logMsg.timestamp = timestamp;

    s_RecentLogs.push_back(logMsg);

    if (s_RecentLogs.size() > s_MaxRecentLogs) {
        s_RecentLogs.erase(s_RecentLogs.begin());
    }
}

} // namespace Amethyst
