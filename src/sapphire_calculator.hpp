#pragma once
#include <string>
#include <cstring>
#include <vector>
#include <functional>
#include <stdexcept>

namespace Sapphire
{
    class CalcError : public std::runtime_error
    {
    public:
        explicit CalcError(const std::string& msg)
            : std::runtime_error(msg)
            {}
    };


    struct CalcToken
    {
        const std::string text;
        int offset = -1;

        explicit CalcToken(const std::string& _text, int _offset)
            : text(_text)
            , offset(_offset)
            {}
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
            int front = -1;

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
                    front = i;
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
                    front = i;
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
                    front = i++;
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


    template <typename value_t, std::size_t stackSize = 1000>
    class Calculator
    {
    public:
        using func_t = std::function<void()>;

    private:
        enum class ScanMode
        {
            Opcode,
            Literal,
        };

        ScanMode mode = ScanMode::Opcode;

        // Lower-case letters are used as variables.
        value_t vars[26]{};       // 0='a', 1='b', ..., 25='z'.

        // Other ASCII characters can be user-defined functions.
        func_t funcs[0x80]{};

        // An evaluation stack of user-defined value types.
        std::vector<value_t> stack;
        std::string literal;

    protected:
        virtual value_t parse(const std::string& text)
        {
            throw CalcError("Literal is not supported: " + text);
        }

    public:
        explicit Calculator()
        {
            stack.reserve(stackSize);
            initialize();
        }

        void initialize()
        {
            mode = ScanMode::Opcode;
            stack.clear();
            literal.clear();

            for (int i = 0; i < 26; ++i)
                vars[i] = value_t{};

            for (int i = 0; i < 0x80; ++i)
                funcs[i] = func_t{};
        }

        static bool IsVarName(char c)
        {
            return (c >= 'a') && (c <= 'z');
        }

        static bool IsOperator(char c)
        {
            return
                (c >= 0) &&
                (c <= 0x7f) &&
                (
                    ((c >= 'A') && (c <= 'Z')) ||
                    strchr("+-*/&^%$#@!?<>,=~:", c)
                );
        }

        bool defineVariable(char symbol, value_t value)
        {
            if (symbol >= 'a' && symbol <= 'z')
            {
                vars[symbol - 'a'] = value;
                return true;
            }
            return false;   // invalid symbol character
        }

        bool defineFunction(char symbol, func_t func)
        {
            if (IsOperator(symbol))
            {
                funcs[static_cast<std::size_t>(symbol)] = func;
                return true;
            }
            return false;
        }

        bool defineMacro(char symbol, const std::string command)
        {
            return defineFunction(symbol,
                [this, command]()
                {
                    this->execute(command.c_str());
                }
            );
        }

        int stackHeight() const
        {
            return static_cast<int>(stack.size());
        }

        void push(const value_t& value)
        {
            if (stack.size() >= stackSize)
                throw CalcError("Stack overflow");

            stack.push_back(value);
        }

        const value_t& peek() const
        {
            if (stack.empty())
                throw CalcError("Attempt to access the top of an empty stack.");

            return stack.back();
        }

        value_t pop()
        {
            if (stack.empty())
                throw CalcError("Attempt to pop from empty stack.");

            value_t x = stack.back();
            stack.pop_back();
            return x;
        }

        void execute(char c)
        {
            if (mode == ScanMode::Opcode)
            {
                if (c == '{')
                {
                    mode = ScanMode::Literal;
                    return;
                }

                const std::size_t opcode = static_cast<std::size_t>(c);
                if (0 < opcode && opcode < 0x80)
                {
                    if (opcode >= 'a' && opcode <= 'z')
                    {
                        push(vars[opcode - 'a']);
                        return;
                    }

                    if (opcode == ';')
                    {
                        push(peek());       // duplicate top-of-stack
                        return;
                    }

                    if (func_t& f = funcs[static_cast<std::size_t>(opcode)])
                    {
                        f();
                        return;
                    }
                }
            }
            else if (mode == ScanMode::Literal)
            {
                if (c == '}')
                {
                    // Convert the accumulated literal into a value and push that on the stack.
                    value_t v = parse(literal);
                    push(v);
                    literal.clear();
                    mode = ScanMode::Opcode;
                }
                else
                {
                    literal += c;
                }
                return;
            }
            throw CalcError(std::string("Invalid opcode: ") + c);
        }

        void execute(const char *postfix)
        {
            mode = ScanMode::Opcode;
            if (postfix)
            {
                for (int i = 0; postfix[i]; ++i)
                    execute(postfix[i]);

                if (mode != ScanMode::Opcode)
                    throw CalcError(std::string("Command has unterminated literal: '") + postfix + std::string("'"));
            }
        }
    };


    template <typename real_t>
    class RealCalculator : public Calculator<real_t>
    {
    public:
        explicit RealCalculator()
        {
            this->defineFunction('+',
                [this]
                {
                    real_t b = this->pop();
                    real_t a = this->pop();
                    this->push(a + b);
                }
            );

            this->defineFunction('-',
                [this]
                {
                    real_t b = this->pop();
                    real_t a = this->pop();
                    this->push(a - b);
                }
            );

            this->defineFunction('*',
                [this]
                {
                    real_t b = this->pop();
                    real_t a = this->pop();
                    this->push(a * b);
                }
            );

            this->defineFunction('/',
                [this]
                {
                    real_t b = this->pop();
                    real_t a = this->pop();
                    this->push(a / b);
                }
            );

            this->defineFunction('N',
                [this]
                {
                    real_t a = this->pop();
                    this->push(-a);
                }
            );
        }

        float parse(const std::string& text) override
        {
            float value{};
            if (1 != sscanf(text.c_str(), "%g", &value))
                throw CalcError("Invalid floating point literal: '" + text + "'");

            return value;
        }
    };
}
