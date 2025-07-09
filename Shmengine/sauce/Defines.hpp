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

namespace Constants
{

	inline constexpr uint32 max_filename_length = 255;
	inline constexpr uint32 max_mesh_name_length = 63;
	inline constexpr uint32 max_geometry_name_length = 63;
	inline constexpr uint32 max_material_name_length = 63;
	inline constexpr uint32 max_shader_name_length = 63;
	inline constexpr uint32 max_shader_stage_name_length = 63;
	inline constexpr uint32 max_shader_attribute_name_length = 63;
	inline constexpr uint32 max_shader_uniform_name_length = 63;
	inline constexpr uint32 max_texture_name_length = 63;
	inline constexpr uint32 max_terrain_name_length = 63;
	inline constexpr uint32 max_buffer_name_length = 63;

	inline constexpr uint32 max_terrain_materials_count = 4;

	inline constexpr float32 PI = 3.14159265358979323846f;
	inline constexpr float32 DOUBLE_PI = (2.0f * PI);
	inline constexpr float32 HALF_PI = (0.5f * PI);
	inline constexpr float32 QUARTER_PI = (0.25f * PI);
	inline constexpr float32 ONE_OVER_PI = (1.0f / PI);
	inline constexpr float32 ONE_OVER_TWO_PI = (1.0f / DOUBLE_PI);
	inline constexpr float32 SQRT_TWO = 1.41421356237309504880f;
	inline constexpr float32 SQRT_THREE = 1.73205080756887729352f;
	inline constexpr float32 SQRT_ONE_OVER_TWO = 0.70710678118654752440f;
	inline constexpr float32 SQRT_ONE_OVER_THREE = 0.57735026918962576450f;
	inline constexpr float32 DEG2RAD_MULTIPLIER = (PI / 180.0f);
	inline constexpr float32 RAD2DEG_MULTIPLIER = (180.0f / PI);

	// A huge number that should be larger than any valid number used.
	inline constexpr float32 INFINITY = 1e30f;

	// Smallest positive number where 1.0 + FLOAT_EPSILON != 0
	inline constexpr float32 FLOAT_EPSILON = 1.192092896e-07f;

	inline constexpr uint32 max_filepath_length = 256;

	inline constexpr uint64 max_u64 = 0xFFFFFFFFFFFFFFFF;
	inline constexpr uint32 max_u32 = 0xFFFFFFFF;
	inline constexpr uint16 max_u16 = 0xFFFF;
	inline constexpr uint8 max_u8 = 0xFF;

}


// Properly define static assertions.
#if defined(__clang__) || defined(__gcc__)
#define STATIC_ASSERT _Static_assert
#else
#define STATIC_ASSERT static_assert
#endif

inline constexpr uint64 kibibytes(uint64 x) { return x * 1024ULL; }
inline constexpr uint64 mebibytes(uint64 x) { return kibibytes(x) * 1024ULL; }
inline constexpr uint64 gibibytes(uint64 x) { return mebibytes(x) * 1024ULL; }
inline constexpr uint64 tebibytes(uint64 x) { return gibibytes(x) * 1024ULL; }

inline constexpr uint8* PTR_BYTES_OFFSET(void* ptr, int64 offset) { return ((uint8*)ptr) + (offset); }

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