// System specific stuff

// Output audio frequency
#define AUDIO_FREQ   44100


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

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define _LE32(X) (X)
#define _LE16(X) (X)
#define _BE32(X) SDL_Swap32(X)
#define _BE16(X) SDL_Swap16(X)
#else
#define _LE32(X) SDL_Swap32(X)
#define _LE16(X) SDL_Swap16(X)
#define _BE32(X) (X)
#define _BE16(X) (X)
#endif
