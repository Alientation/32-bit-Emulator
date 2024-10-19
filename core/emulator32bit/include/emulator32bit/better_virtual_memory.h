#pragma once
#ifndef BETTER_VIRTUAL_MEMORY_H
#define BETTER_VIRTUAL_MEMORY_H

#include "emulator32bit/emulator32bit.h"
#include "emulator32bit/emulator32bit_util.h"

#include <string>

/*
    we need access to the emulator's registers, in particular, the pagetable
    pointer.






*/

#define TLB_PSIZE 12
#define TLB_SIZE (1 << TLB_PSIZE)

class MMU
{
    public:
        MMU(Emulator32bit *processor);

    private:
        Emulator32bit *processor;
};




#endif /* BETTER_VIRTUAL_MEMORY */