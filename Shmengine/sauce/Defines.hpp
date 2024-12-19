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
typedef bool bool8;

typedef unsigned char byte;

typedef uint32 UniqueId;

typedef void* (*FP_allocator_allocate_callback)(uint64 size);

inline static const uint32 max_geometry_name_length = 128;
inline static const uint32 max_material_name_length = 128;
inline static const uint32 max_texture_name_length = 128;
inline static const uint32 max_buffer_name_length = 128;

#define PI 3.14159265358979323846f
#define DOUBLE_PI (2.0f * PI)
#define HALF_PI (0.5f * PI)
#define QUARTER_PI (0.25f * PI)
#define ONE_OVER_PI (1.0f / PI)
#define ONE_OVER_TWO_PI (1.0f / DOUBLE_PI)
#define SQRT_TWO 1.41421356237309504880f
#define SQRT_THREE 1.73205080756887729352f
#define SQRT_ONE_OVER_TWO 0.70710678118654752440f
#define SQRT_ONE_OVER_THREE 0.57735026918962576450f
#define DEG2RAD_MULTIPLIER (PI / 180.0f)
#define RAD2DEG_MULTIPLIER (180.0f / PI)

// A huge number that should be larger than any valid number used.
#define INFINITY 1e30f

// Smallest positive number where 1.0 + FLOAT_EPSILON != 0
#define FLOAT_EPSILON 1.192092896e-07f

#define PTR_BYTES_OFFSET(ptr, bytes) (((uint8*)ptr) + (bytes))

#define MAX_FILEPATH_LENGTH 256

#define INVALID_ID64 0xFFFFFFFFFFFFFFFFU
#define INVALID_ID 0xFFFFFFFFU
#define INVALID_ID16 0xFFFFU
#define INVALID_ID8 0xFFU

// Properly define static assertions.
#if defined(__clang__) || defined(__gcc__)
#define STATIC_ASSERT _Static_assert
#else
#define STATIC_ASSERT static_assert
#endif

#define Kibibytes(x) ((x) * 1024ULL)
#define Mebibytes(x) (Kibibytes(x) * 1024ULL)
#define Gibibytes(x) (Mebibytes(x) * 1024ULL)
#define Tebibytes(x) (Gibibytes(x) * 1024ULL)

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

#ifdef _MSC_VER
#define TYPEOF(x) decltype(x)
#endif

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

#ifdef _MSC_VER
#define SHMINLINE __forceinline
#define SHMNOINLINE __declspec(noinline)
#else
#define SHMINLINE static inline
#define SHMNOINLINE
#endif

#define SHMIN(x, y) (y < x ? y : x)
#define SHMAX(x, y) (y > x ? y : x)