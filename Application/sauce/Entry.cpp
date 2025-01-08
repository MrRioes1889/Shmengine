#include <Entry.hpp>

#include <core/Event.hpp>
#include <platform/Platform.hpp>
#include <platform/FileSystem.hpp>
#include <systems/RenderViewSystem.hpp>

typedef bool32(*FP_create_renderer_module)(Renderer::Module* out_module);
typedef uint64(*FP_get_module_state_size)();

static const char* application_module_name = "A_Sandbox";
static const char* renderer_module_name = "M_VulkanRenderer";

static bool32 load_application_library(Application* app, const char* lib_filename, bool32 reload) 
{

    if (!Platform::load_dynamic_library(application_module_name, lib_filename, &app->application_lib))
        return false;

    if (!Platform::load_dynamic_library_function(&app->application_lib, "application_boot", (void**)&app->boot))
        return false;
    if (!Platform::load_dynamic_library_function(&app->application_lib, "application_init", (void**)&app->init))
        return false;
    if (!Platform::load_dynamic_library_function(&app->application_lib, "application_shutdown", (void**)&app->shutdown))
        return false;
    if (!Platform::load_dynamic_library_function(&app->application_lib, "application_update", (void**)&app->update))
        return false;
    if (!Platform::load_dynamic_library_function(&app->application_lib, "application_render", (void**)&app->render))
        return false;
    if (!Platform::load_dynamic_library_function(&app->application_lib, "application_on_resize", (void**)&app->on_resize))
        return false;
    if (!Platform::load_dynamic_library_function(&app->application_lib, "application_on_module_reload", (void**)&app->on_module_reload))
        return false;
    if (!Platform::load_dynamic_library_function(&app->application_lib, "application_on_module_unload", (void**)&app->on_module_unload))
        return false;

    if (reload)
        app->on_module_reload(app->state);

    return true;

}

static bool32 on_watched_file_written(uint16 code, void* sender, void* listener_inst, EventData e_data)
{

    Application* app = (Application*)listener_inst;
    if (e_data.ui32[0] == app->application_lib.watch_id)
        SHMINFO("Hot reloading application library");

    app->on_module_unload();
    if (!Platform::unload_dynamic_library(&app->application_lib))
    {
        SHMERROR("Failed to unload application library.");
        return false;
    }

    Platform::sleep(100);

    char application_module_filename[MAX_FILEPATH_LENGTH];
    char application_loaded_module_filename[MAX_FILEPATH_LENGTH];
    CString::print_s(application_module_filename, MAX_FILEPATH_LENGTH, "%s%s%s%s", Platform::get_root_dir(), Platform::dynamic_library_prefix, application_module_name, Platform::dynamic_library_ext);
    CString::print_s(application_loaded_module_filename, MAX_FILEPATH_LENGTH, "%s%s%s_loaded%s", Platform::get_root_dir(), Platform::dynamic_library_prefix, application_module_name, Platform::dynamic_library_ext);

    Platform::ReturnCode r_code;
    do
    {    
        r_code = FileSystem::file_copy(application_module_filename, application_loaded_module_filename, true);
        if (r_code == Platform::ReturnCode::FILE_LOCKED)
            Platform::sleep(100);
    } 
    while (r_code == Platform::ReturnCode::FILE_LOCKED);

    if (r_code != Platform::ReturnCode::SUCCESS)
    {
        SHMERRORV("Failed to copy dll file for module '%s'.", application_module_name);
        return false;
    }

    // TODO: turn back to loaded filename
    if (!load_application_library(app, application_loaded_module_filename, true))
    {
        SHMERRORV("Failed to load application library '%s'.", application_module_name);
        return false;
    }

    return true;
}

bool32 create_application(Application* out_app)
{

    out_app->config.start_pos_x = 100;
    out_app->config.start_pos_y = 100;
    out_app->config.start_width = 1600;
    out_app->config.start_height = 900;
    out_app->config.name = (char*)"Shmengine Sandbox";   
    
    char application_module_filename[MAX_FILEPATH_LENGTH];
    char application_loaded_module_filename[MAX_FILEPATH_LENGTH];
    CString::print_s(application_module_filename, MAX_FILEPATH_LENGTH, "%s%s%s%s", Platform::get_root_dir(), Platform::dynamic_library_prefix, application_module_name, Platform::dynamic_library_ext);
    CString::print_s(application_loaded_module_filename, MAX_FILEPATH_LENGTH, "%s%s%s_loaded%s", Platform::get_root_dir(), Platform::dynamic_library_prefix, application_module_name, Platform::dynamic_library_ext);

    Platform::ReturnCode r_code;
    do
    {
        r_code = FileSystem::file_copy(application_module_filename, application_loaded_module_filename, true);
        if (r_code == Platform::ReturnCode::FILE_LOCKED)
            Platform::sleep(100);
    } 
    while (r_code == Platform::ReturnCode::FILE_LOCKED);

    if (r_code != Platform::ReturnCode::SUCCESS)
    {
        SHMERRORV("Failed to copy dll file for module '%s'.", application_module_name);
        return false;
    }

    //TODO: turn back to loaded dll file
    if (!load_application_library(out_app, application_loaded_module_filename, false))
    {
        SHMERRORV("Failed to load application library '%s'.", application_module_name);
        return false;
    }

    out_app->state = 0;
    out_app->engine_state = 0;

    char renderer_module_filename[MAX_FILEPATH_LENGTH];
    CString::print_s(renderer_module_filename, MAX_FILEPATH_LENGTH, "%s%s%s", Platform::dynamic_library_prefix, renderer_module_name, Platform::dynamic_library_ext);
    if (!Platform::load_dynamic_library(renderer_module_name, renderer_module_filename, &out_app->renderer_lib))
        return false;

    FP_create_renderer_module create_renderer_module = 0;
    if (!Platform::load_dynamic_library_function(&out_app->renderer_lib, "create_module", (void**)&create_renderer_module))
        return false;

    if (!create_renderer_module(&out_app->config.renderer_module))
        return false;

    return true;

}

bool32 init_application(Application* app)
{

    Event::event_register(SystemEventCode::WATCHED_FILE_WRITTEN, app, on_watched_file_written);

    char application_module_filename[MAX_FILEPATH_LENGTH];
    CString::print_s(application_module_filename, MAX_FILEPATH_LENGTH, "%s%s%s%s", Platform::get_root_dir(), Platform::dynamic_library_prefix, application_module_name, Platform::dynamic_library_ext);

    if (Platform::register_file_watch(application_module_filename, &app->application_lib.watch_id) != Platform::ReturnCode::SUCCESS)
    {
        SHMERROR("Failed to register library file watch");
        return false;
    }

    return true;

}