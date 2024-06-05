#include "Event.hpp"
#include "core/Memory.hpp"
#include "containers/Darray.hpp"
#include "Logging.hpp"
#include "memory/LinearAllocator.hpp"

namespace Event
{

	struct Listener
	{
		void* ptr;
		FP_OnEvent callback;
	};

	struct EventCodeEntry
	{
		Darray<Listener> listeners = {};
	};

#define MAX_MESSAGE_CODES 4096

	struct SystemState
	{
		EventCodeEntry registered[MAX_MESSAGE_CODES];
	};

	static SystemState* system_state;

	bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state)
	{
		out_state = allocator_callback(sizeof(SystemState));
		system_state = (SystemState*)out_state;

		SHMINFO("Event subsystem initialized!");
		return true;
	}

	void system_shutdown()
	{
		for (uint32 i = 0; i < MAX_MESSAGE_CODES; i++)
		{
			if (system_state->registered[i].listeners.data != 0)
			{
				system_state->registered[i].listeners.free_data();
			}
		}
	}

	bool32 event_register(uint16 code, void* listener, FP_OnEvent on_event)
	{
		if (!system_state)
			return false;

		if (system_state->registered[code].listeners.data == 0)
			system_state->registered[code].listeners.init(1, AllocationTag::MAIN);

		Darray<Listener>& e_listeners = system_state->registered[code].listeners;

		// NOTE: Check if listener is already registered for event
		for (uint32 i = 0; i < e_listeners.count; i++)
		{
			if (e_listeners[i].ptr == listener)
				return false;
		}

		Listener l1;
		l1.ptr = listener;
		l1.callback = on_event;
		e_listeners.push(l1);

		return true;
	}

	bool32 event_unregister(uint16 code, void* listener, FP_OnEvent on_event)
	{
		if (!system_state)
			return false;

		Darray<Listener>& e_listeners = system_state->registered[code].listeners;

		// TODO: Warning
		if (e_listeners.data == 0)
			return false;

		// NOTE: Check if listener is already registered for event
		for (uint32 i = 0; i < e_listeners.count; i++)
		{
			if (e_listeners[i].ptr == listener && e_listeners[i].callback == on_event)
			{
				e_listeners.remove_at(i);
				return true;
			}
		}

		return false;
	}

	bool32 event_fire(uint16 code, void* sender, EventData data)
	{
		if (!system_state)
			return false;

		Darray<Listener>& e_listeners = system_state->registered[code].listeners;

		// TODO: Warning
		if (e_listeners.data == 0)
			return false;

		// NOTE: Check if listener is already registered for event
		for (uint32 i = 0; i < e_listeners.count; i++)
		{
			if (e_listeners[i].callback(code, sender, e_listeners[i].ptr, data))
			{
				// NOTE: return if the message has been handled fully
				return true;
			}
		}

		return false;
	}

}

