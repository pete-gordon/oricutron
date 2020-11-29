cmake_minimum_required(VERSION 3.10)
set (CMAKE_C_STANDARD 99)


project(Oricutron)

  find_library(SDL2_LIBRARY NAME SDL2)

  include_directories(${SDL2_INCLUDE_DIRS})
  add_definitions(-D__SPECIFY_SDL_DIR__=1 -DWWW -DAPP_NAME_FULL="Oricutron 1.2" -DAPP_YEAR="2015"
          -DVERSION_COPYRIGHTS="Oricutron 1.2.0 2016 Peter Gordon"
          -O3 -s USE_SDL=2 -s ASYNCIFY -s INITIAL_MEMORY=256mb
          -fsanitize=undefined -fsanitize-minimal-runtime -s FETCH -s FORCE_FILESYSTEM=1 )

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
)

SET (SRCSDL
  ../filereq_sdl.c
  ../msgbox_sdl.c
)

SET (SRCS
  ../6551.c
  ../6551_com.c
  ../6551_loopback.c
  ../6551_modem.c
  ../gui.c
  ../render_gl.c
  ../render_null.c
  ../render_sw.c
  ../render_sw8.c
  ../6502.c
  ../8912.c
  ../avi.c
  ../plugins/ch376/ch376.c
  ../disk.c
  ../disk_pravetz.c
  ../font.c
  ../joystick.c
  ../keyboard.c
  ../machine.c
  ../main.c
  ../monitor.c
  ../plugins/ch376/oric_ch376_plugin.c
  ../snapshot.c
  ../system_sdl.c
  ../tape.c
  ../ula.c
  ../via.c
)

SET(CMAKE_EXECUTABLE_SUFFIX ".html")

  add_executable(Oricutron ${SRCS} ${SRCSDL})
  set_target_properties(Oricutron PROPERTIES LINK_FLAGS "-O3 -s USE_SDL=2 -s ASYNCIFY --preload-file assets -fsanitize=undefined -fsanitize-minimal-runtime -s INITIAL_MEMORY=256mb -s FETCH -s FORCE_FILESYSTEM=1 -lidbfs.js")
  target_link_libraries(Oricutron ${SDL2_LIBRARIES})
