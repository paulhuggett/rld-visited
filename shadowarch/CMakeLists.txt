add_executable (rld-shadowarch
    main.cpp
    compilationref.cpp
    compilationref.hpp
    context.cpp
    context.hpp
    group.hpp
    print.cpp
    print.hpp
    repo.cpp
    repo.hpp
    shadow.hpp
    symbol.cpp
    symbol.hpp
)

target_compile_features (rld-shadowarch PUBLIC cxx_std_17)
target_compile_options (rld-shadowarch PRIVATE
    $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:${clang_warnings}>
    $<$<CXX_COMPILER_ID:GNU>:${gcc_warnings}>
    $<$<CXX_COMPILER_ID:MSVC>:${msvc_warnings}>
)
find_package (Threads REQUIRED)
target_link_libraries (rld-shadowarch PUBLIC Threads::Threads)
