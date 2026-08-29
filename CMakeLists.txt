set(CMAKE_BUILD_TYPE "Release")

cmake_minimum_required(VERSION 3.21)
project(simple-triangle-webgpu-sdl3-cpp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# Define Project Source Files (Single Source of Truth)
# -----------------------------------------------------------------------------
set(PROJECT_SOURCES
    src/main.cpp
    # Add future source files here
)

# -----------------------------------------------------------------------------
# Declare Target Type based on OS
# -----------------------------------------------------------------------------
if(ANDROID)
    # Android requires a shared library (.so) for SDLActivity
    add_library(app SHARED)
else()
    # Windows & Emscripten require an executable target
    add_executable(app)
endif()

# Attach sources to the target dynamically
target_sources(app PRIVATE ${PROJECT_SOURCES})

# -----------------------------------------------------------------------------
# Platform Linking & Dependencies
# -----------------------------------------------------------------------------
if(EMSCRIPTEN)
    # Emscripten / WebAssembly Build Settings
    add_library(SDL3_Static STATIC IMPORTED)
    set_target_properties(SDL3_Static PROPERTIES
        IMPORTED_LOCATION "C:/libs/sdl3-3.4.14-wasm/lib/libSDL3.a"
        INTERFACE_INCLUDE_DIRECTORIES "C:/libs/sdl3-3.4.14-wasm/include"
    )

    target_link_libraries(app PRIVATE
        SDL3_Static
    )

    target_compile_options(app PRIVATE
        -Os
        --use-port=emdawnwebgpu
    )

    target_link_options(app PRIVATE
        "-Os"
        "-sMALLOC=emmalloc"
        "-sUSE_SDL=0"
        "--use-port=emdawnwebgpu" # Enables browser WebGPU JS support
    )

elseif(ANDROID)
    add_library(SDL3 SHARED IMPORTED)
    set_target_properties(SDL3 PROPERTIES
        IMPORTED_LOCATION "C:/libs/sdl3-3.4.14-android/${ANDROID_ABI}/lib/libSDL3.so"
        INTERFACE_INCLUDE_DIRECTORIES "C:/libs/sdl3-3.4.14-android/${ANDROID_ABI}/include"
    )

    add_library(wgpu_native STATIC IMPORTED)
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION "C:/libs/wgpu-29.0.1.1-android/${ANDROID_ABI}/lib/libwgpu_native.a"
        INTERFACE_INCLUDE_DIRECTORIES "C:/libs/wgpu-29.0.1.1-android/${ANDROID_ABI}/include"
    )

    find_library(ANDROID_LIB android REQUIRED)
    find_library(LOG_LIB log REQUIRED)

    target_link_libraries(app PRIVATE
        SDL3
        wgpu_native
        ${ANDROID_LIB}
        ${LOG_LIB}
    )

elseif(WIN32)
    add_library(SDL3_Static STATIC IMPORTED)
    set_target_properties(SDL3_Static PROPERTIES
        IMPORTED_LOCATION "C:/libs/sdl3-3.4.14-mingw/lib/libSDL3.a"
        INTERFACE_INCLUDE_DIRECTORIES "C:/libs/sdl3-3.4.14-mingw/include"
        INTERFACE_LINK_LIBRARIES "winmm;imm32;version;setupapi;dinput8"
    )

    add_library(wgpu_native STATIC IMPORTED)
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION "C:/libs/wgpu-29.0.1.1-mingw/lib/libwgpu_native.a"
        INTERFACE_INCLUDE_DIRECTORIES "C:/libs/wgpu-29.0.1.1-mingw/include"
        INTERFACE_LINK_LIBRARIES "ws2_32;userenv;ntdll;bcrypt;d3d12;dxgi"
    )

    target_link_libraries(app PRIVATE
        SDL3_Static
        wgpu_native
    )

    # Strip symbols and optimize aggressive for size
    target_compile_options(app PRIVATE -Os)

    # MinGW Static Build Settings for Windows
    target_link_options(app PRIVATE -static -s)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(app PRIVATE -mconsole)
    else()
        target_link_options(app PRIVATE -mwindows)
    endif()
endif()
