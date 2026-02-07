/**
 * @file aml_tokenizer.h
 * @brief Tokenizer for the AML (Amethyst Markup Language) layout format
 */

#ifndef AMETHYST__AML_TOKENIZER_H
#define AMETHYST__AML_TOKENIZER_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Amethyst {

enum class AmlTokenType : uint8_t {
    TAG_OPEN,        // <
    TAG_OPEN_SLASH,  // </
    TAG_CLOSE,       // >
    SLASH_TAG_CLOSE, // />
    EQUALS,          // =
    IDENTIFIER,      // tag names, attribute names
    STRING,          // "quoted string"
    INTEGER,         // 42, -7
    FLOAT,           // 3.14, -0.5
    BOOL,            // true, false
    BRACKET_OPEN,    // [
    BRACKET_CLOSE,   // ]
    COMMA,           // ,
    END_OF_FILE,
};

struct AmlToken {
    AmlTokenType type;
    std::string_view text;
    uint32_t line;
    uint32_t column;
};

struct AmlTokenizeResult {
    std::vector<AmlToken> tokens;
    std::string error;
    uint32_t errorLine = 0;
    uint32_t errorColumn = 0;

    bool ok() const { return error.empty(); }
};

/**
 * @brief Tokenizes .aml source text into a flat token stream.
 *
 * The input string_view must outlive the returned tokens, as token
 * text fields point into the original source.
 */
class AmlTokenizer {
  public:
    AmlTokenizeResult tokenize(std::string_view source);

  private:
    std::string_view m_source;
    size_t m_pos = 0;
    uint32_t m_line = 1;
    uint32_t m_column = 1;

    char peek() const;
    char peekNext() const;
    char advance();
    bool atEnd() const;
    void skipWhitespace();
    void skipComment();

    AmlToken makeToken(AmlTokenType type, std::string_view text, uint32_t line, uint32_t col) const;
    std::string makeError(const std::string &msg) const;
};

} // namespace Amethyst

#endif // AMETHYST__AML_TOKENIZER_H
