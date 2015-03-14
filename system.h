/*
**  Oricutron
**  Copyright (C) 2009-2014 Peter Gordon
**
**  This program is free software; you can redistribute it and/or
**  modify it under the terms of the GNU General Public License
**  as published by the Free Software Foundation, version 2
**  of the License.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
**  System specific stuff
*/

#ifndef ORICUTRON_SYSTEM_H
#define ORICUTRON_SYSTEM_H



/* Output audio frequency */
#define AUDIO_FREQ   44100


#if defined(__amigaos4__)

#define PATHSEP '/'
#define PATHSEPSTR "/"
#define FILEPREFIX "PROGDIR:"
#define ROMPREFIX "PROGDIR:roms/"
#define IMAGEPREFIX "PROGDIR:images/"

#elif defined(WIN32)

#define PATHSEP '\\'
#define PATHSEPSTR "\\"
#define FILEPREFIX
#define ROMPREFIX "roms\\"
#define IMAGEPREFIX "images\\"

#else

#define PATHSEP '/'
#define PATHSEPSTR "/"
#define FILEPREFIX
#define ROMPREFIX "roms/"
#define IMAGEPREFIX "images/"

#endif

/* SDL related stuff */
#include "system_sdl.h"

#endif /* ORICUTRON_SYSTEM_H */
