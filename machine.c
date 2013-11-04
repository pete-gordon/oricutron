/*
**  Oricutron
**  Copyright (C) 2009-2012 Peter Gordon
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
#include <proto/amigaguide.h>
#endif

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "6551.h"
#include "machine.h"
#include "avi.h"
#include "filereq.h"
#include "main.h"
#include "ula.h"
#include "joystick.h"
#include "tape.h"

extern SDL_bool warpspeed, soundavailable, soundon;
extern char diskpath[], diskfile[], filetmp[];
extern char telediskpath[], telediskfile[];
extern char pravdiskpath[], pravdiskfile[];
extern SDL_bool refreshstatus, refreshavi;

char atmosromfile[1024];
char oric1romfile[1024];
char mdiscromfile[1024];
char jasmnromfile[1024];
char pravetzromfile[2][1024];
char telebankfiles[8][1024];

struct avi_handle *vidcap = NULL;
char vidcapname[128];
int vidcapcount = 0;

unsigned char rom_microdisc[8912], rom_jasmin[2048], rom_pravetz[512];
struct symboltable sym_microdisc, sym_jasmin, sym_pravetz;
SDL_bool microdiscrom_valid, jasminrom_valid, pravetzrom_valid;
extern struct osdmenuitem mainitems[];

Uint8 oricpalette[] = { 0x00, 0x00, 0x00,
                        0xff, 0x00, 0x00,
                        0x00, 0xff, 0x00,
                        0xff, 0xff, 0x00,
                        0x00, 0x00, 0xff,
                        0xff, 0x00, 0xff,
                        0x00, 0xff, 0xff,
                        0xff, 0xff, 0xff };

static Uint8 sedoric_detect[] =
  {
    0xfb, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
    0x20, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x53, 0x45, 0x44, 0x4f, 0x52, 0x49, 0x43, 
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
  };

static Uint8 stratsed_detect[] =
  {
    0xfb, 0x00, 0x11, 0x2c, 0x00, 0xd4, 0xa9, 0x00, 0x00, 0x52, 0xa9, 0x00, 0x8d, 0x0f, 0x05, 0x8d, 
    0x49, 0x05, 0xa9, 0x4c, 0xa0, 0x00, 0xa2, 0xd4, 0x8d, 0x4f, 0x05, 0x8c, 0x50, 0x05, 0x8e, 0x51, 
    0x05, 0xa2, 0x03, 0xec, 0x0c, 0x02, 0xd0, 0x03, 0xa9, 0x02, 0x2c, 0xa9, 0x00, 0x9d, 0x09, 0x05, 
  };

static Uint8 ftdos_detect[] =
  {
    0xfb, 0x78, 0xa9, 0x7f, 0x8d, 0x0e, 0x03, 0xa9, 0x01, 0x8d, 0xfa, 0x03, 0xa9, 0x00, 0x8d, 0xfb, 
    0x03, 0x85, 0x04, 0x8d, 0xf4, 0x03, 0xa2, 0x02, 0xac, 0x30, 0x02, 0xc0, 0x40, 0xd0, 0x04, 0xa9, 
    0x04, 0xa2, 0x06, 0x85, 0x01, 0x8e, 0x53, 0x04, 0xa9, 0xac, 0x8d, 0xfe, 0xff, 0xa9, 0x04, 0x8d, 
  };

int detect_image_type(char *filename)
{
  FILE *f;
  size_t size;
  unsigned char tmp[6400];

  f = fopen(filename, "rb");
  if (!f)
    return IMG_I_DUNNO;

  fseek(f, 0, SEEK_END);
  size = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (1 != fread(tmp, 8, 1, f))
  {
    fclose(f);
    return IMG_I_DUNNO;
  }

  /* Look for tape tmps */
  if ((memcmp(tmp, "\x16\x16\x16\x16", 4) == 0) ||
      (memcmp(tmp, "\x16\x16\x16\x24", 4) == 0) ||
      (memcmp(tmp, "\x16\x16\x24", 3) == 0) ||
      (memcmp(tmp, "RIFF", 4) == 0) ||
      (memcmp(tmp, "ORT\x00", 4) == 0))
  {
    fclose(f);
    return IMG_TAPE;
  }

  /* Look for a microdisc or jasmin disk image */
  if ((size > 20) && (memcmp(tmp, "MFM_DISK", 8)==0))
  {
    Uint8 *ptr, *eot;
    int sectorlen;
    SDL_bool gotsector = SDL_FALSE;

    /* Read side 0, track 0 */
    fseek(f, 256, SEEK_SET);
    fread(tmp, 6400, 1, f);
    fclose(f);

    /* Find the first sector */
    ptr = tmp;
    eot = &tmp[6400];
    while (1)
    {
      // Search for ID mark
      while( (ptr<eot) && (ptr[0]!=0xfe) ) ptr++;

      // No sector found?
      if (ptr >= eot)
      {
        printf("%s: couldn't find track 0 sector 1 in %s\n", __func__, filename);
        return IMG_I_DUNNO;
      }

      // Get N value
      sectorlen = (1<<(ptr[4]+7));
      gotsector = (ptr[3] == 1);

      // Skip ID field and CRC
      ptr+=7;

      // Search for data ID
      while( (ptr<eot) && (ptr[0]!=0xfb) && (ptr[0]!=0xf8) ) ptr++;
      if (ptr >= eot)
      {
        printf("%s: couldn't find track 0 sector 1 in %s (2)\n", __func__, filename);
        return IMG_I_DUNNO;
      }

      // ptr now points to the sector data
      if (gotsector)
        break;

      // Skip this sector
      ptr += sectorlen+3;
      if (ptr >= eot)
      {
        printf("%s: couldn't find track 0 sector 1 in %s (3)\n", __func__, filename);
        return IMG_I_DUNNO;
      }
    }

    /* Got track 0, sector 1 */
    /* See if we recognise it .. */
    if (sectorlen == 256)
    {
      if (memcmp(ptr, sedoric_detect, sizeof(sedoric_detect))==0)
        return IMG_ATMOS_MICRODISC;

      if (memcmp(ptr, stratsed_detect, sizeof(stratsed_detect))==0)
        return IMG_TELESTRAT_DISK;

      if (memcmp(ptr, ftdos_detect, sizeof(stratsed_detect))==0)
        return IMG_ATMOS_JASMIN;
    }

    /* No.. best guess is some sort of microdisc image */
    return IMG_GUESS_MICRODISC;
  }

  fclose(f);

  /* Maybe its a pravetz disk... */
  if (size == 143360)
    return IMG_PRAVETZ_DISK;

  return IMG_I_DUNNO;
}

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
      ula_set_dirty( oric );
      break;

    case EM_MENU:
      if( vidcap ) avi_close( &vidcap );
      gotomenu( oric, NULL, 0 );
      SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );
      SDL_EnableUNICODE( SDL_TRUE );
      oric->ay.soundon = SDL_FALSE;
      if( soundavailable )
        SDL_PauseAudio( 1 );
      break;

    case EM_DEBUG:
      if( vidcap ) avi_close( &vidcap );
      mon_enter( oric );
      SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );
      SDL_EnableUNICODE( SDL_TRUE );
      oric->ay.soundon = SDL_FALSE;
      if( soundavailable )
        SDL_PauseAudio( 1 );
      break;
  }
}

void setromon( struct machine *oric )
{
  // Determine if the ROM is currently active
  oric->romon = SDL_TRUE;
  if( oric->drivetype == DRV_JASMIN )
  {
    if( oric->jasmin.olay == 0 )
    {
      oric->romon = !oric->romdis;
    } else {
      oric->romon = SDL_FALSE;
    }
  } 
  else if( oric->drivetype == DRV_PRAVETZ )
  {
    oric->romon = !oric->pravetz.olay;
  } else {
    oric->romon = !oric->romdis;
  }
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
        if( addr >= 0x31c )
          acia_write( &oric->tele_acia, addr, data );
        else
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

// Pravetz
void pravetz_atmoswrite( struct m6502 *cpu, unsigned short addr, unsigned char data )
{
  struct machine *oric = (struct machine *)cpu->userdata;
  
  oric->mem[addr] = data;

  if( 0x300 <= addr && addr <= 0x30f )
  {
    via_write( &oric->via, addr, data );
  }
  else if( 0x310 <= addr && addr <= 0x31f )
  {
    pravetz_write( &oric->pravetz, addr, data );
  }
  else if( 0x320 <= addr && addr <= 0x3ff )
  {
    switch( addr )
    {
      case 0x380:
        oric->pravetz.olay = 0;
        oric->pravetz.romdis = 1;
        oric->pravetz.extension = 0;
        break;
      case 0x381:
        oric->pravetz.olay = 1;
        oric->pravetz.romdis = 0;
        oric->pravetz.extension = 0;
        break;
      case 0x382:
        oric->pravetz.olay = 0;
        oric->pravetz.romdis = 1;
        oric->pravetz.extension = 0x100;
        break;
      case 0x383:
        oric->pravetz.olay = 1;
        oric->pravetz.romdis = 0;
        oric->pravetz.extension = 0x100;
        break;
      default:
        break;
    }
  }
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

    case DRV_PRAVETZ:
      if( oric->pravetz.olay ) return SDL_TRUE;
      if( addr >= 0xc000 ) return SDL_FALSE;
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
        if( addr >= 0x31c )
        {
          return acia_read( &oric->tele_acia, addr );
        }

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

// Pravetz
unsigned char pravetz_atmosread( struct m6502 *cpu, unsigned short addr )
{
  struct machine *oric = (struct machine *)cpu->userdata;

  if( 0x300 == ( addr & 0xfff0 ) )
  {
    return via_read( &oric->via, addr );
  }
  
  if( 0x310 == ( addr & 0xfff0 ) )
  {
    return pravetz_read( &oric->pravetz, addr );
  }
  
  if( 0x320 <= addr && addr <= 0x3ff )
  {
    if( pravetzrom_valid )
    {
      return rom_pravetz[addr - 0x300 + oric->pravetz.extension];
    }
  }

  if( 0xc000 <= addr      /* 0xFFFF */)
  {
    if( !oric->pravetz.olay )
    {
      return oric->rom[addr-0xc000];
    }
  }

  return oric->mem[addr];
}

/* Wrapper for the above when lightpen is enabled */
unsigned char lightpen_read( struct m6502 *cpu, unsigned short addr )
{
  struct machine *oric = (struct machine *)cpu->userdata;

  if( addr == 0x3e0 ) return oric->lightpenx;
  if( addr == 0x3e1 ) return oric->lightpeny;

  return oric->read_not_lightpen( cpu, addr );
}


static SDL_bool load_rom( struct machine *oric, char *fname, int size, unsigned char *where, struct symboltable *stab, int symflags )
{
  SDL_RWops *f;
  char *tmpname;

  // MinGW doesn't have asprintf :-(
  tmpname = malloc( strlen( fname ) + 10 );
  if( !tmpname ) return SDL_FALSE;

  sprintf( tmpname, "%s.rom", fname );
  f = SDL_RWFromFile( tmpname, "rb" );
  if( !f )
  {
    printf( "Unable to open '%s'\n", tmpname );
    free( tmpname );
    return SDL_FALSE;
  }

  if( size < 0 )
  {
    int filesize;
    SDL_RWseek( f, 0, SEEK_END );
    filesize = SDL_RWtell( f );
    SDL_RWseek( f, 0, SEEK_SET );

    if( filesize > -size )
    {
      printf( "ROM '%s' exceeds %d bytes.\n", fname, -size );
      SDL_RWclose( f );
      free( tmpname );
      return SDL_FALSE;
    }

    where += (-size)-filesize;
    size = filesize;
  }

  if( SDL_RWread( f, where, size, 1 ) != 1 )
  {
    printf( "Unable to read '%s'\n", tmpname );
    SDL_RWclose( f );
    free( tmpname );
    return SDL_FALSE;
  }

  SDL_RWclose( f );

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

  oric->tapebuf = NULL;
  oric->tapelen = 0;
  oric->tapemotor = SDL_FALSE;
  oric->vsynchack = SDL_FALSE;
  oric->tapeturbo = SDL_TRUE;
  oric->tapeturbo_forceoff = SDL_FALSE;
  oric->autorewind = SDL_FALSE;
  oric->autoinsert = SDL_TRUE;
  oric->symbolsautoload = SDL_TRUE;
  oric->symbolscase = SDL_FALSE;
  oric->tapename[0] = 0;
  oric->prf = NULL;
  oric->prclock = 0;
  oric->prclose = 0;
  oric->lasttapefile[0] = 0;
  oric->keymap = KMAP_QWERTY;
  oric->showfps = SDL_TRUE;
  oric->popupstr[0] = 0;
  oric->newpopupstr = SDL_FALSE;
  oric->popuptime = 0;
  oric->statusstr[0] = 0;
  oric->newstatusstr = SDL_FALSE;
  oric->sdljoy_a = NULL;
  oric->sdljoy_b = NULL;
  oric->rampattern = 0;
  oric->tapecap = NULL;
  oric->tapenoise = SDL_FALSE;
  oric->rawtape = SDL_FALSE;

  oric->joy_iface = JOYIFACE_NONE;
  oric->joymode_a = JOYMODE_KB1;
  oric->joymode_b = JOYMODE_NONE;
  oric->telejoymode_a = JOYMODE_KB1;
  oric->telejoymode_b = JOYMODE_NONE;

  oric->porta_joy = 0xff;
  oric->porta_ay  = 0;
  oric->porta_is_ay = SDL_TRUE;

  oric->kbjoy1[0] = SDLK_KP8;
  oric->kbjoy1[1] = SDLK_KP2;
  oric->kbjoy1[2] = SDLK_KP4;
  oric->kbjoy1[3] = SDLK_KP6;
  oric->kbjoy1[4] = SDLK_KP_ENTER;
  oric->kbjoy1[5] = SDLK_KP_PLUS;
  oric->kbjoy2[0] = 'w';
  oric->kbjoy2[1] = 's';
  oric->kbjoy2[2] = 'a';
  oric->kbjoy2[3] = 'd';
  oric->kbjoy2[4] = SDLK_SPACE;
  oric->kbjoy2[5] = 'n';

  oric->drivetype = DRV_NONE;
  for( i=0; i<MAX_DRIVES; i++ )
  {
    oric->wddisk.disk[i] = NULL;
    oric->diskname[i][0] = 0;
  }
  oric->diskautosave = SDL_FALSE;
  
  oric->lightpen  = SDL_FALSE;
  oric->lightpenx = 0;
  oric->lightpeny = 0;
  oric->read_not_lightpen = NULL;

  oric->tsavf = NULL;
}

void load_diskroms( struct machine *oric )
{
  microdiscrom_valid = load_rom( oric, mdiscromfile, 8192, rom_microdisc, &sym_microdisc, SYMF_ROMDIS1|SYMF_MICRODISC );
  jasminrom_valid    = load_rom( oric, jasmnromfile, 2048, rom_jasmin,    &sym_jasmin,    SYMF_ROMDIS1|SYMF_JASMIN );
  pravetzrom_valid   = load_rom( oric, pravetzromfile[1], 512, rom_pravetz, &sym_pravetz,  SYMF_PRAVZ8D );
}

// This is currently used to workaround a change in the behaviour of SDL
// on OS4, but in future it would be a handy place to fix keyboard layout
// issues, such as the problems with a non-uk keymap on linux.
// Also helps German keymap on MorphOS.
int mapkey( int key )
{
  switch( key )
  {
#if defined(__amigaos4__) || defined(__MORPHOS__) || defined(__AROS__)
    case '@': return '#';
    case '\xF6':
    case ':': return ';';
    case '<': return ',';
    case '>': return '.';
    case '\xDF':
    case '?': return '/';
    case '#':
    case '~': return '\'';
    case '_': return '-';
    case '+': return '=';
    case '\xE4':
    case '{': return '[';
    case '\xFC':
    case '}': return ']';
    case '^': return '\\';
#elif defined(WIN32)
	case '<': return '\\';
#endif
  }

  return key;
}

static SDL_bool lightpendown = SDL_FALSE;
void move_lightpen( struct machine *oric, int x, int y )
{
  if (!lightpendown) return;

  if ((oric->rendermode == RENDERMODE_SW) || (!oric->hstretch))
  {
    x = (x-80)/2;
    y = (y-14)/2;
  }
  else
  {
    x = ((double)x)/(640.0f/240.0f);
    y = (y-14)/2;
  }

  if ((x>=0) && (x<240) && (y>=0) && (y<224))
  {
    if (oric->scr[y*240+x] != 0)
    {
      oric->lightpenx = (x+219)&0xff;
      oric->lightpeny = (y+54)&0xff;
    }
  }
}

static SDL_bool shifted = SDL_FALSE;
SDL_bool emu_event( SDL_Event *ev, struct machine *oric, SDL_bool *needrender )
{
  Sint32 i;

  if( joy_filter_event( ev, oric ) )
    return SDL_FALSE;

//  char stmp[32];
  switch( ev->type )
  {
/*
    case SDL_USEREVENT:
      *needrender = SDL_TRUE;
      break;
*/
    case SDL_MOUSEBUTTONDOWN:
      switch (ev->button.button)
      {
        case SDL_BUTTON_LEFT:
          lightpendown = SDL_TRUE;
          move_lightpen( oric, ev->button.x, ev->button.y );
          break;

        case SDL_BUTTON_RIGHT:
          setemumode( oric, NULL, EM_MENU );
          *needrender = SDL_TRUE;
          break;
      }
      break;

    case SDL_MOUSEBUTTONUP:
      if( ev->button.button == SDL_BUTTON_LEFT )
        lightpendown = SDL_FALSE;
      break;

    case SDL_MOUSEMOTION:
      move_lightpen( oric, ev->motion.x, ev->motion.y );
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
          if( oric->drivetype == DRV_MICRODISC )
          {
            oric->romdis = SDL_TRUE;
            microdisc_init( &oric->md, &oric->wddisk, oric );
          }
          setromon( oric );
          m6502_reset( &oric->cpu );
          via_init( &oric->via, oric, VIA_MAIN );
          via_init( &oric->tele_via, oric, VIA_TELESTRAT );
          acia_init( &oric->tele_acia, oric );
          break;
        
        case SDLK_F5:
          oric->showfps = oric->showfps ? SDL_FALSE : SDL_TRUE;
          refreshstatus = SDL_TRUE;
          oric->statusstr[0] = 0;
          oric->newstatusstr = SDL_TRUE;
          break;

        case SDLK_F6:
          if( vidcap )
          {
            warpspeed = SDL_FALSE;
          }
          else
          {
            warpspeed = warpspeed ? SDL_FALSE : SDL_TRUE;
          }
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
                  switch (oric->drivetype)
                  {
                    case DRV_PRAVETZ:
                      dpath = pravdiskpath;
                      dfile = pravdiskfile;
                      break;

                    default:
                      dpath = diskpath;
                      dfile = diskfile;
                      break;
                  }
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

        case SDLK_F9:
          toggletapecap( oric, &mainitems[1], 0 );
          break;

        case SDLK_F10:
           if( vidcap )
           {
             ay_lockaudio( &oric->ay );
             avi_close( &vidcap );
             ay_unlockaudio( &oric->ay );
             do_popup( oric, "AVI capture stopped" );
             refreshavi = SDL_TRUE;
             break;
           }

           sprintf( vidcapname, "Capturing to video%02d.avi", vidcapcount );
           warpspeed = SDL_FALSE;
           ay_lockaudio( &oric->ay );
           vidcap = avi_open( &vidcapname[13], oricpalette, soundavailable&&soundon, oric->vid_freq );
           ay_unlockaudio( &oric->ay );
           if( vidcap )
           {
             vidcapcount++;
             do_popup( oric, vidcapname );
           }
           refreshavi = SDL_TRUE;
           break;

#if defined(__BEOS__) || defined(__HAIKU__) || defined(__APPLE__)
        case SDLK_F11:
          clipboard_copy( oric );
          break;
        case SDLK_F12:
          clipboard_paste( oric );
          break;
#elif defined(__WIN32__) || defined(__CYGWIN__)
        case SDLK_F11:
            clipboard_copy( oric );
            break;
        case SDLK_F12:
            clipboard_paste( oric );
            break;
#elif defined(__linux__)
        case SDLK_F11:
            clipboard_copy( oric );
            break;
        case SDLK_F12:
            clipboard_paste( oric );
            break;
#else
        case SDLK_F11:
        case SDLK_F12:
            break;
#endif

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
           {
           struct NewAmigaGuide nag =
           { (BPTR) NULL,
             "Oricutron.guide",
             NULL,
             NULL,
             NULL,
             NULL,
             NULL,
             0,
             NULL,
             "MAIN",
             0,
             NULL,
             NULL
           };

           OpenAmigaGuide(&nag, TAG_DONE);
           }
           break;
#endif

        case SDLK_LSHIFT:
        case SDLK_RSHIFT:
          shifted = SDL_FALSE;

        default:
          ay_keypress( &oric->ay, mapkey( ev->key.keysym.sym ), SDL_FALSE );
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
          ay_keypress( &oric->ay, mapkey( ev->key.keysym.sym ), SDL_TRUE );
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

void clear_patches( struct machine *oric )
{
  oric->pch_fd_cload_getname_pc        = -1;
  oric->pch_fd_csave_getname_pc        = -1;
  oric->pch_fd_store_getname_pc        = -1;
  oric->pch_fd_recall_getname_pc       = -1;
  oric->pch_fd_getname_addr            = -1;
  oric->pch_fd_available               = SDL_FALSE;

  oric->pch_tt_getsync_pc              = -1;
  oric->pch_tt_getsync_end_pc          = -1;
  oric->pch_tt_getsync_loop_pc         = -1;
  oric->pch_tt_readbyte_pc             = -1;
  oric->pch_tt_readbyte_end_pc         = -1;
  oric->pch_tt_readbyte_storebyte_addr = -1;
  oric->pch_tt_readbyte_storezero_addr = -1;
  oric->pch_tt_putbyte_pc              = -1;
  oric->pch_tt_putbyte_end_pc          = -1;
  oric->pch_tt_csave_end_pc            = -1;
  oric->pch_tt_store_end_pc            = -1;
  oric->pch_tt_available               = SDL_FALSE;
  oric->pch_tt_save_available          = SDL_FALSE;

  oric->keymap = KMAP_QWERTY;
}

static char *keymapnames[] = { "qwerty",
                               "azerty",
                               "qwertz",
                               NULL };

void load_patches( struct machine *oric, char *fname )
{
  FILE *f;
  Sint32 i;
  char *tmpname;

  // MinGW doesn't have asprintf :-(
  tmpname = malloc( strlen( fname ) + 10 );
  if( !tmpname ) return;

  sprintf( tmpname, "%s.pch", fname );

  f = fopen( tmpname, "r" );
  free( tmpname );
  if( !f ) return;

  while( !feof( f ) )
  {
    if( !fgets( filetmp, 2048, f ) ) break;

    for( i=0; isws( filetmp[i] ); i++ ) ;

    if( read_config_int(    &filetmp[i], "fd_cload_getname_pc",        &oric->pch_fd_cload_getname_pc, 0, 65535 ) )        continue;
    if( read_config_int(    &filetmp[i], "fd_csave_getname_pc",        &oric->pch_fd_csave_getname_pc, 0, 65535 ) )        continue;
    if( read_config_int(    &filetmp[i], "fd_store_getname_pc",        &oric->pch_fd_store_getname_pc, 0, 65535 ) )        continue;
    if( read_config_int(    &filetmp[i], "fd_recall_getname_pc",       &oric->pch_fd_recall_getname_pc, 0, 65535 ) )       continue;
    if( read_config_int(    &filetmp[i], "fd_getname_addr",            &oric->pch_fd_getname_addr, 0, 65535 ) )            continue;
    if( read_config_int(    &filetmp[i], "tt_getsync_pc",              &oric->pch_tt_getsync_pc, 0, 65535 ) )              continue;
    if( read_config_int(    &filetmp[i], "tt_getsync_end_pc",          &oric->pch_tt_getsync_end_pc, 0, 65535 ) )          continue;
    if( read_config_int(    &filetmp[i], "tt_getsync_loop_pc",         &oric->pch_tt_getsync_loop_pc, 0, 65535 ) )         continue;
    if( read_config_int(    &filetmp[i], "tt_readbyte_pc",             &oric->pch_tt_readbyte_pc, 0, 65535 ) )             continue;
    if( read_config_int(    &filetmp[i], "tt_readbyte_end_pc",         &oric->pch_tt_readbyte_end_pc, 0, 65535 ) )         continue;
    if( read_config_int(    &filetmp[i], "tt_readbyte_storebyte_addr", &oric->pch_tt_readbyte_storebyte_addr, 0, 65535 ) ) continue;
    if( read_config_int(    &filetmp[i], "tt_readbyte_storezero_addr", &oric->pch_tt_readbyte_storezero_addr, 0, 65535 ) ) continue;
    if( read_config_bool(   &filetmp[i], "tt_readbyte_setcarry",       &oric->pch_tt_readbyte_setcarry ) )                 continue;
    if( read_config_int(    &filetmp[i], "tt_putbyte_pc",              &oric->pch_tt_putbyte_pc, 0, 65535 ) )              continue;
    if( read_config_int(    &filetmp[i], "tt_putbyte_end_pc",          &oric->pch_tt_putbyte_end_pc, 0, 65535 ) )          continue;
    if( read_config_int(    &filetmp[i], "tt_csave_end_pc",            &oric->pch_tt_csave_end_pc, 0, 65535 ) )            continue;
    if( read_config_int(    &filetmp[i], "tt_store_end_pc",            &oric->pch_tt_store_end_pc, 0, 65535 ) )            continue;
    if( read_config_int(    &filetmp[i], "tt_writeleader_pc",          &oric->pch_tt_writeleader_pc, 0, 65535 ) )          continue;
    if( read_config_int(    &filetmp[i], "tt_writeleader_end_pc",      &oric->pch_tt_writeleader_end_pc, 0, 65535 ) )      continue;
    if( read_config_option( &filetmp[i], "keymap",                     &oric->keymap, keymapnames ) )                      continue; 
  }

  fclose( f );

  // Got all the addresses needed for each patch?
  if( ( ( oric->pch_fd_cload_getname_pc != -1 ) ||
        ( oric->pch_fd_csave_getname_pc != -1 ) ||
        ( oric->pch_fd_store_getname_pc != -1 ) ||
        ( oric->pch_fd_recall_getname_pc != -1 ) ) &&
      ( oric->pch_fd_getname_addr != -1 ) )
    oric->pch_fd_available = SDL_TRUE;

  if( ( oric->pch_tt_getsync_pc != -1 ) &&
      ( oric->pch_tt_getsync_end_pc != -1 ) &&
      ( oric->pch_tt_getsync_loop_pc != -1 ) &&
      ( oric->pch_tt_readbyte_pc != -1 ) &&
      ( oric->pch_tt_readbyte_end_pc != -1 ) )
    oric->pch_tt_available = SDL_TRUE;

  if( ( oric->pch_tt_putbyte_pc != -1 ) &&
      ( oric->pch_tt_putbyte_end_pc != -1 ) )
    oric->pch_tt_save_available = SDL_TRUE;
}

static void setup_for_microdisc( struct machine *oric, void *readptr, void *writeptr )
{
  oric->cpu.read = readptr;
  oric->cpu.write = writeptr;
  oric->romdis = SDL_TRUE;
  microdisc_init( &oric->md, &oric->wddisk, oric );
  oric->disksyms = &sym_microdisc;
}

static void setup_for_jasmin( struct machine *oric, void *readptr, void *writeptr )
{
  oric->cpu.read = readptr;
  oric->cpu.write = writeptr;
  oric->romdis = SDL_FALSE;
  jasmin_init( &oric->jasmin, &oric->wddisk, oric );
  oric->disksyms = &sym_jasmin;
}

static void setup_for_pravetzdisk( struct machine *oric, void *readptr, void *writeptr )
{
  oric->cpu.read = readptr;
  oric->cpu.write = writeptr;
  oric->romdis = SDL_FALSE;
  pravetz_init( &oric->pravetz, oric );
  oric->disksyms = &sym_pravetz;
}

static void setup_for_no_disk( struct machine *oric, void *readptr, void *writeptr )
{
  oric->drivetype = DRV_NONE;
  oric->cpu.read = readptr;
  oric->cpu.write = writeptr;
  oric->romdis = SDL_FALSE;
  oric->disksyms = NULL;
}

SDL_bool init_machine( struct machine *oric, int type, SDL_bool nukebreakpoints )
{
  int i;

  oric->tapeturbo_forceoff = SDL_FALSE;

  oric->type = type;
  m6502_init( &oric->cpu, (void*)oric, nukebreakpoints );

  oric->tapeturbo_syncstack = -1;

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

  clear_patches( oric );

  switch( type )
  {
    case MACH_ORIC1_16K:
      for( i=0; i<4; i++ )
        oric->vidbases[i] &= 0x7fff;
      oric->memsize = 16384 + 16384;
      oric->mem = malloc( oric->memsize );
      if( !oric->mem )
      {
        printf( "Out of memory\n" );
        return SDL_FALSE;
      }

      blank_ram( oric->rampattern, oric->mem, 16384+16384 );      

      oric->rom = &oric->mem[16384];

      switch( oric->drivetype )
      {
        case DRV_MICRODISC:
          setup_for_microdisc( oric, microdisc_o16kread, microdisc_o16kwrite);
          break;
        
        case DRV_JASMIN:
          setup_for_jasmin( oric, jasmin_o16kread, jasmin_o16kwrite);
          break;

        case DRV_PRAVETZ:
        default:
          setup_for_no_disk( oric, o16kread, o16kwrite );
          break;
      }

      if( !load_rom( oric, oric1romfile, -16384, &oric->rom[0], &oric->romsyms, SYMF_ROMDIS0 ) ) return SDL_FALSE;
      load_patches( oric, oric1romfile );
      break;
    
    case MACH_ORIC1:
      oric->memsize = 65536 + 16384;
      oric->mem = malloc( oric->memsize );
      if( !oric->mem )
      {
        printf( "Out of memory\n" );
        return SDL_FALSE;
      }

      blank_ram( oric->rampattern, oric->mem, 65536+16384 );      

      oric->rom = &oric->mem[65536];

      switch( oric->drivetype )
      {
        case DRV_MICRODISC:
          setup_for_microdisc( oric, microdisc_atmosread, microdisc_atmoswrite);
          break;
        
        case DRV_JASMIN:
          setup_for_jasmin( oric, jasmin_atmosread, jasmin_atmoswrite);
          break;

        case DRV_PRAVETZ:
        default:
          setup_for_no_disk( oric, atmosread, atmoswrite );
          break;
      }

      if( !load_rom( oric, oric1romfile, -16384, &oric->rom[0], &oric->romsyms, SYMF_ROMDIS0 ) ) return SDL_FALSE;
      load_patches( oric, oric1romfile );
      break;
    
    case MACH_ATMOS:
      oric->memsize = 65536 + 16384;
      oric->mem = malloc( oric->memsize );
      if( !oric->mem )
      {
        printf( "Out of memory\n" );
        return SDL_FALSE;
      }

      blank_ram( oric->rampattern, oric->mem, 65536+16384 );      

      oric->rom = &oric->mem[65536];

      switch( oric->drivetype )
      {
        case DRV_MICRODISC:
          setup_for_microdisc( oric, microdisc_atmosread, microdisc_atmoswrite);
          break;
        
        case DRV_JASMIN:
          setup_for_jasmin( oric, jasmin_atmosread, jasmin_atmoswrite);
          break;

        case DRV_PRAVETZ:
        default:
          setup_for_no_disk( oric, atmosread, atmoswrite );
          break;
      }

      if( !load_rom( oric, atmosromfile, -16384, &oric->rom[0], &oric->romsyms, SYMF_ROMDIS0 ) ) return SDL_FALSE;
      load_patches( oric, atmosromfile );
      break;

    case MACH_TELESTRAT:
      oric->drivetype = DRV_MICRODISC;
      oric->memsize = 65536 + 16384*7;
      oric->mem = malloc( oric->memsize );
      if( !oric->mem )
      {
        printf( "Out of memory\n" );
        return SDL_FALSE;
      }

      blank_ram( oric->rampattern, oric->mem, 65536+16384*7 );      

      clear_patches( oric );

      setup_for_microdisc( oric, telestratread, telestratwrite );
      oric->disksyms = NULL;

      for( i=0; i<8; i++ )
      {
        oric->tele_bank[i].ptr  = &oric->mem[0x0c000+(i*0x4000)];
        if( telebankfiles[i][0] )
        {
          oric->tele_bank[i].type = TELEBANK_ROM;
          if( !load_rom( oric, telebankfiles[i], -16384, oric->tele_bank[i].ptr, &oric->tele_banksyms[i], SYMF_TELEBANK0<<i ) ) return SDL_FALSE;
          load_patches( oric, telebankfiles[i] );
        } else {
          oric->tele_bank[i].type = TELEBANK_RAM;
        }
      }

      oric->tele_currbank = 7;
      oric->tele_banktype = oric->tele_bank[7].type;
      oric->rom           = oric->tele_bank[7].ptr;
      break;

    case MACH_PRAVETZ:
      oric->memsize = 65536 + 16384;
      oric->mem = malloc( oric->memsize );
      if( !oric->mem )
      {
        printf( "Out of memory\n" );
        return SDL_FALSE;
      }

      blank_ram( oric->rampattern, oric->mem, 65536+16384 );      

      oric->rom = &oric->mem[65536];

      switch( oric->drivetype )
      {
        case DRV_MICRODISC:
          setup_for_microdisc( oric, microdisc_atmosread, microdisc_atmoswrite);
          break;
        
        case DRV_JASMIN:
          setup_for_jasmin( oric, jasmin_atmosread, jasmin_atmoswrite);
          break;

        case DRV_PRAVETZ:
          setup_for_pravetzdisk( oric, pravetz_atmosread, pravetz_atmoswrite );
          break;

        default:
          setup_for_no_disk( oric, atmosread, atmoswrite );
          break;
      }

      if( !load_rom( oric, pravetzromfile[0], -16384, &oric->rom[0], &oric->romsyms, SYMF_ROMDIS0 ) ) return SDL_FALSE;
      else
      {
          // Patch default Pravetz keyboard layout
          const Uint8 keytab_patch[] =
          {
              0x37, 0xEA, 0xED, 0xEB, 0x20, 0xF5, 0xF9, 0x38, 0xEE, 0xF4, 0x36, 0x39, 0x2C, 0xE9, 0xE8, 0xEC,
              0x35, 0xF2, 0xE2, 0x3B, 0x2E, 0xEF, 0xE7, 0x30, 0xF6, 0xE6, 0x34, 0x2D, 0x0B, 0xF0, 0xE5, 0x2F,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x31, 0x1B, 0xFA, 0x00, 0x08, 0x7F, 0xE1, 0x0D,
              0xF8, 0xF1, 0x32, 0x5C, 0x0A, 0x5D, 0xF3, 0x5E, 0x33, 0xE4, 0xE3, 0x27, 0x09, 0x5B, 0xF7, 0x3D,
              0x26, 0x4A, 0x4D, 0x4B, 0x20, 0x55, 0x59, 0x2A, 0x4E, 0x54, 0x5E, 0x28, 0x3C, 0x49, 0x48, 0x4C,
              0x25, 0x52, 0x42, 0x3A, 0x3E, 0x4F, 0x47, 0x29, 0x56, 0x46, 0x24, 0x5F, 0x0B, 0x50, 0x45, 0x3F,
              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0x1B, 0x5A, 0x00, 0x08, 0x7F, 0x41, 0x0D,
              0x58, 0x51, 0x40, 0x7C, 0x0A, 0x7D, 0x53, 0x7E, 0x23, 0x44, 0x43, 0x22, 0x09, 0x7B, 0x57, 0x2B,
          };
          memcpy(oric->rom + 0x3F78, keytab_patch, 0x0080);
      }
      load_patches( oric, pravetzromfile[0] );
      break;
    
    default:
      // Huh?!
      return SDL_FALSE;
  }

  oric->cyclesperraster = 64;
  oric->vid_start = 65;
  oric->vid_maxrast = 312;
  oric->vid_end     = oric->vid_start + 224;
  oric->vid_raster  = 0;
  ula_powerup_default( oric );

  oric->read_not_lightpen  = oric->cpu.read;
 
  if (oric->lightpen)
    oric->cpu.read  = lightpen_read;

  setromon( oric );
  oric->tapename[0] = 0;
  tape_rewind( oric );
  m6502_reset( &oric->cpu );
  via_init( &oric->via, oric, VIA_MAIN );
  via_init( &oric->tele_via, oric, VIA_TELESTRAT );
  acia_init( &oric->tele_acia, oric );
  ay_init( &oric->ay, oric );
  joy_setup( oric );
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
  if( oric->drivetype == DRV_PRAVETZ )    { pravetz_free( &oric->pravetz ); oric->drivetype = DRV_NONE; }
  if( oric->mem ) { free( oric->mem ); oric->mem = NULL; oric->rom = NULL; }
  if( oric->prf ) { fclose( oric->prf ); oric->prf = NULL; }
  if( oric->tsavf ) tape_stop_savepatch( oric );
  if( oric->tapecap ) toggletapecap( oric, &mainitems[1], 0 );
  mon_freesyms( &sym_microdisc );
  mon_freesyms( &sym_jasmin );
  mon_freesyms( &sym_pravetz );
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

  if( ( type == DRV_PRAVETZ ) &&
      ( oric->type != MACH_PRAVETZ ) )
  {
    swapmach( oric, mitem, (DRV_PRAVETZ<<16)|MACH_PRAVETZ );
    return;
  }

  shut_machine( oric );

  switch( type )
  {
    case DRV_MICRODISC:
    case DRV_JASMIN:
    case DRV_PRAVETZ:
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

  /* The contents of the disk panel depend on the disk type. */
  /* Wipe it to prevent garbage. */
  clear_textzone( oric, TZ_DISK );

  mon_state_reset( oric );
  if( !init_machine( oric, which, which!=oric->type ) )
  {
    shut();
    exit( EXIT_FAILURE );
  }
}

