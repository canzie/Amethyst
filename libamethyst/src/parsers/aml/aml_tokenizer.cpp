#include "parsers/aml/aml_tokenizer.h"


namespace Amethyst {

char AmlTokenizer::peek() const
{
    if (atEnd()) return '\0';
    return m_source[m_pos];
}

char AmlTokenizer::peekNext() const
{
    if (m_pos + 1 >= m_source.size()) return '\0';
    return m_source[m_pos + 1];
}

char AmlTokenizer::advance()
{
    char c = m_source[m_pos++];
    if (c == '\n') {
        m_line++;
        m_column = 1;
    } else {
        m_column++;
    }
    return c;
}

bool AmlTokenizer::atEnd() const
{
    return m_pos >= m_source.size();
}

void AmlTokenizer::skipWhitespace()
{
    while (!atEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '#') {
            skipComment();
        } else {
            break;
        }
    }
}

void AmlTokenizer::skipComment()
{
    while (!atEnd() && peek() != '\n') {
        advance();
    }
}

AmlToken AmlTokenizer::makeToken(AmlTokenType type, std::string_view text, uint32_t line, uint32_t col) const
{
    return AmlToken{type, text, line, col};
}

std::string AmlTokenizer::makeError(const std::string &msg) const
{
    return "[" + std::to_string(m_line) + ":" + std::to_string(m_column) + "] " + msg;
}

AmlTokenizeResult AmlTokenizer::tokenize(std::string_view source)
{
    m_source = source;
    m_pos = 0;
    m_line = 1;
    m_column = 1;

    AmlTokenizeResult result;

    while (true) {
        skipWhitespace();
        if (atEnd()) break;

        uint32_t startLine = m_line;
        uint32_t startCol = m_column;
        size_t startPos = m_pos;
        char c = peek();

        switch (c) {
        case '<':
            advance();
            if (!atEnd() && peek() == '/') {
                advance();
                result.tokens.push_back(makeToken(AmlTokenType::TAG_OPEN_SLASH, m_source.substr(startPos, 2), startLine, startCol));
            } else {
                result.tokens.push_back(makeToken(AmlTokenType::TAG_OPEN, m_source.substr(startPos, 1), startLine, startCol));
            }
            continue;

        case '/':
            advance();
            if (!atEnd() && peek() == '>') {
                advance();
                result.tokens.push_back(
                    makeToken(AmlTokenType::SLASH_TAG_CLOSE, m_source.substr(startPos, 2), startLine, startCol));
            } else {
                result.error = makeError("Expected '>' after '/'");
                result.errorLine = startLine;
                result.errorColumn = startCol;
                return result;
            }
            continue;

        case '>':
            advance();
            result.tokens.push_back(makeToken(AmlTokenType::TAG_CLOSE, m_source.substr(startPos, 1), startLine, startCol));
            continue;

        case '=':
            advance();
            result.tokens.push_back(makeToken(AmlTokenType::EQUALS, m_source.substr(startPos, 1), startLine, startCol));
            continue;

        case '[':
            advance();
            result.tokens.push_back(makeToken(AmlTokenType::BRACKET_OPEN, m_source.substr(startPos, 1), startLine, startCol));
            continue;

        case ']':
            advance();
            result.tokens.push_back(makeToken(AmlTokenType::BRACKET_CLOSE, m_source.substr(startPos, 1), startLine, startCol));
            continue;

        case ',':
            advance();
            result.tokens.push_back(makeToken(AmlTokenType::COMMA, m_source.substr(startPos, 1), startLine, startCol));
            continue;

        case '"': {
            advance();
            size_t contentStart = m_pos;
            while (!atEnd() && peek() != '"') {
                if (peek() == '\\') {
                    advance();
                    if (atEnd()) {
                        result.error = makeError("Unterminated escape sequence in string");
                        result.errorLine = startLine;
                        result.errorColumn = startCol;
                        return result;
                    }
                }
                advance();
            }
            if (atEnd()) {
                result.error = makeError("Unterminated string literal");
                result.errorLine = startLine;
                result.errorColumn = startCol;
                return result;
            }
            size_t contentEnd = m_pos;
            advance();
            result.tokens.push_back(
                makeToken(AmlTokenType::STRING, m_source.substr(contentStart, contentEnd - contentStart), startLine, startCol));
            continue;
        }

        case '-':
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
            bool isFloat = false;
            if (c == '-') advance();

            if (atEnd() || peek() < '0' || peek() > '9') {
                result.error = makeError("Expected digit after '-'");
                result.errorLine = startLine;
                result.errorColumn = startCol;
                return result;
            }

            while (!atEnd() && peek() >= '0' && peek() <= '9') {
                advance();
            }

            if (!atEnd() && peek() == '.') {
                isFloat = true;
                advance();
                if (atEnd() || peek() < '0' || peek() > '9') {
                    result.error = makeError("Expected digit after decimal point");
                    result.errorLine = startLine;
                    result.errorColumn = startCol;
                    return result;
                }
                while (!atEnd() && peek() >= '0' && peek() <= '9') {
                    advance();
                }
            }

            std::string_view numText = m_source.substr(startPos, m_pos - startPos);
            result.tokens.push_back(makeToken(isFloat ? AmlTokenType::FLOAT : AmlTokenType::INTEGER, numText, startLine, startCol));
            continue;
        }

        default:
            break;
        }

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
            while (!atEnd()) {
                char ch = peek();
                if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_') {
                    advance();
                } else {
                    break;
                }
            }

            std::string_view word = m_source.substr(startPos, m_pos - startPos);
            if (word == "true" || word == "false") {
                result.tokens.push_back(makeToken(AmlTokenType::BOOL, word, startLine, startCol));
            } else {
                result.tokens.push_back(makeToken(AmlTokenType::IDENTIFIER, word, startLine, startCol));
            }
            continue;
        }

        result.error = makeError(std::string("Unexpected character '") + c + "'");
        result.errorLine = m_line;
        result.errorColumn = m_column;
        return result;
    }

    result.tokens.push_back(makeToken(AmlTokenType::END_OF_FILE, {}, m_line, m_column));
    return result;
}

} // namespace Amethyst
