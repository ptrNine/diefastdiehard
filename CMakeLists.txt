cmake_minimum_required(VERSION 3.16.3)
project(diefastdiehard)

set(CMAKE_CXX_STANDARD 20)

include(CMake/git_submodule_build.cmake)
include_directories(SYSTEM "${CMAKE_BINARY_DIR}/3rd/include")
link_directories("${CMAKE_BINARY_DIR}/3rd/lib")
link_directories("${CMAKE_BINARY_DIR}/3rd/lib64")

git_submodule_update()
git_submodule_build(SFML )

set(_cxx_flags
    -fstrict-aliasing
    -Wall
    -Wextra
    -Wpedantic
    -Wcast-align
    -Wconversion
    -Wctor-dtor-privacy
    -Wextra-semi
    -Wfloat-equal
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Woverloaded-virtual
    -Wredundant-decls
    -Wsign-conversion
    -Wsign-promo
    -Werror=unused-result
    -Wno-unused-function
    -Wno-missing-braces
)
foreach(_flag ${_cxx_flags})
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${_flag}>)
endforeach()

set(CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/3rd/lib/cmake")

find_package(SFML 2.5.1 REQUIRED window system graphics)

add_executable(diefastdiehard diefastdiehard.cpp)
target_link_libraries(diefastdiehard sfml-system sfml-window sfml-graphics)
