#pragma once
#ifndef EMULATOR32BITTEST_H
#define EMULATOR32BITTEST_H

#include <gtest/gtest.h>
#include <emulator32bit/Emulator32bit.h>
#include <emulator32bit/Emulator32bitUtil.h>

class Test : public testing::Test {
	public: 
        Emulator32bit cpu;

		virtual void SetUp() {
			cpu.reset();	
		}

		virtual void TearDown() {
		
		}
};

#endif /* EMULATOR32BITTEST_H */