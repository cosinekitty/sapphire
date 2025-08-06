#include "sapphire_prog_chaos.hpp"

namespace Sapphire
{
    SlopeVector ProgOscillator::slopes(double x, double y, double z) const
    {
        SlopeVector vec;
        try
        {
            if (prog.outputs.size() == 3)
            {
                prog.reg[inputRegister[0]] = x;
                prog.reg[inputRegister[1]] = y;
                prog.reg[inputRegister[2]] = z;

                prog.run();

                vec.mx = prog.reg[prog.outputs[0]];
                vec.my = prog.reg[prog.outputs[1]];
                vec.mz = prog.reg[prog.outputs[2]];
            }
        }
        catch (const CalcError& ex)
        {
            // FIXFIXFIX: capture and report error.
            printf("ProgOscillator::slopes EXCEPTION: %s\n", ex.what());
        }
        return vec;
    }


    bool BytecodeProgram::isConstantExpression(double& value, const calc_expr_t& expr) const
    {
        if (expr->token.isNumericLiteral())
        {
            value = expr->token.numericValue();
            return true;
        }

        if (expr->children.size() == 1)
        {
            const calc_expr_t& child = expr->children[0];
            if (double childValue{}; isConstantExpression(childValue, child))
            {
                if (expr->isUnary("-"))
                {
                    value = -childValue;
                    return true;
                }
            }
        }
        else if (expr->children.size() == 2)
        {
            const calc_expr_t& left  = expr->children[0];
            const calc_expr_t& right = expr->children[1];
            if (double leftValue{}; isConstantExpression(leftValue, left))
            {
                if (double rightValue{}; isConstantExpression(rightValue, right))
                {
                    if (expr->isBinary("+"))
                    {
                        value = leftValue + rightValue;
                        return true;
                    }
                    if (expr->isBinary("-"))
                    {
                        value = leftValue - rightValue;
                        return true;
                    }
                    if (expr->isBinary("*"))
                    {
                        value = leftValue * rightValue;
                        return true;
                    }
                    if (expr->isBinary("/"))
                    {
                        if (rightValue == 0.0)
                            throw CalcError("Division by zero in constant expression.");
                        value = leftValue / rightValue;
                        return true;
                    }
                    if (expr->isBinary("^"))
                    {
                        value = std::pow(leftValue, rightValue);
                        if (std::isfinite(value))
                            return true;

                        throw CalcError(
                            "Invalid constant exponentiation: (" +
                            std::to_string(leftValue) +
                            ") ^ (" +
                            std::to_string(rightValue)
                            + ")"
                        );
                    }
                }
            }
        }

        value = 0;
        return false;
    }


    int BytecodeProgram::compile(calc_expr_t expr)
    {
        // In this virtual machine, there is only one kind of instruction.
        // Every instruction executed has the following effect:
        // [r] = [a]*[b] + [c]
        // where r, a, b, c are integer indexes into a register array.
        // Each entry in the array stores a `double`.

        // Look at the top level(s) of the parse tree.
        // Match the ideal patterns first:
        // a*b + c
        // c + a*b

        // Recursive part: in general, child nodes (a, b, c)
        // are not variables but subexpressions.
        // Recursively compile each subexpression, resulting in
        // a register index where the answer will be stored.
        // The trivial case: it is a variable or a literal.
        // If a variable, define the variable as needed, then allocate
        // a register index for it.

        // Before allocating a register, look for symbols that
        // already map to a register.
        // This is the leaf of the recursion.
        if (expr->isVariable())
            return variableRegister(expr->variableChar());

        // Fold constant expressions into a single register.
        if (double value{}; isConstantExpression(value, expr))
            return literalRegister(value);

        // Pre-allocate a location for the result.
        int r = allocateRegister();     // FIXFIXFIX: allocate temporary explicitly; assist future disassembler
        int a, b, c, n, z;

        if (expr->isUnary("-"))
        {
            const auto& child = expr->children[0];
            a = compile(child);
            n = literalRegister(-1);
            z = literalRegister(0);
            return emit(r, n, a, z);
        }

        if (expr->children.size() == 2)
        {
            const auto& left  = expr->children[0];
            const auto& right = expr->children[1];
            if (expr->isBinary("+"))
            {
                if (left->isBinary("*"))
                {
                   // a*b + c
                   // left = [* a b]
                   // right = c
                   a = compile(left->children[0]);
                   b = compile(left->children[1]);
                   c = compile(right);
                   return emit(r, a, b, c);
                }
                if (right->isBinary("*"))
                {
                   // c + a*b
                   a = compile(right->children[0]);
                   b = compile(right->children[1]);
                   c = compile(left);
                   return emit(r, a, b, c);
                }
                // Fallback: a + b ==> 1*a + b
                {
                    n = literalRegister(+1);
                    a = compile(left);
                    b = compile(right);
                    return emit(r, n, a, b);
                }
            }
            else if (expr->isBinary("-"))
            {
                // Anytime we see subtraction, replace it with addition and -1 as a factor.
                // b - a ==> (-1)*a + b
                b = compile(left);
                a = compile(right);
                n = literalRegister(-1);
                return emit(r, n, a, b);
            }
            else if (expr->isBinary("*"))
            {
                // a*b ==> a*b + 0
                a = compile(left);
                b = compile(right);
                n = literalRegister(0);
                return emit(r, a, b, n);
            }
            else if (expr->isBinary("/"))
            {
                if (double denom{}; isConstantExpression(denom, right))
                {
                    if (denom == 0.0)
                        throw CalcError("Division by zero detected.");
                    // a/denom ==> (1/denom)*a + 0
                    n = literalRegister(1/denom);
                    z = literalRegister(0);
                    a = compile(left);
                    return emit(r, n, a, z);
                }
                throw CalcError("Division is not supported except when the denominator is a numeric constant.");
            }
            else if (expr->isBinary("^"))
            {
                // In a^b, b must be a positive integer literal.
                if (double expFloat{}; isConstantExpression(expFloat, right))
                {
                    const int exponent = static_cast<int>(std::round(expFloat));
                    if (static_cast<double>(exponent) != expFloat || exponent <= 0)
                        throw CalcError("Exponent must be a positive integer, not " + std::to_string(expFloat));

                    a = compile(left);
                    if (exponent == 1)
                        return a;

                    z = literalRegister(0);
                    switch (exponent)
                    {
                    case 2:
                        // a^2 ==>
                        //      r = a*a + 0
                        emit(r, a, a, z);
                        return r;

                    case 3:
                        // a^3 ==>
                        //      r = a*a + 0
                        //      r = r*a + 0
                        emit(r, a, a, z);
                        emit(r, r, a, z);
                        return r;

                    case 4:
                        // a^4 ==>
                        //      r = a*a + 0
                        //      r = r*r + 0
                        emit(r, a, a, z);
                        emit(r, r, r, z);
                        return r;

                    default:
                        throw CalcError("Exponent " + std::to_string(exponent) + " is not supported.");
                    }
                }
            }
        }

        throw CalcError("Code generation failure");
    }


    void BytecodeProgram::validate() const
    {
        for (const BytecodeInstruction& inst : func)
        {
            validateRegister(inst.r);
            validateRegister(inst.a);
            validateRegister(inst.b);
            validateRegister(inst.c);
        }
    }
}
