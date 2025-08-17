#pragma once

#include "emulator32bit/emulator32bit.h"

/**
 * @brief           Simulates a hardware timer
 *
 * todo, goal of this is to generate timer interrupts every so often
 * in order to do this, we need a way of stopping processor execution to handle
 * this interrupt.
 */
class Timer
{
  public:
    Timer (Emulator32bit *processor);

    inline void tick ()
    {
        m_clock++;
    }

    inline unsigned long long time ()
    {
        return m_clock;
    }

  private:
    Emulator32bit *m_processor;
    unsigned long long m_clock = 0;
};