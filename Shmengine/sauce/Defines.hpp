#pragma once

// Unsigned int types.
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long long uint64;

// Signed int types.
typedef signed char int8;
typedef signed short int16;
typedef signed int int32;
typedef signed long long int64;

// Floating point types
typedef float float32;
typedef double float64;

// Boolean types
typedef int bool32;
typedef char bool8;

// Properly define static assertions.
#if defined(__clang__) || defined(__gcc__)
#define STATIC_ASSERT _Static_assert
#else
#define STATIC_ASSERT static_assert
#endif

#define Kibibytes(x) ((x) * 1024LL)
#define Mebibytes(x) (Kibibytes(x) * 1024LL)
#define Gibibytes(x) (Mebibytes(x) * 1024LL)
#define Tebibytes(x) (Gibibytes(x) * 1024LL)

// Ensure all types are of the correct size.
STATIC_ASSERT(sizeof(uint8) == 1, "Expected u8 to be 1 byte.");
STATIC_ASSERT(sizeof(uint16) == 2, "Expected u16 to be 2 bytes.");
STATIC_ASSERT(sizeof(uint32) == 4, "Expected u32 to be 4 bytes.");
STATIC_ASSERT(sizeof(uint64) == 8, "Expected u64 to be 8 bytes.");

STATIC_ASSERT(sizeof(int8) == 1, "Expected i8 to be 1 byte.");
STATIC_ASSERT(sizeof(int16) == 2, "Expected i16 to be 2 bytes.");
STATIC_ASSERT(sizeof(int32) == 4, "Expected i32 to be 4 bytes.");
STATIC_ASSERT(sizeof(int64) == 8, "Expected i64 to be 8 bytes.");

STATIC_ASSERT(sizeof(float32) == 4, "Expected f32 to be 4 bytes.");
STATIC_ASSERT(sizeof(float64) == 8, "Expected f64 to be 8 bytes.");

#ifdef SHMEXPORT
// Exports
#ifdef _MSC_VER
#define SHMAPI __declspec(dllexport)
#else
#define SHMAPI __attribute__((visibility("default")))
#endif
#else
// Imports
#ifdef _MSC_VER
#define SHMAPI __declspec(dllimport)
#else
#define SHMAPI
#endif
#endif