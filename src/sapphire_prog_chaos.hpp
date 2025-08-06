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

    struct BytecodeLiteral
    {
        int r = -1;     // assigned register index
        double c = 0;   // constant value stored in that register

        explicit BytecodeLiteral(uint8_t registerIndex, double constantValue)
            : r(static_cast<int>(registerIndex))
            , c(ValidateConstant(constantValue))
            {}

        static double ValidateConstant(double x)
        {
            if (!std::isfinite(x))
                throw CalcError("Non-finite constant literal encountered in compiled expression.");
            return x;
        }
    };

    struct BytecodeProgram
    {
        static constexpr int MaxRegisterCount = 0x100;      // register indexes must fit in 8 bits

        BytecodeFunction    func;
        BytecodeRegisters   reg;
        std::vector<int> varIndex;
        std::vector<int> outputs;                   // list of register indices for the calculation results
        std::vector<BytecodeLiteral> literals;      // list of numeric constants, mapped to register indexes

        explicit BytecodeProgram()
        {
            varIndex.resize(0x80);              // cover every ASCII character value
            reg.reserve(MaxRegisterCount);      // maximum possible size; prevent any reallocations later.
            initialize();
        }

        void initialize()
        {
            func.clear();
            reg.clear();
            outputs.clear();
            literals.clear();
            for (int& v : varIndex)
                v = -1;
        }

        void printVariables() const
        {
            printf("    VARIABLES:\n");
            const int n = static_cast<int>(varIndex.size());
            for (int v = 0; v < n; ++v)
                if (int r = varIndex[v]; r >= 0)
                    printf("        '%c' @ [%2d]\n", v, r);
        }

        void printLiterals() const
        {
            printf("    LITERALS:\n");
            for (const BytecodeLiteral& lit : literals)
                printf("        %20.16lf @ [%2d]\n", lit.c, lit.r);
        }

        void printRegisters() const
        {
            printf("    REGISTERS:\n");
            const int nRegisters = static_cast<int>(reg.size());
            for (int i = 0; i < nRegisters; ++i)
                printf("        [%2d] : %20.16lf\n", i, reg.at(i));
        }

        void printFunc() const
        {
            printf("    INSTRUCTIONS:\n");
            for (const BytecodeInstruction& inst : func)
                printf("        [%2d] = [%2d]*[%2d] + [%2d]\n", inst.r, inst.a, inst.b, inst.c);
        }

        void printOutputs() const
        {
            printf("    OUTPUTS:\n");
            for (int r : outputs)
                if (r >= 0 && r < static_cast<int>(reg.size()))
                    printf("        [%2d] = %20.16lf\n", r, reg[r]);
                else
                    printf("        [%2d] = ?\n", r);
        }

        void print() const
        {
            printf("\n");
            printf("PROGRAM:\n");
            printVariables();
            printLiterals();
            printRegisters();
            printOutputs();
            printFunc();
            printf("\n");
        }

        int setVar(char name, double value)
        {
            int& r = varIndex[static_cast<unsigned>(0x7f & name)];
            if (r < 0)
                r = allocateRegister();
            reg[r] = value;
            return r;
        }

        void run()
        {
            for (const BytecodeInstruction& inst : func)
                reg[inst.r] = reg[inst.a]*reg[inst.b] + reg[inst.c];
        }

        void defineVariables(calc_expr_t expr);
        int compile(calc_expr_t expr);
        void validate() const;
        bool isConstantExpression(double& value, const calc_expr_t& expr) const;

    private:
        int allocateRegister(double value = 0.0)
        {
            const int r = static_cast<int>(reg.size());
            if (r >= MaxRegisterCount)
                throw CalcError("Ran out of registers (" + std::to_string(MaxRegisterCount) + " max).");
            reg.push_back(value);
            return r;
        }

        int literalRegister(double value)
        {
            // Search for a register where this value is already stored.
            for (const BytecodeLiteral& lit : literals)
                if (lit.c == value)
                    return lit.r;

            const int r = allocateRegister(value);
            literals.push_back(BytecodeLiteral(r, value));
            return r;
        }

        int validateRegister(int r) const
        {
            if (r < 0 || r >= static_cast<int>(reg.size()))
                throw CalcError("Register index is out of range: " + std::to_string(r));
            return r;
        }

        int emit(int r, int a, int b, int c)
        {
            BytecodeInstruction inst;
            inst.r = validateRegister(r);
            inst.a = validateRegister(a);
            inst.b = validateRegister(b);
            inst.c = validateRegister(c);
            func.push_back(inst);
            return r;
        }
    };

    using BytecodeResult = TranslateResult<BytecodeProgram>;

    class ProgOscillator : public ChaoticOscillator
    {
    public:
        static constexpr int ParamCount = 4;        // knobs are 'a', 'b', 'c', 'd'.
        static constexpr int InputCount = 3;        // input variables are 'x', 'y', 'z'.

    private:
        mutable BytecodeProgram prog;       // a single program that calculates vx, vy, and vz.

        int paramRegister[ParamCount]{};
        int inputRegister[InputCount]{};

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
            resetProgram();
            ProgOscillator_initialize();
        }

        void initialize() override
        {
            ChaoticOscillator::initialize();
            ProgOscillator_initialize();
        }

        void ProgOscillator_initialize()
        {
        }

        int getModeCount() const override
        {
            return 4;
        }

        const char* getModeName(int m) const override
        {
            switch (m)
            {
            case 0:  return "Alpha";
            case 1:  return "Bravo";
            case 2:  return "Charlie";
            case 3:  return "Delta";
            default: return "";
            }
        }

        void resetProgram()
        {
            prog.initialize();

            paramRegister[0] = prog.setVar('a', 0.2);
            paramRegister[1] = prog.setVar('b', 0.2);
            paramRegister[2] = prog.setVar('c', 7.0);
            paramRegister[3] = prog.setVar('d', 0.1);

            inputRegister[0] = prog.setVar('x', x0);
            inputRegister[1] = prog.setVar('y', y0);
            inputRegister[2] = prog.setVar('z', z0);
        }

        BytecodeResult compile(std::string infix)
        {
            try
            {
                auto expr = CalcParseNumericExpression(infix);
                prog.defineVariables(expr);
                const int reg = prog.compile(expr);
                prog.outputs.push_back(reg);
                prog.validate();
                return BytecodeResult::Success(prog);
            }
            catch (const CalcError& ex)
            {
                return BytecodeResult::Fail(ex.what());
            }
        }

        double paramValue(int index) const
        {
            return prog.reg.at(paramRegister[ValidateParamIndex(index)]);
        }

        double& paramValue(int index)
        {
            return prog.reg.at(paramRegister[ValidateParamIndex(index)]);
        }
    };
}
