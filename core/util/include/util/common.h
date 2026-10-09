#pragma once

#include <utility>

/// Branch prediction hints for a condition.
#define UNLIKELY(cond) __builtin_expect(cond, 0)
#define LIKELY(cond) __builtin_expect(cond, 1)

/// Silences the unused variable warning for `x`.
#define UNUSED(x) (void) (x)

/// The number of elements of a C array.
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

/// Marks a declaration that may be unused.
#define MAYBE_UNUSED __attribute_maybe_unused__

/// Runs a function when it goes out of scope, unless it was released. Made with
/// make_scope_exit() or ON_SCOPE_EXIT.
template<typename F>
class ScopeExit
{
    F fn_;
    bool active_;

  public:
    /// @param fn the function to run when this goes out of scope
    explicit ScopeExit(F fn) :
        fn_(fn),
        active_(true)
    {
    }

    ~ScopeExit()
    {
        if (active_)
        {
            fn_();
        }
    }

    ScopeExit(const ScopeExit &) = delete;
    ScopeExit &operator=(const ScopeExit &) = delete;

    ScopeExit(ScopeExit &&other) noexcept :
        fn_(std::move(other.fn_)),
        active_(other.active_)
    {
        other.active_ = false;
    }

    /// Cancels the function: it does not run when this goes out of scope.
    void release()
    {
        active_ = false;
    }
};

/// @param fn the function to run when the returned guard goes out of scope
/// @return the guard
template<typename F>
ScopeExit<F> make_scope_exit(F fn)
{
    return ScopeExit<F>(fn);
}

#define SCOPE_EXIT_CONCAT_IMPL(x, y) x##y
#define SCOPE_EXIT_CONCAT(x, y) SCOPE_EXIT_CONCAT_IMPL(x, y)

/// Runs the code in `(...)` when the enclosing scope ends. The code is wrapped in `[&]() { ... }`,
/// so it sees the local variables by reference.
#define ON_SCOPE_EXIT(...)                                                                         \
    auto SCOPE_EXIT_CONCAT(_scope_guard_, __LINE__) = make_scope_exit([&]() { __VA_ARGS__; })
