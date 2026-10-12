#pragma once

/// @file logger.h
/// @brief Header-only logger.
///
/// Design goals
/// - Configured at RUNTIME (API calls or the AEMU_LOG_LEVEL environment variable), never through
/// per-file #defines. Including this header from another header cannot change the behavior of
/// anything downstream, and every translation unit sees identical inline function bodies
/// (no ODR violations).
/// - Log lines go to stderr, so stdout stays clean for program output (e.g. `emu32` state dumps).
/// - Format strings are checked at compile time (std::format_string). A bad format string or a
/// printf style "%u" with arguments is a compile error or is printed literally, not a
/// runtime std::format_error.
/// - Macro names are prefixed with AEMU_ so they cannot collide with gtest (EXPECT_TRUE), syslog
/// (LOG_DEBUG), Windows (ERROR) or other libraries (DEBUG).
/// - Arguments are only evaluated and formatted when the message will actually be emitted.
/// - Thread safe. Each message is formatted first and then written under a lock.
///
/// Quick reference
///
/// AEMU_DEBUG("loaded {} pages", n);       // level Debug
/// AEMU_INFO(...);                         // level Info
/// AEMU_WARN(...);                         // level Warn
/// AEMU_ERROR(...);                        // level Error, NOT fatal, execution continues
/// AEMU_FATAL(...);                        // logs, then terminates (see FatalAction)
/// AEMU_CHECK(cond, "msg {}", x);          // always on. Fatal if cond is false
/// AEMU_DCHECK(cond, "msg {}", x);         // like AEMU_CHECK but compiled out when NDEBUG is set
/// AEMU_SCOPED_TIMER("assemble");          // logs the scope's duration at Debug level
///
/// Runtime configuration (all in namespace aemu::log)
///
/// set_level(Level::Debug);                // or env AEMU_LOG_LEVEL=debug|info|warn|error|fatal|off
/// set_fatal_action(FatalAction::Throw);   // Exit (default), Abort or Throw (FatalError)
/// set_color(false); set_timestamps(true);
/// set_sink([](const Record &r) { ... });  // redirect output, e.g. to a file (see below)
/// ScopedLevel / ScopedFatalAction         // RAII helpers, handy in unit tests
///
/// Writing logs to a file does not need dedicated machinery. A sink is enough:
///
/// static std::ofstream file("aemu.log");
/// aemu::log::set_sink([](const aemu::log::Record &r)
/// { file << aemu::log::to_string(r.level) << ' ' << r.message << '\n'; });
///
/// The one compile time knob, AEMU_LOG_COMPILE_LEVEL, removes call sites below a level from the
/// binary entirely (and the AEMU_SCOPED_TIMER, which is a debug message, when debug is removed).
/// It is set once for the whole build (core/CMakeLists.txt gives it to every target: 0 in Debug, 2
/// in the optimized configurations, or the cache variable of the same name), NEVER in a source or
/// header file. A level that is not removed still costs a relaxed atomic load and a branch.
///
/// Requires C++20 (<format>, <source_location>).

#include "util/console_color.h"
#include "util/types.h"

#include <atomic>
#include <cctype>
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
enum class Level : U8
{
    Debug = 0,
    Info,
    Warn,
    Error,
    Fatal,
    Off,
};

/// What AEMU_FATAL / a failed AEMU_CHECK does after logging the message.
enum class FatalAction : U8
{
    /// std::exit(EXIT_FAILURE). The default.
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

    /// Nanoseconds since the logger was first used.
    S64 since_start_ns;
};

/// Receives every message that passes the level filter. Called with the logger lock held, so a
/// sink does not need its own synchronization, but it must not log (that would deadlock).
using Sink = std::function<void(const Record &)>;

/// @param[in] level A level.
/// @return The three letter name of the level that is printed in a log line (`DBG`, `INF`, `WRN`,
///     `ERR`, `FTL`, `OFF`).
constexpr std::string_view to_string(const Level level)
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
///
/// @param[in] name The name of a level.
/// @return The level, or nothing if the name is none of those.
inline std::optional<Level> parse_level(const std::string_view name)
{
    std::string lower(name);
    for (char &c : lower)
    {
        c = static_cast<char>(std::tolower(static_cast<U8>(c)));
    }

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

/// Nanoseconds on a clock that only goes forward (util/src/logger.cpp, so that <chrono> is not
/// included by every file that logs).
///
/// @return The time in nanoseconds.
S64 monotonic_ns();

/// Global logger state. One instance for the whole program, since this is a function local
/// static in an inline function. Intentionally leaked so logging from other static destructors
/// is always safe.
struct State
{
    std::atomic<Level> level{Level::Info};
    std::atomic<FatalAction> fatal_action{FatalAction::Exit};
    std::atomic<bool> color{false};
    std::atomic<bool> timestamps{false};

    std::mutex mutex; /// Guards `sink` and serializes output.
    Sink sink;        /// Empty means the default stderr sink.

    const S64 start_ns = monotonic_ns();

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
///
/// @param[in] level The level of a call site.
/// @return Whether it is kept.
constexpr bool compiled_in(const Level level)
{
    constexpr S32 kCompileLevel = AEMU_LOG_COMPILE_LEVEL;
    return static_cast<S32>(level) >= kCompileLevel;
}

/// @param[in] level The level.
/// @return The escape sequence that colors the name of the level in the default sink.
constexpr std::string_view level_color(const Level level)
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
///
/// @param[in] path The path of a source file.
/// @return The file name.
inline std::string_view basename(const std::string_view path)
{
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

/// Default sink: `[ERR] [file.cpp:123]: message` on stderr, with an optional time prefix.
///
/// @param[in] record The message.
/// @param[in] color Whether to use the ANSI colors.
/// @param[in] timestamps Whether to start the line with the time.
inline void write_default(const Record &record, const bool color, const bool timestamps)
{
    std::string line;

    if (timestamps)
    {
        line += std::format("[{:9.3f}s] ", static_cast<double>(record.since_start_ns) / 1e9);
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

/// @return The lowest level that is emitted.
inline Level get_level()
{
    return detail::state().level.load(std::memory_order_relaxed);
}

/// Sets the lowest level that is emitted.
///
/// @param[in] level The lowest level.
inline void set_level(const Level level)
{
    detail::state().level.store(level);
}

/// @param[in] level The level of a message.
/// @return Whether a message of this level would be emitted now.
inline bool enabled(const Level level)
{
    return level >= get_level();
}

/// @return What AEMU_FATAL and a failed AEMU_CHECK do after logging.
inline FatalAction get_fatal_action()
{
    return detail::state().fatal_action.load();
}

/// Chooses what AEMU_FATAL and AEMU_CHECK do after logging.
///
/// @param[in] action What to do.
inline void set_fatal_action(const FatalAction action)
{
    detail::state().fatal_action.store(action);
}

/// Turns the ANSI colors of the default sink on or off. Without a call, they are on when stderr is
/// a terminal and `NO_COLOR` is not set.
///
/// @param[in] enable Whether to use the colors.
inline void set_color(const bool enable)
{
    detail::state().color.store(enable);
}

/// Turns on or off the prefix of each line of the default sink with the seconds since the logger
/// was first used.
///
/// @param[in] enable Whether to print the time.
inline void set_timestamps(const bool enable)
{
    detail::state().timestamps.store(enable);
}

/// Replaces the default stderr output. Pass an empty function (or call reset_sink) to restore it.
///
/// @param[in] sink The function that receives every message.
inline void set_sink(Sink sink)
{
    detail::State &s = detail::state();
    const std::lock_guard lock(s.mutex);
    s.sink = std::move(sink);
}

/// Restores the default stderr output.
inline void reset_sink()
{
    set_sink(nullptr);
}

/// Sets the level for the lifetime of the object, then restores the previous one.
class ScopedLevel
{
  public:
    /// @param[in] level The level to set until this is destroyed.
    explicit ScopedLevel(const Level level) :
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
    /// @param[in] action The action to set until this is destroyed.
    explicit ScopedFatalAction(const FatalAction action) :
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
///
/// @param[in] level The level of the message.
/// @param[in] location Where the message is logged from.
/// @param[in] message The message.
inline void write(const Level level, const std::source_location &location,
                  const std::string_view message)
{
    detail::State &s = detail::state();
    if (level < s.level.load(std::memory_order_relaxed))
    {
        return;
    }

    const Record record{.level = level,
                        .location = location,
                        .message = message,
                        .since_start_ns = detail::monotonic_ns() - s.start_ns};

    const std::lock_guard lock(s.mutex);
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
///
/// @param[in] location Where the message is logged from.
/// @param[in] message The message.
[[noreturn]] inline void fatal(const std::source_location &location, const std::string_view message)
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
// Scoped timer
// ---------------------------------------------------------------------------------------------

/// Logs how long its scope took at Debug level when destroyed. It cannot be left unbalanced, and
/// it keeps no global state.
class ScopedTimer
{
  public:
    /// @param[in] tag The name of what is timed, which the message starts with.
    /// @param[in] location Where the timer is made, which is the place the message is logged at.
    explicit ScopedTimer(std::string tag,
                         const std::source_location location = std::source_location::current()) :
        m_tag(std::move(tag)),
        m_location(location),
        m_start(detail::monotonic_ns())
    {
    }

    ~ScopedTimer()
    {
        if (!enabled(Level::Debug))
        {
            return;
        }

        const S64 ns = detail::monotonic_ns() - m_start;
        write(Level::Debug, m_location, std::format("{} took {}", m_tag, format_duration(ns)));
    }

    ScopedTimer(const ScopedTimer &) = delete;
    ScopedTimer &operator=(const ScopedTimer &) = delete;

  private:
    /// Nanoseconds in a microsecond, a millisecond and a second.
    static constexpr S64 kNsPerUs = 1'000;
    static constexpr S64 kNsPerMs = 1'000'000;
    static constexpr S64 kNsPerS = 1'000'000'000;

    /// A duration is shown in the next unit once it is longer than this many of the unit.
    static constexpr S64 kUnitLimit = 10'000;

    static std::string format_duration(const S64 ns)
    {
        if (ns <= kUnitLimit)
        {
            return std::format("{}ns", ns);
        }
        if (ns <= kUnitLimit * kNsPerUs)
        {
            return std::format("{:.2f}us", static_cast<double>(ns) / kNsPerUs);
        }
        if (ns <= kUnitLimit * kNsPerMs)
        {
            return std::format("{:.2f}ms", static_cast<double>(ns) / kNsPerMs);
        }
        return std::format("{:.2f}s", static_cast<double>(ns) / kNsPerS);
    }

    std::string m_tag;
    std::source_location m_location;
    S64 m_start;
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
#if AEMU_LOG_COMPILE_LEVEL > 0
// The timer is a debug message: nothing is measured, and `tag` is only type checked.
#define AEMU_SCOPED_TIMER(tag) static_assert(sizeof(tag) > 0, "")
#else
#define AEMU_SCOPED_TIMER(tag)                                                                     \
    ::aemu::log::ScopedTimer AEMU_LOG_CONCAT_(aemu_scoped_timer_, __LINE__)(tag)
#endif
