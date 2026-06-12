/*
 * Amethyst logging utilities
 */

#ifndef AMETHYST__LOG_H
#define AMETHYST__LOG_H

#include <format>
#include <string>
#include <string_view>
#include <vector>

#if defined(_MSC_VER)
#define AM_FUNCTION_SIG __FUNCSIG__
#elif defined(__GNUC__) || defined(__clang__)
#define AM_FUNCTION_SIG __PRETTY_FUNCTION__
#else
#define AM_FUNCTION_SIG __func__
#endif

namespace Amethyst {

inline std::string ExtractFunctionName(const char *prettyFunction)
{
    std::string func(prettyFunction);

    size_t parenPos = func.find('(');
    if (parenPos != std::string::npos) {
        func = func.substr(0, parenPos);
    }

    size_t bracketPos = func.find('[');
    if (bracketPos != std::string::npos) {
        func = func.substr(0, bracketPos);
    }

    size_t firstScope = func.find("::");
    if (firstScope != std::string::npos && firstScope + 2 < func.length()) {
        return func.substr(firstScope + 2);
    }

    size_t lastSpace = func.find_last_of(" ");
    if (lastSpace != std::string::npos && lastSpace + 1 < func.length()) {
        return func.substr(lastSpace + 1);
    }

    return func;
}

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERR,
    CRITICAL
};

struct LogMessage {
    std::string message;
    LogLevel level;
    std::string timestamp;
};

class Log {
  public:
    static void Init();
    static void Shutdown();

    static const std::vector<LogMessage> &GetRecentLogs() { return s_RecentLogs; }
    static void ClearRecentLogs() { s_RecentLogs.clear(); }
    static void SetMaxRecentLogs(size_t count) { s_MaxRecentLogs = count; }
    static void SetLogLevel(LogLevel level) { s_Level = level; }

    static void WriteV(LogLevel level, std::string_view tag, std::string msg);

  private:
    static std::vector<LogMessage> s_RecentLogs;
    static size_t s_MaxRecentLogs;
    static LogLevel s_Level;
};

} // namespace Amethyst

#define AM_LOG_TRACE(...) \
    ::Amethyst::Log::WriteV(::Amethyst::LogLevel::TRACE, ::Amethyst::ExtractFunctionName(AM_FUNCTION_SIG), std::format(__VA_ARGS__))
#define AM_LOG_DEBUG(...) \
    ::Amethyst::Log::WriteV(::Amethyst::LogLevel::DEBUG, ::Amethyst::ExtractFunctionName(AM_FUNCTION_SIG), std::format(__VA_ARGS__))
#define AM_LOG_INFO(...) \
    ::Amethyst::Log::WriteV(::Amethyst::LogLevel::INFO, ::Amethyst::ExtractFunctionName(AM_FUNCTION_SIG), std::format(__VA_ARGS__))
#define AM_LOG_WARN(...) \
    ::Amethyst::Log::WriteV(::Amethyst::LogLevel::WARN, ::Amethyst::ExtractFunctionName(AM_FUNCTION_SIG), std::format(__VA_ARGS__))
#define AM_LOG_ERROR(...) \
    ::Amethyst::Log::WriteV(::Amethyst::LogLevel::ERR, ::Amethyst::ExtractFunctionName(AM_FUNCTION_SIG), std::format(__VA_ARGS__))
#define AM_LOG_CRITICAL(...)                                                                                  \
    ::Amethyst::Log::WriteV(::Amethyst::LogLevel::CRITICAL, ::Amethyst::ExtractFunctionName(AM_FUNCTION_SIG), \
                            std::format(__VA_ARGS__))

#endif // AMETHYST__LOG_H
