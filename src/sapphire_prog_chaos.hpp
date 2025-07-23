#pragma once
#include "chaos.hpp"
#include "sapphire_calcparser.hpp"

namespace Sapphire
{
    using prog_calc_t = RealCalculator<double>;

    struct CompileResult
    {
        std::string postfix;
        std::string message;

        bool failure() const
        {
            return postfix.empty();
        }

        bool success() const
        {
            return !failure();
        }

        static CompileResult Success(std::string postfix)
        {
            CompileResult result;
            result.postfix = postfix;
            return result;
        }

        static CompileResult Fail(std::string message)
        {
            CompileResult result;
            result.message = message;
            return result;
        }
    };

    class ProgOscillator : public ChaoticOscillator
    {
    private:
        prog_calc_t calc;
        std::string vxPostfix;
        std::string vyPostfix;
        std::string vzPostfix;

        CompileResult compile(std::string& postfix, std::string infix)
        {
            try
            {
                auto expr = CalcParseNumericExpression(infix);
                postfix = expr->postfixNotation();
                // FIXFIXFIX: verify variables/params: x y z a b c d
                return CompileResult::Success(postfix);
            }
            catch (const CalcError& ex)
            {
                return CompileResult::Fail(ex.what());
            }
        }

    protected:
        SlopeVector slopes(double x, double y, double z) const override;

    public:
        static constexpr int KnobCount = 4;        // knobs are 'a', 'b', 'c', 'd'.

        explicit ProgOscillator(
            double _max_dt = 0.001,
            double _x0 = 0.11, double _y0 = 0.12, double _z0 = 0.13,
            double _xmin = -10, double _xmax = +10,
            double _ymin = -10, double _ymax = +10,
            double _zmin = -10, double _zmax = +10,
            double _xVelScale = 1, double _yVelScale = 1, double _zVelScale = 1
        )
            : ChaoticOscillator(
                _max_dt,
                _x0, _y0, _z0,
                _xmin, _xmax,
                _ymin, _ymax,
                _zmin, _zmax,
                _xVelScale, _yVelScale, _zVelScale)
                {}

        CompileResult xCompile(std::string infix)
        {
            return compile(vxPostfix, infix);
        }

        CompileResult yCompile(std::string infix)
        {
            return compile(vxPostfix, infix);
        }

        CompileResult zCompile(std::string infix)
        {
            return compile(vxPostfix, infix);
        }
    };
}
