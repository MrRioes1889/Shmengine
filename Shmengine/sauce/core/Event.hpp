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

typedef bool32(*FP_OnEvent)(uint16 code, void* sender, void* listener_inst, EventData data);

bool32 event_init();
void event_shutdown();

SHMAPI bool32 event_register(uint16 code, void* listener, FP_OnEvent on_event);
SHMAPI bool32 event_unregister(uint16 code, void* listener, FP_OnEvent on_event);

SHMAPI bool32 event_fire(uint16 code, void* sender, EventData data);

enum SystemEventCode
{
	EVENT_CODE_APPLICATION_QUIT = 1,
	EVENT_CODE_KEY_PRESSED = 2,
	EVENT_CODE_KEY_RELEASED = 3,
	EVENT_CODE_BUTTON_PRESSED = 4,
	EVENT_CODE_BUTTON_RELEASED = 5,
	EVENT_CODE_MOUSE_MOVED = 6,
	EVENT_CODE_MOUSE_SCROLL = 7,
	EVENT_CODE_WINDOW_RESIZED = 8,

	EVENT_CODE_MAX_SYSTEM = 0xFF
};