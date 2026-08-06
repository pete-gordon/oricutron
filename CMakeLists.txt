cmake_minimum_required(VERSION 3.10)
set(CMAKE_C_STANDARD 99)

project(Oricutron LANGUAGES C)

# ----------------------------------------------------------------------
# Options
# ----------------------------------------------------------------------
option(USE_SDL2 "Use SDL2 instead of SDL1" OFF)

# ----------------------------------------------------------------------
# Help find packages on macOS (Homebrew)
# ----------------------------------------------------------------------
if(APPLE)
  list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
  set(ENV{PKG_CONFIG_PATH}
      "/opt/homebrew/lib/pkgconfig:/usr/local/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
endif()

# ----------------------------------------------------------------------
# Version / application information
# ----------------------------------------------------------------------
set(ORICUTRON_APPNAME Oricutron)
set(ORICUTRON_APPYEAR 2019)          # matched to the working Makefile
set(ORICUTRON_VERSION_MAJOR 1)
set(ORICUTRON_VERSION_MINOR 2)
set(ORICUTRON_VERSION_PATCH 0)
set(ORICUTRON_COMPANYNAME "pete-gordon/oricutron")
set(ORICUTRON_LINK "https://github.com/pete-gordon/oricutron/")
set(ORICUTRON_VERSION_NUMBER "${ORICUTRON_VERSION_MAJOR}.${ORICUTRON_VERSION_MINOR}")
set(ORICUTRON_VERSION_NUMBER_FULL "${ORICUTRON_VERSION_MAJOR}.${ORICUTRON_VERSION_MINOR}.${ORICUTRON_VERSION_PATCH}")
set(ORICUTRON_APPNAME_FULL "${ORICUTRON_APPNAME} ${ORICUTRON_VERSION_MAJOR}.${ORICUTRON_VERSION_MINOR}")
set(ORICUTRON_VERSION_COPYRIGHTS "${ORICUTRON_APPNAME} ${ORICUTRON_VERSION_MAJOR}.${ORICUTRON_VERSION_MINOR}.${ORICUTRON_VERSION_PATCH} (c)${ORICUTRON_APPYEAR} Peter Gordon (pete@gordon.plus)")

# ----------------------------------------------------------------------
# Source groups
# ----------------------------------------------------------------------
set(SRCAMIGA
  filereq_amiga.c
  msgbox_os4.c
)

set(SRCBEOS
  filereq_beos.cpp
  gui_beos.cpp
  msgbox_beos.cpp
)

set(SRCGTK
  filereq_gtk.c
  gui_x11.c
  msgbox_gtk.c
)

set(SRCSDL
  filereq_sdl.c
  gui_x11.c
  msgbox_sdl.c
)

set(SRCSWIN
  filereq_win32.c
  gui_win.c
  msgbox_win32.c
)

# Native macOS sources (matches the working Makefile)
set(SRCSOSX
  gui_osx.m
  filereq_osx.m
  msgbox_osx.m
)

set(SRCS
  6551.c
  6551_com.c
  6551_loopback.c
  6551_modem.c
  gui.c
  render_gl.c
  render_null.c
  render_sw.c
  render_sw8.c
  6502.c
  8912.c
  avi.c
  plugins/ch376/ch376.c
  disk.c
  disk_pravetz.c
  font.c
  joystick.c
  keyboard.c
  machine.c
  main.c
  monitor.c
  plugins/ch376/oric_ch376_plugin.c
  plugins/twilighte_board/oric_twilighte_board_plugin.c
  snapshot.c
  system_sdl.c
  tape.c
  ula.c
  via.c
)

set(ORICUTRON_RC ${PROJECT_SOURCE_DIR}/msvc/Oricutron.rc)

# ----------------------------------------------------------------------
# Platform source selection
# ----------------------------------------------------------------------
if(APPLE)
  enable_language(OBJC)
  set(PLATFORM_SOURCES ${SRCSOSX})
elseif(UNIX)
  if(USE_SDL2)
    set(PLATFORM_SOURCES ${SRCSDL})
  else()
    set(PLATFORM_SOURCES ${SRCGTK})
  endif()
endif()

# ----------------------------------------------------------------------
# Common compiler definitions (matches the Makefile)
# ----------------------------------------------------------------------
add_definitions(
  -DAPP_NAME_FULL="${ORICUTRON_APPNAME_FULL}"
  -DAPP_YEAR="${ORICUTRON_APPYEAR}"
  -DVERSION_COPYRIGHTS="${ORICUTRON_VERSION_COPYRIGHTS}"
  -D__CBCOPY__
  -D__CBPASTE__
)

if(APPLE)
  add_definitions(-DGL_SILENCE_DEPRECATION)
endif()

# ----------------------------------------------------------------------
# Windows setup
# ----------------------------------------------------------------------
if(WIN32)
  set(OPENGL_LIBRARY "opengl32.lib")

  if(USE_SDL2)
    add_library(SDL2 STATIC IMPORTED)
    set_target_properties(SDL2 PROPERTIES
      IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/msvc/vcpkg/installed/x64-windows/lib/SDL2.lib"
      INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/msvc/vcpkg/installed/x64-windows/include/SDL2"
    )
    add_definitions(
      -DSDL_MAJOR_VERSION=2
      -DSDL_MAIN_HANDLED=1
      -D__OPENGL_AVAILABLE__
      -D_MBCS -DWIN32
      -D_CRT_SECURE_NO_WARNINGS
      -D_CRT_NONSTDC_NO_WARNINGS
    )
  else()
    add_library(SDL STATIC IMPORTED)
    set_target_properties(SDL PROPERTIES
      IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/msvc/vcpkg/installed/x64-windows/lib/SDL.lib"
      INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/msvc/vcpkg/installed/x64-windows/include/SDL"
    )
    add_definitions(
      -DSDL_MAJOR_VERSION=1
      -DSDL_MAIN_HANDLED=1
      -D__OPENGL_AVAILABLE__
      -D_MBCS -DWIN32
      -D_CRT_SECURE_NO_WARNINGS
      -D_CRT_NONSTDC_NO_WARNINGS
    )
  endif()
endif()

# ----------------------------------------------------------------------
# UNIX / macOS dependency discovery
# ----------------------------------------------------------------------
if(UNIX)
  if(USE_SDL2)
    find_package(SDL2 REQUIRED)
    find_package(OpenGL)

    include_directories(${SDL2_INCLUDE_DIRS})

    if(OPENGL_FOUND)
      add_definitions(-D__OPENGL_AVAILABLE__)
    endif()

    add_definitions(-DSDL_MAJOR_VERSION=2 -D__SPECIFY_SDL_DIR__=1)
  else()
    # SDL 1.x – fix include path for <SDL/SDL.h>
    find_package(SDL REQUIRED)
    find_package(OpenGL)

    get_filename_component(SDL_INCLUDE_PARENT "${SDL_INCLUDE_DIRS}" DIRECTORY)
    include_directories(${SDL_INCLUDE_PARENT})

    if(OPENGL_FOUND)
      add_definitions(-D__OPENGL_AVAILABLE__)
    endif()

    add_definitions(-DSDL_MAJOR_VERSION=1 -D__SPECIFY_SDL_DIR__=1)

    if(NOT APPLE)
      find_package(PkgConfig REQUIRED)
      pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
      include_directories(${GTK3_INCLUDE_DIRS})
      link_directories(${GTK3_LIBRARY_DIRS})
      add_definitions(${GTK3_CFLAGS_OTHER})
    endif()
  endif()
endif()

# ----------------------------------------------------------------------
# Common include directories
# ----------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_BINARY_DIR})

if(WIN32)
  include_directories("${PROJECT_SOURCE_DIR}/msvc")
endif()

# ----------------------------------------------------------------------
# Create the executable
# ----------------------------------------------------------------------
if(WIN32)
  if(USE_SDL2)
    add_executable(Oricutron-sdl2 ${SRCS} ${SRCSWIN} ${ORICUTRON_RC})
    target_link_libraries(Oricutron-sdl2 SDL2 ${OPENGL_LIBRARY})
  else()
    add_executable(Oricutron ${SRCS} ${SRCSWIN} ${ORICUTRON_RC})
    target_link_libraries(Oricutron SDL ${OPENGL_LIBRARY})
  endif()
endif()

if(UNIX)
  if(USE_SDL2)
    add_executable(Oricutron-sdl2 ${SRCS} ${PLATFORM_SOURCES})

    if(APPLE)
      target_link_libraries(Oricutron-sdl2
        ${SDL2_LIBRARIES}
        ${OPENGL_LIBRARIES}
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework CoreFoundation"
        "-framework AppKit"
      )
    else()
      target_link_libraries(Oricutron-sdl2 ${OPENGL_LIBRARIES} ${SDL2_LIBRARIES})
    endif()
  else()
    # SDL 1.x path – matches the working Makefile on macOS
    add_executable(Oricutron ${SRCS} ${PLATFORM_SOURCES})

    if(APPLE)
      target_link_libraries(Oricutron
        ${SDL_LIBRARIES}
        ${OPENGL_LIBRARIES}
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework CoreFoundation"
        "-framework AppKit"
      )
    else()
      target_link_libraries(Oricutron
        ${GTK3_LIBRARIES}
        ${SDL_LIBRARIES}
        GL
        X11
      )
    endif()
  endif()
endif()
