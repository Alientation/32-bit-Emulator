#pragma once

#include <emulator32bit/emulator32bit.h>
#include <emulator32bit/emulator32bit_util.h>
#include <gtest/gtest.h>
#include <new>


class EmulatorFixture : public ::testing::Test
{
  protected:
    Emulator32bit *cpu = new Emulator32bit (1, 0, {}, 0, 1);
};