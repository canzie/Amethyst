/**
 * @file aml_parser.h
 * @brief Parser for the AML (Amethyst Markup Language) layout format
 *
 * Produces a tree of AmlNode from a tokenized .aml source.
 * Attribute values are stored as typed variants (int, float, string, bool, array).
 */

#ifndef AMETHYST__AML_PARSER_H
#define AMETHYST__AML_PARSER_H

#include "parsers/aml/aml_tokenizer.h"

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Amethyst {

struct AmlValue;

using AmlArray = std::vector<AmlValue>;

struct AmlValue {
    std::variant<int64_t, double, bool, std::string, AmlArray> data;

    bool isInt() const { return std::holds_alternative<int64_t>(data); }
    bool isFloat() const { return std::holds_alternative<double>(data); }
    bool isBool() const { return std::holds_alternative<bool>(data); }
    bool isString() const { return std::holds_alternative<std::string>(data); }
    bool isArray() const { return std::holds_alternative<AmlArray>(data); }

    int64_t asInt() const { return std::get<int64_t>(data); }
    double asFloat() const { return std::get<double>(data); }
    bool asBool() const { return std::get<bool>(data); }
    const std::string &asString() const { return std::get<std::string>(data); }
    const AmlArray &asArray() const { return std::get<AmlArray>(data); }

    /**
     * @brief Returns a numeric value as double regardless of whether it was
     * parsed as int or float. Useful for properties that accept either.
     */
    double asNumber() const;
};

struct AmlNode {
    std::string tag;
    std::unordered_map<std::string, AmlValue> attributes;
    std::vector<AmlNode> children;
    uint32_t line = 0;
    uint32_t column = 0;
};

struct AmlParseResult {
    std::vector<AmlNode> roots;
    std::string error;
    uint32_t errorLine = 0;
    uint32_t errorColumn = 0;

    bool ok() const { return error.empty(); }
};

/**
 * @brief Parses AML token streams into a node tree.
 *
 * Supports multiple root elements. Each element can have typed attributes
 * and nested child elements.
 */
class AmlParser {
  public:
    AmlParseResult parse(const std::vector<AmlToken> &tokens);

  private:
    const std::vector<AmlToken> *m_tokens = nullptr;
    size_t m_pos = 0;

    const AmlToken &current() const;
    const AmlToken &peek() const;
    bool atEnd() const;
    const AmlToken &advance();
    bool expect(AmlTokenType type, const std::string &context);

    AmlNode parseElement();
    AmlValue parseValue();
    AmlArray parseArray();

    std::string m_error;
    uint32_t m_errorLine = 0;
    uint32_t m_errorColumn = 0;

    void setError(const std::string &msg, const AmlToken &at);
    bool hasError() const { return !m_error.empty(); }
};

} // namespace Amethyst

#endif // AMETHYST__AML_PARSER_H
