#pragma once

#include "util/console_color.h"
#include "util/string_util.h"

#include <iostream>

/*
    what i want for a logger

    // -     toggleable logs (levels + each unit that uses a log has the ability to easily disable logs in that file)
    // -     print + noprint
    -    dump to file with filters, query logs
    // -    timer/profiler
        // - tag a profile log
        - dump to file and query profiler
        - have separate log type for profiler so it can be also queried from the log file
    // -     color print
    // -     automatically detect the line and file a log occurs in, this means a macro. problem with macros is
        // they do not show types well
    // -    ability to use printf to log (so pass variadic args to printf), also can use std::stringstream, though
        // problem with this is how do we capture the result of printf to save to file when needed??
    // -    fast, but not a high concern as for performance we can disable all low level logs
*/

/**
 * @defgroup             AEMU_LOG_LEVELS
 * @brief                 Log levels for AEMU. Set @ref AEMU_LOG_LEVEL to one of these levels to
 *                         control logging.
 *
 *                         This group includes the different logging levels available to configure the
 *                         logger.
 *                         - `AEMU_LOG_DEBUG` (4) : Debug level, all logs will be displayed and
 *                             tracked.
 *                         - `AEMU_LOG_INFO` (3)  : Information level, informational logs will be
 *                             displayed.
 *                         - `AEMU_LOG_WARN` (2)  : Warning level, only warnings and higher severity
 *                             logs will be displayed.
 *                         - `AEMU_LOG_ERROR` (1) : Error level, only errors will be displayed.
 *                         - `AEMU_LOG_NONE` (0)  : No logging.
 * @todo                TODO: rethink this, a file that includes this won't have access to
 *                         these constants before the header is included.
 * @{
 */
constexpr int AEMU_LOG_DEBUG = 4;
constexpr int AEMU_LOG_INFO = 3;
constexpr int AEMU_LOG_WARN = 2;
constexpr int AEMU_LOG_ERROR = 1;
constexpr int AEMU_LOG_NONE = 0;
/**@} */ // end of AEMU_LOG_LEVELS

/**
 * @defgroup            AEMU_LOG_CONTROL
 * @brief                 Controls functionality of the logger. Set these control boolean flags to
 *                         control print and tracking.
 *
 *                         The available control flags are.
 *                         - `AEMU_ONLY_CRITICAL_LOG` : Only logs WARN and ERROR messages.
 *                         - `AEMU_LOG_LEVEL` : Set the level of logs to be displayed, corresponding to
 *                             @ref AEMU_LOG_LEVELS
 *                         - `AEMU_LOG_ENABLED` : Toggle whether logs will happen. Turn off to
 *                             minimize/nullify the performance impact of logs.
 *                         - `AEMU_PRINT_ENABLED` : Toggle whether logs will be printed to standard
 *                             out. If @ref AEMU_LOG_ENABLED is disabled, no print will occur.
 *                         - `AEMU_EXCEPT_ON_ERROR` : Toggle whether error logs will exit with failure
 *                             code.
 *                         - `AEMU_PROJECT_ROOT_DIR` : Set to a c++ string of the full path to project
 *                             root directory. The default implementation is dependent on the root
 *                             folder `core`.
 * @{
 */
#ifdef AEMU_ONLY_CRITICAL_LOG
#define AEMU_LOG_LEVEL AEMU_LOG_WARN
#endif /* AEMU_ONLY_CRITICAL_LOG */

#ifndef AEMU_LOG_LEVEL
#define AEMU_LOG_LEVEL AEMU_LOG_DEBUG
#endif /* AEMU_LOG_LEVEL */

#ifndef AEMU_LOG_ENABLED
#define AEMU_LOG_ENABLED true
#endif /* AEMU_LOG_DISABLED */

#ifndef AEMU_PRINT_ENABLED
#define AEMU_PRINT_ENABLED true
#endif /* AEMU_PRINT_DISABLED */

#ifndef AEMU_EXCEPT_ON_ERROR
#define AEMU_EXCEPT_ON_ERROR true
#endif /* AEMU_EXCEPT_ON_ERROR */

#ifndef AEMU_PROJECT_ROOT_DIR
#define AEMU_PROJECT_ROOT_DIR                                                                      \
    std::string (__FILE__).substr (0, std::string (__FILE__).rfind ("core"))
#endif   /* AEMU_PROJECT_ROOT_DIR */
/**@} */ // end of AEMU_LOG_CONTROL

#ifndef AEMU_PROFILER_ENABLED
#define AEMU_PROFILER_ENABLED true
#endif /* AEMU_PROFILER_ENABLED */

#ifndef AEMU_PROFILER_LOG_ENABLED
#define AEMU_PROFILER_LOG_ENABLED true
#endif /* AEMU_PROFILER_LOG_ENABLED */

namespace logger
{
/**
     * @brief            Tracks a log into memory.
     *
     * @param type         The type of log stringified.
     * @param format     Format of the string. (passed into printf)
     * @param file        The file the log occured in.
     * @param line        Line of code the log occured in.
     * @param func        Function name the log occured in.
     * @param ...        Any extra arguments used with the supplied format to construct the log.
     */
void track (const std::string &type, const char *format, const char *file, int line,
            const char *func, ...);

/**
     * @brief            Tracks the start of running time that the profiler is observing.
     *
     * @param file
     * @param line
     * @param func
     */
void clock_start_master (const char *file, int line, const char *func);

/**
     * @brief            Tracks the end of running time that the profiler is observing. The master
     *                     clock can be restarted.
     */
void clock_stop_master ();

/**
     * @brief             Tracks the start of a specific clock
     *
     * @param tag
     * @param file
     * @param line
     * @param func
     */
void clock_start (const std::string &tag, const char *file, int line, const char *func);

/**
     * @brief             Stops the most recently started clock, but can be restarted with start call.
     *                     Clocks are organized in a hierarchy,
     *                     so starting sequential clocks will mean the top most clock will have a
     *                     longer lifespan than the clock at the root.
     */
void clock_stop ();

/**
     * @brief            Ends the most recently started clock, moving the current clock down the
     *                     hierarchy.
     *
     */
void clock_end ();

/* TODO: query profile logs, dump to file, etc */

/* TODO: query logs, dump to file, etc */

/**
     * @brief             Prints the formated header.
     *
     * @param type        Type of log.
     * @param file        File the log occured in.
     * @param line        Line of code the log occured in.
     * @param func        Function name the log occured in.
     */
inline void print_header (const std::string &type, const char *file, int line, const char *func)
{
    std::cout << "[" << ccolor::BOLD << type << ccolor::RESET << "] [" << ccolor::BLUE
              << string_util::replaceFirst (file, AEMU_PROJECT_ROOT_DIR, "") << ccolor::RESET << ":"
              << ccolor::MAGENTA << line << ccolor::RESET << "] [" << ccolor::YELLOW << func
              << ccolor::RESET << "]: ";
}

/**
     * @brief             Debug log.
     *
     * @tparam Args        Variadic arguments to be passed into printf.
     * @param format    Format of the printf.
     * @param file        File the log occured in.
     * @param line        Line of code the log occured in.
     * @param func        Function name the log occured in.
     * @param args        Arguments passed into printf.
     */
template<typename... Args>
inline void log_debug (const char *format, const char *file, int line, const char *func,
                       Args &&...args)
{
    if (AEMU_LOG_ENABLED)
    {
        if (AEMU_PRINT_ENABLED && AEMU_LOG_LEVEL >= AEMU_LOG_DEBUG)
        {
            print_header (ccolor::MAGENTA + "DBG", file, line, func);
            printf (format, args...);
            std::cout << "\n";
        }

        track ("DBG", format, file, line, func, args...);
    }
}

/**
     * @brief             Info log.
     *
     * @tparam Args        Variadic arguments to be passed into printf.
     * @param format    Format of the printf.
     * @param file        File the log occured in.
     * @param line        Line of code the log occured in.
     * @param func        Function name the log occured in.
     * @param args        Arguments passed into printf.
     *
     * @todo            TODO: Cannot disable log_info when setting AEMU_LOG_INFO definition to
     *                     LOG_WARN
     */
template<typename... Args>
inline void log_info (const char *format, const char *file, int line, const char *func,
                      Args &&...args)
{
    if (AEMU_LOG_ENABLED)
    {
        if (AEMU_PRINT_ENABLED && AEMU_LOG_LEVEL >= AEMU_LOG_INFO)
        {
            print_header (ccolor::BLUE + "INF", file, line, func);
            printf (format, args...);
            std::cout << "\n";
        }

        track ("INF", format, file, line, func, args...);
    }
}

/**
     * @brief             Warn log.
     *
     * @tparam Args        Variadic arguments to be passed into printf.
     * @param format    Format of the printf.
     * @param file        File the log occured in.
     * @param line        Line of code the log occured in.
     * @param func        Function name the log occured in.
     * @param args        Arguments passed into printf.
     */
template<typename... Args>
inline void log_warn (const char *format, const char *file, int line, const char *func,
                      Args &&...args)
{
    if (AEMU_LOG_ENABLED)
    {
        if (AEMU_PRINT_ENABLED && AEMU_LOG_LEVEL >= AEMU_LOG_WARN)
        {
            print_header (ccolor::YELLOW + "WRN", file, line, func);
            printf (format, args...);
            std::cout << "\n";
        }

        track ("WRN", format, file, line, func, args...);
    }
}

/**
     * @brief             Error log.
     * @note            This will not terminate the program.
     *
     * @tparam Args        Variadic arguments to be passed into printf.
     * @param format    Format of the printf.
     * @param file        File the log occured in.
     * @param line        Line of code the log occured in.
     * @param func        Function name the log occured in.
     * @param args        Arguments passed into printf.
     */
template<typename... Args>
inline void log_error (const char *format, const char *file, int line, const char *func,
                       Args &&...args)
{
    if (AEMU_LOG_ENABLED)
    {
        if (AEMU_PRINT_ENABLED && AEMU_LOG_LEVEL >= AEMU_LOG_ERROR)
        {
            print_header (ccolor::RED + "ERR", file, line, func);
            printf (format, args...);
            std::cout << "\n";
        }

        track ("ERR", format, file, line, func, args...);
    }

    if (AEMU_EXCEPT_ON_ERROR)
    {
        exit (EXIT_FAILURE);
    }
}

template<typename... Args>
inline void expect_true (bool condition, const char *format, const char *file, int line,
                         const char *func, Args &&...args)
{
    if (!condition)
    {
        log_error (format, file, line, func, args...);
    }
}

template<typename... Args>
inline void expect_false (bool condition, const char *format, const char *file, int line,
                          const char *func, Args &&...args)
{
    if (condition)
    {
        log_error (format, file, line, func, args...);
    }
}

template<typename T1, typename T2, typename... Args>
inline void expect_equal (T1 t1, T2 t2, const char *format, const char *file, int line,
                          const char *func, Args &&...args)
{
    if (t1 != t2)
    {
        log_error (format, file, line, func, args...);
    }
}

template<typename T1, typename T2, typename... Args>
inline void expect_not_equal (T1 t1, T2 t2, const char *format, const char *file, int line,
                              const char *func, Args &&...args)
{
    if (t1 == t2)
    {
        log_error (format, file, line, func, args...);
    }
}

/**
     * @def                DEBUG(format, ...)
     * @brief             Wrap log to gather information like file, LOC, and function name.
     * @param format    The format string.
     * @param ...        Additional arguments for the format string.
     */

#ifndef AEMU_ONLY_CRITICAL_LOG
#define DEBUG(format, ...) logger::log_debug (format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define DEBUG(format, ...) ;
#endif

/**
     * @def                DEBUG_SS(msg)
     * @brief             Wrap log to gather information like file, LOC, and function name.
     * @param msg        stringstream log.
     */
#ifndef AEMU_ONLY_CRITICAL_LOG
#define DEBUG_SS(msg) logger::log_debug ((msg).str ().c_str (), __FILE__, __LINE__, __func__)
#else
#define DEBUG_SS(msg) ;
#endif

/**
     * @def                INFO(format, ...)
     * @brief             Wrap log to gather information like file, LOC, and function name.
     * @param format    The format string.
     * @param ...        Additional arguments for the format string.
     */
#ifndef AEMU_ONLY_CRITICAL_LOG
#define INFO(format, ...) logger::log_info (format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define INFO(format, ...) ;
#endif

/**
     * @def                INFO_SS(msg)
     * @brief             Wrap log to gather information like file, LOC, and function name.
     * @param msg        stringstream log.
     */
#ifndef AEMU_ONLY_CRITICAL_LOG
#define INFO_SS(msg) logger::log_info ((msg).str ().c_str (), __FILE__, __LINE__, __func__)
#else
#define INFO_SS(msg) ;
#endif

/**
     * @def                WARN(format, ...)
     * @brief             Wrap log to gather information like file, LOC, and function name.
     * @param format    The format string.
     * @param ...        Additional arguments for the format string.
     */
#define WARN(format, ...) logger::log_warn (format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

/**
     * @def                WARN_SS(msg)
     * @brief             Wrap log to gather information like file, LOC, and function name.
     * @param msg        stringstream log.
     */
#define WARN_SS(msg) logger::log_warn ((msg).str ().c_str (), __FILE__, __LINE__, __func__)

/**
     * @def                ERROR(format, ...)
     * @brief             Wrap log to gather information like file, LOC, and function name.
     * @param format    The format string.
     * @param ...        Additional arguments for the format string.
     */
#define ERROR(format, ...) logger::log_error (format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

/**
     * @def                ERROR_SS(msg)
     * @brief             Wrap log to gather information like file, LOC, and function name.
     * @param msg        stringstream log.
     */
#define ERROR_SS(msg) logger::log_error ((msg).str ().c_str (), __FILE__, __LINE__, __func__)

#define EXPECT_TRUE(condition, format, ...)                                                        \
    logger::expect_true (condition, format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define EXPECT_TRUE_SS(condition, msg)                                                             \
    logger::expect_true (condition, (msg).str ().c_str (), __FILE__, __LINE__, __func__)
#define EXPECT_FALSE(condition, format, ...)                                                       \
    logger::expect_false (condition, format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define EXPECT_FALSE_SS(condition, msg)                                                            \
    logger::expect_false (condition, (msg).str ().c_str (), __FILE__, __LINE__, __func__)
#define EXPECT_EQUAL(t1, t2, format, ...)                                                          \
    logger::expect_equal (t1, t2, format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define EXPECT_EQUAL_SS(t1, t2, msg)                                                               \
    logger::expect_equal (t1, t2, (msg).str ().c_str (), __FILE__, __LINE__, __func__)
#define EXPECT_NOT_EQUAL(t1, t2, format, ...)                                                      \
    logger::expect_equal (t1, t2, format, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define EXPECT_NOT_EQUAL_SS(t1, t2, msg)                                                           \
    logger::expect_equal (t1, t2, (msg).str ().c_str (), __FILE__, __LINE__, __func__)

#define PROFILE_START logger::clock_start_master (__FILE__, __LINE__, __func__);
#define PROFILE_STOP logger::clock_stop_master ();
#define CLOCK_START(tag) logger::clock_start (tag, __FILE__, __LINE__, __func__);
#define CLOCK_STOP logger::clock_stop ();
#define CLOCK_END logger::clock_end ();

}; // namespace logger