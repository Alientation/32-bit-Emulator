# Deletes the gcov run data (*.gcda) under DIR. Run with: cmake -DDIR=<dir> -P remove_gcda.cmake
#
# A .gcda file only matches the .gcno written by the compile that produced it. After a rebuild
# the old data is stale, and libgcov prints "profiling error" lines when a program loads it.
file(GLOB_RECURSE stale "${DIR}/*.gcda")
if(stale)
    file(REMOVE ${stale})
endif()
