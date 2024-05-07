#pragma once

#include "Defines.hpp"

#define LOG_WARN_ENABLED 1
#define LOG_INFO_ENABLED 1
#define LOG_DEBUG_ENABLED 1
#define LOG_TRACE_ENABLED 1

// Disable debug and trace logging for release builds.
#if SHMRELEASE == 1
#define LOG_DEBUG_ENABLED 0
#define LOG_TRACE_ENABLED 0
#endif

namespace Log
{

    enum LogLevel {
        LOG_LEVEL_FATAL = 0,
        LOG_LEVEL_ERROR = 1,
        LOG_LEVEL_WARN = 2,
        LOG_LEVEL_INFO = 3,
        LOG_LEVEL_DEBUG = 4,
        LOG_LEVEL_TRACE = 5
    };

    bool32 initialize_logging();
    void logging_shutdown();

    SHMAPI void log_output(LogLevel level, const char* message, ...);
    //SHMAPI void log_output(LogLevel level, const char* message);

}

    // Logs a fatal-level message.
#define SHMFATALV(message, ...) Log::log_output(Log::LOG_LEVEL_FATAL, message, ##__VA_ARGS__)
#define SHMFATAL(message) Log::log_output(Log::LOG_LEVEL_FATAL, message)

#ifndef SHMERROR
// Logs an error-level message.
#define SHMERRORV(message, ...) Log::log_output(Log::LOG_LEVEL_ERROR, message, ##__VA_ARGS__)
#define SHMERROR(message) Log::log_output(Log::LOG_LEVEL_ERROR, message)
#endif

#if LOG_WARN_ENABLED == 1
// Logs a warning-level message.
#define SHMWARNV(message, ...) Log::log_output(Log::LOG_LEVEL_WARN, message, ##__VA_ARGS__)
#define SHMWARN(message) Log::log_output(Log::LOG_LEVEL_WARN, message)
#else
// Does nothing when LOG_WARN_ENABLED != 1
#define SHMWARNV(message, ...)
#define SHMWARN(message)
#endif

#if LOG_INFO_ENABLED == 1
// Logs a info-level message.
#define SHMINFOV(message, ...) Log::log_output(Log::LOG_LEVEL_INFO, message, ##__VA_ARGS__)
#define SHMINFO(message) Log::log_output(Log::LOG_LEVEL_INFO, message)
#else
// Does nothing when LOG_INFO_ENABLED != 1
#define SHMINFOV(message, ...)
#define SHMINFO(message)
#endif

#if LOG_DEBUG_ENABLED == 1
// Logs a debug-level message.
#define SHMDEBUGV(message, ...) Log::log_output(Log::LOG_LEVEL_DEBUG, message, ##__VA_ARGS__)
#define SHMDEBUG(message) Log::log_output(Log::LOG_LEVEL_DEBUG, message)
#else
// Does nothing when LOG_DEBUG_ENABLED != 1
#define SHMDEBUGV(message, ...)
#define SHMDEBUG(message)
#endif

#if LOG_TRACE_ENABLED == 1
// Logs a trace-level message.
#define SHMTRACEV(message, ...) Log::log_output(Log::LOG_LEVEL_TRACE, message, ##__VA_ARGS__)
#define SHMTRACE(message) Log::log_output(Log::LOG_LEVEL_TRACE, message)
#else
// Does nothing when LOG_TRACE_ENABLED != 1
#define SHMTRACEV(message, ...)
#define SHMTRACE(message)
#endif
