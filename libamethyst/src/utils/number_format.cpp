#include "utils/number_format.h"

#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Amethyst {

// keeps a hostile "%99999999d" from asking snprintf for a huge buffer
static constexpr int MAX_FIELD_DIGITS = 2;

static size_t s_skipDigits(std::string_view s, size_t i, int &count)
{
    count = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])) != 0) {
        ++i;
        ++count;
    }
    return i;
}

std::optional<NumberFormat> NumberFormat::parse(std::string_view format, NumberKind kind)
{
    NumberFormat out;
    out.m_kind = kind;
    int conversions = 0;

    for (size_t i = 0; i < format.size(); ++i) {
        char c = format[i];
        out.m_format.push_back(c);
        if (c != '%') {
            continue;
        }
        if (i + 1 < format.size() && format[i + 1] == '%') {
            out.m_format.push_back('%');
            ++i;
            continue;
        }

        size_t j = i + 1;
        while (j < format.size() && std::strchr("-+ #0", format[j]) != nullptr) {
            out.m_format.push_back(format[j]);
            ++j;
        }
        int digits = 0;
        size_t widthEnd = s_skipDigits(format, j, digits);
        if (digits > MAX_FIELD_DIGITS) {
            return std::nullopt;
        }
        out.m_format.append(format.substr(j, widthEnd - j));
        j = widthEnd;
        if (j < format.size() && format[j] == '.') {
            size_t precisionEnd = s_skipDigits(format, j + 1, digits);
            if (digits > MAX_FIELD_DIGITS) {
                return std::nullopt;
            }
            out.m_format.append(format.substr(j, precisionEnd - j));
            j = precisionEnd;
        }
        // whatever length modifier the caller wrote is dropped and replaced with the one matching
        // the argument actually passed below
        while (j < format.size() && std::strchr("hlLqjzt", format[j]) != nullptr) {
            ++j;
        }
        if (j >= format.size()) {
            return std::nullopt;
        }

        char conv = format[j];
        if (kind == NumberKind::INTEGER && std::strchr("diouxX", conv) != nullptr) {
            out.m_format += "ll";
            out.m_unsigned = std::strchr("ouxX", conv) != nullptr;
        } else if (kind == NumberKind::FLOATING && std::strchr("fFeEgGaA", conv) != nullptr) {
            // double takes no length modifier
        } else {
            return std::nullopt;
        }
        out.m_format.push_back(conv);
        ++conversions;
        i = j;
    }

    if (conversions != 1) {
        return std::nullopt;
    }
    return out;
}

template <typename T> static std::string s_format(const std::string &fmt, T value)
{
    char buffer[128];
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    int n = std::snprintf(buffer, sizeof(buffer), fmt.c_str(), value);
    if (n < 0) {
        return {};
    }
    if (static_cast<size_t>(n) < sizeof(buffer)) {
        return std::string(buffer, static_cast<size_t>(n));
    }
    std::string out(static_cast<size_t>(n) + 1, '\0');
    std::snprintf(out.data(), out.size(), fmt.c_str(), value);
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    out.resize(static_cast<size_t>(n));
    return out;
}

std::string NumberFormat::format(long long value) const
{
    if (m_kind == NumberKind::FLOATING) {
        return format(static_cast<double>(value));
    }
    if (m_unsigned) {
        return s_format(m_format, static_cast<unsigned long long>(value));
    }
    return s_format(m_format, value);
}

std::string NumberFormat::format(double value) const
{
    if (m_kind == NumberKind::INTEGER) {
        // the format was checked for an integer conversion; feeding it a double would be UB
        if (std::isnan(value)) {
            return format(0LL);
        }
        if (value >= 9223372036854775808.0) {
            return format(LLONG_MAX);
        }
        if (value < -9223372036854775808.0) {
            return format(LLONG_MIN);
        }
        return format(static_cast<long long>(value));
    }
    return s_format(m_format, value);
}

} // namespace Amethyst
