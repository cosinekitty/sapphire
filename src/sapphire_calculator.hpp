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


    template <typename value_t>
    class Calculator
    {
    public:
        using func_t = std::function<void()>;

    private:
        // Lower-case letters are used as variables.
        value_t vars[26]{};       // 0='a', 1='b', ..., 25='z'.
        func_t funcs[0x80]{};
        std::vector<value_t> stack;

    public:
        explicit Calculator()
        {
            stack.reserve(16);
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
                    strchr("+-*&^%$#@!/?<>,=~;:", c)
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
            throw CalcError(std::string("Invalid opcode: ") + c);
        }

        void execute(const char *postfix)
        {
            if (postfix)
                for (int i = 0; postfix[i]; ++i)
                    execute(postfix[i]);
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
        }
    };
}
