#include "ccompiler/ccompiler.h"

// todo implement arg parser
int main (int argc, char* argv[])
{
    for (int i = 1; i < argc; i++)
    {
        ccompile (argv[i]);
    }
    return 0;
}