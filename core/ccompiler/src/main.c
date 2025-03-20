#include "ccompiler/ccompiler.h"

// todo implement arg parser
int main (int argc, char* argv[])
{
    for (int i = 1; i < argc; i++)
    {
        int exitcode = ccompile (argv[i]);

        if (exitcode)
        {
            return exitcode;
        }
    }
    return 0;
}