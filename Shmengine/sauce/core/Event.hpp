#pragma once

#include "Defines.hpp"

struct EventData
{
	union
	{
		int64 i64[2];
		uint64 ui4[2];
		float64 f64[2];

		int32 i32[4];
		uint32 ui32[4];
		float32 f32[4];

		int16 i16[8];
		uint16 ui16[8];

		int8 i8[16];
		uint8 ui8[16];

		char c[16];
	};
};

namespace SystemEventCode
{
	enum Value
	{
		APPLICATION_QUIT = 1,
		KEY_PRESSED = 2,
		KEY_RELEASED = 3,
		BUTTON_PRESSED = 4,
		BUTTON_RELEASED = 5,
		MOUSE_MOVED = 6,
		MOUSE_SCROLL = 7,
		MOUSE_INTERNAL_MOVED = 8,
		WINDOW_RESIZED = 9,

		SET_RENDER_MODE = 10,
		OBJECT_HOVER_ID_CHANGED = 11,
		DEFAULT_RENDERTARGET_REFRESH_REQUIRED = 12,

		DEBUG0 = 0xFA,
		DEBUG1 = 0xFB,
		DEBUG2 = 0xFC,
		DEBUG3 = 0xFD,
		DEBUG4 = 0xFE,

		MAX_SYSTEM = 0xFF
	};	
};

namespace Event
{

	typedef bool32(*FP_OnEvent)(uint16 code, void* sender, void* listener_inst, EventData data);

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state);
	void system_shutdown();

	SHMAPI bool32 event_register(uint16 code, void* listener, FP_OnEvent on_event);
	SHMAPI bool32 event_unregister(uint16 code, void* listener, FP_OnEvent on_event);

	SHMAPI bool32 event_fire(uint16 code, void* sender, EventData data);

}

