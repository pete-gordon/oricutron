#ifdef _MSC_VER
#ifdef __SPECIFY_SDL_DIR__
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif
#define strcasecmp SDL_strcasecmp
#define strncasecmp SDL_strncasecmp
#endif
