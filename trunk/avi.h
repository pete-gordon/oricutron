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

#define 	AVIF_HASINDEX         0x00000010
#define 	AVIF_MUSTUSEINDEX     0x00000020
#define 	AVIF_ISINTERLEAVED    0x00000100
#define 	AVIF_TRUSTCKTYPE      0x00000800
#define 	AVIF_WASCAPTUREFILE   0x00010000
#define 	AVIF_COPYRIGHTED      0x00020000

struct avi_handle
{
  FILE     *f;
  Uint32   csize;
  Uint32   frames;
  Uint32   hdrlsize;
  Uint32   movisize;
  Uint32   audiolen;
  Uint32   offs_riffsize;
  Uint32   offs_hdrlsize;
  Uint32   offs_frames;
  Uint32   offs_frames2;
  Uint32   offs_movisize;
  Uint32   offs_audiolen;
};

struct avi_handle *avi_open( char *filename, Uint8 *pal );
SDL_bool avi_addframe( struct avi_handle **ah, Uint8 *srcdata );
SDL_bool avi_addaudio( struct avi_handle **ah, Sint16 *audiodata, Uint32 audiosize );
void avi_close( struct avi_handle **ah );

