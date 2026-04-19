#pragma once

#define ENABLE_CHECKFLOAT

#if defined(ENABLE_CHECKFLOAT)
    #define checkfloat(x)   (Sapphire::ValidateNumber((x), __FILE__, __LINE__, __func__, #x))
#else
    #define checkfloat(x)   (x)
#endif

namespace Sapphire
{
    float ValidateNumber(
        float value,
        const char *sourceFileName,
        int sourceLineNumber,
        const char *functionName,
        const char *expression
    );
}
