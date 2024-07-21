#pragma once
#ifndef VIRTUALMEMORY_H
#define VIRTUALMEMORY_H

#include "emulator32bit/Emulator32bitUtil.h"

#define VM_PAGESIZE 4096
class VirtualMemory {
	public:
		VirtualMemory();

		struct Exception {
			enum Type {
				AOK, INVALID_ADDRESS,
			};

			Type type = AOK;
			word address = 0;
		};

		word map_address(word address, Exception& exception);

	private:
		struct MemoryMap {

		};
};




#endif /* VIRTUALMEMORY_H */