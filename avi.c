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
**  AVI capturing
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
#define _BE16(X) SDL_Swap16(X)
#else
#define _LE32(X) SDL_Swap32(X)
#define _LE16(X) SDL_Swap16(X)
#define _BE32(X) (X)
#define _BE16(X) (X)
#endif

static Uint8 rledata[240*224*4];

// Write a 32bit value to a stream in little-endian format
static SDL_bool write32l( SDL_bool stillok, struct avi_handle *ah, Uint32 val, Uint32 *rem )
{
  if( !stillok ) return SDL_FALSE; // Something failed earlier, so we're aborting
  if( rem ) (*rem) = ah->csize;
  ah->csize += 4;
  val = _LE32(val);
  return (fwrite( &val, 4, 1, ah->f ) == 1);
}

// Write a 16bit value to a stream in little-endian format
static SDL_bool write16l( SDL_bool stillok, struct avi_handle *ah, Uint16 val, Uint32 *rem )
{
  if( !stillok ) return SDL_FALSE; // Something failed earlier, so we're aborting
  if( rem ) (*rem) = ah->csize;
  ah->csize += 2;
  val = _LE16(val);
  return (fwrite( &val, 2, 1, ah->f ) == 1);
}


// Write a string to a stream
static SDL_bool writestr( SDL_bool stillok, struct avi_handle *ah, char *str, Uint32 *rem )
{
  Sint32 i = strlen( str );
  if( !stillok ) return SDL_FALSE; // Something failed earlier, so we're aborting
  if( rem ) (*rem) = ah->csize;
  ah->csize += i;
  return (fwrite( str, i, 1, ah->f ) == 1);
}

// Write a 32bit value to a stream in little-endian format at a specific offset
static SDL_bool seek_write32l( SDL_bool stillok, struct avi_handle *ah, Uint32 offs, Uint32 val )
{
  if( !stillok ) return SDL_FALSE; // Something failed earlier, so we're aborting
  fseek( ah->f, offs, SEEK_SET );
  val = _LE32(val);
  return (fwrite( &val, 4, 1, ah->f ) == 1);
}

// Write a block of memory
static SDL_bool writeblk( SDL_bool stillok, struct avi_handle *ah, Uint8 *blk, Uint32 blklen )
{
  if( !stillok ) return SDL_FALSE;
  ah->csize += blklen;
  return (fwrite( blk, blklen, 1, ah->f ) == 1);
}

// Write an 8bit value to a stream
static SDL_bool writebyt( SDL_bool stillok, struct avi_handle *ah, Uint8 val )
{
  if( !stillok ) return SDL_FALSE; // Something failed earlier, so we're aborting
  ah->csize += 1;
  return (fwrite( &val, 1, 1, ah->f ) == 1);
}

#define AVIFLAGS (AVIF_ISINTERLEAVED|AVIF_WASCAPTUREFILE)
struct avi_handle *avi_open( char *filename, Uint8 *pal )
{
  struct avi_handle *ah;
  SDL_bool ok;
  Sint32 i;

  ah = malloc( sizeof( struct avi_handle ) );
  if( !ah ) return NULL;
  
  memset( ah, 0, sizeof( struct avi_handle ) );

  ah->f = fopen( filename, "wb" );
  if( !ah->f )
  {
    free( ah );
    return NULL;
  }

  ah->csize = 0;

  ok = SDL_TRUE;
  ok &= writestr( ok, ah, "RIFF"        , NULL               );   // RIFF header
  ok &= write32l( ok, ah,              0, &ah->offs_riffsize );   // RIFF size
  ok &= writestr( ok, ah, "AVI "        , NULL               );   // RIFF type
  ok &= writestr( ok, ah, "LIST"        , NULL               );   // Header list
  ok &= write32l( ok, ah,              0, &ah->offs_hdrlsize );   // Header list size
  ok &= writestr( ok, ah, "hdrl"        , NULL               );   // Header list identifier

  ok &= writestr( ok, ah, "avih"        , NULL               );   // MainAVIHeader chunk
  ok &= write32l( ok, ah,             56, NULL               );   // Chunk size
  ok &= write32l( ok, ah,          20000, NULL               );   // Microseconds per frame
  ok &= write32l( ok, ah,              0, NULL               );   // Max bytes per second
  ok &= write32l( ok, ah,              0, NULL               );   // Padding granularity
  ok &= write32l( ok, ah,       AVIFLAGS, NULL               );   // Flags
  ok &= write32l( ok, ah,              0, &ah->offs_frames   );   // Number of frames
  ok &= write32l( ok, ah,              0, NULL               );   // Initial frames
  ok &= write32l( ok, ah,              2, NULL               );   // Stream count
  ok &= write32l( ok, ah,              0, NULL               );   // Suggested buffer size
  ok &= write32l( ok, ah,            240, NULL               );   // Width
  ok &= write32l( ok, ah,            224, NULL               );   // Height
  ok &= write32l( ok, ah,              0, NULL               );   // Reserved
  ok &= write32l( ok, ah,              0, NULL               );   // Reserved
  ok &= write32l( ok, ah,              0, NULL               );   // Reserved
  ok &= write32l( ok, ah,              0, NULL               );   // Reserved

  ok &= writestr( ok, ah, "LIST"        , NULL               );   // Stream header list
  ok &= write32l( ok, ah,4+8+56+8+40+8*4, NULL               );   // Stream header list size
  ok &= writestr( ok, ah, "strl"        , NULL               );   // Stream header list identifier

  ok &= writestr( ok, ah, "strh"        , NULL               );   // Video stream header
  ok &= write32l( ok, ah,             56, NULL               );   // Chunk size
  ok &= writestr( ok, ah, "vids"        , NULL               );   // Type
  ok &= writestr( ok, ah, "MRLE"        , NULL               );   // Microsoft Run Length Encoding
  ok &= write32l( ok, ah,              0, NULL               );   // Flags
  ok &= write32l( ok, ah,              0, NULL               );   // Reserved
  ok &= write32l( ok, ah,              0, NULL               );   // Initial frames
  ok &= write32l( ok, ah,        1000000, NULL               );   // Scale
  ok &= write32l( ok, ah,       50000000, NULL               );   // Rate
  ok &= write32l( ok, ah,              0, NULL               );   // Start
  ok &= write32l( ok, ah,              0, &ah->offs_frames2  );   // Length
  ok &= write32l( ok, ah,              0, NULL               );   // Suggested buffer size
  ok &= write32l( ok, ah,          10000, NULL               );   // Quality
  ok &= write32l( ok, ah,              0, NULL               );   // Sample size
  ok &= write32l( ok, ah,              0, NULL               );   // Frame
  ok &= write32l( ok, ah,              0, NULL               );   // Frame

  ok &= writestr( ok, ah, "strf"        , NULL               );   // Video stream format
  ok &= write32l( ok, ah,         40+8*4, NULL               );   // Chunk size
  ok &= write32l( ok, ah,             40, NULL               );   // Structure size
  ok &= write32l( ok, ah,            240, NULL               );   // Width
  ok &= write32l( ok, ah,            224, NULL               );   // Height
  ok &= write16l( ok, ah,              1, NULL               );   // Planes
  ok &= write16l( ok, ah,              8, NULL               );   // Bit count
  ok &= write32l( ok, ah,              1, NULL               );   // Compression
  ok &= write32l( ok, ah,        240*244, NULL               );   // Image size
  ok &= write32l( ok, ah,              0, NULL               );   // XPelsPerMeter
  ok &= write32l( ok, ah,              0, NULL               );   // YPelsPerMeter
  ok &= write32l( ok, ah,              8, NULL               );   // Colours used
  ok &= write32l( ok, ah,              8, NULL               );   // Colours important

  for( i=0; i<8; i++ )
  {
    ok &= writeblk( ok, ah, &pal[i*3], 3 );
    ok &= writebyt( ok, ah, 0 );
  }

  ok &= writestr( ok, ah, "LIST"      , NULL               );   // Stream header list
  ok &= write32l( ok, ah,  4+8+56+8+16, NULL               );   // Stream header list size
  ok &= writestr( ok, ah, "strl"      , NULL               );   // Stream header identifier

  ok &= writestr( ok, ah, "strh"      , NULL               );   // Audio stream header
  ok &= write32l( ok, ah,           56, NULL               );   // Chunk size
  ok &= writestr( ok, ah, "auds"      , NULL               );   // Type
  ok &= write32l( ok, ah,            0, NULL               );   // Codec
  ok &= write32l( ok, ah,            0, NULL               );   // Flags
  ok &= write32l( ok, ah,            0, NULL               );   // Reserved
  ok &= write32l( ok, ah,            0, NULL               );   // Initial frames
  ok &= write32l( ok, ah,            1, NULL               );   // Scale
  ok &= write32l( ok, ah,   AUDIO_FREQ, NULL               );   // Rate
  ok &= write32l( ok, ah,            0, NULL               );   // Start
  ok &= write32l( ok, ah,            0, &ah->offs_audiolen );   // Length
  ok &= write32l( ok, ah,            0, NULL               );   // Suggested buffer size
  ok &= write32l( ok, ah,        10000, NULL               );   // Quality
  ok &= write32l( ok, ah,            2, NULL               );   // Sample size
  ok &= write32l( ok, ah,            0, NULL               );   // Frame
  ok &= write32l( ok, ah,            0, NULL               );   // Frame

  ok &= writestr( ok, ah, "strf"      , NULL               );   // Audio stream format
  ok &= write32l( ok, ah,           16, NULL               );   // Chunk size
  ok &= write16l( ok, ah,            1, NULL               );   // Format (PCM)
  ok &= write16l( ok, ah,            1, NULL               );   // Channels
  ok &= write32l( ok, ah,   AUDIO_FREQ, NULL               );   // Samples per second
  ok &= write32l( ok, ah, AUDIO_FREQ*2, NULL               );   // Bytes per second
  ok &= write16l( ok, ah,            2, NULL               );   // BlockAlign
  ok &= write16l( ok, ah,           16, NULL               );   // BitsPerSample

  ah->hdrlsize = ah->csize - ah->offs_hdrlsize - 4;

  ok &= writestr( ok, ah, "LIST"      , NULL               );   // movi list
  ok &= write32l( ok, ah,            0, &ah->offs_movisize );   // Size
  ok &= writestr( ok, ah, "movi"      , NULL               );   // identifier

  if( !ok )
  {
    free( ah );
    return NULL;
  }

  ah->frames       = 0;
  ah->audiolen     = 0;
  ah->movisize     = 0;

  return ah;
}

static Uint32 rle_putpixels( Uint8 *srcdata, Uint32 rlec, Uint32 srcc, Uint32 length )
{
  Uint32 c, chunker;

  if( length == 0 ) return rlec;

  while( length > 254 )
  {
    // Try not to leave an annoying dangly 1,2 or 3 byte section
    chunker = (length<258) ? 250 : 254;
    rlec = rle_putpixels( srcdata, rlec, srcc, chunker );
    srcc   += chunker;
    length -= chunker;
  }

  // Somehow "chunker" failed :-(
  if( length < 3 )
  {
    while( length > 0 )
    {
      rledata[rlec++] = 0x01;
      rledata[rlec++] = srcdata[srcc++];
      length--;
    }
    return rlec;
  }

  c = length;

  rledata[rlec++] = 0x00;
  rledata[rlec++] = length;
  while( c > 0 )
  {
    rledata[rlec++] = srcdata[srcc++];
    c--;
  }

  if( length & 1 )
    rledata[rlec++] = 0;

  return rlec;
}

// RLE encode the frame
#define NOABS 0xffffffff
#define ENCODEEND 240
static Uint32 rle_encode( Uint8 *srcdata, Uint32 rlec )
{
  Uint32 srcc, runc, abspos, chunker;
  Uint8 thiscol;

  srcc = 0; // Source count
  abspos = NOABS;
  while( srcc < ENCODEEND )
  {
    // Look for a run
    thiscol = srcdata[srcc];

    // At the last pixel?
    if( srcc == ENCODEEND-1 )
    {
      // Need to dump an absolute section?
      if( abspos != NOABS ) rlec = rle_putpixels( srcdata, rlec, abspos, srcc-abspos );

      // Just encode it
      rledata[rlec++] = 0x01;
      rledata[rlec++] = thiscol;

      rledata[rlec++] = 0x00; // The end!
      rledata[rlec++] = 0x00;
      return rlec;
    }

    runc = 1; // Set the run count to 1
    while( srcdata[srcc+runc] == thiscol )
    {
      runc++;
      if( (srcc+runc) >= ENCODEEND ) break;
    }

    // Need an absolute mode section?
    if( runc < 2 )
    {
      // Starting a new absolute mode section?
      if( abspos == NOABS ) abspos = srcc;

      // Next pixel!
      srcc++;
      continue;
    }

    // Need to dump an absolute section?
    if( abspos != NOABS )
    {
      rlec = rle_putpixels( srcdata, rlec, abspos, srcc-abspos );
      abspos = NOABS;
    }

    // Encode that run!
    srcc += runc;   // Skip past the run

    while( runc > 255 )  // Do any 255 byte chunks until runc < 255
    {
      chunker = (runc<258) ? 250 : 255;
      rledata[rlec++] = chunker;
      rledata[rlec++] = thiscol;
      runc -= chunker;
    }

    rledata[rlec++] = runc;   // Encode the bastard!
    rledata[rlec++] = thiscol;
  }

  if( abspos != NOABS ) rlec = rle_putpixels( srcdata, rlec, abspos, srcc-abspos );
  rledata[rlec++] = 0x00; // The end!
  rledata[rlec++] = 0x00;
  return rlec;
}

SDL_bool avi_addframe( struct avi_handle **ah, Uint8 *srcdata )
{
  SDL_bool ok;
  Sint32 i;
  Uint32 size;

  for( i=223,size=0; i>=0; i-- )
    size = rle_encode( &srcdata[i*240], size );

  ok = SDL_TRUE;
  ok &= writestr( ok, *ah, "00dc", NULL );  // Chunk header
  ok &= write32l( ok, *ah, size, NULL );    // Chunk size
  ok &= writeblk( ok, *ah, rledata, size ); // Chunk data

  (*ah)->movisize += size + 8;
  (*ah)->frames++;

  if( !ok )
  {
    avi_close( ah );
    return SDL_FALSE;
  }

  return SDL_TRUE;
}

SDL_bool avi_addaudio( struct avi_handle **ah, Sint16 *audiodata, Uint32 audiosize )
{
  SDL_bool ok;

  ok = SDL_TRUE;
  ok &= writestr( ok, *ah, "01wb", NULL );       // Chunk header
  ok &= write32l( ok, *ah, audiosize, NULL );    // Chunk size
  ok &= writeblk( ok, *ah, (Uint8 *)audiodata, audiosize ); // Chunk data

  (*ah)->movisize += audiosize+8;
  (*ah)->audiolen += audiosize;

  if( !ok )
  {
    avi_close( ah );
    return SDL_FALSE;
  }

  return SDL_TRUE;
}

void avi_close( struct avi_handle **ah )
{
  SDL_bool ok;
  if( ( !ah ) || (!(*ah)) ) return;

  if( (*ah)->f )
  {
    ok = SDL_TRUE;
    ok &= seek_write32l( ok, *ah, (*ah)->offs_riffsize, (*ah)->csize-8  );
    ok &= seek_write32l( ok, *ah, (*ah)->offs_hdrlsize, (*ah)->hdrlsize );
    ok &= seek_write32l( ok, *ah, (*ah)->offs_frames,   (*ah)->frames   );
    ok &= seek_write32l( ok, *ah, (*ah)->offs_frames2,  (*ah)->frames   );
    ok &= seek_write32l( ok, *ah, (*ah)->offs_movisize, (*ah)->movisize );
    ok &= seek_write32l( ok, *ah, (*ah)->offs_audiolen, (*ah)->audiolen );
    fclose( (*ah)->f );
  }
  free( (*ah) );
  *ah = NULL;
}

