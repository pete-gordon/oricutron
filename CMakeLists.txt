cmake_minimum_required(VERSION 3.10)
set (CMAKE_C_STANDARD 99)

project(Oricutron)

set(OPENGL_LIBRARY "opengl32.lib")

add_library(SDL2 STATIC IMPORTED)
set_target_properties(SDL2
  PROPERTIES
  IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/msvc/vcpkg/installed/x64-windows/lib/SDL2.lib"
  INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/msvc/vcpkg/installed/x64-windows/include/SDL2"
)

add_definitions(-DSDL_MAIN_HANDLED=1 -D__OPENGL_AVAILABLE__ -D_MBCS -DWIN32 -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -D__CBCOPY__ -D__CBPASTE__ -DAPP_NAME_FULL="Oricutron 1.2" -DAPP_YEAR="2015" -DVERSION_COPYRIGHTS="Oricutron 1.2.0 2016 Peter Gordon")

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  "${PROJECT_SOURCE_DIR}/msvc"
)

SET (SRCS
  6551.c
  6551_com.c
  6551_loopback.c
  6551_modem.c
#  filereq_amiga.c
#  filereq_beos.cpp
#  filereq_gtk.c
#  filereq_sdl.c
  filereq_win32.c
  gui.c
#  gui_beos.cpp
  gui_win.c
#  gui_x11.c
#  msgbox_beos.cpp
#  msgbox_gtk.c
#  msgbox_os2.c
#  msgbox_os4.c
#  msgbox_sdl.c
  msgbox_win32.c
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

add_executable(Oricutron ${SRCS})

target_link_libraries(Oricutron SDL2 ${OPENGL_LIBRARY} )
