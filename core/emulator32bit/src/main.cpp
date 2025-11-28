#include "emulator32bit/emulator32bit.h"

#include "util/file.h"

#include "cxxopts.hpp"

int main (int argc, char *argv[])
{
    cxxopts::Options options ("emulator32bit", "32 bit cpu emulator");

    // clang-format off
    options.add_options ()
        ("rs,ram-start", "Starting RAM page", cxxopts::value<word> ()->default_value ("0"))
        ("rp,ram-pages", "Number of RAM pages", cxxopts::value<word> ()->default_value ("32"))
        ("os,rom-start", "Starting ROM page", cxxopts::value<word> ()->default_value ("32"))
        ("op,rom-pages", "Number of ROM pages", cxxopts::value<word> ()->default_value ("32"))
        ("of,rom-file", "File containing ROM data")
        ("ds,disk-start", "Starting Disk page")
        ("dp,disk-pages", "Number of Disk pages")
        ("df,disk-file", "File containing Disk data")
        ("h,help", "Show help");
    ;
    // clang-format on

    const cxxopts::ParseResult result = options.parse (argc, argv);

    if (result.count ("help"))
    {
        std::cout << options.help () << "\n";
        return 0;
    }

    const word ram_npages = result["ram-pages"].as<uint32_t> ();
    const word ram_start_page = result["ram-start"].as<uint32_t> ();

    const word rom_npages = result["rom-pages"].as<uint32_t> ();
    const word rom_start_page = result["rom-start"].as<uint32_t> ();

    RAM *ram = new RAM (ram_npages, ram_start_page);
    ROM *rom = nullptr;

    if (result.count ("rom-file"))
    {
        if (!result.count ("rom-pages") || !result.count ("rom-start"))
        {
            std::cout << "Warning: ROM file specified without page address start or length. Using "
                         "defaults.\n";
        }
        rom = new ROM (File (result["rom-file"].as<std::string> ()), rom_npages, rom_start_page);
    }
    else
    {
        rom = new ROM (rom_npages, rom_start_page);
    }

    Disk *disk;
    if (result.count ("disk-file"))
    {
        if (!result.count ("disk-pages") || !result.count ("disk-start"))
        {
            std::cerr
                << "Expected 'disk-pages' and 'disk-start' to be passed alongside 'disk-file'\n";
            return EXIT_FAILURE;
        }

        const word disk_npages = result["disk-pages"].as<word> ();
        const word disk_start_page = result["disk-start"].as<word> ();
        disk =
            new Disk (File (result["disk-file"].as<std::string> ()), disk_npages, disk_start_page);
    }
    else
    {
        disk = new Disk ();
    }

    Emulator32bit emu (ram, rom, disk);
}