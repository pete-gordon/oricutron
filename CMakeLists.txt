cmake_minimum_required(VERSION 3.10)
set (CMAKE_C_STANDARD 99)

project(Oricutron)

option(USE_SDL2 "Use SDL2 instead of SDL1" OFF)

set(ORICUTRON_APPNAME Oricutron)
set(ORICUTRON_APPYEAR 2015)
SET(ORICUTRON_VERSION_MAJOR 1)
SET(ORICUTRON_VERSION_MINOR 2)
SET(ORICUTRON_VERSION_PATCH 0)
set(ORICUTRON_COMPANYNAME "pete-gordon/oricutron")
set(ORICUTRON_LINK "https://github.com/pete-gordon/oricutron/")
set(ORICUTRON_VERSION_NUMBER "${ORICUTRON_VERSION_MAJOR}.${ORICUTRON_VERSION_MINOR}")
set(ORICUTRON_VERSION_NUMBER_FULL "${ORICUTRON_VERSION_MAJOR}.${ORICUTRON_VERSION_MINOR}.${ORICUTRON_VERSION_PATCH}")
set(ORICUTRON_APPNAME_FULL "${ORICUTRON_APPNAME} ${ORICUTRON_VERSION_MAJOR}.${ORICUTRON_VERSION_MINOR}")
set(ORICUTRON_VERSION_COPYRIGHTS "${ORICUTRON_APPNAME} ${ORICUTRON_VERSION_MAJOR}.${ORICUTRON_VERSION_MINOR}.${ORICUTRON_VERSION_PATCH} (c)${ORICUTRON_APPYEAR} Peter Gordon (pete@gordon.plus)")

if (WIN32)
  set(OPENGL_LIBRARY "opengl32.lib")

  if (USE_SDL2)
    add_library(SDL2 STATIC IMPORTED)
      set_target_properties(SDL2
        PROPERTIES
        IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/msvc/vcpkg/installed/x64-windows/lib/SDL2.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/msvc/vcpkg/installed/x64-windows/include/SDL2"
      )
    add_definitions(-DSDL_MAJOR_VERSION=2 -DSDL_MAIN_HANDLED=1 -D__OPENGL_AVAILABLE__ -D_MBCS -DWIN32 -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -D__CBCOPY__ -D__CBPASTE__ -DAPP_NAME_FULL="${ORICUTRON_APPNAME_FULL}" -DAPP_YEAR="${ORICUTRON_APPYEAR}" -DVERSION_COPYRIGHTS="${ORICUTRON_VERSION_COPYRIGHTS}")
  else()
    add_library(SDL STATIC IMPORTED)
      set_target_properties(SDL
        PROPERTIES
        IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/msvc/vcpkg/installed/x64-windows/lib/SDL.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/msvc/vcpkg/installed/x64-windows/include/SDL"
      )
    add_definitions(-DSDL_MAJOR_VERSION=1 -DSDL_MAIN_HANDLED=1 -D__OPENGL_AVAILABLE__ -D_MBCS -DWIN32 -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -D__CBCOPY__ -D__CBPASTE__ -DAPP_NAME_FULL="${ORICUTRON_APPNAME_FULL}" -DAPP_YEAR="${ORICUTRON_APPYEAR}" -DVERSION_COPYRIGHTS="${ORICUTRON_VERSION_COPYRIGHTS}")
  endif (USE_SDL2)
endif (WIN32)

if (UNIX)

  if (USE_SDL2)
    find_package(SDL2 REQUIRED)
    find_package(OpenGL)
    # Setup CMake to use GTK+ and SDL2, tell the compiler where to look for headers
    # and to the linker where to look for libraries
    include_directories(${GTK3_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
    if (OPENGL_FOUND)
      add_definitions(-D__OPENGL_AVAILABLE__)
    endif(OPENGL_FOUND)
    add_definitions(${GTK3_CFLAGS_OTHER} -DSDL_MAJOR_VERSION=2 -D__SPECIFY_SDL_DIR__=1 -DAPP_NAME_FULL="${ORICUTRON_APPNAME_FULL}" -DAPP_YEAR="${ORICUTRON_APPYEAR}" -DVERSION_COPYRIGHTS="${ORICUTRON_VERSION_COPYRIGHTS}")
  else()
    # Use the package PkgConfig to detect GTK+ headers/library files
    find_package(PkgConfig REQUIRED)
    PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)
    find_package(SDL REQUIRED)

    # Setup CMake to use GTK+ and SDL, tell the compiler where to look for headers
    # and to the linker where to look for libraries
    include_directories(${GTK3_INCLUDE_DIRS} ${SDL_INCLUDE_DIRS})
    link_directories(${GTK3_LIBRARY_DIRS})
    add_definitions(${GTK3_CFLAGS_OTHER} -DSDL_MAJOR_VERSION=1 -D__SPECIFY_SDL_DIR__=1 -DAPP_NAME_FULL="${ORICUTRON_APPNAME_FULL}" -DAPP_YEAR="${ORICUTRON_APPYEAR}" -DVERSION_COPYRIGHTS="${ORICUTRON_VERSION_COPYRIGHTS}")
  endif (USE_SDL2)
endif (UNIX)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
)

if (WIN32)
include_directories(
  "${PROJECT_SOURCE_DIR}/msvc"
)
endif (WIN32)

SET (SRCAMIGA
  filereq_amiga.c
  msgbox_os4.c
)

SET (SRCBEOS
  filereq_beos.cpp
  gui_beos.cpp
  msgbox_beos.cpp
)

SET (SRCGTK
  filereq_gtk.c
  gui_x11.c
  msgbox_gtk.c
)

SET (SRCSDL
  filereq_sdl.c
  gui_x11.c
  msgbox_sdl.c
)


SET (SRCSWIN
  filereq_win32.c
  gui_win.c
  msgbox_win32.c
)

SET (SRCS
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

if (WIN32)
  if (USE_SDL2)
    add_executable(Oricutron-sdl2 ${SRCS} ${SRCSWIN} ${ORICUTRON_RC})
    target_link_libraries(Oricutron-sdl2 SDL2 ${OPENGL_LIBRARY})
  else()
    add_executable(Oricutron ${SRCS} ${SRCSWIN} ${ORICUTRON_RC})
    target_link_libraries(Oricutron SDL ${OPENGL_LIBRARY})
  endif (USE_SDL2)
endif (WIN32)

if (UNIX)
  if (USE_SDL2)
    add_executable(Oricutron-sdl2 ${SRCS} ${SRCSDL})
    target_link_libraries(Oricutron-sdl2 ${OPENGL_LIBRARIES} ${SDL2_LIBRARIES})
  else()
    add_executable(Oricutron ${SRCS} ${SRCGTK})
    target_link_libraries(Oricutron ${GTK3_LIBRARIES} SDL GL X11)
  endif (USE_SDL2)
endif (UNIX)
