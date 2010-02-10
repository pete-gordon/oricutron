// System specific stuff

#if defined(__amigaos4__)

#define PATHSEP '/'
#define PATHSEPSTR "\\"
#define FILEPREFIX "PROGDIR:"
#define ROMPREFIX "PROGDIR:roms/"

#elif defined(WIN32)

#define PATHSEP '\\'
#define PATHSEPSTR "\\"
#define FILEPREFIX
#define ROMPREFIX "roms\\"

#else

#define PATHSEP '/'
#define FILEPREFIX
#define ROMPREFIX "roms/"

#endif

#ifdef __SPECIFY_SDL_DIR__
#include <SDL/SDL.h>
#ifdef WANT_WMINFO
#include <SDL/SDL_syswm.h>
#endif
#else
#include <SDL.h>
#ifdef WANT_WMINFO
#include <SDL_syswm.h>
#endif
#endif
