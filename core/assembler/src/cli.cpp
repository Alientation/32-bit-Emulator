#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

#include "assembler/build.h"
#include "util/logger.h"

int main(int argc, char *argv[])
{
    // The arguments are passed as they are, a path with spaces in it is one argument.
    const std::vector<std::string> args(argv + 1, argv + argc);

    // The library reports errors by throwing. The error has already been logged by the time it is
    // caught here, so all that is left to do is to fail.
    aemu::log::set_fatal_action(aemu::log::FatalAction::Throw);
    try
    {
        Build build(args);
        build.run();
    }
    catch (const aemu::log::FatalError &)
    {
        return EXIT_FAILURE;
    }
    catch (const std::exception &error)
    {
        AEMU_ERROR("{}", error.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
