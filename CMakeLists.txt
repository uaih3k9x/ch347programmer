# CH341 Compatibility Layer for CH347
# CMake build configuration

cmake_minimum_required(VERSION 3.10)
project(CH341Compat VERSION 1.0.0 LANGUAGES C)

# Options
option(BUILD_SHARED_LIBS "Build shared library (DLL)" ON)
option(BUILD_STATIC_LIBS "Build static library" OFF)

# Source files
set(SOURCES
    ch341_compat.c
)

set(HEADERS
    ch341_compat.h
)

# Windows specific settings
if(WIN32)
    # Shared library (DLL)
    if(BUILD_SHARED_LIBS)
        add_library(CH341DLL SHARED ${SOURCES} ${HEADERS} ch341_compat.def)
        target_compile_definitions(CH341DLL PRIVATE _WIN32_WINNT=0x0601)
        set_target_properties(CH341DLL PROPERTIES
            OUTPUT_NAME "CH341DLL"
            PREFIX ""
            VERSION ${PROJECT_VERSION}
        )

        # For MSVC, use the def file for exports
        if(MSVC)
            set_target_properties(CH341DLL PROPERTIES
                LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/ch341_compat.def"
            )
        endif()
    endif()

    # Static library
    if(BUILD_STATIC_LIBS)
        add_library(CH341DLL_static STATIC ${SOURCES} ${HEADERS})
        target_compile_definitions(CH341DLL_static PRIVATE _WIN32_WINNT=0x0601)
        set_target_properties(CH341DLL_static PROPERTIES
            OUTPUT_NAME "CH341DLL_static"
        )
    endif()
endif()

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Install targets
install(FILES ${HEADERS} DESTINATION include)
if(BUILD_SHARED_LIBS AND TARGET CH341DLL)
    install(TARGETS CH341DLL
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
    )
endif()
if(BUILD_STATIC_LIBS AND TARGET CH341DLL_static)
    install(TARGETS CH341DLL_static
        ARCHIVE DESTINATION lib
    )
endif()
