#ifndef BIOS_H
#define BIOS_H

class BIOS;

// https://en.wikipedia.org/wiki/BIOS
// Basic Input Output System
// provides runtime services for OS and programs and performs
// hardware initialization during booting process
//
// --- PROCESS ---
// BIOS starts up
// BIOS initiializes and tests hardware components (POST)
// BIOS loads boot loader program from a storage device
// Initiializes a kernel
// BIOS provides BIOS interrupt calls for keyboard, 
// display, storage, other input/output devices
// Used to be stored on ROM chip (Motherboard) but now is stored in
// flash memory

class BIOS {

};

#endif // BIOS_H