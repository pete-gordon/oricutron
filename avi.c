/*
**  Oricutron
**  Copyright (C) 2009-2011 Peter Gordon
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
#include <math.h>

#include "system.h"
#include "avi.h"


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
  if( offs == 0 ) return SDL_TRUE; // Don't overwrite RIFF!
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
struct avi_handle *avi_open( char *filename, Uint8 *pal, SDL_bool dosound, int is50hz )
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

  ah->dosnd = dosound;
  ah->csize = 0;
  ah->is50hz = is50hz;

  ok = SDL_TRUE;
  ok &= writestr( ok, ah, "RIFF"        , NULL               );   // RIFF header
  ok &= write32l( ok, ah,              0, &ah->offs_riffsize );   // RIFF size
  ok &= writestr( ok, ah, "AVI "        , NULL               );   // RIFF type
  ok &= writestr( ok, ah, "LIST"        , NULL               );   // Header list
  ok &= write32l( ok, ah,              0, &ah->offs_hdrlsize );   // Header list size
  ok &= writestr( ok, ah, "hdrl"        , NULL               );   // Header list identifier

  ok &= writestr( ok, ah, "avih"        , NULL               );   // MainAVIHeader chunk
  ok &= write32l( ok, ah,             56, NULL               );   // Chunk size
  ok &= write32l( ok, ah,is50hz?20000:16667,&ah->offs_usperfrm);  // Microseconds per frame
  ok &= write32l( ok, ah,              0, NULL               );   // Max bytes per second
  ok &= write32l( ok, ah,              0, NULL               );   // Padding granularity
  ok &= write32l( ok, ah,       AVIFLAGS, NULL               );   // Flags
  ok &= write32l( ok, ah,              0, &ah->offs_frames   );   // Number of frames
  ok &= write32l( ok, ah,              0, NULL               );   // Initial frames
  ok &= write32l( ok, ah,  ah->dosnd?2:1, NULL               );   // Stream count
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
  ok &= write32l( ok, ah,is50hz?50000000:60000000,&ah->offs_frmrate);   // Rate
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
    ok &= writebyt( ok, ah, pal[i*3+2] );
    ok &= writebyt( ok, ah, pal[i*3+1] );
    ok &= writebyt( ok, ah, pal[i*3+0] );
    ok &= writebyt( ok, ah, 0 );
  }

  if( ah->dosnd )
  {
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
  }

  ah->hdrlsize = ah->csize - ah->offs_hdrlsize - 4;

  ok &= writestr( ok, ah, "LIST"      , NULL               );   // movi list
  ok &= write32l( ok, ah,            0, &ah->offs_movisize );   // Size
  ok &= writestr( ok, ah, "movi"      , NULL               );   // identifier

  if( !ok )
  {
    free( ah );
    return NULL;
  }

  ah->frames         = 0;
  ah->frameadjust    = 0;
  ah->audiolen       = 0;
  ah->movisize       = 0;
  ah->lastframevalid = SDL_FALSE;
  ah->time_start     = 0;

  return ah;
}

static Uint32 rle_putpixels( struct avi_handle *ah, Uint8 *srcdata, Uint32 rlec, Uint32 srcc, Uint32 length )
{
  Uint32 c, chunker;

  if( length == 0 ) return rlec;

  while( length > 254 )
  {
    // Try not to leave an annoying dangly 1,2 or 3 byte section
    chunker = (length<258) ? 250 : 254;
    rlec = rle_putpixels( ah, srcdata, rlec, srcc, chunker );
    srcc   += chunker;
    length -= chunker;
  }

  // Somehow "chunker" failed :-(
  if( length < 3 )
  {
    while( length > 0 )
    {
      ah->rledata[rlec++] = 0x01;
      ah->rledata[rlec++] = srcdata[srcc++];
      length--;
    }
    return rlec;
  }

  c = length;

  ah->rledata[rlec++] = 0x00;
  ah->rledata[rlec++] = length;
  while( c > 0 )
  {
    ah->rledata[rlec++] = srcdata[srcc++];
    c--;
  }

  if( length & 1 )
    ah->rledata[rlec++] = 0;

  return rlec;
}

// RLE encode the frame
#define NOABS 0xffffffff
static Uint32 rle_encode( struct avi_handle *ah, Uint8 *srcdata, Uint32 rlec, Uint32 eol )
{
  Uint32 srcc, runc, abspos, chunker;
  Uint8 thiscol;

  if( eol == 0 )
  {
    ah->rledata[rlec++] = 0x00;
    ah->rledata[rlec++] = 0x00;
    return rlec;
  }

  srcc = 0; // Source count
  abspos = NOABS;
  while( srcc < eol )
  {
    // Look for a run
    thiscol = srcdata[srcc];

    // At the last pixel?
    if( srcc == eol-1 )
    {
      // Need to dump an absolute section?
      if( abspos != NOABS ) rlec = rle_putpixels( ah, srcdata, rlec, abspos, srcc-abspos );

      // Just encode it
      ah->rledata[rlec++] = 0x01;
      ah->rledata[rlec++] = thiscol;

      ah->rledata[rlec++] = 0x00; // The end!
      ah->rledata[rlec++] = 0x00;
      return rlec;
    }

    runc = 1; // Set the run count to 1
    while( srcdata[srcc+runc] == thiscol )
    {
      runc++;
      if( (srcc+runc) >= eol ) break;
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
      rlec = rle_putpixels( ah, srcdata, rlec, abspos, srcc-abspos );
      abspos = NOABS;
    }

    // Encode that run!
    srcc += runc;   // Skip past the run

    while( runc > 255 )  // Do any 255 byte chunks until runc < 255
    {
      chunker = (runc<258) ? 250 : 255;
      ah->rledata[rlec++] = chunker;
      ah->rledata[rlec++] = thiscol;
      runc -= chunker;
    }

    ah->rledata[rlec++] = runc;   // Encode the bastard!
    ah->rledata[rlec++] = thiscol;
  }

  if( abspos != NOABS ) rlec = rle_putpixels( ah, srcdata, rlec, abspos, srcc-abspos );
  ah->rledata[rlec++] = 0x00; // The end!
  ah->rledata[rlec++] = 0x00;
  return rlec;
}

SDL_bool avi_addframe( struct avi_handle **ah, Uint8 *srcdata )
{
  SDL_bool ok;
  Sint32 i, x, lastline, lastx;
  Uint32 size;

  lastline = 0;
  if( (*ah)->lastframevalid )
  {
    // Find the last line that differs
    for( i=0; i<224; i++ )
    {
      for( x=0; x<240; x++ )
      {
        if( (*ah)->lastframe[i*240+x] != srcdata[i*240+x] )
          break;
      }
      if( x<240 ) break;
    }
    lastline = i;
  }

  if( lastline == 224 )
  {
    size = 0;
    (*ah)->rledata[size++] = 0x00;
    (*ah)->rledata[size++] = 0x01;
  } else {
    for( i=223,size=0; i>=lastline; i-- )
    {
      lastx = 240;
      if( (*ah)->lastframevalid )
      {
        // Find the last pixel we need to encode
        for( x=239; x>=0; x-- )
        {
          if( (*ah)->lastframe[i*240+x] != srcdata[i*240+x] )
            break;
        }
        lastx = x+1;
      }
      size = rle_encode( *ah, &srcdata[i*240], size, lastx );
    }
    (*ah)->rledata[size-1] = 0x01;
  }

  memcpy( (*ah)->lastframe, srcdata, 240*224 );
  (*ah)->lastframevalid = SDL_TRUE;

  ok = SDL_TRUE;
  ok &= writestr( ok, *ah, "00dc", NULL );  // Chunk header
  ok &= write32l( ok, *ah, size, NULL );    // Chunk size
  ok &= writeblk( ok, *ah, (*ah)->rledata, size ); // Chunk data

  (*ah)->movisize += size + 8;
  (*ah)->frames++;

  if( !ok )
  {
    avi_close( ah );
    return SDL_FALSE;
  }

  if( !(*ah)->time_start )
  {
    (*ah)->time_start = SDL_GetTicks();
  }

  return SDL_TRUE;
}

SDL_bool avi_addaudio( struct avi_handle **ah, Sint16 *audiodata, Uint32 audiosize )
{
  SDL_bool ok;

  if( !(*ah)->dosnd ) return SDL_TRUE;

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
  Uint32 time_end = SDL_GetTicks();
  double time_ms, rate, usperfrm;

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

    time_ms = (double)(time_end - (*ah)->time_start);  // Time in milliseconds

    rate     = ((double)((*ah)->frames) * 1000000000.0f) / time_ms;
    usperfrm = (time_ms*1000.0f) / ((double)(*ah)->frames); 

    ok &= seek_write32l( ok, *ah, (*ah)->offs_frmrate,  (Uint32)round(rate) );
    ok &= seek_write32l( ok, *ah, (*ah)->offs_usperfrm, (Uint32)round(usperfrm) );

    fclose( (*ah)->f );
  }
  free( (*ah) );
  *ah = NULL;
}

