#pragma once

/**
 * @file logger_v2.h
 * @brief Header-only logger. Replaces util/logger.h + util/logger.cpp.
 *
 * Design goals
 *  - Configured at RUNTIME (API calls or the AEMU_LOG_LEVEL environment variable), never through
 *    per-file #defines. Including this header from another header cannot change the behavior of
 *    anything downstream, and every translation unit sees identical inline function bodies
 *    (no ODR violations).
 *  - Log lines go to stderr, so stdout stays clean for program output (e.g. `emu32` state dumps).
 *  - Format strings are checked at compile time (std::format_string). A bad format string or a
 *    printf style "%u" with arguments is a compile error or is printed literally, not a
 *    runtime std::format_error.
 *  - Macro names are prefixed with AEMU_ so they cannot collide with gtest (EXPECT_TRUE), syslog
 *    (LOG_DEBUG), Windows (ERROR) or other libraries (DEBUG).
 *  - Arguments are only evaluated and formatted when the message will actually be emitted.
 *  - Thread safe. Each message is formatted first and then written under a lock.
 *
 * Quick reference
 *
 *   AEMU_DEBUG("loaded {} pages", n);       // level Debug
 *   AEMU_INFO(...);                         // level Info
 *   AEMU_WARN(...);                         // level Warn
 *   AEMU_ERROR(...);                        // level Error, NOT fatal, execution continues
 *   AEMU_FATAL(...);                        // logs, then terminates (see FatalAction)
 *   AEMU_CHECK(cond, "msg {}", x);          // always on. Fatal if cond is false
 *   AEMU_DCHECK(cond, "msg {}", x);         // like AEMU_CHECK but compiled out when NDEBUG is set
 *   AEMU_SCOPED_TIMER("assemble");          // logs the scope's duration at Debug level
 *
 * Runtime configuration (all in namespace aemu::log)
 *
 *   set_level(Level::Debug);                // or env AEMU_LOG_LEVEL=debug|info|warn|error|fatal|off
 *   set_fatal_action(FatalAction::Throw);   // Exit (default), Abort or Throw (FatalError)
 *   set_color(false); set_timestamps(true);
 *   set_sink([](const Record &r) { ... });  // redirect output, e.g. to a file (see below)
 *   ScopedLevel / ScopedFatalAction         // RAII helpers, handy in unit tests
 *
 * Writing logs to a file does not need dedicated machinery. A sink is enough:
 *
 *   static std::ofstream file("aemu.log");
 *   aemu::log::set_sink([](const aemu::log::Record &r)
 *                       { file << aemu::log::to_string(r.level) << ' ' << r.message << '\n'; });
 *
 * The one compile time knob, AEMU_LOG_COMPILE_LEVEL, removes call sites below a level from the
 * binary entirely. It is intended to be set once for the whole build (target_compile_definitions
 * on every target), NEVER in a source or header file. Leave it at 0 unless profiling says the
 * runtime level check matters. It is a single relaxed atomic load and a branch.
 *
 * Requires C++20 (<format>, <source_location>).
 */

#include "util/console_color.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <functional>
#include <mutex>
#include <optional>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

/// Calls to AEMU_<LEVEL> below this level (0=Debug, 1=Info, 2=Warn, 3=Error) are compiled out.
/// Build-wide setting only. See the file comment.
#ifndef AEMU_LOG_COMPILE_LEVEL
#define AEMU_LOG_COMPILE_LEVEL 0
#endif

namespace aemu::log
{

/// Severity of a message, in increasing order. `Off` is only meaningful as a threshold.
enum class Level : std::uint8_t
{
    Debug = 0,
    Info,
    Warn,
    Error,
    Fatal,
    Off,
};

/// What AEMU_FATAL / a failed AEMU_CHECK does after logging the message.
enum class FatalAction : std::uint8_t
{
    /// std::exit(EXIT_FAILURE). Default, matches the old behavior of ERROR().
    Exit,

    /// std::abort(). Produces a core dump / debugger break.
    Abort,

    /// Throws FatalError. Lets unit tests assert on failures instead of killing the test binary.
    Throw,
};

/// Thrown by fatal errors when the FatalAction is Throw.
class FatalError : public std::runtime_error
{
  public:
    using std::runtime_error::runtime_error;
};

/// A single log message as handed to a sink.
struct Record
{
    Level level;
    std::source_location location;

    /// Fully formatted message, without a trailing newline. Only valid during the sink call.
    std::string_view message;

    /// Time since the logger was first used.
    std::chrono::steady_clock::duration since_start;
};

/// Receives every message that passes the level filter. Called with the logger lock held, so a
/// sink does not need its own synchronization, but it must not log (that would deadlock).
using Sink = std::function<void(const Record &)>;

constexpr std::string_view to_string(Level level)
{
    switch (level)
    {
    case Level::Debug:
        return "DBG";
    case Level::Info:
        return "INF";
    case Level::Warn:
        return "WRN";
    case Level::Error:
        return "ERR";
    case Level::Fatal:
        return "FTL";
    case Level::Off:
        return "OFF";
    }
    return "???";
}

/// Parses "debug", "info", "warn"/"warning", "error", "fatal", "off"/"none". Case insensitive.
inline std::optional<Level> parse_level(std::string_view name)
{
    std::string lower(name);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "debug")
    {
        return Level::Debug;
    }
    if (lower == "info")
    {
        return Level::Info;
    }
    if (lower == "warn" || lower == "warning")
    {
        return Level::Warn;
    }
    if (lower == "error")
    {
        return Level::Error;
    }
    if (lower == "fatal")
    {
        return Level::Fatal;
    }
    if (lower == "off" || lower == "none")
    {
        return Level::Off;
    }
    return std::nullopt;
}

namespace detail
{

inline bool stream_is_tty(std::FILE *stream)
{
#ifdef _WIN32
    return _isatty(_fileno(stream)) != 0;
#else
    return isatty(fileno(stream)) != 0;
#endif
}

/// Global logger state. One instance for the whole program, since this is a function local
/// static in an inline function. Intentionally leaked so logging from other static destructors
/// is always safe.
struct State
{
    std::atomic<Level> level{Level::Info};
    std::atomic<FatalAction> fatal_action{FatalAction::Exit};
    std::atomic<bool> color{false};
    std::atomic<bool> timestamps{false};

    std::mutex mutex; ///< Guards `sink` and serializes output.
    Sink sink;        ///< Empty means the default stderr sink.

    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    State()
    {
        if (const char *env = std::getenv("AEMU_LOG_LEVEL"))
        {
            if (const std::optional<Level> parsed = parse_level(env))
            {
                level.store(*parsed);
            }
        }

        // Only color when writing to a terminal. https://no-color.org
        color.store(std::getenv("NO_COLOR") == nullptr && stream_is_tty(stderr));
    }
};

inline State &state()
{
    static State *instance = new State();
    return *instance;
}

/// Whether call sites of this level survive AEMU_LOG_COMPILE_LEVEL.
constexpr bool compiled_in(Level level)
{
    constexpr int kCompileLevel = AEMU_LOG_COMPILE_LEVEL;
    return static_cast<int>(level) >= kCompileLevel;
}

constexpr std::string_view level_color(Level level)
{
    switch (level)
    {
    case Level::Debug:
        return ccolor::MAGENTA;
    case Level::Info:
        return ccolor::BLUE;
    case Level::Warn:
        return ccolor::YELLOW;
    case Level::Error:
        return ccolor::RED;
    case Level::Fatal:
        return ccolor::BOLD_RED;
    default:
        return "";
    }
}

/// "/a/b/c.cpp" -> "c.cpp". Source paths are printed without their directory.
inline std::string_view basename(std::string_view path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

/// Default sink: `[ERR] [file.cpp:123]: message` on stderr, with an optional time prefix.
inline void write_default(const Record &record, bool color, bool timestamps)
{
    std::string line;

    if (timestamps)
    {
        line +=
            std::format("[{:9.3f}s] ", std::chrono::duration<double>(record.since_start).count());
    }

    if (color)
    {
        line += std::format("[{}{}\033[0m]", level_color(record.level), to_string(record.level));
    }
    else
    {
        line += std::format("[{}]", to_string(record.level));
    }

    line += std::format(" [{}:{}]: {}\n", basename(record.location.file_name()),
                        record.location.line(), record.message);

    // A single write keeps lines from different threads/processes from interleaving.
    std::fwrite(line.data(), 1, line.size(), stderr);
    if (record.level >= Level::Error)
    {
        std::fflush(stderr);
    }
}

} // namespace detail

// ---------------------------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------------------------

/// Current minimum level that is emitted.
inline Level get_level()
{
    return detail::state().level.load(std::memory_order_relaxed);
}

inline void set_level(Level level)
{
    detail::state().level.store(level);
}

/// Whether a message of this level would currently be emitted.
inline bool enabled(Level level)
{
    return level >= get_level();
}

inline FatalAction get_fatal_action()
{
    return detail::state().fatal_action.load();
}

inline void set_fatal_action(FatalAction action)
{
    detail::state().fatal_action.store(action);
}

/// Enable or disable ANSI colors in the default sink. Auto detected by default (tty, NO_COLOR).
inline void set_color(bool enable)
{
    detail::state().color.store(enable);
}

/// Prefix each line of the default sink with the seconds since the logger started.
inline void set_timestamps(bool enable)
{
    detail::state().timestamps.store(enable);
}

/// Replaces the default stderr output. Pass an empty function (or call reset_sink) to restore it.
inline void set_sink(Sink sink)
{
    detail::State &s = detail::state();
    std::lock_guard lock(s.mutex);
    s.sink = std::move(sink);
}

inline void reset_sink()
{
    set_sink(nullptr);
}

/// Sets the level for the lifetime of the object, then restores the previous one.
class ScopedLevel
{
  public:
    explicit ScopedLevel(Level level) :
        m_previous(get_level())
    {
        set_level(level);
    }

    ~ScopedLevel()
    {
        set_level(m_previous);
    }

    ScopedLevel(const ScopedLevel &) = delete;
    ScopedLevel &operator=(const ScopedLevel &) = delete;

  private:
    Level m_previous;
};

/// Sets the fatal action for the lifetime of the object, then restores the previous one.
/// e.g. `ScopedFatalAction guard(FatalAction::Throw); EXPECT_THROW(f(), FatalError);`
class ScopedFatalAction
{
  public:
    explicit ScopedFatalAction(FatalAction action) :
        m_previous(get_fatal_action())
    {
        set_fatal_action(action);
    }

    ~ScopedFatalAction()
    {
        set_fatal_action(m_previous);
    }

    ScopedFatalAction(const ScopedFatalAction &) = delete;
    ScopedFatalAction &operator=(const ScopedFatalAction &) = delete;

  private:
    FatalAction m_previous;
};

// ---------------------------------------------------------------------------------------------
// Emitting messages
// ---------------------------------------------------------------------------------------------

/// Emits an already formatted message if `level` passes the filter. Use this when the message is
/// not a format string, otherwise prefer the AEMU_* macros.
inline void write(Level level, const std::source_location &location, std::string_view message)
{
    detail::State &s = detail::state();
    if (level < s.level.load(std::memory_order_relaxed))
    {
        return;
    }

    const Record record{level, location, message, std::chrono::steady_clock::now() - s.start};

    std::lock_guard lock(s.mutex);
    if (s.sink)
    {
        s.sink(record);
    }
    else
    {
        detail::write_default(record, s.color.load(), s.timestamps.load());
    }
}

/// Logs at Fatal level, then terminates according to the FatalAction. Never returns.
[[noreturn]] inline void fatal(const std::source_location &location, std::string_view message)
{
    write(Level::Fatal, location, message);

    const FatalAction action = get_fatal_action();
    if (action == FatalAction::Throw)
    {
        throw FatalError(std::string(message));
    }
    if (action == FatalAction::Abort)
    {
        std::abort();
    }
    std::exit(EXIT_FAILURE);
}

namespace detail
{

template<typename... Args>
void log(Level level, const std::source_location &location, std::format_string<Args...> fmt,
         Args &&...args)
{
    write(level, location, std::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
[[noreturn]] void fatal(const std::source_location &location, std::format_string<Args...> fmt,
                        Args &&...args)
{
    ::aemu::log::fatal(location, std::string_view(std::format(fmt, std::forward<Args>(args)...)));
}

template<typename... Args>
[[noreturn]] void check_failed(const std::source_location &location, std::string_view expression,
                               std::format_string<Args...> fmt, Args &&...args)
{
    ::aemu::log::fatal(
        location, std::string_view(std::format("Check failed: ({}): {}", expression,
                                               std::format(fmt, std::forward<Args>(args)...))));
}

} // namespace detail

// ---------------------------------------------------------------------------------------------
// Scoped timer (replaces CLOCK_START / CLOCK_END)
// ---------------------------------------------------------------------------------------------

/// Logs how long its scope took at Debug level when destroyed. Unlike the old start/end clocks
/// it cannot be left unbalanced, and it keeps no global state.
class ScopedTimer
{
  public:
    explicit ScopedTimer(std::string tag,
                         std::source_location location = std::source_location::current()) :
        m_tag(std::move(tag)),
        m_location(location),
        m_start(std::chrono::steady_clock::now())
    {
    }

    ~ScopedTimer()
    {
        if (!enabled(Level::Debug))
        {
            return;
        }

        const long long ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                 std::chrono::steady_clock::now() - m_start)
                                 .count();
        write(Level::Debug, m_location, std::format("{} took {}", m_tag, format_duration(ns)));
    }

    ScopedTimer(const ScopedTimer &) = delete;
    ScopedTimer &operator=(const ScopedTimer &) = delete;

  private:
    static std::string format_duration(long long ns)
    {
        if (ns <= 10'000)
        {
            return std::format("{}ns", ns);
        }
        if (ns <= 10'000'000)
        {
            return std::format("{:.2f}us", static_cast<double>(ns) / 1e3);
        }
        if (ns <= 10'000'000'000)
        {
            return std::format("{:.2f}ms", static_cast<double>(ns) / 1e6);
        }
        return std::format("{:.2f}s", static_cast<double>(ns) / 1e9);
    }

    std::string m_tag;
    std::source_location m_location;
    std::chrono::steady_clock::time_point m_start;
};

} // namespace aemu::log

// ---------------------------------------------------------------------------------------------
// Macros
//
// Macros are only used so that (1) the source location is the caller's, and (2) arguments are not
// evaluated when the message is filtered out. The condition below is still type checked when a
// level is compiled out, so there are no unused variable warnings.
// ---------------------------------------------------------------------------------------------

#define AEMU_LOG_AT_(level, ...)                                                                   \
    do                                                                                             \
    {                                                                                              \
        if constexpr (::aemu::log::detail::compiled_in(level))                                     \
        {                                                                                          \
            if (::aemu::log::enabled(level))                                                       \
            {                                                                                      \
                ::aemu::log::detail::log(level, std::source_location::current(), __VA_ARGS__);     \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define AEMU_DEBUG(...) AEMU_LOG_AT_(::aemu::log::Level::Debug, __VA_ARGS__)
#define AEMU_INFO(...) AEMU_LOG_AT_(::aemu::log::Level::Info, __VA_ARGS__)
#define AEMU_WARN(...) AEMU_LOG_AT_(::aemu::log::Level::Warn, __VA_ARGS__)
#define AEMU_ERROR(...) AEMU_LOG_AT_(::aemu::log::Level::Error, __VA_ARGS__)

/// Logs and terminates. Never returns, so no `return;` is needed afterwards.
#define AEMU_FATAL(...) ::aemu::log::detail::fatal(std::source_location::current(), __VA_ARGS__)

/// Always evaluated. Fatal when `cond` is false. The message is mandatory (use "" if none).
#define AEMU_CHECK(cond, ...)                                                                      \
    do                                                                                             \
    {                                                                                              \
        if (!(cond)) [[unlikely]]                                                                  \
        {                                                                                          \
            ::aemu::log::detail::check_failed(std::source_location::current(), #cond,              \
                                              __VA_ARGS__);                                        \
        }                                                                                          \
    } while (0)

/// Like AEMU_CHECK but removed (still type checked) when NDEBUG is defined. For hot paths.
#ifdef NDEBUG
#define AEMU_DCHECK(cond, ...)                                                                     \
    do                                                                                             \
    {                                                                                              \
        if constexpr (false)                                                                       \
        {                                                                                          \
            AEMU_CHECK(cond, __VA_ARGS__);                                                         \
        }                                                                                          \
    } while (0)
#else
#define AEMU_DCHECK(cond, ...) AEMU_CHECK(cond, __VA_ARGS__)
#endif

#define AEMU_LOG_CONCAT_INNER_(a, b) a##b
#define AEMU_LOG_CONCAT_(a, b) AEMU_LOG_CONCAT_INNER_(a, b)

/// `AEMU_SCOPED_TIMER("name");` logs "name took 1.23ms" at Debug level when the scope ends.
#define AEMU_SCOPED_TIMER(tag)                                                                     \
    ::aemu::log::ScopedTimer AEMU_LOG_CONCAT_(aemu_scoped_timer_, __LINE__)(tag)
