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
            if (!vxPostfix.empty() && !vyPostfix.empty() && !vzPostfix.empty())
            {
                calc.defineVariable('x', x);
                calc.defineVariable('y', y);
                calc.defineVariable('z', z);
                for (int i = 0; i < ParamCount; ++i)
                {
                    char name = static_cast<char>('a' + i);
                    const double p = paramValue(i);
                    calc.defineVariable(name, p);
                }
                vx = eval(vxPostfix);
                vy = eval(vyPostfix);
                vz = eval(vzPostfix);
            }
        }
        catch (const CalcError& ex)
        {
            // FIXFIXFIX: capture and report error.
            vx = vy = vz = 0;
            calc.clearStack();
        }

        return SlopeVector(vx, vy, vz);
    }


    BytecodeProgram BytecodeProgram::Compile(calc_expr_t expr)
    {
        BytecodeProgram prog;
        prog.defineVariables(expr);
        prog.compile(expr);
        return prog;
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
                    // Allocate a new register position for it and initialize to zero.
                    varIndex[v] = allocateRegister(0.0);
                }
            }
        }
        else
        {
            for (calc_expr_t child : expr->children)
                defineVariables(child);
        }
    }


    void BytecodeProgram::compile(calc_expr_t expr)
    {
        // In this virtual machine, there is only one instruction.
        // Every instruction executed has the following effect:
        // r := a*b + c
        // where r, a, b, c are integer indexes into a register array.
        // Each entry in the array stores a double-precision float.

        // Anytime we see subtraction, replace it with addition and another factor.
        // b - a ==> (-1)*a + b

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
        // FIXFIXFIX: Move compiler logic into BytecodeProgram?
    }
}
