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
        clock++;
    }

    inline unsigned long long time ()
    {
        return clock;
    }

  private:
    Emulator32bit *processor;
    unsigned long long clock = 0;
};