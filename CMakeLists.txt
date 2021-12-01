project (rld-visited CXX)
cmake_minimum_required (VERSION 3.10)

add_executable (rld-visited main.cpp Visited.h Visited.cpp)

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

target_compile_features (rld-visited PUBLIC cxx_std_17)
target_compile_options (rld-visited PRIVATE
        $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:${clang_warnings}>
        $<$<CXX_COMPILER_ID:GNU>:${gcc_warnings}>
        $<$<CXX_COMPILER_ID:MSVC>:${msvc_warnings}>
    )
find_package (Threads REQUIRED)
target_link_libraries (rld-visited PUBLIC Threads::Threads)
