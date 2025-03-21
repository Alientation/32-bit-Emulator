#include "ccompiler/ccompiler.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*
This is the ccompiler. Usage:
--                                      end of options, rest are parsed as files
-v, --version                           version info
-ar, --archive                          compile into archive library
-o, --output                            output file path
-d, --debug                             enable debug output
-O<level>, --optimize=<level>           optimization level *UNIMPLEMENTED*
-oall                                   optimize everything *UNIMPLEMENTED*
-W<level>, --warning=<level>            warning level *UNIMPLEMENTED*
-wall                                   enable all warnings *UNIMPLEMENTED*
-werror                                 convert warnings to errors *UNIMPLEMENTED*
-I <dir>, --include <dir>               add directory to search for include files
-l <file>, --library <file>             link library
-L <dir>, --librarydir <dir>            link all libraries in directory
-D <flag>                               define a C preprocessor flag
-Dbasm                                  define a basm preprocessor flag
-kp                                     keep preprocessor output
-E                                      only run preprocessor
-C                                      keep comments in preprocessed output
-h, --help                              display this

*/

int main (int argc, char* argv[])
{
    for (int i = 1; i < argc; i++)
    {
        ccompile (argv[i]);
    }
    return 0;
}