#include <Entry.hpp>

#include <platform/Platform.hpp>

typedef bool32(*FP_create_renderer_module)(Renderer::Module* out_module);
typedef uint64(*FP_get_module_state_size)();

bool32 create_application(Application* out_app) 
{

    out_app->config.start_pos_x = 100;
    out_app->config.start_pos_y = 100;
    out_app->config.start_width = 1600;
    out_app->config.start_height = 900;
    out_app->config.name = (char*)"Shmengine Sandbox";

    if (!Platform::load_dynamic_library("A_Sandbox", &out_app->application_lib))
        return false;

    FP_get_module_state_size get_state_size = 0;
    if (!Platform::load_dynamic_library_function(&out_app->application_lib, "get_module_state_size"))
        return false;
    get_state_size = (FP_get_module_state_size)out_app->application_lib.functions[0].function_ptr;

    if (!Platform::load_dynamic_library_function(&out_app->application_lib, "application_boot"))
        return false;
    if (!Platform::load_dynamic_library_function(&out_app->application_lib, "application_init"))
        return false;
    if (!Platform::load_dynamic_library_function(&out_app->application_lib, "application_shutdown"))
        return false;
    if (!Platform::load_dynamic_library_function(&out_app->application_lib, "application_update"))
        return false;
    if (!Platform::load_dynamic_library_function(&out_app->application_lib, "application_render"))
        return false;
    if (!Platform::load_dynamic_library_function(&out_app->application_lib, "application_on_resize"))
        return false;

    out_app->boot = (Application::FP_boot)out_app->application_lib.functions[1].function_ptr;
    out_app->init = (Application::FP_init)out_app->application_lib.functions[2].function_ptr;
    out_app->shutdown = (Application::FP_shutdown)out_app->application_lib.functions[3].function_ptr;
    out_app->update = (Application::FP_update)out_app->application_lib.functions[4].function_ptr;
    out_app->render = (Application::FP_render)out_app->application_lib.functions[5].function_ptr;
    out_app->on_resize = (Application::FP_on_resize)out_app->application_lib.functions[6].function_ptr;

    out_app->state_size = get_state_size();
    out_app->state = 0;
   
    out_app->engine_state = 0;

    if (!Platform::load_dynamic_library("M_VulkanRenderer", &out_app->renderer_lib))
        return false;

    FP_create_renderer_module create_renderer_module = 0;
    if (!Platform::load_dynamic_library_function(&out_app->renderer_lib, "create_module"))
        return false;
    create_renderer_module = (FP_create_renderer_module)out_app->renderer_lib.functions[0].function_ptr;

    if (!create_renderer_module(&out_app->config.renderer_module))
        return false;   

    return true;
}