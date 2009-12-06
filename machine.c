/*
**  Oriculator
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
**  Oric machine stuff
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <SDL.h>

#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "machine.h"
#include "monitor.h"

extern SDL_Surface *screen;
extern int pixpitch;
SDL_bool needclr = SDL_TRUE;
extern Uint16 gpal[];
extern SDL_bool showfps, warpspeed, soundavailable, soundon;
extern volatile Sint32 currsndpos, rendsndpos, nextsndpos;

extern struct osdmenuitem hwopitems[];

unsigned char rom_microdisc[8912], rom_jasmin[2048];
SDL_bool microdiscrom_valid, jasminrom_valid;

// Switch between emulation/monitor/menus etc.
void setemumode( struct machine *oric, struct osdmenuitem *mitem, int mode )
{
  oric->emu_mode = mode;

  switch( mode )
  {
    case EM_RUNNING:
      SDL_EnableKeyRepeat( 0, 0 );
      SDL_EnableUNICODE( SDL_FALSE );
      rendsndpos = 0;
      nextsndpos = 0;
      oric->ay.soundon = soundavailable && soundon && (!warpspeed);
      if( oric->ay.soundon )
        SDL_PauseAudio( 0 );
      break;

    case EM_MENU:
      gotomenu( oric, NULL, 0 );

    case EM_DEBUG:
      mon_enter( oric );
      SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );
      SDL_EnableUNICODE( SDL_TRUE );
      oric->ay.soundon = SDL_FALSE;
      if( soundavailable )
        SDL_PauseAudio( 1 );
      break;
  }
}

// Refresh the video base pointer
static inline void video_refresh_charset( struct machine *oric )
{
  if( oric->vid_textattrs & 1 )
  {
    oric->vid_ch_data = &oric->vid_ch_base[128*8];
  } else {
    oric->vid_ch_data = oric->vid_ch_base;
  }
}

// Decode an oric video attribute
void video_decode_attr( struct machine *oric, int attr )
{
  switch( attr & 0x18 )
  {
    case 0x00: // Foreground colour
      oric->vid_fg_col = attr & 0x07;
      break;

    case 0x08: // Text attribute
      oric->vid_textattrs = attr & 0x07;
      oric->vid_blinkmask = attr & 0x04 ? 0x00 : 0x3f;
      video_refresh_charset( oric );
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

      video_refresh_charset( oric );
      break;
  }   
}

// Render a 6x1 block
void video_render_block( struct machine *oric, int fg, int bg, SDL_bool inverted, int data )
{
  int i;

  if( inverted )
  {
    fg ^= 0x07;
    bg ^= 0x07;
  }
  
  for( i=0; i<6; i++ )
  {
    *(oric->scrpt++) = oric->pal[ (data & 0x20) ? fg : bg ];
    data <<= 1;
  }
}

// Copy the video output buffer to the SDL surface
void video_show( struct machine *oric )
{
  int x, y;
  Uint16 *sptr, *dptr;

  if( !oric->scr )
    return;

  if( ( oric->vid_double ) && ( oric->emu_mode != EM_DEBUG ) )
  {
    if( needclr )
    {
      dptr = (Uint16 *)screen->pixels;
      for( y=0; y<480; y++ )
      {
        for( x=0; x<640; x++ )
          *(dptr++) = gpal[4];
      }
      needclr = SDL_FALSE;
    }
    sptr = oric->scr;
    dptr = &((Uint16 *)screen->pixels)[320-240+pixpitch*(240-224)];
    for( y=0; y<224; y++ )
    {
      for( x=0; x<240; x++ )
      {
        *(dptr++) = *sptr;
        *(dptr++) = *(sptr++);
      }
      dptr += pixpitch-480;
      sptr -= 240;
      for( x=0; x<240; x++ )
      {
        *(dptr++) = *sptr;
        *(dptr++) = *(sptr++);
      }
      dptr += pixpitch-480;
    }
    return;
  }

  needclr = SDL_TRUE;

  sptr = oric->scr;
  dptr = (Uint16 *)screen->pixels;

  for( y=0; y<4; y++ )
  {
    memset( dptr, 0, 480 );
    dptr += pixpitch;
  }
  for( ; y<228; y++ )
  {
    memcpy( dptr, sptr, 240*2 );
    sptr += 240;
    dptr += pixpitch;
  }
}

// Draw one rasterline
SDL_bool video_doraster( struct machine *oric )
{
  int b, c, bitmask;
  SDL_bool hires, needrender;
  unsigned long read_addr, y, cy;

  needrender = SDL_FALSE;

  oric->vid_raster++;
  if( oric->vid_raster == oric->vid_maxrast )
  {
    oric->vid_raster = 0;
    needrender = SDL_TRUE;
    oric->frames++;
  }

  // Are we on a visible rasterline?
  if( ( oric->vid_raster < oric->vid_start ) ||
      ( oric->vid_raster >= oric->vid_end ) ) return needrender;

  y = oric->vid_raster - oric->vid_start;

  oric->scrpt = &oric->scr[y*240];
  
  cy = ((oric->vid_raster - oric->vid_start)>>3) * 40;

  // Always start each scanline with white on black
  video_decode_attr( oric, 0x07 );
  video_decode_attr( oric, 0x08 );
  video_decode_attr( oric, 0x10 );

//  if( oric->vid_raster == oric->vid_special )
//    oric->vid_addr = 0xbf68;
  
  for( b=0; b<40; b++ )
  {
    if( oric->vid_raster < oric->vid_special )
    {
      if( oric->vid_mode & 0x04 ) // HIRES?
      {
        hires = SDL_TRUE;
        read_addr = oric->vid_addr + b + y*40;
      } else {
        hires = SDL_FALSE;
        read_addr = oric->vid_addr + b + cy;
      }
    } else {
      hires = SDL_FALSE;

//      read_addr = oric->vid_addr + b + ((y-200)>>3);
      read_addr = oric->vidbases[2] + b + cy;   // bb80 = bf68 - (200/8*40)
    }
    
    c = oric->mem[ read_addr ];

    /* if bits 6 and 5 are zero, the byte contains a serial attribute */
    if( ( c & 0x60 ) == 0 )
    {
      video_decode_attr( oric, c );
      video_render_block( oric, oric->vid_fg_col, oric->vid_bg_col, c & 0x80, 0 );
      if( oric->vid_raster < oric->vid_special )
      {
        if( oric->vid_mode & 0x04 )
          hires = SDL_TRUE;
        else
          hires = SDL_FALSE;
      }
    } else {
      bitmask = (oric->frames&0x10)?0x3f:oric->vid_blinkmask;
      if( hires )
      {
        video_render_block( oric, oric->vid_fg_col, oric->vid_bg_col, c & 0x80, c & bitmask );
      } else {
        int ch_ix, ch_dat, ch_line;
          
        ch_ix   = c & 0x7f;
        if( oric->vid_textattrs & 0x02 )
          ch_line = (y>>1) & 0x07;
        else
          ch_line = y & 0x07;
          
        ch_dat = oric->vid_ch_data[ (ch_ix<<3) | ch_line ] & bitmask;
        
        video_render_block( oric, oric->vid_fg_col, oric->vid_bg_col, c & 0x80, ch_dat  );
      }
    }
  }

  return needrender;
}

// Oric Atmos CPU write
void atmoswrite( struct m6502 *cpu, unsigned short addr, unsigned char data )
{
  struct machine *oric = (struct machine *)cpu->userdata;
  if( ( !oric->romdis ) && ( addr >= 0xc000 ) ) return;  // Can't write to ROM!
  if( ( addr & 0xff00 ) == 0x0300 )
  {
    via_write( &oric->via, addr, data );
    return;
  }
  oric->mem[addr] = data;
}

// 16k oric-1 CPU write
void o16kwrite( struct m6502 *cpu, unsigned short addr, unsigned char data )
{
  struct machine *oric = (struct machine *)cpu->userdata;
  if( ( !oric->romdis ) && ( addr >= 0xc000 ) ) return;  // Can't write to ROM!
  if( ( addr & 0xff00 ) == 0x0300 )
  {
    via_write( &oric->via, addr, data );
    return;
  }

  oric->mem[addr&0x3fff] = data;
}

// Atmos + jasmin
void jasmin_atmoswrite( struct m6502 *cpu, unsigned short addr, unsigned char data )
{
  struct machine *oric = (struct machine *)cpu->userdata;
  if( oric->romdis )
  {
    if( addr >= 0xf800 ) return; // Can't write to ROM!
  } else {
    if( addr >= 0xc000 ) return;
  }

  if( ( addr & 0xff00 ) == 0x0300 )
  {
    via_write( &oric->via, addr, data );
    return;
  }
  oric->mem[addr] = data;
}

// 16k + jasmin
void jasmin_o16kwrite( struct m6502 *cpu, unsigned short addr, unsigned char data )
{
  struct machine *oric = (struct machine *)cpu->userdata;
  if( oric->romdis )
  {
    if( addr >= 0xf800 ) return; // Can't write to ROM!
  } else {
    if( addr >= 0xc000 ) return;
  }

  if( ( addr & 0xff00 ) == 0x0300 )
  {
    via_write( &oric->via, addr, data );
    return;
  }

  oric->mem[addr&0x3fff] = data;
}

// Atmos + microdisc
void microdisc_atmoswrite( struct m6502 *cpu, unsigned short addr, unsigned char data )
{
  struct machine *oric = (struct machine *)cpu->userdata;
  if( oric->romdis )
  {
    if( ( oric->md.diskrom ) && ( addr >= 0xe000 ) ) return; // Can't write to ROM!
  } else {
    if( addr >= 0xc000 ) return;
  }

  if( ( addr & 0xff00 ) == 0x0300 )
  {
    if( ( addr >= 0x310 ) && ( addr < 0x31c ) )
    {
      microdisc_write( &oric->md, addr, data );
    } else {
      via_write( &oric->via, addr, data );
    }
    return;
  }
  oric->mem[addr] = data;
}

// Atmos + microdisc
void microdisc_o16kwrite( struct m6502 *cpu, unsigned short addr, unsigned char data )
{
  struct machine *oric = (struct machine *)cpu->userdata;
  if( oric->romdis )
  {
    if( ( oric->md.diskrom ) && ( addr >= 0xe000 ) ) return; // Can't write to ROM!
  } else {
    if( addr >= 0xc000 ) return;
  }

  if( ( addr & 0xff00 ) == 0x0300 )
  {
    if( ( addr >= 0x310 ) && ( addr < 0x31c ) )
    {
      microdisc_write( &oric->md, addr, data );
    } else {
      via_write( &oric->via, addr, data );
    }
    return;
  }
  oric->mem[addr&0x3fff] = data;
}

// Oric Atmos CPU read
unsigned char atmosread( struct m6502 *cpu, unsigned short addr )
{
  struct machine *oric = (struct machine *)cpu->userdata;

  if( ( addr & 0xff00 ) == 0x0300 )
    return via_read( &oric->via, addr );

  if( ( !oric->romdis ) && ( addr >= 0xc000 ) )
    return oric->rom[addr-0xc000];

  return oric->mem[addr];
}

// Oric-1 16K CPU read
unsigned char o16kread( struct m6502 *cpu, unsigned short addr )
{
  struct machine *oric = (struct machine *)cpu->userdata;

  if( ( addr & 0xff00 ) == 0x0300 )
    return via_read( &oric->via, addr );

  if( ( !oric->romdis ) && ( addr >= 0xc000 ) )
    return oric->rom[addr-0xc000];

  return oric->mem[addr&0x3fff];
}

// Atmos + jasmin
unsigned char jasmin_atmosread( struct m6502 *cpu, unsigned short addr )
{
  struct machine *oric = (struct machine *)cpu->userdata;

  if( oric->romdis )
  {
    if( addr >= 0xf800 )
      return rom_jasmin[addr-0xf800];
  } else {
    if( addr >= 0xc000 )
      return oric->rom[addr-0xc000];
  }

  if( ( addr & 0xff00 ) == 0x0300 )
    return via_read( &oric->via, addr );

  return oric->mem[addr];
}

// 16k + jasmin
unsigned char jasmin_o16kread( struct m6502 *cpu, unsigned short addr )
{
  struct machine *oric = (struct machine *)cpu->userdata;

  if( oric->romdis )
  {
    if( addr >= 0xf800 )
      return rom_jasmin[addr-0xf800];
  } else {
    if( addr >= 0xc000 )
      return oric->rom[addr-0xc000];
  }

  if( ( addr & 0xff00 ) == 0x0300 )
    return via_read( &oric->via, addr );

  return oric->mem[addr&0x3fff];
}

// Atmos + microdisc
unsigned char microdisc_atmosread( struct m6502 *cpu, unsigned short addr )
{
  struct machine *oric = (struct machine *)cpu->userdata;

  if( oric->romdis )
  {
    if( ( oric->md.diskrom ) && ( addr >= 0xe000 ) )
      return rom_microdisc[addr-0xe000];
  } else {
    if( addr >= 0xc000 )
      return oric->rom[addr-0xc000];
  }

  if( ( addr & 0xff00 ) == 0x0300 )
  {
    if( ( addr >= 0x310 ) && ( addr < 0x31c ) )
      return microdisc_read( &oric->md, addr );

    return via_read( &oric->via, addr );
  }

  return oric->mem[addr];
}

// Atmos + microdisc
unsigned char microdisc_o16kread( struct m6502 *cpu, unsigned short addr )
{
  struct machine *oric = (struct machine *)cpu->userdata;

  if( oric->romdis )
  {
    if( ( oric->md.diskrom ) && ( addr >= 0xe000 ) )
      return rom_microdisc[addr-0xe000];
  } else {
    if( addr >= 0xc000 )
      return oric->rom[addr-0xc000];
  }

  if( ( addr & 0xff00 ) == 0x0300 )
  {
    if( ( addr >= 0x310 ) && ( addr < 0x31c ) )
      return microdisc_read( &oric->md, addr );

    return via_read( &oric->via, addr );
  }

  return oric->mem[addr&0x3fff];
}

static SDL_bool load_rom( char *fname, int size, unsigned char *where )
{
  FILE *f;

  f = fopen( fname, "rb" );
  if( !f )
  {
    printf( "Unable to open '%s'\n", fname );
    return SDL_FALSE;
  }

  if( fread( where, size, 1, f ) != 1 )
  {
    fclose( f );
    printf( "Unable to read '%s'\n", fname );
    return SDL_FALSE;
  }

  fclose( f );
  return SDL_TRUE;
}

void preinit_machine( struct machine *oric )
{
  int i;

  oric->mem = NULL;
  oric->rom = NULL;
  oric->scr = NULL;

  oric->tapebuf = NULL;
  oric->tapelen = 0;
  oric->tapemotor = SDL_FALSE;
  oric->tapeturbo = SDL_TRUE;
  oric->autorewind = SDL_FALSE;
  oric->autoinsert = SDL_TRUE;
  oric->symbolsautoload = SDL_TRUE;
  oric->symbolscase = SDL_FALSE;
  oric->tapename[0] = 0;

  oric->drivetype = DRV_NONE;
  for( i=0; i<MAX_DRIVES; i++ )
  {
    oric->wddisk.disk[i] = NULL;
    oric->diskname[i][0] = 0;
  }

  microdiscrom_valid = load_rom( "roms/microdis.rom", 8192, rom_microdisc );
  jasminrom_valid    = load_rom( "roms/jasmin.rom"  , 2048, rom_jasmin );
}

SDL_bool emu_event( SDL_Event *ev, struct machine *oric, SDL_bool *needrender )
{
//  char stmp[32];
  switch( ev->type )
  {
/*
    case SDL_USEREVENT:
      *needrender = SDL_TRUE;
      break;
*/
    case SDL_MOUSEBUTTONDOWN:
      if( ( ev->button.button == SDL_BUTTON_LEFT ) ||
          ( ev->button.button == SDL_BUTTON_RIGHT ) )
        setemumode( oric, NULL, EM_MENU );
      *needrender = SDL_TRUE;
      break;

    case SDL_KEYUP:
      switch( ev->key.keysym.sym )
      {
        case SDLK_F1:
          setemumode( oric, NULL, EM_MENU );
          *needrender = SDL_TRUE;
          break;
 
        case SDLK_F2:
          setemumode( oric, NULL, EM_DEBUG );
          *needrender = SDL_TRUE;
          break;
        
        case SDLK_F5:
          showfps = showfps ? SDL_FALSE : SDL_TRUE;
          if( !showfps ) needclr = SDL_TRUE;
          break;

        case SDLK_F6:
          warpspeed = warpspeed ? SDL_FALSE : SDL_TRUE;
          if( soundavailable && soundon )
          {
            oric->ay.soundon = !warpspeed;
            SDL_PauseAudio( warpspeed );
          }
          break;

        default:
          ay_keypress( &oric->ay, ev->key.keysym.sym, SDL_FALSE );
          break;
      }
      break;

    case SDL_KEYDOWN:
      ay_keypress( &oric->ay, ev->key.keysym.sym, SDL_TRUE );
      break;
  }

  return SDL_FALSE;
}

SDL_bool init_machine( struct machine *oric, int type )
{
  int i;

  oric->type = type;
  m6502_init( &oric->cpu, (void*)oric );

  oric->vidbases[0] = 0xa000;
  oric->vidbases[1] = 0x9800;
  oric->vidbases[2] = 0xbb80;
  oric->vidbases[3] = 0xb400;

  switch( type )
  {
    case MACH_ORIC1_16K:
      for( i=0; i<4; i++ )
        oric->vidbases[i] &= 0x7fff;
      hwopitems[0].name = " Oric-1";
      hwopitems[1].name = "\x0e""Oric-1 16K";
      hwopitems[2].name = " Atmos";
      hwopitems[3].name = " Telestrat";
      oric->mem = malloc( 16384 + 16384 );
      if( !oric->mem )
      {
        printf( "Out of memory\n" );
        return SDL_FALSE;
      }
      
      memset( oric->mem, 0, 16384+16384 );

      oric->rom = &oric->mem[16384];

      switch( oric->drivetype )
      {
        case DRV_MICRODISC:
          oric->cpu.read = microdisc_o16kread;
          oric->cpu.write = microdisc_o16kwrite;
          oric->romdis = SDL_TRUE;
          microdisc_init( &oric->md, &oric->wddisk, oric );
          break;
        
        case DRV_JASMIN:
          oric->cpu.read = jasmin_o16kread;
          oric->cpu.write = jasmin_o16kwrite;
          oric->romdis = SDL_TRUE;
          break;

        default:
          oric->cpu.read = o16kread;
          oric->cpu.write = o16kwrite;
          oric->romdis = SDL_FALSE;
          break;
      }

      if( !load_rom( "roms/basic10.rom", 16384, &oric->rom[0] ) )
        return SDL_FALSE;

      oric->pal[0] = SDL_MapRGB( screen->format, 0x00, 0x00, 0x00 );
      oric->pal[1] = SDL_MapRGB( screen->format, 0xff, 0x00, 0x00 );
      oric->pal[2] = SDL_MapRGB( screen->format, 0x00, 0xff, 0x00 );
      oric->pal[3] = SDL_MapRGB( screen->format, 0xff, 0xff, 0x00 );
      oric->pal[4] = SDL_MapRGB( screen->format, 0x00, 0x00, 0xff );
      oric->pal[5] = SDL_MapRGB( screen->format, 0xff, 0x00, 0xff );
      oric->pal[6] = SDL_MapRGB( screen->format, 0x00, 0xff, 0xff );
      oric->pal[7] = SDL_MapRGB( screen->format, 0xff, 0xff, 0xff );

      oric->scr = (Uint16 *)malloc( 240*224*2 );
      if( !oric->scr ) return SDL_FALSE;

      oric->cyclesperraster = 64;
      oric->vid_start = 65;
      oric->vid_maxrast = 312;
      oric->vid_special = oric->vid_start + 200;
      oric->vid_end     = oric->vid_start + 224;
      oric->vid_raster  = 0;
      video_decode_attr( oric, 0x18 );
      break;
    
    case MACH_ORIC1:
      hwopitems[0].name = "\x0e""Oric-1";
      hwopitems[1].name = " Oric-1 16K";
      hwopitems[2].name = " Atmos";
      hwopitems[3].name = " Telestrat";
      oric->mem = malloc( 65536 + 16384 );
      if( !oric->mem )
      {
        printf( "Out of memory\n" );
        return SDL_FALSE;
      }

      memset( oric->mem, 0, 65536+16384 );

      oric->rom = &oric->mem[65536];

      switch( oric->drivetype )
      {
        case DRV_MICRODISC:
          oric->cpu.read = microdisc_atmosread;
          oric->cpu.write = microdisc_atmoswrite;
          oric->romdis = SDL_TRUE;
          microdisc_init( &oric->md, &oric->wddisk, oric );
          break;
        
        case DRV_JASMIN:
          oric->cpu.read = jasmin_atmosread;
          oric->cpu.write = jasmin_atmoswrite;
          oric->romdis = SDL_TRUE;
          break;

        default:
          oric->cpu.read = atmosread;
          oric->cpu.write = atmoswrite;
          oric->romdis = SDL_FALSE;
          break;
      }

      if( !load_rom( "roms/basic10.rom", 16384, &oric->rom[0] ) )
        return SDL_FALSE;

      oric->pal[0] = SDL_MapRGB( screen->format, 0x00, 0x00, 0x00 );
      oric->pal[1] = SDL_MapRGB( screen->format, 0xff, 0x00, 0x00 );
      oric->pal[2] = SDL_MapRGB( screen->format, 0x00, 0xff, 0x00 );
      oric->pal[3] = SDL_MapRGB( screen->format, 0xff, 0xff, 0x00 );
      oric->pal[4] = SDL_MapRGB( screen->format, 0x00, 0x00, 0xff );
      oric->pal[5] = SDL_MapRGB( screen->format, 0xff, 0x00, 0xff );
      oric->pal[6] = SDL_MapRGB( screen->format, 0x00, 0xff, 0xff );
      oric->pal[7] = SDL_MapRGB( screen->format, 0xff, 0xff, 0xff );

      oric->scr = (Uint16 *)malloc( 240*224*2 );
      if( !oric->scr ) return SDL_FALSE;

      oric->cyclesperraster = 64;
      oric->vid_start = 65;
      oric->vid_maxrast = 312;
      oric->vid_special = oric->vid_start + 200;
      oric->vid_end     = oric->vid_start + 224;
      oric->vid_raster  = 0;
      video_decode_attr( oric, 0x18 );
      break;
    
    case MACH_ATMOS:
      hwopitems[0].name = " Oric-1";
      hwopitems[1].name = " Oric-1 16K";
      hwopitems[2].name = "\x0e""Atmos";
      hwopitems[3].name = " Telestrat";
      oric->mem = malloc( 65536 + 16384 );
      if( !oric->mem )
      {
        printf( "Out of memory\n" );
        return SDL_FALSE;
      }

      memset( oric->mem, 0, 65536+16384 );

      oric->rom = &oric->mem[65536];

      switch( oric->drivetype )
      {
        case DRV_MICRODISC:
          oric->cpu.read = microdisc_atmosread;
          oric->cpu.write = microdisc_atmoswrite;
          oric->romdis = SDL_TRUE;
          microdisc_init( &oric->md, &oric->wddisk, oric );
          break;
        
        case DRV_JASMIN:
          oric->cpu.read = jasmin_atmosread;
          oric->cpu.write = jasmin_atmoswrite;
          oric->romdis = SDL_TRUE;
          break;

        default:
          oric->cpu.read = atmosread;
          oric->cpu.write = atmoswrite;
          oric->romdis = SDL_FALSE;
          break;
      }

      if( !load_rom( "roms/basic11b.rom", 16384, &oric->rom[0] ) )
        return SDL_FALSE;

      oric->pal[0] = SDL_MapRGB( screen->format, 0x00, 0x00, 0x00 );
      oric->pal[1] = SDL_MapRGB( screen->format, 0xff, 0x00, 0x00 );
      oric->pal[2] = SDL_MapRGB( screen->format, 0x00, 0xff, 0x00 );
      oric->pal[3] = SDL_MapRGB( screen->format, 0xff, 0xff, 0x00 );
      oric->pal[4] = SDL_MapRGB( screen->format, 0x00, 0x00, 0xff );
      oric->pal[5] = SDL_MapRGB( screen->format, 0xff, 0x00, 0xff );
      oric->pal[6] = SDL_MapRGB( screen->format, 0x00, 0xff, 0xff );
      oric->pal[7] = SDL_MapRGB( screen->format, 0xff, 0xff, 0xff );

      oric->scr = (Uint16 *)malloc( 240*224*2 );
      if( !oric->scr ) return SDL_FALSE;

      oric->cyclesperraster = 64;
      oric->vid_start = 65;
      oric->vid_maxrast = 312;
      oric->vid_special = oric->vid_start + 200;
      oric->vid_end     = oric->vid_start + 224;
      oric->vid_raster  = 0;
      video_decode_attr( oric, 0x18 );
      break;

    case MACH_TELESTRAT:
      hwopitems[0].name = " Oric-1";
      hwopitems[1].name = " Oric-1 16K";
      hwopitems[2].name = " Atmos";
      hwopitems[3].name = "\x0e""Telestrat";
      printf( "Telestrat not implimented yet\n" );
      return SDL_FALSE;
  }

  oric->tapename[0] = 0;
  tape_rewind( oric );
  m6502_reset( &oric->cpu );
  via_init( &oric->via, oric );
  ay_init( &oric->ay, oric );
  oric->cpu.rastercycles = oric->cyclesperraster;
  oric->frames = 0;
  oric->vid_double = SDL_TRUE;
  setemumode( oric, NULL, EM_RUNNING );
//  setemumode( oric, NULL, EM_DEBUG );

  if( oric->autorewind ) tape_rewind( oric );

  setmenutoggles( oric );

  return SDL_TRUE;
}

void shut_machine( struct machine *oric )
{
  if( oric->drivetype == DRV_MICRODISC ) { microdisc_free( &oric->md ); oric->drivetype = DRV_NONE; }
  if( oric->mem ) { free( oric->mem ); oric->mem = NULL; oric->rom = NULL; }
  if( oric->scr ) { free( oric->scr ); oric->scr = NULL; }
}

void shut( void );
void setdrivetype( struct machine *oric, struct osdmenuitem *mitem, int type )
{
  if( oric->drivetype == type )
    return;

  shut_machine( oric );

  switch( type )
  {
    case DRV_MICRODISC:
    case DRV_JASMIN:
      oric->drivetype = type;
      break;
    
    default:
      oric->drivetype = DRV_NONE;
      break;
  }

  if( !init_machine( oric, oric->type ) )
  {
    shut();
    exit(0);
  }

  setmenutoggles( oric );
}

void swapmach( struct machine *oric, struct osdmenuitem *mitem, int which )
{
  int curr_drivetype;
  
  curr_drivetype = oric->drivetype;

  shut_machine( oric );

  if( ((which>>16)&0xffff) != 0xffff )
    curr_drivetype = (which>>16)&0xffff;

  oric->drivetype = curr_drivetype;

  if( !init_machine( oric, which&0xffff ) )
  {
    shut();
    exit(0);
  }
}

