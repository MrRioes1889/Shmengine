#include "Event.hpp"
#include "core/Memory.hpp"
#include "containers/Darray.hpp"
#include "Logging.hpp"
#include "memory/LinearAllocator.hpp"

struct Listener
{
	void* ptr;
	FP_OnEvent callback;
};

struct EventCodeEntry
{
	Listener* listeners;
};

#define MAX_MESSAGE_CODES 4096

struct EventSystemState
{
	EventCodeEntry registered[MAX_MESSAGE_CODES];
};

static EventSystemState* system_state;

bool32 event_system_init(void* linear_allocator, void*& out_state)
{
	Memory::LinearAllocator* allocator = (Memory::LinearAllocator*)linear_allocator;
	out_state = Memory::linear_allocator_allocate(allocator, sizeof(EventSystemState));
	system_state = (EventSystemState*)out_state;

	Memory::zero_memory(system_state, sizeof(EventSystemState));

	SHMINFO("Event subsystem initialized!");
	return true;
}

void event_system_shutdown()
{
	for (uint32 i = 0; i < MAX_MESSAGE_CODES; i++)
	{
		if (system_state->registered[i].listeners != 0)
		{
			darray_destroy(system_state->registered[i].listeners);
			system_state->registered[i].listeners = 0;
		}
	}
}

bool32 event_register(uint16 code, void* listener, FP_OnEvent on_event)
{
	if (!system_state)
		return false;

	if (system_state->registered[code].listeners == 0)
		system_state->registered[code].listeners = darray_create(Listener, AllocationTag::MAIN);

	Listener* e_listeners = system_state->registered[code].listeners;

	// NOTE: Check if listener is already registered for event
	uint32 registered_count = darray_count(e_listeners);
	for (uint32 i = 0; i < registered_count; i++)
	{
		if (e_listeners[i].ptr == listener)
			return false;
	}

	Listener l1;
	l1.ptr = listener;
	l1.callback = on_event;
	darray_push(e_listeners, l1);

	return true;
}

bool32 event_unregister(uint16 code, void* listener, FP_OnEvent on_event)
{
	if (!system_state)
		return false;

	Listener* e_listeners = system_state->registered[code].listeners;

	// TODO: Warning
	if (e_listeners == 0)
		return false;

	// NOTE: Check if listener is already registered for event
	uint32 registered_count = darray_count(e_listeners);
	for (uint32 i = 0; i < registered_count; i++)
	{
		if (e_listeners[i].ptr == listener && e_listeners[i].callback == on_event)
		{
			darray_remove_at(e_listeners, i);
			return true;
		}
	}

	return false;
}

bool32 event_fire(uint16 code, void* sender, EventData data)
{
	if (!system_state)
		return false;

	Listener* e_listeners = system_state->registered[code].listeners;

	// TODO: Warning
	if (e_listeners == 0)
		return false;

	// NOTE: Check if listener is already registered for event
	uint32 registered_count = darray_count(e_listeners);
	for (uint32 i = 0; i < registered_count; i++)
	{
		if (e_listeners[i].callback(code, sender, e_listeners[i].ptr, data))
		{
			// NOTE: return if the message has been handled fully
			return true;
		}
	}

	return false;
}