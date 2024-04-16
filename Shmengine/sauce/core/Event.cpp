#include "Event.hpp"
#include "Memory.hpp"
#include "containers/Darray.hpp"

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

struct EventSystem
{
	EventCodeEntry registered[MAX_MESSAGE_CODES];
};

static bool32 system_initialized = false;
static EventSystem system_state;

bool32 event_init()
{
	if (system_initialized)
		return false;

	Memory::zero_memory(&system_state, sizeof(EventSystem));

	system_initialized = true;

	return system_initialized;
}

void event_shutdown()
{
	for (uint32 i = 0; i < MAX_MESSAGE_CODES; i++)
	{
		if (system_state.registered[i].listeners != 0)
		{
			darray_destroy(system_state.registered[i].listeners);
			system_state.registered[i].listeners = 0;
		}
	}
}

bool32 event_register(uint16 code, void* listener, FP_OnEvent on_event)
{
	if (!system_initialized)
		return false;

	Listener* e_listeners = system_state.registered[code].listeners;

	if (e_listeners == 0)
		e_listeners = darray_create(Listener);

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
	if (!system_initialized)
		return false;

	Listener* e_listeners = system_state.registered[code].listeners;

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
	if (!system_initialized)
		return false;

	Listener* e_listeners = system_state.registered[code].listeners;

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