cmake_minimum_required(VERSION 3.10)
set (CMAKE_C_STANDARD 99)

project(Oricutron)

if (WIN32)
  set(OPENGL_LIBRARY "opengl32.lib")

  add_library(SDL2 STATIC IMPORTED)
    set_target_properties(SDL2
      PROPERTIES
      IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/msvc/vcpkg/installed/x64-windows/lib/SDL2.lib"
      INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/msvc/vcpkg/installed/x64-windows/include/SDL2"
    )

  add_definitions(-DSDL_MAIN_HANDLED=1 -D__OPENGL_AVAILABLE__ -D_MBCS -DWIN32 -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -D__CBCOPY__ -D__CBPASTE__ -DAPP_NAME_FULL="Oricutron 1.2" -DAPP_YEAR="2015" -DVERSION_COPYRIGHTS="Oricutron 1.2.0 2016 Peter Gordon")
endif (WIN32)

if (UNIX)
  # Use the package PkgConfig to detect GTK+ headers/library files
  find_package(PkgConfig REQUIRED)
  PKG_CHECK_MODULES(GTK3 REQUIRED gtk+-3.0)

  find_package(SDL2 REQUIRED)

  # Setup CMake to use GTK+ and SDL2, tell the compiler where to look for headers
  # and to the linker where to look for libraries
  include_directories(${GTK3_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
  link_directories(${GTK3_LIBRARY_DIRS})

  add_definitions(${GTK3_CFLAGS_OTHER} -D__SPECIFY_SDL_DIR__=1 -DAPP_NAME_FULL="Oricutron 1.2" -DAPP_YEAR="2015" -DVERSION_COPYRIGHTS="Oricutron 1.2.0 2016 Peter Gordon")
endif (UNIX)

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
if (WIN32)
  "${PROJECT_SOURCE_DIR}/msvc"
endif (WIN32)
)

SET (SRCAMIGA
  filereq_amiga.c
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
#  msgbox_os2.c
#  msgbox_os4.c
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
  snapshot.c
  system_sdl.c
  tape.c
  ula.c
  via.c
)

if (WIN32)
  add_executable(Oricutron ${SRCS} ${SRCSWIN})
  target_link_libraries(Oricutron SDL2 ${OPENGL_LIBRARY})
endif (WIN32)

if (UNIX)
  add_executable(Oricutron ${SRCS} ${SRCGTK})
  target_link_libraries(Oricutron ${GTK3_LIBRARIES} ${SDL2_LIBRARIES})
endif (UNIX)
