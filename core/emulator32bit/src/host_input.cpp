#include "emulator32bit/host_input.h"

void HostInput::push(const std::string &bytes)
{
    {
        const std::lock_guard lock(m_mutex);
        for (const char c : bytes)
        {
            m_bytes.push_back(byte(c));
        }
        m_pending.store(!m_bytes.empty(), std::memory_order_relaxed);
    }
    m_arrived.notify_all();
}

void HostInput::close()
{
    {
        const std::lock_guard lock(m_mutex);
        m_closed = true;
        m_pending.store(true, std::memory_order_relaxed);
    }
    m_arrived.notify_all();
}

void HostInput::take(std::deque<byte> &out)
{
    const std::lock_guard lock(m_mutex);
    out.insert(out.end(), m_bytes.begin(), m_bytes.end());
    m_bytes.clear();
    m_pending.store(false, std::memory_order_relaxed);
}

bool HostInput::finished() const
{
    const std::lock_guard lock(m_mutex);
    return m_closed && m_bytes.empty();
}

bool HostInput::wait()
{
    std::unique_lock lock(m_mutex);
    m_arrived.wait(lock, [this] { return !m_bytes.empty() || m_closed; });
    return !m_bytes.empty();
}
