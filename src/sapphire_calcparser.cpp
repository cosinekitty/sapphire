#include "sapphire_calcparser.hpp"

namespace Sapphire
{
    std::shared_ptr<CalcExpr> CalcParseNumeric(std::string text)
    {
        CalcScanner scanner(text);
        if (auto token = scanner.getNextToken())
            return std::make_shared<CalcExpr>(*token);

        throw CalcError("syntax error");
    }
}
