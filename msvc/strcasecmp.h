#ifndef ORICUTRON_STRCASECMP_H
#define ORICUTRON_STRCASECMP_H

#ifdef _MSC_VER
#ifdef __SPECIFY_SDL_DIR__
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif
#ifndef strcasecmp
#define strcasecmp SDL_strcasecmp
#endif
#ifndef strncasecmp
#define strncasecmp SDL_strncasecmp
#endif
#endif

#endif /* ORICUTRON_STRCASECMP_H */
