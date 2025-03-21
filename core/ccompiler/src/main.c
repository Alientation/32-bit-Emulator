#include "ccompiler/ccompiler.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// todo implement arg parser
int main (int argc, char* argv[])
{
    for (int i = 1; i < argc; i++)
    {
        ccompile (argv[i]);
    }
    return 0;
}