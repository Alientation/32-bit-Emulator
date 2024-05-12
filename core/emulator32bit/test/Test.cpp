#include <emulator32bit/Emulator32bit.h>
#include <emulator32bit/SystemBus.h>
#include <emulator32bit/Memory.h>
#include <util/Logger.h>

#include <iostream>

// forward declare
void test_logger();
void test_bus();


int main() {
	test_bus();	
	return 0;
};

void test_logger() {
	lgr::log(lgr::Logger::LogType::INFO, "testing logger", "test_logger");
	lgr::Logger *logger = lgr::create_logger("test_logger", lgr::Logger::CONFIG()
			.print_logs(false)
			.output_file(".\\files\\test_logger.log")
	);
	logger->log(lgr::Logger::LogType::INFO, "this is a message", "test_logger");
	lgr::log(lgr::Logger::LogType::INFO, "finished testing logger", "test_logger");
}

void test_bus() {
	lgr::log(lgr::Logger::LogType::INFO, "testing bus", "test_bus");
	lgr::log(lgr::Logger::LogType::LOG, "creating memory", "test_bus");
	RAM* ram = new RAM(1024, 0, 1023);
	byte rom_data[] = {1,2,3,4,5,6,7,8};
	ROM* rom = new ROM(rom_data, 1024, 1031);
	SystemBus bus(ram, rom);

	lgr::log(lgr::Logger::LogType::LOG, "reading from memory", "test_bus");
	std::stringstream data_stream;
	for (int i = 0; i < sizeof(rom_data); i++) {
		SystemBus::SystemBusException bus_exception;
		Memory::MemoryReadException mem_exception;
		byte data = bus.readByte(1024 + i, bus_exception, mem_exception);
		if (bus_exception.type != SystemBus::SystemBusException::AOK) {
			lgr::log(lgr::Logger::LogType::ERROR, "error reading from memory", "test_bus");
		}
		data_stream << (int) data << " ";
	}
	lgr::log(lgr::Logger::LogType::LOG, data_stream, "test_bus");

	lgr::log(lgr::Logger::LogType::LOG, "cleaning up memory", "test_bus");
	bus.~SystemBus();
	delete ram;
	delete rom;
	lgr::log(lgr::Logger::LogType::LOG, "finished testing bus", "test_bus");
}