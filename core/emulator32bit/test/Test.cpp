#include "emulator32bit/Emulator32bit.h"
#include "emulator32bit/SystemBus.h"
#include "emulator32bit/Memory.h"
#include "util/Logger.h"

#include "iostream"

int main() {
	lgr::Logger *logger = lgr::create_logger("test_logger", lgr::Logger::CONFIG()
			.print_logs(true)
			.output_file(".\\files\\test_logger.log")
	);
	logger->log(lgr::Logger::LogType::INFO, "this is a message", "test_logger");

	std::cout << "creating memory" << std::endl;
	RAM* ram = new RAM(1024, 0, 1023);
	byte rom_data[] = {1,2,3,4,5,6,7,8};
	ROM* rom = new ROM(rom_data, 1024, 1031);
	SystemBus bus(ram, rom);

	std::cout << "reading from memory " << std::endl;
	for (int i = 0; i < sizeof(rom_data); i++) {
		SystemBus::SystemBusException bus_exception;
		Memory::MemoryReadException mem_exception;
		byte data = bus.readByte(1024 + i, &bus_exception, &mem_exception);
		if (bus_exception.type != SystemBus::SystemBusException::AOK) {
			std::cout << "error reading from memory" << std::endl;
			return 1;
		}
		std::cout << (int)data << " ";
	}
	std::cout << std::endl;
	
	std::cout << "cleaning up memory" << std::endl;
	bus.~SystemBus();
	delete ram;
	delete rom;
	return 0;
};