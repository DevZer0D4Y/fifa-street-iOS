# fifastreet - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(fifastreet LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(FIFASTREET_SOURCES
    src/main.cpp
)

if(WIN32)
    enable_language(RC)
    add_executable(fifastreet WIN32 ${FIFASTREET_SOURCES})
    target_sources(fifastreet PRIVATE src/fifastreet.rc)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set_source_files_properties(src/fifastreet.rc PROPERTIES
            COMPILE_OPTIONS "/nologo"
        )
        target_include_directories(fifastreet PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
        )
    endif()
else()
    add_executable(fifastreet ${FIFASTREET_SOURCES})
endif()

rexglue_setup_target(fifastreet GPU_PLUGINS xenos)
target_include_directories(fifastreet PRIVATE src)

# On-screen football gamepad (src/ios/touch_pad.h): on by default on iOS, --touch_controls=true
# elsewhere. Not on Windows: rexruntime.dll does not export its static SDL3.
if(NOT WIN32)
    find_package(SDL3 REQUIRED CONFIG COMPONENTS Headers)
    target_link_libraries(fifastreet PRIVATE SDL3::Headers)
    target_sources(fifastreet PRIVATE src/ios/touch_pad.cpp)
    target_compile_definitions(fifastreet PRIVATE FIFASTREET_TOUCH=1)
endif()

# iPhone / iPad (docs/IOS.md): SDK built with patches/rexglue-ios.patch, Vulkan through MoltenVK.
# The guest vblank comes from the screen (src/ios/display_link.mm, used by the SDK's
# Presenter::WaitForMonitorVBlank), so 60 fps lands on every display refresh.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJCXX)
    target_sources(fifastreet PRIVATE src/ios/display_link.mm)
    set_source_files_properties(src/ios/display_link.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(fifastreet PRIVATE "-framework UIKit" "-framework QuartzCore" "-framework Foundation")
    # rex_wait_display_vblank is looked up with dlsym from the SDK's runtime library; it is
    # marked visibility("default") + used, so the executable exports it.
endif()

# The source SDK keeps ImGui private to rexruntime; its public UI headers
# still require the include directory when compiling the host application.
# Propagate headers only, keeping the ImGui objects in the shared runtime.
if(TARGET imgui::imgui)
    target_include_directories(fifastreet PRIVATE
        $<TARGET_PROPERTY:imgui::imgui,INTERFACE_INCLUDE_DIRECTORIES>)
endif()

# Guest SIMD code must be optimized even when using the debug runtime. Keep
# debug symbols and checked runtime libraries while avoiding unoptimized SIMD
# wrappers in the game and its separately compiled DLL.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(fifastreet PRIVATE -O2)
    if(TARGET fifastreet_recomp)
        target_compile_options(fifastreet_recomp PRIVATE -O2)
    endif()
endif()

# FIFA Street DLL-specific kernel stubs.
if(TARGET fifastreet_fifadllzf_xex)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(fifastreet_fifadllzf_xex PRIVATE -O2)
    endif()
    target_sources(fifastreet_fifadllzf_xex PRIVATE
        src/stubs.cpp
        src/native_libc_hooks.cpp
    )
endif()

# World Tour competition engine DLL. Keep it optimized, but do not attach
# fifadllzf-specific stubs to this independent guest module.
if(TARGET fifastreet_FootballCompEngzf_xex)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(fifastreet_FootballCompEngzf_xex PRIVATE -O2)
    endif()
endif()
