#pragma once
#include <memory>
#include "sapphire_calculator.hpp"

namespace Sapphire
{
    struct CalcToken
    {
        const std::string text;
        int offset = -1;

        explicit CalcToken(const std::string& _text, int _offset)
            : text(_text)
            , offset(_offset)
            {}

        char front() const
        {
            return text.size() ? text.at(0) : '\0';
        }

        bool isNumericLiteral() const
        {
            const char f = front();
            return (f >= '0') && (f <= '9');
        }

        bool isIdentifier() const
        {
            const char f = front();
            return (f >= 'a' && f <= 'z') || (f >= 'A' && f <= 'Z') || (f == '_');
        }

        bool isPunctuation() const
        {
            return text.size() && !isNumericLiteral() && !isIdentifier();
        }
    };


    class CalcScanner
    {
    private:
        std::string text;
        int offset = 0;
        std::vector<CalcToken> tokens;

        void tokenize()
        {
            std::string tok;

            // token ::= identifier | punctuation | real
            // identifier ::= alpha { alphanum  }
            // alpha ::= 'a'..'z' | 'A'..'Z' | '_'
            // alphanum ::= alpha | '0'..'9'
            // punctuation ::= '*' | '/' | '-' | '+' | '^' | '(' | ')'
            const int len = static_cast<int>(text.size());
            for (int i = 0; i < len; )
            {
                char c = text[i];
                if (isspace(c))
                {
                    ++i;
                }
                else if ((c>='A' && c<='Z') || (c>='a' && c<='z') || (c=='_'))
                {
                    // identifier
                    int front = i;
                    tok.clear();
                    tok.push_back(c);
                    ++i;
                    while (i < len)
                    {
                        c = text[i];
                        if ((c>='A' && c<='Z') || (c>='a' && c<='z') || (c=='_') || (c>='0' && c<='9'))
                            tok.push_back(c);
                        else
                            break;
                        ++i;
                    }
                    tokens.push_back(CalcToken(tok, front));
                }
                else if (c>='0' && c<='9')
                {
                    // numeric
                    int front = i;
                    tok.clear();
                    tok.push_back(c);
                    ++i;

                    // Consume any remaining consecutive digits.
                    // Allow a maximum of one decimal point.
                    int dpcount = 0;
                    while (i < len)
                    {
                        c = text[i];
                        if (c == '.')
                        {
                            if (++dpcount > 1)
                                throw CalcError("Invalid character in expression");
                        }
                        else if (c >= '0' && c <= '9')
                        {
                            // OK
                        }
                        else
                            break;
                        tok.push_back(c);
                        ++i;
                    }

                    if (c == 'e' || c == 'E')
                    {
                        // Exponential notation.
                        tok.push_back(c);
                        if (i >= len)
                            throw CalcError("Unterminated exponent in numeric literal.");
                        ++i;
                        c = text[i];
                        if (c == '+' || c == '-')
                        {
                            tok.push_back(c);
                            ++i;
                        }
                        if (i >= len)
                            throw CalcError("Unterminated exponent in numeric literal.");
                        while (i < len)
                        {
                            c = text[i];
                            if (c >= '0' && c <= '9')
                                tok.push_back(c);
                            else
                                break;
                            ++i;
                        }
                    }

                    // Verify that the token is a valid floating point number.
                    float testValue{};
                    if (1 != sscanf(tok.c_str(), "%g", &testValue))
                        throw CalcError("Invalid numeric literal: '" + tok + "'");

                    tokens.push_back(CalcToken(tok, front));
                }
                else if (strchr("+-*/^()", c))
                {
                    int front = i++;
                    tok.clear();
                    tok.push_back(c);
                    tokens.push_back(CalcToken(tok, front));
                }
            }
        }

    public:
        explicit CalcScanner(const std::string& _text)
            : text(_text)
        {
            tokenize();
        }

        bool moreTokensAvailable() const
        {
            return offset < static_cast<int>(tokens.size());
        }

        const CalcToken* peekNextToken() const
        {
            if (moreTokensAvailable())
                return &tokens.at(offset);

            return nullptr;
        }

        const CalcToken* getNextToken()
        {
            if (const CalcToken* token = peekNextToken())
            {
                ++offset;
                return token;
            }
            return nullptr;
        }
    };


    struct CalcExpr
    {
        CalcToken token;
        std::vector<std::shared_ptr<CalcExpr>> children;

        explicit CalcExpr(const CalcToken& _token)
            : token(_token)
            {}
    };


    std::shared_ptr<CalcExpr> CalcParseNumeric(std::string text);
}