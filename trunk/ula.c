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
**  Oric video ULA
*/

#include <stdlib.h>

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "disk.h"
#include "gui.h"
#include "monitor.h"
#include "6551.h"
#include "machine.h"
#include "ula.h"
#include "avi.h"

extern struct avi_handle *vidcap;
extern SDL_bool warpspeed;

// Refresh the video base pointer
static inline void ula_refresh_charset( struct machine *oric )
{
  if( oric->vid_textattrs & 1 )
  {
    oric->vid_ch_data = &oric->vid_ch_base[128*8];
  } else {
    oric->vid_ch_data = oric->vid_ch_base;
  }
}

// Decode an oric video attribute
void ula_decode_attr( struct machine *oric, int attr )
{
  switch( attr & 0x18 )
  {
    case 0x00: // Foreground colour
      oric->vid_fg_col = attr & 0x07;
      break;

    case 0x08: // Text attribute
      oric->vid_textattrs = attr & 0x07;
      oric->vid_blinkmask = attr & 0x04 ? 0x00 : 0x3f;
      ula_refresh_charset( oric );
      break;

    case 0x10: // Background colour
      oric->vid_bg_col = attr & 0x07;
      break;

    case 0x18: // Video mode
      oric->vid_mode = attr & 0x07;
      
      // Set up pointers for new mode
      if( oric->vid_mode & 4 )
      {
        oric->vid_addr = oric->vidbases[0];
        oric->vid_ch_base = &oric->mem[oric->vidbases[1]];
      } else {
        oric->vid_addr = oric->vidbases[2];
        oric->vid_ch_base = &oric->mem[oric->vidbases[3]];
      }

      ula_refresh_charset( oric );
      break;
  }   
}

// Render a 6x1 block
void ula_render_block( struct machine *oric, int fg, int bg, SDL_bool inverted, int data )
{
  int i;

  if( inverted )
  {
    fg ^= 0x07;
    bg ^= 0x07;
  }
  
  for( i=0; i<6; i++ )
  {
    *(oric->scrpt++) = /*oric->pal[*/ (data & 0x20) ? fg : bg /*]*/;
    data <<= 1;
  }
}

// Render current screen (used by the monitor)
void ula_renderscreen( struct machine *oric )
{
  Uint8 *scrpt, *rptr;
  int b, y, c, cy, bitmask;
  SDL_bool hires;

  scrpt = oric->scrpt;
  oric->scrpt = oric->scr;
  for( y=0; y<224; y++)
  {
    cy = (y>>3) * 40;

    // Always start each scanline with white on black
    ula_decode_attr( oric, 0x07 );
    ula_decode_attr( oric, 0x08 );
    ula_decode_attr( oric, 0x10 );

    if( y < 200 )
    {
      if( oric->vid_mode & 0x04 ) // HIRES?
      {
        hires = SDL_TRUE;
        rptr = &oric->mem[oric->vid_addr + y*40 -1];
      } else {
        hires = SDL_FALSE;
        rptr = &oric->mem[oric->vid_addr + cy -1];
      }
    } else {
      hires = SDL_FALSE;

      rptr = &oric->mem[oric->vidbases[2] + cy -1];  // bb80 = bf68 - (200/8*40)
    }
    bitmask = (oric->frames&0x10)?0x3f:oric->vid_blinkmask;
    
    for( b=0; b<40; b++ )
    {
      c = *(++rptr);

      /* if bits 6 and 5 are zero, the byte contains a serial attribute */
      if( ( c & 0x60 ) == 0 )
      {
        ula_decode_attr( oric, c );
        ula_render_block( oric, oric->vid_fg_col, oric->vid_bg_col, (c & 0x80)!=0, 0 );
        if( oric->vid_raster < oric->vid_special )
        {
          if( oric->vid_mode & 0x04 ) // HIRES?
          {
            hires = SDL_TRUE;
            rptr = &oric->mem[oric->vid_addr + b + y*40];
          } else {
            hires = SDL_FALSE;
            rptr = &oric->mem[oric->vid_addr + b + cy];
          }
        } else {
          hires = SDL_FALSE;

          rptr = &oric->mem[oric->vidbases[2] + b + cy];   // bb80 = bf68 - (200/8*40)
        }
        bitmask = (oric->frames&0x10)?0x3f:oric->vid_blinkmask;
    
      } else {
        if( hires )
        {
          ula_render_block( oric, oric->vid_fg_col, oric->vid_bg_col, (c & 0x80)!=0, c & bitmask );
        } else {
          int ch_ix, ch_dat, ch_line;
          
          ch_ix   = c & 0x7f;
          if( oric->vid_textattrs & 0x02 )
            ch_line = (y>>1) & 0x07;
          else
            ch_line = y & 0x07;
          
          ch_dat = oric->vid_ch_data[ (ch_ix<<3) | ch_line ] & bitmask;
        
          ula_render_block( oric, oric->vid_fg_col, oric->vid_bg_col, (c & 0x80)!=0, ch_dat  );
        }
      }
    }
  }
  oric->scrpt = scrpt;
}

// Draw one rasterline
SDL_bool ula_doraster( struct machine *oric )
{
  int b, c, bitmask;
  SDL_bool hires, needrender;
  unsigned int y, cy;
  Uint8 *rptr;

  needrender = SDL_FALSE;

  oric->vid_raster++;
  if( oric->vid_raster == oric->vid_maxrast )
  {
    if( vidcap )
    {
      // If we're recording with sound, and they do warp speed,
      // stop writing frames to the AVI, since it'll just get out of sync.
      if ( ( !vidcap->dosnd ) || ( !warpspeed ) )
      {
        // If the oric refresh rate and AVI refresh rate match, just output every frame
        if( vidcap->is50hz == oric->vid_freq )
        {
          ay_lockaudio( &oric->ay ); // Gets unlocked at the end of each frame
          avi_addframe( &vidcap, oric->scr );
        }
        // Check for 60hz oric & 50 hz AVI
        else if( vidcap->is50hz )
        {
          // In this case we need to throw away every sixth frame
          if( (vidcap->frameadjust%6) != 5 )
          {
            ay_lockaudio( &oric->ay ); // Gets unlocked at the end of each frame
            avi_addframe( &vidcap, oric->scr );
          }

          vidcap->frameadjust++;
        }
        // Must be 50hz oric & 60 hz AVI
        else
        {
          // In this case we need to duplicate every fifth frame
          ay_lockaudio( &oric->ay ); // Gets unlocked at the end of each frame
          avi_addframe( &vidcap, oric->scr );

          if( (vidcap->frameadjust%5) == 4 )
            avi_addframe( &vidcap, oric->scr );

          vidcap->frameadjust++;
        }
      }
    }

    oric->vid_raster = 0;
    oric->vsync      = oric->cyclesperraster / 2;
    needrender = SDL_TRUE;
    oric->frames++;

    if( oric->vid_freq != (oric->vid_mode&2) )
    {
      oric->vid_freq = oric->vid_mode&2;

      // PAL50 = 50Hz = 1,000,000/50 = 20000 cpu cycles/frame
      // 312 scanlines/frame, so 20000/312 = ~64 cpu cycles / scanline

      // PAL60 = 60Hz = 1,000,000/60 = 16667 cpu cycles/frame
      // 312 scanlines/frame, so 16667/312 = ~53 cpu cycles / scanline

      // NTSC = 60Hz = 1,000,000/60 = 16667 cpu cycles/frame
      // 262 scanlines/frame, so 16667/262 = ~64 cpu cycles / scanline
      if( oric->vid_freq )
      {
        // PAL50
        oric->cyclesperraster = 64;
        oric->vid_start       = 65;
        oric->vid_maxrast     = 312;
        oric->vid_special     = oric->vid_start + 200;
        oric->vid_end         = oric->vid_start + 224;
      } else {
        // NTSC
        oric->cyclesperraster = 64;
        oric->vid_start       = 32;
        oric->vid_maxrast     = 262;
        oric->vid_special     = oric->vid_start + 200;
        oric->vid_end         = oric->vid_start + 224;
      }
    }
  }

  // Are we on a visible rasterline?
  if( ( oric->vid_raster < oric->vid_start ) ||
      ( oric->vid_raster >= oric->vid_end ) ) return needrender;

  y = oric->vid_raster - oric->vid_start;

  oric->scrpt = &oric->scr[y*240];
  
  cy = (y>>3) * 40;

  // Always start each scanline with white on black
  ula_decode_attr( oric, 0x07 );
  ula_decode_attr( oric, 0x08 );
  ula_decode_attr( oric, 0x10 );

  if( oric->vid_raster < oric->vid_special )
  {
    if( oric->vid_mode & 0x04 ) // HIRES?
    {
      hires = SDL_TRUE;
      rptr = &oric->mem[oric->vid_addr + y*40 -1];
    } else {
      hires = SDL_FALSE;
      rptr = &oric->mem[oric->vid_addr + cy -1];
    }
  } else {
    hires = SDL_FALSE;

    rptr = &oric->mem[oric->vidbases[2] + cy -1];  // bb80 = bf68 - (200/8*40)
  }
  bitmask = (oric->frames&0x10)?0x3f:oric->vid_blinkmask;
    
  for( b=0; b<40; b++ )
  {
    c = *(++rptr);

    /* if bits 6 and 5 are zero, the byte contains a serial attribute */
    if( ( c & 0x60 ) == 0 )
    {
      ula_decode_attr( oric, c );
      ula_render_block( oric, oric->vid_fg_col, oric->vid_bg_col, (c & 0x80)!=0, 0 );
      if( oric->vid_raster < oric->vid_special )
      {
        if( oric->vid_mode & 0x04 ) // HIRES?
        {
          hires = SDL_TRUE;
          rptr = &oric->mem[oric->vid_addr + b + y*40];
        } else {
          hires = SDL_FALSE;
          rptr = &oric->mem[oric->vid_addr + b + cy];
        }
      } else {
        hires = SDL_FALSE;

        rptr = &oric->mem[oric->vidbases[2] + b + cy];   // bb80 = bf68 - (200/8*40)
      }
      bitmask = (oric->frames&0x10)?0x3f:oric->vid_blinkmask;
    
    } else {
      if( hires )
      {
        ula_render_block( oric, oric->vid_fg_col, oric->vid_bg_col, (c & 0x80)!=0, c & bitmask );
      } else {
        int ch_ix, ch_dat, ch_line;
          
        ch_ix   = c & 0x7f;
        if( oric->vid_textattrs & 0x02 )
          ch_line = (y>>1) & 0x07;
        else
          ch_line = y & 0x07;
          
        ch_dat = oric->vid_ch_data[ (ch_ix<<3) | ch_line ] & bitmask;
        
        ula_render_block( oric, oric->vid_fg_col, oric->vid_bg_col, (c & 0x80)!=0, ch_dat  );
      }
    }
  }

  return needrender;
}

void preinit_ula( struct machine *oric )
{
  oric->scr = NULL;
  oric->hstretch = SDL_TRUE;
  oric->scanlines = SDL_FALSE;
}

SDL_bool init_ula( struct machine *oric )
{
  oric->scr = (Uint8 *)malloc( 240*224 );
  if( !oric->scr ) return SDL_FALSE;

  return SDL_TRUE;
}

void shut_ula( struct machine *oric )
{
  if( oric->scr ) free( oric->scr );
  oric->scr = NULL;
}
