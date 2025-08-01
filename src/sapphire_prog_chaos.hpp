#pragma once
#include <cinttypes>
#include <vector>
#include <string>
#include "chaos.hpp"
#include "sapphire_calcparser.hpp"

namespace Sapphire
{
    using prog_calc_t = RealCalculator<double>;

    template <typename payload_t>
    struct TranslateResult
    {
        payload_t payload{};
        std::string message;

        bool success() const
        {
            return message.empty();
        }

        bool failure() const
        {
            return !success();
        }

        static TranslateResult Success(payload_t payload)
        {
            TranslateResult result;
            result.payload = payload;
            return result;
        }

        static TranslateResult Fail(std::string message)
        {
            TranslateResult result;
            result.message = message;
            return result;
        }
    };

    using PostfixResult = TranslateResult<std::string>;


    struct BytecodeInstruction      // reg[r] = reg[a]*reg[b] + reg[c]
    {
        uint8_t     r = 0;      // result index
        uint8_t     a = 0;      // a*b + c
        uint8_t     b = 0;
        uint8_t     c = 0;
    };

    using BytecodeFunction  = std::vector<BytecodeInstruction>;
    using BytecodeRegisters = std::vector<double>;

    struct BytecodeProgram
    {
        BytecodeFunction    func;
        BytecodeRegisters   reg;
        int varIndex[0x100];

        explicit BytecodeProgram()
        {
            for (int i = 0; i < 0x100; ++i)
                varIndex[i] = -1;
        }

        void printRegisters()
        {
            printf("REGISTERS:\n");
            const int nRegisters = static_cast<int>(reg.size());
            for (int i = 0; i < nRegisters; ++i)
                printf("[%2d] : %20.16lf\n", i, reg.at(i));
        }

        void printFunc()
        {
            printf("INSTRUCTIONS:\n");
            for (const BytecodeInstruction& inst : func)
                printf("[%2d] = [%2d]*[%2d] + [%2d]\n", inst.r, inst.a, inst.b, inst.c);
        }

        void print()
        {
            printRegisters();
            printFunc();
        }

        void setVar(char name, double value)
        {
            const int index = static_cast<int>(name);
            if (index < 0x00 || index > 0xff)
                throw CalcError("Invalid variable name");

            const int addr = varIndex[index];
            if (addr < 0)
            {
                std::string message = "Undefined variable ";
                message.push_back(name);
                throw CalcError(message);
            }
            reg.at(addr) = value;
        }

        double evaluate()
        {
            double answer = 0;
            for (const BytecodeInstruction& inst : func)
                answer = reg.at(inst.r) = reg.at(inst.a)*reg.at(inst.b) + reg.at(inst.c);
            return answer;
        }
    };

    using BytecodeResult = TranslateResult<BytecodeProgram>;


    BytecodeProgram CompileBytecode(calc_expr_t expr);


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

        BytecodeResult compile(std::string& postfix, std::string infix)
        {
            try
            {
                auto expr = CalcParseNumericExpression(infix);
                BytecodeProgram prog = CompileBytecode(expr);
                return BytecodeResult::Success(prog);
            }
            catch (const CalcError& ex)
            {
                postfix.clear();
                return BytecodeResult::Fail(ex.what());
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
            double _xmin = +10, double _xmax = -10,     // backwards to disable scaling
            double _ymin = +10, double _ymax = -10,
            double _zmin = +10, double _zmax = -10,
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
            ProgOscillator_initialize();
        }

        void initialize() override
        {
            ChaoticOscillator::initialize();
            ProgOscillator_initialize();
        }

        void ProgOscillator_initialize()
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

        BytecodeResult compile(
            int varIndex,       // 0=vx, 1=vy, 2=vz
            std::string infix)
        {
            switch (varIndex)
            {
            case 0:  return compile(vxPostfix, infix);
            case 1:  return compile(vyPostfix, infix);
            case 2:  return compile(vzPostfix, infix);
            default: return BytecodeResult::Fail("Invalid varIndex=" + std::to_string(varIndex));
            }
        }
    };
}
