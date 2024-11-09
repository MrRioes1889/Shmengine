#pragma once

#include "Defines.hpp"

#define SHMASSERTIONS_ENABLED

#ifdef SHMASSERTIONS_ENABLED

#if _MSC_VER
#include <intrin.h>
#define debug_break() __debugbreak()
#else
#define debug_break() __builtin_trap()
#endif

struct AssertException
{
    int32 line;
    const char* file;
    const char* message;
};

SHMAPI void report_assertion_failure(const char* expression, const char* message, const char* file, int32 line);

#define SHMASSERT(expr)                                               \
    {                                                                \
        if (expr) {                                                  \
        } else {                                                     \
            report_assertion_failure(#expr, "", __FILE__, __LINE__); \
            debug_break();                                           \
            throw AssertException({__LINE__, __FILE__, "Critical assertion failure!"});         \
        }                                                            \
    }

#define SHMASSERT_MSG(expr, message)                                        \
    {                                                                     \
        if (expr) {                                                       \
        } else {                                                          \
            report_assertion_failure(#expr, message, __FILE__, __LINE__); \
            debug_break();                                                \
            throw AssertException({__LINE__, __FILE__, message});         \
        }                                                                 \
    }

#ifdef _DEBUG
#define SHMASSERT_DEBUG(expr)                                          \
    {                                                                \
        if (expr) {                                                  \
        } else {                                                     \
            report_assertion_failure(#expr, "", __FILE__, __LINE__); \
            debug_break();                                           \
            throw AssertException({__LINE__, __FILE__, "Critical assertion failure"});         \
        }                                                            \
    }
#else
#define SHMASSERT_DEBUG(expr)  // Does nothing at all
#endif

#else
#define SHMASSERT(expr)               // Does nothing at all
#define SHMASSERT_MSG(expr, message)  // Does nothing at all
#define SHMASSERT_DEBUG(expr)         // Does nothing at all


#endif
