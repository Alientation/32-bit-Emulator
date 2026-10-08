#pragma once

#include <utility>

#define UNLIKELY(cond) __builtin_expect(cond, 0)
#define LIKELY(cond) __builtin_expect(cond, 1)

#define UNUSED(x) (void) (x)

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define MAYBE_UNUSED __attribute_maybe_unused__

template<typename F>
class ScopeExit
{
    F fn_;
    bool active_;

  public:
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

    void release()
    {
        active_ = false;
    }
};

template<typename F>
ScopeExit<F> make_scope_exit(F fn)
{
    return ScopeExit<F>(fn);
}

#define SCOPE_EXIT_CONCAT_IMPL(x, y) x##y
#define SCOPE_EXIT_CONCAT(x, y) SCOPE_EXIT_CONCAT_IMPL(x, y)

// Notice the `[&]() { ... }` wrapping around whatever code you pass in `(...)`
#define ON_SCOPE_EXIT(...)                                                                         \
    auto SCOPE_EXIT_CONCAT(_scope_guard_, __LINE__) = make_scope_exit([&]() { __VA_ARGS__; })
