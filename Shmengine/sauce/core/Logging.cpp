#include "Logging.hpp"
#include "Assert.hpp"
#include "platform/Platform.hpp"
#include "utility/string/String.hpp"

#include <stdarg.h>

namespace Log
{

    static const char* level_strings[6] = { "[FATAL]: ", "[ERROR]: ", "[WARN]: ", "[INFO]: ", "[DEBUG]: ", "[TRACE]: " };

    bool32 initialize_logging() {
        // TODO: create log file.
        return true;
    }

    void logging_shutdown() {
        // TODO: cleanup logging/write queued entries.
    }

    void log_output(LogLevel level, const char* message, ...) {
        bool32 is_error = level < LOG_LEVEL_WARN;

        const uint32 msg_length = 2048;
        char out_message[msg_length] = {};

        String::append_to_string(msg_length, out_message, (char*)level_strings[level]);

        va_list arg_ptr;
        va_start(arg_ptr, message);
        String::print_s(out_message + (sizeof(level_strings[level])), msg_length - (sizeof(level_strings[level])), (char*)message, arg_ptr);
        //vsnprintf(out_message, msg_length, message, arg_ptr);
        va_end(arg_ptr);

        String::append_to_string(msg_length, out_message, (char*)"\n");

        //sprintf(out_message2, "%s%s\n", level_strings[level], out_message);

        // Platform-specific output.
        if (is_error) {
            Platform::console_write_error(out_message, (uint8)level);
        }
        else {
            Platform::console_write(out_message, (uint8)level);
        }
    }

}

void report_assertion_failure(const char* expression, const char* message, const char* file, int32 line) {
    Log::log_output(Log::LOG_LEVEL_FATAL, "Assertion Failure: %s, message: '%s', in file: %s, line: %d\n", expression, message, file, line);
}

