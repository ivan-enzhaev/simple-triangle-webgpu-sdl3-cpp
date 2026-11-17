cmake_minimum_required(VERSION 3.21)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

project(simple-triangle-webgpu-sdl3-cpp CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------------------------------------------
# Paths
# -----------------------------------------------------------------------------
if(EMSCRIPTEN)
    if(CMAKE_HOST_WIN32 OR WIN32)
        set(SDL3_PATH "C:/libs/sdl3-3.4.20-wasm")
    else()
        set(SDL3_PATH "$ENV{HOME}/libs/sdl3-3.4.20-wasm")
    endif()
elseif(ANDROID)
    set(SDL3_PATH "C:/libs/sdl3-3.4.20-android/${ANDROID_ABI}")
    set(WGPU_PATH "C:/libs/wgpu-native-29.0.1.1-android/${ANDROID_ABI}")
elseif(WIN32)
    set(SDL3_PATH "C:/libs/sdl3-3.4.20-mingw")
    set(WGPU_PATH "C:/libs/wgpu-native-29.0.1.1-mingw")
elseif(UNIX AND NOT APPLE)
    set(SDL3_PATH "$ENV{HOME}/libs/sdl3-3.4.20-ubuntu")
    set(WGPU_PATH "$ENV{HOME}/libs/wgpu-native-29.0.1.1-ubuntu")
endif()

# -----------------------------------------------------------------------------
# Target
# -----------------------------------------------------------------------------
set(PROJECT_SOURCES
    src/main.cpp
)

if(ANDROID)
    add_library(app SHARED ${PROJECT_SOURCES})
else()
    add_executable(app ${PROJECT_SOURCES})
endif()

# -----------------------------------------------------------------------------
# Include Directories
# -----------------------------------------------------------------------------
target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# -----------------------------------------------------------------------------
# Imported Libraries
# -----------------------------------------------------------------------------
if(EMSCRIPTEN)
    add_library(SDL3_Static STATIC IMPORTED)
    set_target_properties(SDL3_Static PROPERTIES
        IMPORTED_LOCATION "${SDL3_PATH}/lib/libSDL3.a"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL3_PATH}/include"
    )

elseif(ANDROID)
    add_library(SDL3 SHARED IMPORTED)
    set_target_properties(SDL3 PROPERTIES
        IMPORTED_LOCATION "${SDL3_PATH}/lib/libSDL3.so"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL3_PATH}/include"
    )

    add_library(wgpu_native STATIC IMPORTED)
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION "${WGPU_PATH}/lib/libwgpu_native.a"
        INTERFACE_INCLUDE_DIRECTORIES "${WGPU_PATH}/include"
    )

    find_library(ANDROID_LIB android REQUIRED)
    find_library(LOG_LIB log REQUIRED)

elseif(WIN32)
    add_library(SDL3_Static STATIC IMPORTED)
    set_target_properties(SDL3_Static PROPERTIES
        IMPORTED_LOCATION "${SDL3_PATH}/lib/libSDL3.a"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL3_PATH}/include"
        INTERFACE_LINK_LIBRARIES "winmm;imm32;version;setupapi;dinput8"
    )

    add_library(wgpu_native STATIC IMPORTED)
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION "${WGPU_PATH}/lib/libwgpu_native.a"
        INTERFACE_INCLUDE_DIRECTORIES "${WGPU_PATH}/include"
        INTERFACE_LINK_LIBRARIES "ws2_32;userenv;ntdll;bcrypt;d3d12;dxgi"
    )

elseif(UNIX AND NOT APPLE)
    add_library(SDL3_Static STATIC IMPORTED)
    set_target_properties(SDL3_Static PROPERTIES
        IMPORTED_LOCATION "${SDL3_PATH}/lib/libSDL3.a"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL3_PATH}/include"
        INTERFACE_LINK_LIBRARIES "dl;pthread;m"
    )

    add_library(wgpu_native STATIC IMPORTED)
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION "${WGPU_PATH}/lib/libwgpu_native.a"
        INTERFACE_INCLUDE_DIRECTORIES "${WGPU_PATH}/include"
        INTERFACE_LINK_LIBRARIES "dl;pthread;m"
    )
endif()

# -----------------------------------------------------------------------------
# Link Libraries, Flags & Stripping
# -----------------------------------------------------------------------------
if(EMSCRIPTEN)
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
        "-sDEFAULT_TO_CXX"
        "--use-port=emdawnwebgpu"
    )

elseif(ANDROID)
    target_link_libraries(app PRIVATE
        SDL3
        wgpu_native
        ${ANDROID_LIB}
        ${LOG_LIB}
    )

elseif(WIN32)
    target_link_libraries(app PRIVATE
        SDL3_Static
        wgpu_native
    )

    target_link_options(app PRIVATE -static-libgcc -static-libstdc++)

    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(app PRIVATE -s)
    endif()

    target_link_options(app PRIVATE -static)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(app PRIVATE -mconsole)
    else()
        target_compile_options(app PRIVATE -Os)
        target_link_options(app PRIVATE -mwindows)
    endif()

elseif(UNIX AND NOT APPLE)
    target_link_libraries(app PRIVATE
        SDL3_Static
        wgpu_native
    )

    target_link_options(app PRIVATE -static-libgcc -static-libstdc++)

    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(app PRIVATE -Os)
        target_link_options(app PRIVATE -s)
    endif()
endif()