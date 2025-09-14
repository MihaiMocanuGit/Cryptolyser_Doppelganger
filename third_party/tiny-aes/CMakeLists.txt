if(DEFINED ENV{PATCH_TINY_AES_PADDING})
    set(AES_PADDING $ENV{PATCH_TINY_AES_PADDING})
else()
    message(STATUS "No ENV VAR PATH_TINY_AES_PADDING")
    set(AES_PADDING 0)
endif()

message(STATUS "PATCH_TINY_AES_PADDING = ${AES_PADDING} (ENV: $ENV{PATCH_TINY_AES_PADDING})")

Include(FetchContent)

FetchContent_Declare(
        tiny-aes
        GIT_REPOSITORY https://github.com/kokke/tiny-AES-c
        GIT_TAG master
        PATCH_COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/patchTinyAES.py ./aes.c ${AES_PADDING} &> patch.log
)

FetchContent_MakeAvailable(tiny-aes)

