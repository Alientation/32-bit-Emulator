#pragma once

#include <iostream>
#include <string>

#include "util/console_color.h"
#include "util/string_util.h"
#include "util/types.h"

//
// Release build has logs disabled
// Debug build will check for a config file to determine what logs are enabled
//
// Log types
//  - Debug
//  - Info
//  - Warn
//  - Error *note* does not throw an error.
//
// Each log type contains a subcategory.
//  - How are subcategories defined by the user?
//      - They should just be the relative file path from the log config file?
//
// Log config file
//  - *.logconfig
//  - line separated, empty lines ignored
//  - file path separated with '.'
//  - file extensions if not specified accept any
//  - lines that begin with '#' are comments and ignored.
//  - Example
//      debug: false
//      info: true
//      warn: true
//      error: true
//
//      debug.core.util.logger: true
//
//  - Default Log configs, everything enabled.
//  - Ideally, each subdirectory would contain a log config file for its files.
//
// On start up, search from the 'core' subdirectory
//

namespace logger_v3
{
inline void print_header (const std::string &type, const char *file, U32 line, const char *func)
{
    static const std::string root_directory =
        std::string (__FILE__).substr (0, std::string (__FILE__).rfind ("core"));

    std::cout << "[" << ccolor::BOLD << type << ccolor::RESET << "] [" << ccolor::BLUE
              << string_util::replaceFirst (file, root_directory, "") << ccolor::RESET << ":"
              << ccolor::MAGENTA << line << ccolor::RESET << "] [" << ccolor::YELLOW << func
              << ccolor::RESET << "]: ";
}

template<typename... Args>
inline void log_debug (const char *format, const char *file, int line, const char *func,
                       Args &&...args)
{
}

template<typename... Args>
inline void log_info (const char *format, const char *file, int line, const char *func,
                      Args &&...args)
{
}

template<typename... Args>
inline void log_warn (const char *format, const char *file, int line, const char *func,
                      Args &&...args)
{
}

template<typename... Args>
inline void log_error (const char *format, const char *file, int line, const char *func,
                       Args &&...args)
{
}

} // namespace logger_v3

#ifndef NDEBUG
#define DEBUG(format, ...)                                                                         \
    logger_v3::log_debug (format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define INFO(format, ...) logger_v3::log_info (format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define WARN(format, ...) logger_v3::log_warn (format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define ERROR(format, ...)                                                                         \
    logger_v3::log_error (format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define DEBUG(format, ...) ;
#define INFO(format, ...) ;
#define WARN(format, ...) ;
#define ERROR(format, ...) ;
#endif