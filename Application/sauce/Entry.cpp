#include "core/Engine.hpp"
#include "ApplicationTypes.hpp"
#include "core/Logging.hpp"
#include "core/Assert.hpp"
#include "platform/Platform.hpp"

#ifdef _WIN32
#pragma comment(linker, "/subsystem:windows")

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int WinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE prev_instance, _In_ LPSTR lpCmdLine, _In_ int n_show_cmd)
#else
int main(void) 
#endif
{
	Application app_inst = {};

	Platform::init_console();
	
	SHMINFOV("Shmengine Engine Version: %s", "0.001a");
	SHMINFO("Starting the engines :)");    

	// Initialization.
	if (!Engine::init(&app_inst)) {
		SHMERROR("Failed to init engine!");
		return -2;
	}

	// Begin the game loop.
	if (!Engine::run(&app_inst)) 
	{
		return -1;
	}

	return 0;
}
