#include <string>

#include "assembler/build.h"

int main (int argc, char *argv[])
{
    std::string build_cmd = "";
    for (int i = 1; i < argc; i++)
    {
        build_cmd += std::string (argv[i]);
    }

    Process process (build_cmd);
}