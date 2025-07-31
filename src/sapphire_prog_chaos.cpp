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


    BytecodeProgram CompileBytecode(calc_expr_t expr)
    {
        return BytecodeProgram();
    }
}
