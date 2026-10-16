#pragma once

#include "emulator32bit/devices.h"
#include "util/types.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

/// What arrives from another thread while the machine runs: bytes for the console (a terminal, the
/// text typed in the window) and the events of the keyboard and the mouse for the input device.
///
/// The thread of the host pushes, the thread of the emulator takes: the devices do that between
/// instructions, so everything the devices do stays on the thread of the emulator. Flags that can
/// be read without the lock tell the emulator whether there is anything to take.
class HostInput
{
  public:
    /// Adds bytes to the input. Any thread.
    ///
    /// @param[in] bytes The bytes that arrived.
    void push(const std::string &bytes);

    /// Adds an event of the keyboard or the mouse. Any thread.
    ///
    /// @param[in] event The event.
    void push_event(const InputEvent &event);

    /// Says that no more bytes will come (the end of the input). Any thread.
    void close();

    /// @return Whether bytes are waiting to be taken, or the input has ended (so that `take`
    ///         would change something). Cheap.
    bool pending() const
    {
        return m_pending.load(std::memory_order_relaxed);
    }

    /// Moves the bytes that arrived to the end of `out`.
    ///
    /// @param[in,out] out Where they go.
    void take(std::deque<byte> &out);

    /// @return Whether events are waiting to be taken. Cheap.
    bool events_pending() const
    {
        return m_events_pending.load(std::memory_order_relaxed);
    }

    /// Moves the events that arrived to `out`, in the order they arrived.
    ///
    /// @param[out] out The events. What it held is replaced.
    void take_events(std::vector<InputEvent> &out);

    /// @return Whether the input has ended and everything was taken.
    bool finished() const;

    /// Blocks until bytes or events are waiting or the input has ended.
    ///
    /// @return Whether there are bytes or events to take.
    bool wait();

  private:
    mutable std::mutex m_mutex;
    std::condition_variable m_arrived;
    std::deque<byte> m_bytes;
    std::vector<InputEvent> m_events;
    bool m_closed = false;
    std::atomic<bool> m_pending{false};
    std::atomic<bool> m_events_pending{false};
};
