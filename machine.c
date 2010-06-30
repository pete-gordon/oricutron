/*
**  Oricutron
**  Copyright (C) 2009-2010 Peter Gordon
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

#if defined(__amigaos4__) || defined(__MORPHOS__)
#include <proto/dos.h>
#include <dos/dostags.h>
#endif

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "machine.h"
#include "avi.h"
#include "filereq.h"

extern SDL_Surface *screen;
extern int pixpitch;
extern Uint16 gpal[];
extern SDL_bool showfps, warpspeed, soundavailable, soundon;
extern char diskpath[], diskfile[], filetmp[];
extern char telediskpath[], telediskfile[];

char atmosromfile[1024];
char oric1romfile[1024];
char mdiscromfile[1024];
char jasmnromfile[1024];
char telebankfiles[8][1024];

SDL_bool needclr = SDL_TRUE, refreshstatus = SDL_TRUE, refreshdisks = SDL_TRUE, refreshavi = SDL_TRUE, refreshtape = SDL_TRUE;

struct avi_handle *vidcap = NULL;
char vidcapname[128];
int vidcapcount = 0;

extern struct osdmenuitem hwopitems[];

unsigned char rom_microdisc[8912], rom_jasmin[2048];
struct symboltable sym_microdisc, sym_jasmin;
SDL_bool microdiscrom_valid, jasminrom_valid;

Uint8 oricpalette[] = { 0x00, 0x00, 0x00,
                        0xff, 0x00, 0x00,
                        0x00, 0xff, 0x00,
                        0xff, 0xff, 0x00,
                        0x00, 0x00, 0xff,
                        0xff, 0x00, 0xff,
                        0x00, 0xff, 0xff,
                        0xff, 0xff, 0xff };

// Switch between emulation/monitor/menus etc.
void setemumode( struct machine *oric, struct osdmenuitem *mitem, int mode )
{
  oric->emu_mode = mode;

  switch( mode )
  {
    case EM_RUNNING:
      SDL_EnableKeyRepeat( 0, 0 );
      SDL_EnableUNICODE( SDL_FALSE );
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
    *(oric->scrpt++) = /*oric->pal[*/ (data & 0x20) ? fg : bg /*]*/;
    data <<= 1;
  }
}

// Copy the video output buffer to the SDL surface
void video_show( struct machine *oric )
{
  int x, y;
  Uint8 *sptr;
  Uint16 *dptr;
  Uint32 *dptr2, *dptr3, c, pp2;

  if( !oric->scr )
    return;
    
  pp2 = pixpitch/2;

  if( ( oric->vid_double ) && ( oric->emu_mode != EM_DEBUG ) )
  {
    if( needclr )
    {
      refreshstatus = SDL_TRUE;
      dptr = (Uint16 *)screen->pixels;
      for( y=0; y<480; y++ )
      {
        for( x=0; x<640; x++ )
          *(dptr++) = gpal[4];
        dptr += pixpitch-640;
      }
      needclr = SDL_FALSE;
    }

    if( refreshstatus )
      draw_statusbar();

    if( refreshdisks || refreshstatus )
    {
      draw_disks( oric );
      refreshdisks = SDL_FALSE;
    }

    if( refreshavi || refreshstatus )
    {
      draw_avirec( vidcap != NULL );
      refreshavi = SDL_FALSE;
    }

    if( refreshtape || refreshstatus )
    {
      draw_tape( oric );
      refreshtape = SDL_FALSE;
    }

    refreshstatus = SDL_FALSE;

    sptr = oric->scr;
    dptr2 = (Uint32 *)(&((Uint16 *)screen->pixels)[320-240+pixpitch*(240-226)]);
    dptr3 = dptr2+pp2;
    for( y=0; y<224; y++ )
    {
      for( x=0; x<240; x++ )
      {
        c = oric->dpal[*(sptr++)];
        *(dptr2++) = c;
        *(dptr3++) = c;
      }
      dptr2 += pixpitch-240;
      dptr3 += pixpitch-240;
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
    for( x=0; x<240; x++ )
      *(dptr++) = oric->pal[*(sptr++)];
    dptr += pixpitch-240;
  }
}

// Draw one rasterline
SDL_bool video_doraster( struct machine *oric )
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
      ay_lockaudio( &oric->ay ); // Gets unlocked at the end of each frame
      avi_addframe( &vidcap, oric->scr );
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
  
  cy = ((oric->vid_raster - oric->vid_start)>>3) * 40;

  // Always start each scanline with white on black
  video_decode_attr( oric, 0x07 );
  video_decode_attr( oric, 0x08 );
  video_decode_attr( oric, 0x10 );

//  if( oric->vid_raster == oric->vid_special )
//    oric->vid_addr = 0xbf68;
  
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

//      read_addr = oric->vid_addr + b + ((y-200)>>3);
    rptr = &oric->mem[oric->vidbases[2] + cy -1];  // bb80 = bf68 - (200/8*40)
  }
  bitmask = (oric->frames&0x10)?0x3f:oric->vid_blinkmask;
    
  for( b=0; b<40; b++ )
  {
    c = *(++rptr);

    /* if bits 6 and 5 are zero, the byte contains a serial attribute */
    if( ( c & 0x60 ) == 0 )
    {
      video_decode_attr( oric, c );
      video_render_block( oric, oric->vid_fg_col, oric->vid_bg_col, (c & 0x80)!=0, 0 );
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

//        read_addr = oric->vid_addr + b + ((y-200)>>3);
        rptr = &oric->mem[oric->vidbases[2] + b + cy];   // bb80 = bf68 - (200/8*40)
      }
      bitmask = (oric->frames&0x10)?0x3f:oric->vid_blinkmask;
    
    } else {
      if( hires )
      {
        video_render_block( oric, oric->vid_fg_col, oric->vid_bg_col, (c & 0x80)!=0, c & bitmask );
      } else {
        int ch_ix, ch_dat, ch_line;
          
        ch_ix   = c & 0x7f;
        if( oric->vid_textattrs & 0x02 )
          ch_line = (y>>1) & 0x07;
        else
          ch_line = y & 0x07;
          
        ch_dat = oric->vid_ch_data[ (ch_ix<<3) | ch_line ] & bitmask;
        
        video_render_block( oric, oric->vid_fg_col, oric->vid_bg_col, (c & 0x80)!=0, ch_dat  );
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

// Oric Telestrat CPU write
void telestratwrite( struct m6502 *cpu, unsigned short addr, unsigned char data )
{
  struct machine *oric = (struct machine *)cpu->userdata;

  if( addr >= 0xc000 )
  {
//    if( oric->romdis )
//    {
//      if( ( oric->md.diskrom ) && ( addr >= 0xe000 ) ) return; // Can't write to ROM!
//    } else {
      switch( oric->tele_banktype )
      {
        case TELEBANK_HALFNHALF:
          if( addr >= 0xe000 ) break;
        case TELEBANK_RAM:
          oric->rom[addr-0xc000] = data;
          break;
      }
      return;
//    }
  }

  if( ( addr & 0xff00 ) == 0x0300 )
  {
    switch( addr & 0x0f0 )
    {
      case 0x20:
        via_write( &oric->tele_via, addr, data );
        break;
      
      case 0x10:
        microdisc_write( &oric->md, addr, data );
        break;
      
      default:
        via_write( &oric->via, addr, data );
        break;
    }
    return;
  }

  oric->mem[addr] = data;
}

// Atmos + jasmin
void jasmin_atmoswrite( struct m6502 *cpu, unsigned short addr, unsigned char data )
{
  struct machine *oric = (struct machine *)cpu->userdata;

  if( oric->jasmin.olay == 0 )
  {
    if( oric->romdis )
    {
      if( addr >= 0xf800 ) return; // Can't write to jasmin rom
    } else {
      if( addr >= 0xc000 ) return; // Can't write to BASIC rom
    }
  }
  
  if( ( addr & 0xff00 ) == 0x0300 )
  {
    if( ( addr >= 0x3f4 ) && ( addr < 0x400 ) )
    {
      jasmin_write( &oric->jasmin, addr, data );
      return;
    }

    via_write( &oric->via, addr, data );
    return;
  }
  oric->mem[addr] = data;
}

// 16k + jasmin
void jasmin_o16kwrite( struct m6502 *cpu, unsigned short addr, unsigned char data )
{
  struct machine *oric = (struct machine *)cpu->userdata;

  if( oric->jasmin.olay == 0 )
  {
    if( oric->romdis )
    {
      if( addr >= 0xf800 ) return; // Can't write to jasmin rom
    } else {
      if( addr >= 0xc000 ) return; // Can't write to BASIC rom
    }
  }

  if( ( addr & 0xff00 ) == 0x0300 )
  {
    if( ( addr >= 0x3f4 ) && ( addr < 0x400 ) )
    {
      jasmin_write( &oric->jasmin, addr, data );
      return;
    }

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

// VIA is returned as RAM since it isn't ROM
SDL_bool isram( struct machine *oric, unsigned short addr )
{
  if( addr < 0xc000 ) return SDL_TRUE;

  if( oric->type == MACH_TELESTRAT )
  {
    return oric->tele_banktype == TELEBANK_RAM;
  }

  if( !oric->romdis ) return SDL_FALSE;
  
  switch( oric->drivetype )
  {
    case DRV_MICRODISC:
      if( !oric->md.diskrom ) return SDL_TRUE;
      if( addr >= 0xe000 ) return SDL_FALSE;
      break;
    
    case DRV_JASMIN:
      if( oric->jasmin.olay ) return SDL_TRUE;
      if( addr >= 0xf800 ) return SDL_FALSE;
      break;
  }
  
  return SDL_TRUE;
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

// Oric Telestrat CPU read
unsigned char telestratread( struct m6502 *cpu, unsigned short addr )
{
  struct machine *oric = (struct machine *)cpu->userdata;

  if( ( addr & 0xff00 ) == 0x0300 )
  {
    switch( addr & 0x0f0 )
    {
      case 0x010:
        return microdisc_read( &oric->md, addr );

      case 0x020:
        return via_read( &oric->tele_via, addr );
    }

    return via_read( &oric->via, addr );
  }

  if( addr >= 0xc000 )
  {
//    if( oric->romdis )
//    {
//      if( ( oric->md.diskrom ) && ( addr >= 0xe000 ) )
//        return rom_microdisc[addr-0xe000];
//    } else {
      return oric->rom[addr-0xc000];
//    }
  }

  return oric->mem[addr];
}

// Atmos + jasmin
unsigned char jasmin_atmosread( struct m6502 *cpu, unsigned short addr )
{
  struct machine *oric = (struct machine *)cpu->userdata;

  if( oric->jasmin.olay == 0 )
  {
    if( oric->romdis )
    {
      if( addr >= 0xf800 ) return rom_jasmin[addr-0xf800];
    } else {
      if( addr >= 0xc000 ) return oric->rom[addr-0xc000];
    }
  }
  
  if( ( addr & 0xff00 ) == 0x0300 )
  {
    if( ( addr >= 0x3f4 ) && ( addr < 0x400 ) )
      return jasmin_read( &oric->jasmin, addr );

    return via_read( &oric->via, addr );
  }

  return oric->mem[addr];
}

// 16k + jasmin
unsigned char jasmin_o16kread( struct m6502 *cpu, unsigned short addr )
{
  struct machine *oric = (struct machine *)cpu->userdata;

  if( oric->jasmin.olay == 0 )
  {
    if( oric->romdis )
    {
      if( addr >= 0xf800 ) return rom_jasmin[addr-0xf800];
    } else {
      if( addr >= 0xc000 ) return oric->rom[addr-0xc000];
    }
  }

  if( ( addr & 0xff00 ) == 0x0300 )
  {
    if( ( addr >= 0x3f4 ) && ( addr < 0x400 ) )
      return jasmin_read( &oric->jasmin, addr );

    return via_read( &oric->via, addr );
  }

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

static SDL_bool load_rom( struct machine *oric, char *fname, int size, unsigned char *where, struct symboltable *stab, int symflags )
{
  FILE *f;
  char *tmpname;

  // MinGW doesn't have asprintf :-(
  tmpname = malloc( strlen( fname ) + 10 );
  if( !tmpname ) return SDL_FALSE;

  sprintf( tmpname, "%s.rom", fname );
  f = fopen( tmpname, "rb" );
  if( !f )
  {
    printf( "Unable to open '%s'\n", tmpname );
    free( tmpname );
    return SDL_FALSE;
  }

  if( size < 0 )
  {
    int filesize;
    fseek( f, 0, SEEK_END );
    filesize = ftell( f );
    fseek( f, 0, SEEK_SET );

    if( filesize > -size )
    {
      printf( "ROM '%s' exceeds %d bytes.\n", fname, -size );
      fclose( f );
      free( tmpname );
      return SDL_FALSE;
    }

    where += (-size)-filesize;
    size = filesize;
  }

  if( fread( where, size, 1, f ) != 1 )
  {
    printf( "Unable to read '%s'\n", tmpname );
    fclose( f );
    free( tmpname );
    return SDL_FALSE;
  }

  fclose( f );

  sprintf( tmpname, "%s.sym", fname );
  mon_new_symbols( stab, oric, tmpname, symflags, SDL_FALSE, SDL_FALSE );
  free( tmpname );

  return SDL_TRUE;
}

void preinit_machine( struct machine *oric )
{
  int i;

  mon_init_symtab( &sym_microdisc );
  mon_init_symtab( &sym_jasmin );
  mon_init_symtab( &oric->romsyms );
  mon_init_symtab( &oric->tele_banksyms[0] );
  mon_init_symtab( &oric->tele_banksyms[1] );
  mon_init_symtab( &oric->tele_banksyms[2] );
  mon_init_symtab( &oric->tele_banksyms[3] );
  mon_init_symtab( &oric->tele_banksyms[4] );
  mon_init_symtab( &oric->tele_banksyms[5] );
  mon_init_symtab( &oric->tele_banksyms[6] );
  mon_init_symtab( &oric->tele_banksyms[7] );

  oric->mem = NULL;
  oric->rom = NULL;
  oric->scr = NULL;

  oric->tapebuf = NULL;
  oric->tapelen = 0;
  oric->tapemotor = SDL_FALSE;
  oric->vsynchack = SDL_FALSE;
  oric->tapeturbo = SDL_TRUE;
  oric->autorewind = SDL_FALSE;
  oric->autoinsert = SDL_TRUE;
  oric->symbolsautoload = SDL_TRUE;
  oric->symbolscase = SDL_FALSE;
  oric->tapename[0] = 0;
  oric->prf = NULL;
  oric->prclock = 0;
  oric->prclose = 0;
  oric->lasttapefile[0] = 0;
  oric->azerty = SDL_FALSE;

  oric->drivetype = DRV_NONE;
  for( i=0; i<MAX_DRIVES; i++ )
  {
    oric->wddisk.disk[i] = NULL;
    oric->diskname[i][0] = 0;
  }
}

void load_diskroms( struct machine *oric )
{
  microdiscrom_valid = load_rom( oric, mdiscromfile, 8192, rom_microdisc, &sym_microdisc, SYMF_ROMDIS1|SYMF_MICRODISC );
  jasminrom_valid    = load_rom( oric, jasmnromfile, 2048, rom_jasmin,    &sym_jasmin,    SYMF_ROMDIS1|SYMF_JASMIN );
}

static SDL_bool shifted = SDL_FALSE;
SDL_bool emu_event( SDL_Event *ev, struct machine *oric, SDL_bool *needrender )
{
  Sint32 i;

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
        
        case SDLK_F3:
          oric->cpu.nmi = SDL_TRUE;
          oric->cpu.nmicount = 2;
          break;
        
        case SDLK_F4:
          if( ( shifted ) && ( oric->drivetype == DRV_JASMIN ) )
            oric->cpu.write( &oric->cpu, 0x3fb, 1 ); // ROMDIS
          m6502_reset( &oric->cpu );
          via_init( &oric->via, oric, VIA_MAIN );
          via_init( &oric->tele_via, oric, VIA_TELESTRAT );
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
        
        case SDLK_F7:
          if( oric->drivetype == DRV_NONE ) break;

          for( i=0; i<4; i++ )
          {
            if( ( oric->wddisk.disk[i] ) && ( oric->wddisk.disk[i]->modified ) )
            {
              if( shifted )
              {
                char frname[64];
                char *dpath, *dfile;

                if( oric->type != MACH_TELESTRAT )
                {
                  dpath = diskpath;
                  dfile = diskfile;
                } else {
                  dpath = telediskpath;
                  dfile = telediskfile;
                }

                sprintf( frname, "Save disk %d", i );
                if( filerequester( oric, frname, dpath, dfile, FR_DISKSAVE ) )
                {
                  joinpath( dpath, dfile );
                  diskimage_save( oric, filetmp, i );
                }
              } else {
                diskimage_save( oric, oric->wddisk.disk[i]->filename, i );
              }
            }
          }
          break;
        
        case SDLK_F10:
           if( vidcap )
           {
             ay_lockaudio( &oric->ay );
             avi_close( &vidcap );
             ay_unlockaudio( &oric->ay );
             do_popup( "AVI capture stopped" );
             refreshavi = SDL_TRUE;
             break;
           }

           sprintf( vidcapname, "Capturing to video%02d.avi", vidcapcount );
           ay_lockaudio( &oric->ay );
           vidcap = avi_open( &vidcapname[13], oricpalette, soundavailable&&soundon );
           ay_unlockaudio( &oric->ay );
           if( vidcap )
           {
             vidcapcount++;
             do_popup( vidcapname );
           }
           refreshavi = SDL_TRUE;
           break;

#ifdef __amigaos4__
        case SDLK_HELP:
           IDOS->SystemTags( "Multiview Oricutron.guide",
             SYS_Input, IDOS->Open( "NIL:", MODE_NEWFILE ),
             SYS_Output, IDOS->Open( "NIL:", MODE_NEWFILE ),
             SYS_Asynch, TRUE,
             TAG_DONE );
           break;
#endif

#ifdef __MORPHOS__
        case SDLK_HELP:
           SystemTags( "Multiview Oricutron.guide",
             SYS_Input, Open( "NIL:", MODE_NEWFILE ),
             SYS_Output, Open( "NIL:", MODE_NEWFILE ),
             SYS_Asynch, TRUE,
             TAG_DONE );
           break;
#endif

        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
          shifted = SDL_FALSE;

        default:
          ay_keypress( &oric->ay, ev->key.keysym.sym, SDL_FALSE );
          break;
      }
      break;

    case SDL_KEYDOWN:
      switch( ev->key.keysym.sym )
      {
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
          shifted = SDL_TRUE;
        
        default:
          ay_keypress( &oric->ay, ev->key.keysym.sym, SDL_TRUE );
          break;
      }
      break;
  }

  return SDL_FALSE;
}

void blank_ram( Sint32 how, Uint8 *mem, Uint32 size )
{
  Uint32 i, j;

  switch( how )
  {
    case 0:
      for( i=0; i<size; i+=256 )
      {
        for( j=0; j<128; j++ )
        {
          mem[i+j    ] = 0;
          mem[i+j+128] = 255;
        }
      }
      break;
    
    default:
      for( i=0; i<size; i+=2 )
      {
        mem[i  ] = 0xff;
        mem[i+1] = 0x00;
      }
      break;
  }
}

SDL_bool init_machine( struct machine *oric, int type, SDL_bool nukebreakpoints )
{
  int i;

  oric->type = type;
  m6502_init( &oric->cpu, (void*)oric, nukebreakpoints );

  oric->vidbases[0] = 0xa000;
  oric->vidbases[1] = 0x9800;
  oric->vidbases[2] = 0xbb80;
  oric->vidbases[3] = 0xb400;

  oric->romsyms.numsyms = 0;
  oric->tele_banksyms[0].numsyms = 0;
  oric->tele_banksyms[1].numsyms = 0;
  oric->tele_banksyms[2].numsyms = 0;
  oric->tele_banksyms[3].numsyms = 0;
  oric->tele_banksyms[4].numsyms = 0;
  oric->tele_banksyms[5].numsyms = 0;
  oric->tele_banksyms[6].numsyms = 0;
  oric->tele_banksyms[7].numsyms = 0;

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

      blank_ram( 0, oric->mem, 16384+16384 );      

      oric->rom = &oric->mem[16384];

      switch( oric->drivetype )
      {
        case DRV_MICRODISC:
          oric->cpu.read = microdisc_o16kread;
          oric->cpu.write = microdisc_o16kwrite;
          oric->romdis = SDL_TRUE;
          microdisc_init( &oric->md, &oric->wddisk, oric );
          oric->disksyms = &sym_microdisc;
          break;
        
        case DRV_JASMIN:
          oric->cpu.read = jasmin_o16kread;
          oric->cpu.write = jasmin_o16kwrite;
          oric->romdis = SDL_FALSE;
          jasmin_init( &oric->jasmin, &oric->wddisk, oric );
          oric->disksyms = &sym_jasmin;
          break;

        default:
          oric->cpu.read = o16kread;
          oric->cpu.write = o16kwrite;
          oric->romdis = SDL_FALSE;
          oric->disksyms = NULL;
          break;
      }

      if( !load_rom( oric, oric1romfile, -16384, &oric->rom[0], &oric->romsyms, SYMF_ROMDIS0 ) )
        return SDL_FALSE;

      for( i=0; i<8; i++ )
      {
        oric->pal[i] = SDL_MapRGB( screen->format, oricpalette[i*3], oricpalette[i*3+1], oricpalette[i*3+2] );
        oric->dpal[i] = (oric->pal[i]<<16)|oric->pal[i];
      }

      oric->scr = (Uint8 *)malloc( 240*224 );
      if( !oric->scr ) return SDL_FALSE;

      oric->cyclesperraster = 64;
      oric->vid_start = 65;
      oric->vid_maxrast = 312;
      oric->vid_special = oric->vid_start + 200;
      oric->vid_end     = oric->vid_start + 224;
      oric->vid_raster  = 0;
      video_decode_attr( oric, 0x1a );
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

      blank_ram( 0, oric->mem, 65536+16384 );      

      oric->rom = &oric->mem[65536];

      switch( oric->drivetype )
      {
        case DRV_MICRODISC:
          oric->cpu.read = microdisc_atmosread;
          oric->cpu.write = microdisc_atmoswrite;
          oric->romdis = SDL_TRUE;
          microdisc_init( &oric->md, &oric->wddisk, oric );
          oric->disksyms = &sym_microdisc;
          break;
        
        case DRV_JASMIN:
          oric->cpu.read = jasmin_atmosread;
          oric->cpu.write = jasmin_atmoswrite;
          oric->romdis = SDL_FALSE;
          jasmin_init( &oric->jasmin, &oric->wddisk, oric );
          oric->disksyms = &sym_jasmin;
          break;

        default:
          oric->cpu.read = atmosread;
          oric->cpu.write = atmoswrite;
          oric->romdis = SDL_FALSE;
          oric->disksyms = NULL;
          break;
      }

      if( !load_rom( oric, oric1romfile, -16384, &oric->rom[0], &oric->romsyms, SYMF_ROMDIS0 ) )
        return SDL_FALSE;

      oric->pal[0] = SDL_MapRGB( screen->format, 0x00, 0x00, 0x00 );
      oric->pal[1] = SDL_MapRGB( screen->format, 0xff, 0x00, 0x00 );
      oric->pal[2] = SDL_MapRGB( screen->format, 0x00, 0xff, 0x00 );
      oric->pal[3] = SDL_MapRGB( screen->format, 0xff, 0xff, 0x00 );
      oric->pal[4] = SDL_MapRGB( screen->format, 0x00, 0x00, 0xff );
      oric->pal[5] = SDL_MapRGB( screen->format, 0xff, 0x00, 0xff );
      oric->pal[6] = SDL_MapRGB( screen->format, 0x00, 0xff, 0xff );
      oric->pal[7] = SDL_MapRGB( screen->format, 0xff, 0xff, 0xff );

      for( i=0; i<8; i++ )
        oric->dpal[i] = (oric->pal[i]<<16)|oric->pal[i];

      oric->scr = (Uint8 *)malloc( 240*224 );
      if( !oric->scr ) return SDL_FALSE;

      oric->cyclesperraster = 64;
      oric->vid_start = 65;
      oric->vid_maxrast = 312;
      oric->vid_special = oric->vid_start + 200;
      oric->vid_end     = oric->vid_start + 224;
      oric->vid_raster  = 0;
      video_decode_attr( oric, 0x1a );
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

      blank_ram( 0, oric->mem, 65536+16384 );      

      oric->rom = &oric->mem[65536];

      switch( oric->drivetype )
      {
        case DRV_MICRODISC:
          oric->cpu.read = microdisc_atmosread;
          oric->cpu.write = microdisc_atmoswrite;
          oric->romdis = SDL_TRUE;
          microdisc_init( &oric->md, &oric->wddisk, oric );
          oric->disksyms = &sym_microdisc;
          break;
        
        case DRV_JASMIN:
          oric->cpu.read = jasmin_atmosread;
          oric->cpu.write = jasmin_atmoswrite;
          oric->romdis = SDL_FALSE;
          jasmin_init( &oric->jasmin, &oric->wddisk, oric );
          oric->disksyms = &sym_jasmin;
          break;

        default:
          oric->cpu.read = atmosread;
          oric->cpu.write = atmoswrite;
          oric->romdis = SDL_FALSE;
          oric->disksyms = NULL;
          break;
      }

      if( !load_rom( oric, atmosromfile, -16384, &oric->rom[0], &oric->romsyms, SYMF_ROMDIS0 ) )
        return SDL_FALSE;

      oric->pal[0] = SDL_MapRGB( screen->format, 0x00, 0x00, 0x00 );
      oric->pal[1] = SDL_MapRGB( screen->format, 0xff, 0x00, 0x00 );
      oric->pal[2] = SDL_MapRGB( screen->format, 0x00, 0xff, 0x00 );
      oric->pal[3] = SDL_MapRGB( screen->format, 0xff, 0xff, 0x00 );
      oric->pal[4] = SDL_MapRGB( screen->format, 0x00, 0x00, 0xff );
      oric->pal[5] = SDL_MapRGB( screen->format, 0xff, 0x00, 0xff );
      oric->pal[6] = SDL_MapRGB( screen->format, 0x00, 0xff, 0xff );
      oric->pal[7] = SDL_MapRGB( screen->format, 0xff, 0xff, 0xff );

      for( i=0; i<8; i++ )
        oric->dpal[i] = (oric->pal[i]<<16)|oric->pal[i];

      oric->scr = (Uint8 *)malloc( 240*224 );
      if( !oric->scr ) return SDL_FALSE;

      oric->cyclesperraster = 64;
      oric->vid_start = 65;
      oric->vid_maxrast = 312;
      oric->vid_special = oric->vid_start + 200;
      oric->vid_end     = oric->vid_start + 224;
      oric->vid_raster  = 0;
      video_decode_attr( oric, 0x1a );
      break;

    case MACH_TELESTRAT:
      hwopitems[0].name = " Oric-1";
      hwopitems[1].name = " Oric-1 16K";
      hwopitems[2].name = " Atmos";
      hwopitems[3].name = "\x0e""Telestrat";
      oric->mem = malloc( 65536+16384*7 );
      if( !oric->mem )
      {
        printf( "Out of memory\n" );
        return SDL_FALSE;
      }

      blank_ram( 0, oric->mem, 65536+16384*7 );      

      oric->cpu.read = telestratread;
      oric->cpu.write = telestratwrite;
      oric->drivetype = DRV_MICRODISC;
      microdisc_init( &oric->md, &oric->wddisk, oric );
      oric->romdis = SDL_FALSE;
      oric->disksyms = NULL;

      for( i=0; i<8; i++ )
      {
        oric->tele_bank[i].ptr  = &oric->mem[0x0c000+(i*0x4000)];
        if( telebankfiles[i][0] )
        {
          oric->tele_bank[i].type = TELEBANK_ROM;
          if( !load_rom( oric, telebankfiles[i], -16384, oric->tele_bank[i].ptr, &oric->tele_banksyms[i], SYMF_TELEBANK1<<i ) ) return SDL_FALSE;
        } else {
          oric->tele_bank[i].type = TELEBANK_RAM;
        }
      }

      oric->tele_currbank = 7;
      oric->tele_banktype = oric->tele_bank[7].type;
      oric->rom           = oric->tele_bank[7].ptr;

      oric->pal[0] = SDL_MapRGB( screen->format, 0x00, 0x00, 0x00 );
      oric->pal[1] = SDL_MapRGB( screen->format, 0xff, 0x00, 0x00 );
      oric->pal[2] = SDL_MapRGB( screen->format, 0x00, 0xff, 0x00 );
      oric->pal[3] = SDL_MapRGB( screen->format, 0xff, 0xff, 0x00 );
      oric->pal[4] = SDL_MapRGB( screen->format, 0x00, 0x00, 0xff );
      oric->pal[5] = SDL_MapRGB( screen->format, 0xff, 0x00, 0xff );
      oric->pal[6] = SDL_MapRGB( screen->format, 0x00, 0xff, 0xff );
      oric->pal[7] = SDL_MapRGB( screen->format, 0xff, 0xff, 0xff );

      for( i=0; i<8; i++ )
        oric->dpal[i] = (oric->pal[i]<<16)|oric->pal[i];

      oric->scr = (Uint8 *)malloc( 240*224 );
      if( !oric->scr ) return SDL_FALSE;

      oric->cyclesperraster = 64;
      oric->vid_start = 65;
      oric->vid_maxrast = 312;
      oric->vid_special = oric->vid_start + 200;
      oric->vid_end     = oric->vid_start + 224;
      oric->vid_raster  = 0;
      video_decode_attr( oric, 0x1a );
      break;
  }

  oric->tapename[0] = 0;
  tape_rewind( oric );
  m6502_reset( &oric->cpu );
  via_init( &oric->via, oric, VIA_MAIN );
  via_init( &oric->tele_via, oric, VIA_TELESTRAT );
  ay_init( &oric->ay, oric );
  oric->cpu.rastercycles = oric->cyclesperraster;
  oric->frames = 0;
  oric->vid_double = SDL_TRUE;
  setemumode( oric, NULL, EM_RUNNING );

  if( oric->autorewind ) tape_rewind( oric );

  setmenutoggles( oric );
  refreshstatus = SDL_TRUE;

  return SDL_TRUE;
}

void shut_machine( struct machine *oric )
{
  if( oric->drivetype == DRV_MICRODISC ) { microdisc_free( &oric->md ); oric->drivetype = DRV_NONE; }
  if( oric->drivetype == DRV_JASMIN )    { jasmin_free( &oric->jasmin ); oric->drivetype = DRV_NONE; }
  if( oric->mem ) { free( oric->mem ); oric->mem = NULL; oric->rom = NULL; }
  if( oric->scr ) { free( oric->scr ); oric->scr = NULL; }
  if( oric->prf ) { fclose( oric->prf ); oric->prf = NULL; }
  mon_freesyms( &sym_microdisc );
  mon_freesyms( &sym_jasmin );
  mon_freesyms( &oric->romsyms );
  mon_freesyms( &oric->tele_banksyms[0] );
  mon_freesyms( &oric->tele_banksyms[1] );
  mon_freesyms( &oric->tele_banksyms[2] );
  mon_freesyms( &oric->tele_banksyms[3] );
  mon_freesyms( &oric->tele_banksyms[4] );
  mon_freesyms( &oric->tele_banksyms[5] );
  mon_freesyms( &oric->tele_banksyms[6] );
  mon_freesyms( &oric->tele_banksyms[7] );
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

  mon_state_reset( oric );
  if( !init_machine( oric, oric->type, SDL_FALSE ) )
  {
    shut();
    exit( EXIT_FAILURE );
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

  which &= 0xffff;

  oric->drivetype = curr_drivetype;

  mon_state_reset( oric );
  if( !init_machine( oric, which, which!=oric->type ) )
  {
    shut();
    exit( EXIT_FAILURE );
  }
}
