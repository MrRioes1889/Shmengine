#pragma once

#include "Defines.hpp"
#include "Input.hpp"

namespace Input
{

	namespace KeymapModifierFlags
	{
		enum
		{
			SHIFT = 1 << 0,
			CONTROL = 1 << 1,
			ALT = 1 << 2,
			ALT_GR = 1 << 3
		};
		typedef uint32 Value;
	}

	enum class KeymapBindingType
	{
		UNDEFINED = 0,
		PRESS = 1,
		RELEASE = 2,
		HOLD = 3,
		UNSET = 4
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
		bool32 overrides_all;
		KeymapEntry entries[KeyCode::MAX_KEYS];

		void init();
		void destroy();
		void add_binding(KeyCode::Value key, KeymapBindingType type, KeymapModifierFlags::Value modifiers, void* user_data, FP_keybind_callback callback);
		void remove_binding(KeyCode::Value key, KeymapBindingType type, KeymapModifierFlags::Value modifiers, FP_keybind_callback callback);
	};

}
