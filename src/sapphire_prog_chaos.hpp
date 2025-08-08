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
        std::vector<int> registerForSymbol;
        std::vector<int> outputs;                   // list of register indices for the calculation results
        std::vector<BytecodeLiteral> literals;      // list of numeric constants, mapped to register indexes

        explicit BytecodeProgram()
        {
            registerForSymbol.resize(0x80);              // cover every ASCII character value
            reg.reserve(MaxRegisterCount);      // maximum possible size; prevent any reallocations later.
            initialize();
        }

        void initialize()
        {
            func.clear();
            reg.clear();
            outputs.clear();
            literals.clear();
            for (int& v : registerForSymbol)
                v = -1;
        }

        void printRegisters() const
        {
            printf("    REGISTERS:\n");
            const int nRegisters = static_cast<int>(reg.size());
            for (int r = 0; r < nRegisters; ++r)
            {
                printf("        [%2d] : %20.16lf", r, reg[r]);
                std::string comment = registerComment(r);
                if (comment.size())
                    printf("    // %s", comment.c_str());
                printf("\n");
            }
        }

        std::string registerComment(int r) const
        {
            if (r >= 0 && r < 0x100)
            {
                validateRegister(r);

                const int n = static_cast<int>(registerForSymbol.size());
                for (int c = 0; c < n; ++c)
                    if (r == registerForSymbol[c])
                        return std::string{static_cast<char>(c)};

                for (const BytecodeLiteral& lit : literals)
                {
                    if (r == lit.r)
                    {
                        char text[256];
                        snprintf(text, sizeof(text), "(%g)", lit.c);
                        return std::string(text);
                    }
                }

                int index = 0;
                for (int outputRegister : outputs)
                {
                    if (r == outputRegister)
                        return "out_" + std::to_string(index);
                    ++index;
                }
            }
            return "";
        }

        std::string registerDisasm(int r) const
        {
            if (std::string s = registerComment(r); s.size() > 0)
                return s;
            return "reg_" + std::to_string(r);
        }

        void printFunc() const
        {
            printf("    INSTRUCTIONS:\n");
            for (const BytecodeInstruction& inst : func)
            {
                printf("        [%2d] = [%2d]*[%2d] + [%2d]", inst.r, inst.a, inst.b, inst.c);
                auto rt = registerDisasm(inst.r);
                auto at = registerDisasm(inst.a);
                auto bt = registerDisasm(inst.b);
                auto ct = registerDisasm(inst.c);
                printf("    // %s = %s*%s + %s\n", rt.c_str(), at.c_str(), bt.c_str(), ct.c_str());
            }
        }

        void print() const
        {
            printf("\n");
            printf("PROGRAM:\n");
            printRegisters();
            printFunc();
            printf("\n");
        }

        int setVar(char name, double value)
        {
            const int r = variableRegister(name);
            reg[r] = value;
            return r;
        }

        int compile(calc_expr_t expr)
        {
            const int resultRegister = gencode(expr, 0);
            outputs.push_back(resultRegister);
            return resultRegister;
        }

        void run()
        {
            for (const BytecodeInstruction& inst : func)
                reg[inst.r] = reg[inst.a]*reg[inst.b] + reg[inst.c];
        }

        void validate() const;
        bool isConstantExpression(double& value, const calc_expr_t& expr) const;

    private:
        int gencode(calc_expr_t expr, int depth);

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

        int variableRegister(char symbol)
        {
            int& r = registerForSymbol[0x7f & static_cast<unsigned>(symbol)];
            if (r < 0)
                r = allocateRegister();
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


    struct KnobParameterMapping
    {
        // The default mapping is an identity operator:  paramValue(x) == x.
        double center = 0;
        double spread = 1;

        double paramValue(double knob) const
        {
            const double x = std::clamp<double>(knob, -1, +1);
            return center + x*spread;
        }

        void initialize()
        {
            center = 0;
            spread = 1;
        }
    };


    class ProgOscillator : public ChaoticOscillator
    {
    public:
        static constexpr int ParamCount = 4;        // knobs are 'a', 'b', 'c', 'd'.
        static constexpr int InputCount = 3;        // input variables are 'x', 'y', 'z'.
        KnobParameterMapping knobMap[ProgOscillator::ParamCount];

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
        void updateParameters() override;

    public:
        explicit ProgOscillator(
            double _max_dt = 0.01,
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
            dilate = 1;

            for (int i=0; i < ProgOscillator::ParamCount; ++i)
                knobMap[i].initialize();
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
                prog.compile(expr);
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
