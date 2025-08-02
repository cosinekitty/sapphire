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
        std::vector<int> outputs;       // list of register indices for the calculation results

        explicit BytecodeProgram()
        {
            for (int i = 0; i < 0x100; ++i)
                varIndex[i] = -1;
        }

        void printVariables()
        {
            printf("    VARIABLES:\n");
            for (int v = 0; v < 0x100; ++v)
                if (int r = varIndex[v]; r >= 0)
                    printf("        '%c' @ [%2d]\n", v, r);
        }

        void printRegisters()
        {
            printf("    REGISTERS:\n");
            const int nRegisters = static_cast<int>(reg.size());
            for (int i = 0; i < nRegisters; ++i)
                printf("        [%2d] : %20.16lf\n", i, reg.at(i));
        }

        void printFunc()
        {
            printf("    INSTRUCTIONS:\n");
            for (const BytecodeInstruction& inst : func)
                printf("        [%2d] = [%2d]*[%2d] + [%2d]\n", inst.r, inst.a, inst.b, inst.c);
        }

        void printOutputs()
        {
            printf("    OUTPUTS:\n");
            for (int r : outputs)
                if (r >= 0 && r < static_cast<int>(reg.size()))
                    printf("        [%2d] = %20.16lf\n", r, reg[r]);
                else
                    printf("        [%2d] = ?\n", r);
        }

        void print()
        {
            printf("\n");
            printf("PROGRAM:\n");
            printVariables();
            printRegisters();
            printOutputs();
            printFunc();
            printf("\n");
        }

        void setVar(char name, double value)
        {
            const int index = static_cast<int>(name);
            if (index < 0x00 || index > 0xff)
                throw CalcError("Invalid variable name");

            int& r = varIndex[index];
            if (r < 0)
                r = allocateRegister();

            reg.at(r) = value;
        }

        void run()
        {
            for (const BytecodeInstruction& inst : func)
                reg[inst.r] = reg[inst.a]*reg[inst.b] + reg[inst.c];
        }

        void defineVariables(calc_expr_t expr);
        int compile(calc_expr_t expr);
        void validate() const;

    private:
        int r_negOne = -1;
        int r_zero = -1;
        int r_posOne = -1;

        int allocateRegister(double value = 0.0)
        {
            const int r = static_cast<int>(reg.size());
            reg.push_back(value);
            return r;
        }

        int allocateConstant(int& rindex, double value)
        {
            if (rindex < 0)
                rindex = allocateRegister(value);

            return rindex;
        }

        void validateRegister(int r) const
        {
            if (r < 0 || r >= static_cast<int>(reg.size()))
                throw CalcError("Register index is out of range: " + std::to_string(r));
        }

        int emit(int r, int a, int b, int c)
        {
            BytecodeInstruction inst;
            inst.r = r;
            inst.a = a;
            inst.b = b;
            inst.c = c;
            func.push_back(inst);
            return r;
        }
    };

    using BytecodeResult = TranslateResult<BytecodeProgram>;

    class ProgOscillator : public ChaoticOscillator
    {
    public:
        static constexpr int ParamCount = 4;        // knobs are 'a', 'b', 'c', 'd'.

    private:
        mutable BytecodeProgram prog;       // a single program that calculates vx, vy, and vz.
        double param[ParamCount]{};

        static int ValidateParamIndex(int index)
        {
            if (index >= 0 && index < ParamCount)
                return index;
            throw CalcError(std::string("parameter index is out of range: ") + std::to_string(index));
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

        BytecodeResult compile(int varIndex, std::string infix)
        {
            try
            {
                auto expr = CalcParseNumericExpression(infix);
                prog.defineVariables(expr);
                const int reg = prog.compile(expr);
                prog.outputs.push_back(reg);
                return BytecodeResult::Success(prog);
            }
            catch (const CalcError& ex)
            {
                return BytecodeResult::Fail(ex.what());
            }
        }

        double paramValue(int index) const
        {
            return param[ValidateParamIndex(index)];
        }

        double& paramValue(int index)
        {
            return param[ValidateParamIndex(index)];
        }
    };
}
