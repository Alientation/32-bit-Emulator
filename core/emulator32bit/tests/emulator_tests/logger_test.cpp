// The compile time level of the logger (AEMU_LOG_COMPILE_LEVEL, set for the whole build by
// core/CMakeLists.txt) decides which calls exist at all, whatever the runtime level is.

#include "util/logger.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{

/// The levels of the messages that the macros emit while the runtime level lets everything pass.
std::vector<aemu::log::Level> emitted_levels()
{
    std::vector<aemu::log::Level> levels;
    aemu::log::set_sink([&levels](const aemu::log::Record &r) { levels.push_back(r.level); });
    {
        aemu::log::ScopedLevel everything(aemu::log::Level::Debug);
        AEMU_DEBUG("debug");
        AEMU_INFO("info");
        AEMU_WARN("warn");
        AEMU_ERROR("error");
    }
    aemu::log::reset_sink();
    return levels;
}

} // namespace

TEST(Logger, the_compile_level_removes_the_calls_below_it)
{
    using aemu::log::Level;
    const int compile_level = AEMU_LOG_COMPILE_LEVEL;
    std::vector<Level> expected;
    for (const Level level : {Level::Debug, Level::Info, Level::Warn, Level::Error})
    {
        if (static_cast<int>(level) >= compile_level) expected.push_back(level);
    }
    EXPECT_EQ(emitted_levels(), expected) << "AEMU_LOG_COMPILE_LEVEL = " << compile_level;
}

TEST(Logger, a_fatal_error_and_a_failed_check_are_never_removed)
{
    aemu::log::ScopedFatalAction guard(aemu::log::FatalAction::Throw);
    aemu::log::ScopedLevel quiet(aemu::log::Level::Off);
    EXPECT_THROW(AEMU_FATAL("fatal"), aemu::log::FatalError);
    EXPECT_THROW(AEMU_CHECK(false, "check"), aemu::log::FatalError);
}

TEST(Logger, a_scoped_timer_compiles_at_every_level)
{
    // Debug: logs when the scope ends. Otherwise it is not even constructed.
    AEMU_SCOPED_TIMER("a scope");
    SUCCEED();
}
