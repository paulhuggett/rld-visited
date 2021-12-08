project (rld-visited CXX)
cmake_minimum_required (VERSION 3.10)

# Warning settings for the major compilers.
set (clang_warnings
    -Weverything
    -Wno-c++98-compat
    -Wno-c++98-compat-pedantic
    -Wno-c++98-compat-bind-to-temporary-copy
    -Wno-exit-time-destructors
    -Wno-global-constructors
    -Wno-padded
)
set (gcc_warnings
    -Wall
    -Wextra
    -pedantic
)
set (msvc_warnings
    /W4
    /wd4324
)

add_subdirectory (shadowarch)
add_subdirectory (visited)

