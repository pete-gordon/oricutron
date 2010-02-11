/*
**  Oricutron
**  Copyright (C) 2009 Peter Gordon
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
*/

#include <stdio.h>
#include <stdlib.h>

#include "system.h"
#include "avi.h"

#if SDL_BYTEORDER == SDL_LIL_ENDIAN
#define _LE32(X) (X)
#define _LE16(X) (X)
#define _BE32(X) SDL_Swap32(X)
#define _BE16(X) SDL_Swap32(X)
#else
#define _LE32(X) SDL_Swap32(X)
#define _LE16(X) SDL_Swap32(X)
#define _BE32(X) (X)
#define _BE16(X) (X)
#endif

// Write a 32bit value to a stream in little-endian format
static SDL_bool write32l( SDL_bool stillok, struct avi_handle *ah, Uint32 val )
{
  if( !stillok ) return SDL_FALSE; // Something failed earlier, so we're aborting
  ah->csize += 4;
  val = _LE32(val);
  return (fwrite( &val, 4, 1, ah->f ) == 1);
}

// Write a 16bit value to a stream in little-endian format
static SDL_bool write16l( SDL_bool stillok, struct avi_handle *ah, Uint16 val )
{
  if( !stillok ) return SDL_FALSE; // Something failed earlier, so we're aborting
  ah->csize += 2;
  val = _LE16(val);
  return (fwrite( &val, 2, 1, ah->f ) == 1);
}


// Write a string to a stream
static SDL_bool writestr( SDL_bool stillok, struct avi_handle *ah, char *str )
{
  Sint32 i = strlen( str );
  if( !stillok ) return SDL_FALSE; // Something failed earlier, so we're aborting
  ah->csize += i;
  return (fwrite( str, i, 1, ah->f ) == 1);
}

struct avi_handle *avi_open( char *filename )
{
  struct avi_handle *ah;
  SDL_bool ok;

  ah = malloc( sizeof( struct avi_handle ) );
  if( !ah ) return NULL;

  ah->f = fopen( filename, "wb" );
  if( !ah->f )
  {
    free( ah );
    return NULL;
  }

  ah->csize = 0;

  ok = SDL_TRUE;
  ok &= writestr( ok, ah, "RIFF    AVI LIST" );   // RIFF header and first LIST
  ok &= write32l( ok, ah, 306 );                  // LIST size
  ok &= writestr( ok, ah, "hdrlavih" );           // Header list, AVI header
  ok &= write32l( ok, ah, 56 );                   // AVI header size
  ok &= write32l( ok, ah, 20000 );                // dwMicroSecPerFrame   (20000us per frame)
  ok &= write32l( ok, ah, 3*240*224*50 );         // dwMaxBytesPerSec     (RGB*W*H*FPS)
  ok &= write32l( ok, ah, 4 );                    // dwPaddingGranularity
  ok &= write32l( ok, ah, AVIF_WASCAPTUREFILE );  // dwFlags
  ok &= write32l( ok, ah, 0 );                    // dwTotalFrames
  ok &= write32l( ok, ah, 0 );                    // dwInitialFrames
  ok &= write32l( ok, ah, 1 );                    // dwStreams
  ok &= write32l( ok, ah, 3*240*224*50 );         // dwSuggestedBufferSize (1 seconds worth?!)
  ok &= write32l( ok, ah, 240 );                  // dwWidth
  ok &= write32l( ok, ah, 224 );                  // dwHeight
  ok &= write32l( ok, ah, 0 );                    // Reserved 1
  ok &= write32l( ok, ah, 0 );                    // Reserved 2
  ok &= write32l( ok, ah, 0 );                    // Reserved 3
  ok &= write32l( ok, ah, 0 );                    // Reserved 4
  ok &= writestr( ok, ah, "LIST" );               // Next LIST
  ok &= write32l( ok, ah, 116 );                  // LIST size
  ok &= writestr( ok, ah, "strlstrh" );           // Stream list, stream header
  ok &= write32l( ok, ah, 56 );                   // Stream header size
  ok &= writestr( ok, ah, "vidsDIB " );           // video stream, uncompressed RGB
  ok &= write32l( ok, ah, 0 );                    // dwFlags
  ok &= write32l( ok, ah, 0 );                    // dwPriority
  ok &= write32l( ok, ah, 0 );                    // dwInitialFrames (?)
  ok &= write32l( ok, ah, 200000 );               // dwScale
  ok &= write32l( ok, ah, 1000000 );              // dwRate
  ok &= write32l( ok, ah, 0 );                    // dwStart
  ok &= write32l( ok, ah, 0 );                    // dwLength
  ok &= write32l( ok, ah, 3*240*224*50 );         // dwSuggestedBufferSize
  ok &= write32l( ok, ah, 10000 );                // dwQuality
  ok &= write32l( ok, ah, 3*240*224 );            // dwSampleSize
  ok &= write32l( ok, ah, 0 );                    // rcFrame top and left
  ok &= write16l( ok, ah, 240 );                  // rcFrame width
  ok &= write16l( ok, ah, 224 );                  // rcFrame height
  ok &= writestr( ok, ah, "strf" );               // strf chunk
  ok &= write32l( ok, ah, 40 );                   // size
  ok &= write32l( ok, ah, 40 );                   // BITMAPINFOHEADER biSize
  ok &= write32l( ok, ah, 224 );                  // biWidth
  ok &= write32l( ok, ah, 240 );                  // biHeight
  ok &= write16l( ok, ah, 1 );                    // biPlanes
  ok &= write16l( ok, ah, 24 );                   // biBitCount
  ok &= write32l( ok, ah, 0 );                    // biRGB (BI_RGB uncompressed)
  ok &= write32l( ok, ah, 240*224*3 );            // biSizeImage
  ok &= write32l( ok, ah, 0 );                    // biXPelsPerMeter
  ok &= write32l( ok, ah, 0 );                    // biYPelsPerMeter
  ok &= write32l( ok, ah, 0 );                    // biClrUsed
  ok &= write32l( ok, ah, 0 );                    // blClrImportant
  ok &= writestr( ok, ah, "LIST" );
  ah->movipos = ah->csize;
  ok &= write32l( ok, ah, 0 );
  ok &= writestr( ok, ah, "movi" );

  // ... still more to do!

  if( !ok )
  {
    free( ah );
    return NULL;
  }

  ah->csize = 52;
  ah->recpos = 0;
  ah->recframes = 0;

  return ah;
}

SDL_bool avi_addframe( struct avi_handle **ah, Uint8 *rgbdata )
{
  SDL_bool ok;
  
  ok = SDL_TRUE;
  if( !(*ah)->recpos )
  {
    ok &= writestr( ok, *ah, "LIST" );
    (*ah)->recpos = (*ah)->csize;
    ok &= write32l( ok, *ah, 0 );
    ok &= writestr( ok, *ah, "movi" );
  }  
  
  if( !ok )
  {
    avi_close( ah );
    return SDL_FALSE;
  }

  return SDL_TRUE;
}

void avi_close( struct avi_handle **ah )
{
  if( ( !ah ) || (!(*ah)) ) return;

  if( (*ah)->f )
  {
    fseek( (*ah)->f, 4, SEEK_SET );
    write32l( SDL_TRUE, (*ah), (*ah)->csize );
    fclose( (*ah)->f );
  }
  free( (*ah) );
  *ah = NULL;
}

