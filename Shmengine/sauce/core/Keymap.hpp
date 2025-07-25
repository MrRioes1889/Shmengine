#pragma once

#include "Defines.hpp"
#include "Input.hpp"

namespace Input
{

	namespace KeymapModifierFlags
	{
		enum
		{
			Shift = 1 << 0,
			Control = 1 << 1,
			Alt = 1 << 2,
			AltGr = 1 << 3
		};
		typedef uint32 Value;
	}

	enum class KeymapBindingType
	{
		Undefined = 0,
		Press = 1,
		Release = 2,
		Hold = 3,
		Unset = 4
	};

	typedef void (*FP_keybind_callback)(KeyCode::Value key, KeymapBindingType binding_type, KeymapModifierFlags::Value modifiers, void* user_data);

	struct KeymapBinding
	{
		KeymapBindingType type;
		KeymapModifierFlags::Value modifiers;
		FP_keybind_callback callback;
		void* user_data;
		KeymapBinding* next;
	};

	struct KeymapEntry
	{
		KeyCode::Value key;
		KeymapBinding* bindings;
	};

	struct Keymap
	{
		bool8 overrides_all;
		KeymapEntry entries[KeyCode::MAX_KEYS];

		SHMAPI void init();
		SHMAPI void destroy();
		SHMAPI void add_binding(KeyCode::Value key, KeymapBindingType type, KeymapModifierFlags::Value modifiers, void* user_data, FP_keybind_callback callback);
		SHMAPI void remove_binding(KeyCode::Value key, KeymapBindingType type, KeymapModifierFlags::Value modifiers, FP_keybind_callback callback);
		SHMAPI void clear();
	};

}
