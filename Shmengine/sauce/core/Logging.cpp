#include "Application.hpp"
#include "Logging.hpp"
#include "Assert.hpp"
#include "platform/Platform.hpp"
#include "platform/FileSystem.hpp"

#include "utility/String.hpp"
#include "memory/LinearAllocator.hpp"

#include <stdarg.h>

namespace Log
{

    struct SystemState
    {
        FileSystem::FileHandle log_file;
    };

    static const char* level_strings[6] = { "[FATAL]: ", "[ERROR]: ", "[WARN]: ", "[INFO]: ", "[DEBUG]: ", "[TRACE]: " };
    static SystemState* system_state;

    bool32 system_init(PFN_allocator_allocate_callback allocator_callback, void*& out_state)
    {
        out_state = allocator_callback(sizeof(SystemState));
        system_state = (SystemState*)out_state;

        if (!FileSystem::file_open("D:/dev/Shmengine/bin/Debug-windows-x86_64/Sandbox/console.log", FILE_MODE_WRITE, &system_state->log_file))
        {
            Platform::console_write_error("Error: Unable to open console.log file for writing", LOG_LEVEL_ERROR);
            return false;
        }

        return true;

    }

    void system_shutdown() 
    {
        // TODO: cleanup logging/write queued entries.

        FileSystem::file_close(&system_state->log_file);

        system_state = 0;
    }

    static void append_to_log_file(const char* message)
    {

        uint32 length = String::length(message);
        uint32 written = 0;

        if (!FileSystem::write(&system_state->log_file, length, message, &written))
        {
            Platform::console_write_error("Error: Unable to write to console.log file", LOG_LEVEL_ERROR);
        }

    }

    void log_output(LogLevel level, const char* message, ...) {
        // TODO: Execute all of the logging stuff on another thread

        bool32 is_error = level < LOG_LEVEL_WARN;

        const uint32 msg_length = 4096;
        char out_message[msg_length] = {};

        String::append_to_string(msg_length, out_message, level_strings[level]);

        va_list arg_ptr;
        va_start(arg_ptr, message);
        String::print_s(out_message + (sizeof(level_strings[level])), msg_length - (sizeof(level_strings[level])), message, arg_ptr);
        va_end(arg_ptr);

        String::append_to_string(msg_length, out_message, "\n");

        // Platform-specific output.
        if (is_error) {
            Platform::console_write_error(out_message, (uint8)level);
        }
        else {
            Platform::console_write(out_message, (uint8)level);
        }

        append_to_log_file(out_message);
    }

}

void report_assertion_failure(const char* expression, const char* message, const char* file, int32 line) {
    Log::log_output(Log::LOG_LEVEL_FATAL, "Assertion Failure: %s, message: '%s', in file: %s, line: %d\n", expression, message, file, line);
}

