#pragma once

#include "emulator32bit/devices.h"

#include <atomic>
#include <mutex>

/// The frames of the display, for a window that another thread of the host draws (docs/display.md).
///
/// The thread of the emulator presents, the thread of the window takes. The frames are swapped, not
/// copied, so neither side allocates once both buffers have their size. What the program sees (the
/// registers, the interrupt) does not depend on the window: only how fast the machine runs does,
/// when a frame rate is set.
class HostDisplay
{
  public:
    /// @param[in] frames_per_second Present waits so that no more frames than this pass in a second
    ///                              of the host. 0 does not wait.
    explicit HostDisplay(word frames_per_second);

    /// The thread of the emulator: hands over the newest frame, and waits for its turn if a frame
    /// rate is set (not once the window is closed).
    ///
    /// @param[in,out] frame The frame, swapped with a buffer to reuse.
    void present(Frame &frame);

    /// The thread of the window: takes the newest frame if one came since the last take.
    ///
    /// @param[in,out] frame Swapped with the newest frame.
    /// @return Whether there was a new frame.
    bool take(Frame &frame);

    /// The window was closed. Any thread.
    void close();

    /// @return Whether the window was closed.
    bool closed() const
    {
        return m_closed.load(std::memory_order_relaxed);
    }

  private:
    std::mutex m_mutex;
    Frame m_latest;
    bool m_new = false;
    std::atomic<bool> m_closed{false};

    U64 m_period_ns;   /// Of a frame, 0 when present does not wait.
    U64 m_next_ns = 0; /// The time of the steady clock when the next frame is due.
};
