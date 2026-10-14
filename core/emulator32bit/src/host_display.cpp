#include "emulator32bit/host_display.h"

#include <chrono>
#include <thread>

/// @return The time of the steady clock in nanoseconds.
static U64 now_ns()
{
    return U64(std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
                   .count());
}

HostDisplay::HostDisplay(const word frames_per_second) :
    m_period_ns(frames_per_second == 0 ? 0 : 1'000'000'000 / frames_per_second)
{
}

void HostDisplay::present(Frame &frame)
{
    {
        const std::lock_guard lock(m_mutex);
        std::swap(m_latest, frame);
        m_new = true;
    }

    if (m_period_ns == 0 || closed()) return;

    // A machine that fell behind (a slow frame) does not run fast to catch up.
    const U64 now = now_ns();
    if (m_next_ns > now)
    {
        std::this_thread::sleep_for(std::chrono::nanoseconds(m_next_ns - now));
        m_next_ns += m_period_ns;
    }
    else
    {
        m_next_ns = now + m_period_ns;
    }
}

bool HostDisplay::take(Frame &frame)
{
    const std::lock_guard lock(m_mutex);
    if (!m_new) return false;
    std::swap(m_latest, frame);
    m_new = false;
    return true;
}

void HostDisplay::close()
{
    m_closed.store(true, std::memory_order_relaxed);
}
