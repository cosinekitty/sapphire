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
    public:
        static constexpr int ParamCount = 4;        // knobs are 'a', 'b', 'c', 'd'.

    private:
        mutable prog_calc_t calc;
        std::string vxPostfix;
        std::string vyPostfix;
        std::string vzPostfix;
        double param[ParamCount]{};

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
                postfix.clear();
                return CompileResult::Fail(ex.what());
            }
        }

        double eval(const std::string& postfix) const
        {
            calc.clearStack();
            calc.execute(postfix);
            if (calc.stackHeight() != 1)
                throw CalcError("Postfix resulted in stack height = " + std::to_string(calc.stackHeight()));
            return calc.pop();
        }

    protected:
        SlopeVector slopes(double x, double y, double z) const override;

    public:
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
        {
            ChaoticOscillator_initialize();
        }

        void ChaoticOscillator_initialize()
        {
            // FIXFIXFIX: goofy values. User needs to be able to define initial state.
            paramValue(0) = 0.2;
            paramValue(1) = 0.2;
            paramValue(2) = 7.0;
            paramValue(3) = 0.1;
        }

        double paramValue(int index) const
        {
            if (index >= 0 && index < ParamCount)
                return param[index];
            throw std::range_error(std::string("paramValue(const): invalid index=") + std::to_string(index));
        }

        double& paramValue(int index)
        {
            if (index >= 0 && index < ParamCount)
                return param[index];
            throw std::range_error(std::string("paramValue: invalid index=") + std::to_string(index));
        }

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
