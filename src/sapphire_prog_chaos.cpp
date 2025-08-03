#include "sapphire_prog_chaos.hpp"

namespace Sapphire
{
    SlopeVector ProgOscillator::slopes(double x, double y, double z) const
    {
        double vx = 0;
        double vy = 0;
        double vz = 0;

        try
        {
            for (int i = 0; i < ParamCount; ++i)
            {
                int varIndex = 'a' + i;
                double value = paramValue(i);
                prog.setVar(varIndex, value);
            }

            prog.setVar('x', x);
            prog.setVar('y', y);
            prog.setVar('z', z);
            prog.run();

            if (prog.outputs.size() == 3)
            {
                vx = prog.reg.at(prog.outputs[0]);
                vy = prog.reg.at(prog.outputs[1]);
                vz = prog.reg.at(prog.outputs[2]);
            }
        }
        catch (const CalcError& ex)
        {
            // FIXFIXFIX: capture and report error.
            printf("ProgOscillator::slopes EXCEPTION: %s\n", ex.what());
            vx = vy = vz = 0;
        }

        return SlopeVector(vx, vy, vz);
    }


    void BytecodeProgram::defineVariables(calc_expr_t expr)
    {
        if (expr->isFunctionCall)
            throw CalcError("Function calls not yet implemented");

        if (expr->children.size() == 0)
        {
            if (expr->token.isIdentifier())
            {
                if (expr->token.text.size() != 1)
                    throw CalcError("Variable name must be one letter: " + expr->token.text);

                const int v = expr->token.text[0];
                if (varIndex[v] < 0)
                {
                    // This is the first time we have seen this variable.
                    // Allocate a new register position for it.
                    varIndex[v] = allocateRegister();
                }
            }
        }
        else
        {
            for (calc_expr_t child : expr->children)
                defineVariables(child);
        }
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
        {
            const int v = expr->variableIndex();
            return varIndex[v];
        }

        if (expr->token.isNumericLiteral())
        {
            const double value = expr->token.numericValue();
            return allocateRegister(value);
        }

        // Pre-allocate a location for the result.
        int r = allocateRegister();
        int a, b, c, n, z;

        if (expr->isUnary("-"))
        {
            // Unary negation: -a ==> (-1)*a + 0
            a = compile(expr->children[0]);
            n = negativeOneRegister();
            z = zeroRegister();
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
                    n = positiveOneRegister();
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
                n = negativeOneRegister();
                return emit(r, n, a, b);
            }
            else if (expr->isBinary("*"))
            {
                // a*b ==> a*b + 0
                a = compile(left);
                b = compile(right);
                n = zeroRegister();
                return emit(r, a, b, n);
            }
            else if (expr->isBinary("/"))
            {
                // FIXFIXFIX: allow dividing by a numeric constant.
                throw CalcError("Division is not yet supported.");
            }
            else if (expr->isBinary("^"))
            {
                // In a^b, b must be a positive integer literal.
                if (right->token.isNumericLiteral())
                {
                    const double expFloat = right->token.numericValue();
                    const int exponent = static_cast<int>(std::round(expFloat));

                    a = compile(left);
                    z = zeroRegister();
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
                        throw CalcError("Exponent " + std::to_string(exponent) + " is not yet supported.");
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
