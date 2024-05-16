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
	RAM ram = RAM(1024, 0);
	const byte rom_data[] = {1,2,3,4,5,6,7,8};
	ROM rom = ROM(rom_data, 8, 1024);
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
	log(lgr::Logger::LogType::LOG, "finished testing bus");
}