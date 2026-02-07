#include "parsers/aml/aml_parser.h"

#include <charconv>

namespace Amethyst {

double AmlValue::asNumber() const
{
    if (isInt()) return static_cast<double>(asInt());
    return asFloat();
}

const AmlToken &AmlParser::current() const
{
    return (*m_tokens)[m_pos];
}

const AmlToken &AmlParser::peek() const
{
    if (m_pos + 1 >= m_tokens->size()) return m_tokens->back();
    return (*m_tokens)[m_pos + 1];
}

bool AmlParser::atEnd() const
{
    return m_pos >= m_tokens->size() || current().type == AmlTokenType::END_OF_FILE;
}

const AmlToken &AmlParser::advance()
{
    const AmlToken &tok = current();
    if (!atEnd()) m_pos++;
    return tok;
}

bool AmlParser::expect(AmlTokenType type, const std::string &context)
{
    if (atEnd() || current().type != type) {
        const AmlToken &tok = current();
        setError("Expected " + context + ", got '" + std::string(tok.text) + "'", tok);
        return false;
    }
    advance();
    return true;
}

void AmlParser::setError(const std::string &msg, const AmlToken &at)
{
    if (hasError()) return;
    m_error = "[" + std::to_string(at.line) + ":" + std::to_string(at.column) + "] " + msg;
    m_errorLine = at.line;
    m_errorColumn = at.column;
}

AmlParseResult AmlParser::parse(const std::vector<AmlToken> &tokens)
{
    m_tokens = &tokens;
    m_pos = 0;
    m_error.clear();
    m_errorLine = 0;
    m_errorColumn = 0;

    AmlParseResult result;

    while (!atEnd() && !hasError()) {
        if (current().type != AmlTokenType::TAG_OPEN) {
            setError("Expected '<', got '" + std::string(current().text) + "'", current());
            break;
        }
        result.roots.push_back(parseElement());
    }

    if (hasError()) {
        result.error = std::move(m_error);
        result.errorLine = m_errorLine;
        result.errorColumn = m_errorColumn;
        result.roots.clear();
    }

    return result;
}

AmlNode AmlParser::parseElement()
{
    AmlNode node;

    const AmlToken &openTok = current();
    node.line = openTok.line;
    node.column = openTok.column;
    advance(); // consume <

    if (atEnd() || current().type != AmlTokenType::IDENTIFIER) {
        setError("Expected tag name after '<'", current());
        return node;
    }
    node.tag = std::string(advance().text);

    // Parse attributes until > or />
    while (!atEnd() && !hasError()) {
        AmlTokenType t = current().type;
        if (t == AmlTokenType::TAG_CLOSE || t == AmlTokenType::SLASH_TAG_CLOSE) break;

        if (t != AmlTokenType::IDENTIFIER) {
            setError("Expected attribute name or '>' in <" + node.tag + ">, got '" + std::string(current().text) + "'", current());
            return node;
        }

        std::string attrName = std::string(advance().text);

        if (atEnd() || current().type != AmlTokenType::EQUALS) {
            setError("Expected '=' after attribute '" + attrName + "' in <" + node.tag + ">", current());
            return node;
        }
        advance(); // consume =

        if (atEnd()) {
            setError("Expected value for attribute '" + attrName + "' in <" + node.tag + ">", current());
            return node;
        }

        AmlValue val = parseValue();
        if (hasError()) return node;

        node.attributes[std::move(attrName)] = std::move(val);
    }

    if (atEnd()) {
        setError("Unterminated tag <" + node.tag + ">", current());
        return node;
    }

    if (current().type == AmlTokenType::SLASH_TAG_CLOSE) {
        advance(); // consume />
        return node;
    }

    advance(); // consume >

    // Parse children until </tag>
    while (!atEnd() && !hasError()) {
        if (current().type == AmlTokenType::TAG_OPEN_SLASH) break;

        if (current().type != AmlTokenType::TAG_OPEN) {
            setError("Expected child element or '</" + node.tag + ">', got '" + std::string(current().text) + "'", current());
            return node;
        }
        node.children.push_back(parseElement());
    }

    if (hasError()) return node;

    if (atEnd() || current().type != AmlTokenType::TAG_OPEN_SLASH) {
        setError("Expected closing tag '</" + node.tag + ">'", current());
        return node;
    }
    advance(); // consume </

    if (atEnd() || current().type != AmlTokenType::IDENTIFIER) {
        setError("Expected tag name in closing tag '</" + node.tag + ">'", current());
        return node;
    }

    const AmlToken &closingName = advance();
    if (closingName.text != node.tag) {
        setError("Mismatched closing tag: expected '</" + node.tag + ">', got '</" + std::string(closingName.text) + ">'",
                 closingName);
        return node;
    }

    if (!expect(AmlTokenType::TAG_CLOSE, "'>' in closing tag '</" + node.tag + ">'")) {
        return node;
    }

    return node;
}

AmlValue AmlParser::parseValue()
{
    const AmlToken &tok = current();

    switch (tok.type) {
    case AmlTokenType::STRING: {
        advance();
        return AmlValue{std::string(tok.text)};
    }

    case AmlTokenType::INTEGER: {
        advance();
        int64_t val = 0;
        std::from_chars(tok.text.data(), tok.text.data() + tok.text.size(), val);
        return AmlValue{val};
    }

    case AmlTokenType::FLOAT: {
        advance();
        double val = 0.0;
        std::from_chars(tok.text.data(), tok.text.data() + tok.text.size(), val);
        return AmlValue{val};
    }

    case AmlTokenType::BOOL: {
        advance();
        return AmlValue{tok.text == "true"};
    }

    case AmlTokenType::BRACKET_OPEN:
        return AmlValue{parseArray()};

    default:
        setError("Expected value, got '" + std::string(tok.text) + "'", tok);
        return AmlValue{0LL};
    }
}

AmlArray AmlParser::parseArray()
{
    advance(); // consume [
    AmlArray arr;

    if (!atEnd() && current().type == AmlTokenType::BRACKET_CLOSE) {
        advance(); // empty array
        return arr;
    }

    while (!atEnd() && !hasError()) {
        arr.push_back(parseValue());
        if (hasError()) return arr;

        if (current().type == AmlTokenType::BRACKET_CLOSE) {
            advance();
            return arr;
        }

        if (current().type != AmlTokenType::COMMA) {
            setError("Expected ',' or ']' in array, got '" + std::string(current().text) + "'", current());
            return arr;
        }
        advance(); // consume ,
    }

    if (!hasError()) {
        setError("Unterminated array", current());
    }
    return arr;
}

} // namespace Amethyst
