#include "sapphire_calcparser.hpp"

namespace Sapphire
{
    class CalcParser
    {
    private:
        CalcScanner scanner;

        calc_expr_t leaf(const CalcToken* token)
        {
            return std::make_shared<CalcExpr>(*token);
        }

        calc_expr_t unary(const CalcToken* optoken, calc_expr_t child)
        {
            calc_expr_t parent = leaf(optoken);
            parent->children.push_back(child);
            return parent;
        }

        calc_expr_t binary(const CalcToken* optoken, calc_expr_t left, calc_expr_t right)
        {
            calc_expr_t parent = leaf(optoken);
            parent->children.push_back(left);
            parent->children.push_back(right);
            return parent;
        }

    public:
        explicit CalcParser(const std::string& text)
            : scanner(text)
            {}

        calc_expr_t expr()
        {
            // expr ::= term { addop term }
            // addop ::= '+' | '-'
            calc_expr_t a = term();
            while (scanner.nextTokenIs("+") || scanner.nextTokenIs("-"))
            {
                auto op = scanner.requireToken();
                calc_expr_t b = term();
                a = binary(op, a, b);
            }
            return a;
        }

        calc_expr_t term()
        {
            // term ::= factor { mulop factor }
            // mulop ::= '*' | '/'
            calc_expr_t a = factor();
            while (scanner.nextTokenIs("*") || scanner.nextTokenIs("/"))
            {
                auto op = scanner.requireToken();
                calc_expr_t b = factor();
                a = binary(op, a, b);
            }
            return a;
        }

        calc_expr_t factor()
        {
            // factor ::= atom [ ^ factor ]
            calc_expr_t a = atom();
            if (scanner.nextTokenIs("^"))
            {
                auto op = scanner.requireToken();
                calc_expr_t b = factor();
                a = binary(op, a, b);
            }
            return a;
        }

        calc_expr_t atom()
        {
            // atom ::=
            //     numeric |
            //     ident [ '(' [ expr { ',' expr } ] ')' ] |
            //     '(' expr ')' |
            //     '+' {'+'} atom
            //     '-' atom
            auto token = scanner.requireToken();

            // Unary positives are ignored... just use the value of the ultimate child expression.
            while (token->text == "+")
                token = scanner.requireToken();

            // Unary negative is a valid operator.
            if (token->text == "-")
                return unary(token, atom());

            if (token->text == "(")
            {
                auto child = expr();
                scanner.requireToken(")");
                return child;
            }

            if (token->isNumericLiteral())
                return leaf(token);

            if (token->isIdentifier())
            {
                auto ident = leaf(token);
                if (scanner.nextTokenIs("("))
                {
                    ident->isFunctionCall = true;    // distinguish "fred" from "fred()", both of which have zero children
                    scanner.getNextToken();
                    while (!scanner.nextTokenIs(")"))
                    {
                        auto arg = expr();
                        ident->children.push_back(arg);
                        if (!scanner.nextTokenIs(","))
                            break;
                        scanner.getNextToken();
                    }
                }
                return ident;
            }

            throw ParseError("Syntax error: cannot parse atom", *token);
        }

        void finalize()
        {
            if (scanner.moreTokensAvailable())
                throw ParseError(std::string("Syntax error"), *scanner.peekNextToken());
        }
    };


    calc_expr_t CalcParseNumericExpression(std::string text)
    {
        CalcParser parser(text);
        calc_expr_t expr = parser.expr();
        parser.finalize();
        return expr;
    }
}
