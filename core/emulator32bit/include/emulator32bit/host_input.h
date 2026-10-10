#pragma once

#include "util/types.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>

/// Bytes that arrive from another thread while the machine runs (a terminal), for the console.
///
/// The thread of the host pushes, the thread of the emulator takes: the console does that between
/// instructions, so everything the devices do stays on the thread of the emulator. A flag that can
/// be read without the lock tells the emulator whether there is anything to take.
class HostInput
{
  public:
    /// Adds bytes to the input. Any thread.
    ///
    /// @param bytes the bytes that arrived
    void push(const std::string &bytes);

    /// Says that no more bytes will come (the end of the input). Any thread.
    void close();

    /// @return whether bytes are waiting to be taken, or the input has ended (so that `take`
    ///         would change something). Cheap.
    bool pending() const
    {
        return m_pending.load(std::memory_order_relaxed);
    }

    /// Moves the bytes that arrived to the end of `out`.
    ///
    /// @param out where they go
    void take(std::deque<byte> &out);

    /// @return whether the input has ended and everything was taken
    bool finished() const;

    /// Blocks until bytes are waiting or the input has ended.
    ///
    /// @return whether there are bytes to take
    bool wait();

  private:
    mutable std::mutex m_mutex;
    std::condition_variable m_arrived;
    std::deque<byte> m_bytes;
    bool m_closed = false;
    std::atomic<bool> m_pending{false};
};
