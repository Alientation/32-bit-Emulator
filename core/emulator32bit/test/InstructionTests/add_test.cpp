#include <emulator32bit_test/Emulator32bitTest.h>

TEST(add, add_default) {
	SystemBus::SystemBusException sys_bus_exception;
	Memory::MemoryWriteException mem_write_exception;
	Emulator32bit cpu = Emulator32bit();

	cpu.system_bus.writeByte(0, Emulator32bit::asm_add(false, 0, 1, 10), sys_bus_exception, mem_write_exception);
	cpu._pc = 0;
	cpu._x[1] = 1;

	Emulator32bit::EmulatorException exception;
	cpu.run(1, exception);

	EXPECT_EQ(cpu._x[0], 11) << "\'add x0, x1 10\' : where x1=1, should result in x0=11";
	EXPECT_EQ(test_bit(cpu._pstate, N_FLAG), 0);
	EXPECT_EQ(test_bit(cpu._pstate, Z_FLAG), 0);
	EXPECT_EQ(test_bit(cpu._pstate, C_FLAG), 0);
	EXPECT_EQ(test_bit(cpu._pstate, V_FLAG), 0);
	EXPECT_EQ(cpu._pc, 4) << "PC must increment by word length each execution";
}