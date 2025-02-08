cmake_minimum_required(VERSION 3.25)
project(Cryptolyser_Doppelganger VERSION 0.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

if (WIN32)
    FATAL_ERROR("Windows is not supported.")
endif (WIN32)

set(GCC_FLAGS_DEBUG "-O0 -Wall -Wextra -Wpedantic -Werror -Wno-unused-variable -Wno-unused-parameter -fsanitize=address,undefined")
set(GCC_LINK_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address,undefined")

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${GCC_FLAGS_DEBUG}")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} ${GCC_LINK_FLAGS_DEBUG}")

add_subdirectory(third-party)
add_subdirectory(src)
add_subdirectory(Cryptolyser_Common)
set(SRC_LIBS AES CacheFlush ConnectionHandler Cryptolyser_Common)

add_executable(${PROJECT_NAME} main.c)
target_include_directories(${PROJECT_NAME} PRIVATE src Cryptolyser_Common)
target_link_libraries(${PROJECT_NAME} PRIVATE ${SRC_LIBS})
