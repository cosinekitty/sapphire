#include "sapphire_prog_chaos.hpp"

namespace Sapphire
{
    void ProgOscillator::updateParameters()
    {
        // Update the parameters at audio rate.
        // Only one parameter can be varied at a time.
        // The parameter's upper case symbol is shown on top of the CHAOS knob.
        // All other parameters revert to their default value.

        for (int p = 0; p < ProgOscillator::ParamCount; ++p)
            paramValue(p) = (p == mode) ? knobMap[p].paramValue(knob) : knobMap[p].fallbackValue();
    }


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


    int BytecodeProgram::gencode(calc_expr_t expr, int depth)
    {
        if (depth > 100)
            throw CalcError("Recursion limit reached while compiling expression.");

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
        {
            char symbol = expr->variableChar();
            if (isBadVariable(symbol))
                throw CalcError("Bad variable: " + std::string{symbol});
            lowercaseVarsMask |= LowercaseMask(symbol);
            return variableRegister(symbol);
        }

        // Fold constant expressions into a single register.
        if (double value{}; isConstantExpression(value, expr))
            return literalRegister(value);

        // Pre-allocate a location for the result.
        int r = allocateRegister();     // FIXFIXFIX: allocate temporary explicitly; assist future disassembler
        int a, b, c, n, z;

        if (expr->isUnary("-"))
        {
            const auto& child = expr->children[0];
            a = gencode(child, 1+depth);
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
                   a = gencode(left->children[0], 1+depth);
                   b = gencode(left->children[1], 1+depth);
                   c = gencode(right, 1+depth);
                   return emit(r, a, b, c);
                }
                if (right->isBinary("*"))
                {
                   // c + a*b
                   a = gencode(right->children[0], 1+depth);
                   b = gencode(right->children[1], 1+depth);
                   c = gencode(left, 1+depth);
                   return emit(r, a, b, c);
                }
                // Fallback: a + b ==> 1*a + b
                {
                    n = literalRegister(+1);
                    a = gencode(left,  1+depth);
                    b = gencode(right, 1+depth);
                    return emit(r, n, a, b);
                }
            }
            else if (expr->isBinary("-"))
            {
                // Anytime we see subtraction, replace it with addition and -1 as a factor.
                // b - a ==> (-1)*a + b
                b = gencode(left, 1+depth);
                a = gencode(right, 1+depth);
                n = literalRegister(-1);
                return emit(r, n, a, b);
            }
            else if (expr->isBinary("*"))
            {
                // a*b ==> a*b + 0
                a = gencode(left, 1+depth);
                b = gencode(right, 1+depth);
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
                    a = gencode(left, 1+depth);
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

                    a = gencode(left, 1+depth);
                    if (exponent == 1)
                        return a;

                    z = literalRegister(0);
                    switch (exponent)
                    {
                    case 2:
                        emit(r, a, a, z);   // r = a^2
                        return r;

                    case 3:
                        emit(r, a, a, z);   // r = a^2
                        emit(r, r, a, z);   // r = a^3
                        return r;

                    case 4:
                        emit(r, a, a, z);   // r = a^2
                        emit(r, r, r, z);   // r = a^4
                        return r;

                    case 5:
                        emit(r, a, a, z);   // r = a^2
                        emit(r, r, r, z);   // r = a^4
                        emit(r, r, a, z);   // r = a^5
                        return r;

                    case 6:
                        emit(r, a, a, z);   // r = a^2
                        emit(r, r, a, z);   // r = a^3
                        emit(r, r, r, z);   // r = a^6
                        return r;

                    case 7:
                        emit(r, a, a, z);   // r = a^2
                        emit(r, r, a, z);   // r = a^3
                        emit(r, r, r, z);   // r = a^6
                        emit(r, r, a, z);   // r = a^7
                        return r;

                    case 8:
                        emit(r, a, a, z);   // r = a^2
                        emit(r, r, r, z);   // r = a^4
                        emit(r, r, r, z);   // r = a^8
                        return r;

                    case 9:
                        emit(r, a, a, z);   // r = a^2
                        emit(r, r, r, z);   // r = a^4
                        emit(r, r, r, z);   // r = a^8
                        emit(r, r, a, z);   // r = a^9
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
