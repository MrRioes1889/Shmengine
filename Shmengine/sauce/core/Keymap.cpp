#include "Keymap.hpp"
#include "core/Memory.hpp"

namespace Input
{

	void Keymap::init()
	{
		overrides_all = false;

		for (uint32 i = 0; i < KeyCode::MAX_KEYS; i++)
		{
			entries[i].bindings = 0;
			entries[i].key = (KeyCode::Value)i;
		}
	}

	void Keymap::destroy()
	{
		for (uint32 i = 0; i < KeyCode::MAX_KEYS; i++)
		{
			KeymapBinding* head = entries[i].bindings;
			while (head)
			{
				KeymapBinding* next = head->next;
				Memory::free_memory(head);
				head = next;
			}
			entries[i].bindings = 0;
		}
	}

	void Keymap::add_binding(KeyCode::Value key, KeymapBindingType type, KeymapModifierFlags::Value modifiers, void* user_data, FP_keybind_callback callback)
	{

		KeymapBinding* head = entries[key].bindings;
		if (head)
		{
			while (head->next)
			{
				head = head->next;
			}
		}

		KeymapBinding* new_entry = (KeymapBinding*)Memory::allocate(sizeof(KeymapBinding), AllocationTag::UNKNOWN);
		new_entry->callback = callback;
		new_entry->modifiers = modifiers;
		new_entry->type = type;
		new_entry->user_data = user_data;
		new_entry->next = 0;

		if (head)
			head->next = new_entry;
		else
			entries[key].bindings = new_entry;

	}

	void Keymap::remove_binding(KeyCode::Value key, KeymapBindingType type, KeymapModifierFlags::Value modifiers, FP_keybind_callback callback)
	{

		KeymapBinding* binding = entries[key].bindings;
		KeymapBinding* prev = entries[key].bindings;

		while (binding)
		{
			if (binding->callback == callback && binding->modifiers == modifiers && binding->type == type)
			{
				prev->next = binding->next;
				if (binding == entries[key].bindings)
					entries[key].bindings = 0;

				Memory::free_memory(binding);
				return;
			}
			prev = binding;
			binding = binding->next;
		}

	}

}
