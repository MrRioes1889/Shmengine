#pragma once

#include "Defines.hpp"
#include "Logging.hpp"
#include "Subsystems.hpp"

namespace Console
{

    struct CommandContext;

    typedef bool32(*FP_consumer_write)(void* inst, Log::LogLevel level, const char* message);
    typedef void (*FP_command)(CommandContext context);

    struct CommandArg
    {
        const char* value;
    };

    struct CommandContext
    {
        uint32 argument_count;
        CommandArg* arguments;
    };

    bool32 system_init(FP_allocator_allocate allocator_callback, void* allocator, void* config);
    void system_shutdown(void* state);

    SHMAPI void register_consumer(void* inst, FP_consumer_write callback);
    void write_line(Log::LogLevel level, const char* message);
    SHMAPI bool32 register_command(const char* command, uint32 arg_count, FP_command callback);
    SHMAPI bool32 execute_command(const char* command);

}