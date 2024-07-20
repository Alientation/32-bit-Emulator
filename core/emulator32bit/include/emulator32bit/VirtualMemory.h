#pragma once
#ifndef VIRTUALMEMORY_H
#define VIRTUALMEMORY_H

#include "emulator32bit/Emulator32bitUtil.h"

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

};




#endif /* VIRTUALMEMORY_H */