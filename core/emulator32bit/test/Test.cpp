#include <emulator32bit/Emulator32bit.h>
#include <emulator32bit/SystemBus.h>
#include <emulator32bit/Memory.h>
#include <util/Logger.h>

#include <iostream>

// forward declare
void test_emulator32bit();
void _test_hlt(Emulator32bit& emulator);
void _test_add(Emulator32bit& emulator);
void _test_sub(Emulator32bit& emulator);

void test_logger();
void test_bus();


int main() {
	test_emulator32bit();	
	return 0;
};

void test_emulator32bit() {
	Emulator32bit emulator;
	_test_hlt(emulator);
	_test_add(emulator);
	_test_sub(emulator);
}

void _test_hlt(Emulator32bit& emulator) {
	log(lgr::Logger::LogType::INFO, "testing hlt");
	SystemBus::SystemBusException sys_bus_exception;
	Memory::MemoryWriteException mem_write_exception;

	emulator.system_bus.writeByte(0, Emulator32bit::asm_hlt(), sys_bus_exception, mem_write_exception);
	emulator._pc = 0;
	
	Emulator32bit::EmulatorException exception;
	emulator.run(1, exception);
	
	EXPECT_TRUE(exception.type == Emulator32bit::EmulatorException::Type::HALT, lgr::Logger::LogType::TEST, std::stringstream() << "HLT instruction must cause HLT exception.");
	EXPECT_TRUE(emulator._pc == 4, lgr::Logger::LogType::TEST, std::stringstream() << "PC must increment by word length each execution. EXPECTED: 4\n GOT: " << emulator._pc << ".");

	log(lgr::Logger::LogType::INFO, "finished testing hlt");
}

void _test_add(Emulator32bit& emulator) {
	log(lgr::Logger::LogType::INFO, "testing add");
	
	SystemBus::SystemBusException sys_bus_exception;
	Memory::MemoryWriteException mem_write_exception;

	emulator.system_bus.writeByte(0, Emulator32bit::asm_add(false, 0, 1, 10), sys_bus_exception, mem_write_exception);
	log(lgr::Logger::LogType::DEBUG, std::stringstream() << "add instr: " << Emulator32bit::asm_add(false, 0, 1, 10));
	emulator._pc = 0;
	emulator._x[1] = 1;

	Emulator32bit::EmulatorException exception;
	emulator.run(1, exception);

	EXPECT_TRUE(emulator._x[0] == 11, lgr::Logger::LogType::TEST, std::stringstream() << "Expected result in x0: 11\nGot: " << emulator._x[0]);
	EXPECT_TRUE(test_bit(emulator._pstate, N_FLAG) == 0, lgr::Logger::LogType::TEST, std::stringstream() << "Expected N flag: 0\nGot: " << test_bit(emulator._pstate, N_FLAG));
	EXPECT_TRUE(test_bit(emulator._pstate, Z_FLAG) == 0, lgr::Logger::LogType::TEST, std::stringstream() << "Expected N flag: 0\nGot: " << test_bit(emulator._pstate, Z_FLAG));
	EXPECT_TRUE(test_bit(emulator._pstate, C_FLAG) == 0, lgr::Logger::LogType::TEST, std::stringstream() << "Expected N flag: 0\nGot: " << test_bit(emulator._pstate, C_FLAG));
	EXPECT_TRUE(test_bit(emulator._pstate, V_FLAG) == 0, lgr::Logger::LogType::TEST, std::stringstream() << "Expected N flag: 0\nGot: " << test_bit(emulator._pstate, V_FLAG));
	EXPECT_TRUE(emulator._pc == 4, lgr::Logger::LogType::TEST, std::stringstream() << "PC must increment by word length each execution. EXPECTED: 4\n GOT: " << emulator._pc << ".");

	log(lgr::Logger::LogType::INFO, "finished testing add");
}

void _test_sub(Emulator32bit& emulator) {

}

void test_logger() {
	log(lgr::Logger::LogType::INFO, "testing logger");
	lgr::Logger *logger = lgr::create_logger("test_logger", lgr::Logger::CONFIG()
			.print_logs(false)
			.output_file(".\\files\\test_logger.log")
	);
	logger->log(lgr::Logger::LogType::INFO, "this is a message");
	log(lgr::Logger::LogType::INFO, "finished testing logger");
}

void test_bus() {
	log(lgr::Logger::LogType::INFO, "testing bus");
	log(lgr::Logger::LogType::LOG, "creating memory");
	RAM* ram = new RAM(1024, 0);
	const byte rom_data[] = {1,2,3,4,5,6,7,8};
	ROM* rom = new ROM(rom_data, 8, 1024);
	SystemBus bus(ram, rom);

	log(lgr::Logger::LogType::LOG, "reading from memory");
	std::stringstream data_stream;
	for (int i = 0; i < sizeof(rom_data); i++) {
		SystemBus::SystemBusException bus_exception;
		Memory::MemoryReadException mem_exception;
		byte data = bus.readByte(1024 + i, bus_exception, mem_exception);
		if (bus_exception.type != SystemBus::SystemBusException::AOK) {
			log(lgr::Logger::LogType::ERROR, "error reading from memory");
		}
		data_stream << (int) data << " ";
	}
	log(lgr::Logger::LogType::LOG, data_stream);

	log(lgr::Logger::LogType::LOG, "cleaning up memory");
	bus.~SystemBus();
	delete ram;
	delete rom;
	log(lgr::Logger::LogType::LOG, "finished testing bus");
}