add_executable (rld-visited main.cpp Visited.h Visited.cpp)
target_compile_features (rld-visited PUBLIC cxx_std_17)
target_compile_options (rld-visited PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:${clang_warnings}>
    $<$<CXX_COMPILER_ID:GNU>:${gcc_warnings}>
    $<$<CXX_COMPILER_ID:MSVC>:${msvc_warnings}>
)
find_package (Threads REQUIRED)
target_link_libraries (rld-visited PUBLIC Threads::Threads)
