#include "emulator32bit/emulator32bit.h"

#include "util/file.h"

#include "cxxopts.hpp"

int main (int argc, char *argv[])
{
    cxxopts::Options options ("emulator32bit", "32 bit cpu emulator");

    // clang-format off
    options.add_options ("Memory")
        ("rs,ram-start", "Starting RAM page", cxxopts::value<std::string> ()->default_value ("0"))
        ("rp,ram-pages", "Number of RAM pages", cxxopts::value<std::string> ()->default_value ("32"))
        ("os,rom-start", "Starting ROM page", cxxopts::value<std::string> ()->default_value ("32"))
        ("op,rom-pages", "Number of ROM pages", cxxopts::value<std::string> ()->default_value ("32"))
        ("of,rom-file", "File containing ROM data")
        ("ds,disk-start", "Starting Disk page")
        ("dp,disk-pages", "Number of Disk pages")
        ("df,disk-file", "File containing Disk data");

    options.add_options ()
        ("h,help", "Show help");

    options.add_options ("Register and State")
        ("pc,program-counter", "Initial PC", cxxopts::value<std::string> ()->default_value ("0"))
        ("l,limit", "Number of instructions to execute", cxxopts::value<std::string> ()->default_value ("0"))
        ("f,flags", "Initial NZCV flags (e.g. 0b0100)", cxxopts::value<std::string> ()->default_value ("0b0100"))
        ("r,reg", "Initial register values (e.g. x0=10, x1=20)");

    // clang-format on

    const cxxopts::ParseResult result = options.parse (argc, argv);

    auto convert_integer = [&] (const char *option)
    { return static_cast<word> (std::stoull (result[option].as<std::string> ())); };

    if (result.count ("help"))
    {
        std::cout << options.help () << "\n";
        return 0;
    }

    const word ram_npages = convert_integer ("ram-pages");
    const word ram_start_page = convert_integer ("ram-start");

    const word rom_npages = convert_integer ("rom-pages");
    const word rom_start_page = convert_integer ("rom-start");

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

        const word disk_npages = convert_integer ("disk-pages");
        const word disk_start_page = convert_integer ("disk-start");
        disk =
            new Disk (File (result["disk-file"].as<std::string> ()), disk_npages, disk_start_page);
    }
    else
    {
        disk = new Disk ();
    }

    Emulator32bit emu (ram, rom, disk);

    emu.set_pc (convert_integer ("pc"));
    const word limit = convert_integer ("limit");
    const word flags = convert_integer ("flags");
    emu.set_NZCV (flags & 0b1000, flags & 0b0100, flags & 0b0010, flags & 0b0001);

    {
        // Extract registers
        U64 written_register = 0;
        std::stringstream ss (result["regs"].as<std::string> ());
        std::string entry;
        while (std::getline (ss, entry, 's'))
        {
            if (entry.empty ())
                continue;

            const size_t pos = entry.find ('=');
            if (pos == std::string::npos)
            {
                std::cerr << "ERROR: Invalid -reg format: " << entry << "\n";
                exit (EXIT_FAILURE);
            }

            const U8 reg = static_cast<U8> (std::stoull (entry.substr (0, pos)));
            const word val = static_cast<word> (std::stoull (entry.substr (pos + 1)));

            if (reg >= Emulator32bit::kNumReg)
            {
                std::cerr << "ERROR: Invalid register: " << entry << "\n";
                exit (EXIT_FAILURE);
            }
            else if (written_register &= (1 << reg))
            {
                std::cerr << "ERROR: Duplicate defined register: " << entry << "\n";
                exit (EXIT_FAILURE);
            }

            written_register |= (1 << reg);
            emu.write_reg (reg, val);
        }
    }

    emu.run (limit);
    emu.print ();
}