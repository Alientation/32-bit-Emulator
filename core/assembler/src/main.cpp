#include <string>

#include "assembler/build.h"

int main(int argc, char *argv[])
{
    // Process re-tokenizes the command on whitespace, so the arguments must be space separated.
    std::string build_cmd = "";
    for (int i = 1; i < argc; i++)
    {
        if (i > 1)
        {
            build_cmd += " ";
        }
        build_cmd += std::string(argv[i]);
    }

    Process process(build_cmd);
}
