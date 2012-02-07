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
extern SDL_bool refreshstatus, refreshavi;
extern struct osdmenuitem mainitems[];

char atmosromfile[1024];
char oric1romfile[1024];
char mdiscromfile[1024];
char jasmnromfile[1024];
char pravzromfile[1024];
char telebankfiles[8][1024];

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

  if( oric->lightpen )
  {
    if( addr == 0x3e0 ) return oric->lightpenx;
    if( addr == 0x3e1 ) return oric->lightpeny;
  }

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

  if( oric->lightpen )
  {
    if( addr == 0x3e0 ) return oric->lightpenx;
    if( addr == 0x3e1 ) return oric->lightpeny;
  }

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

  if( oric->lightpen )
  {
    if( addr == 0x3e0 ) return oric->lightpenx;
    if( addr == 0x3e1 ) return oric->lightpeny;
  }

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

  if( oric->lightpen )
  {
    if( addr == 0x3e0 ) return oric->lightpenx;
    if( addr == 0x3e1 ) return oric->lightpeny;
  }

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

  if( oric->lightpen )
  {
    if( addr == 0x3e0 ) return oric->lightpenx;
    if( addr == 0x3e1 ) return oric->lightpeny;
  }

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

  if( oric->lightpen )
  {
    if( addr == 0x3e0 ) return oric->lightpenx;
    if( addr == 0x3e1 ) return oric->lightpeny;
  }

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

  if( oric->lightpen )
  {
    if( addr == 0x3e0 ) return oric->lightpenx;
    if( addr == 0x3e1 ) return oric->lightpeny;
  }

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
  oric->lightpenx = 255;
  oric->lightpeny = 255;

  oric->tsavf = NULL;
}

void load_diskroms( struct machine *oric )
{
  microdiscrom_valid = load_rom( oric, mdiscromfile, 8192, rom_microdisc, &sym_microdisc, SYMF_ROMDIS1|SYMF_MICRODISC );
  jasminrom_valid    = load_rom( oric, jasmnromfile, 2048, rom_jasmin,    &sym_jasmin,    SYMF_ROMDIS1|SYMF_JASMIN );
}

// This is currently used to workaround a change in the behaviour of SDL
// on OS4, but in future it would be a handy place to fix keyboard layout
// issues, such as the problems with a non-uk keymap on linux.
int mapkey( int key )
{
#ifdef __amigaos4__
  switch( key )
  {
    case '@': return '#';
    case ':': return ';';
    case '<': return ',';
    case '>': return '.';
    case '?': return '/';
    case '~': return '\'';
    case '_': return '-';
    case '+': return '=';
    case '{': return '[';
    case '}': return ']';
  }
#endif
  return key;
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
      if( ev->button.button == SDL_BUTTON_LEFT )
      {
        oric->lightpenx = ev->button.x/2.5;
        oric->lightpeny = ev->button.y/1.875;
      }
      if( ev->button.button == SDL_BUTTON_RIGHT )
        setemumode( oric, NULL, EM_MENU );
      *needrender = SDL_TRUE;
      break;

    case SDL_MOUSEBUTTONUP:
      if( ev->button.button == SDL_BUTTON_LEFT )
      {
        oric->lightpenx = 255;
        oric->lightpeny = 255;
      }
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
      hwopitems[0].name = " Oric-1";
      hwopitems[1].name = "\x0e""Oric-1 16K";
      hwopitems[2].name = " Atmos";
      hwopitems[3].name = " Telestrat";
      hwopitems[4].name = " Pravetz 8D";
      oric->mem = malloc( 16384 + 16384 );
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

        case DRV_PRAVETZ:
        default:
          oric->drivetype = DRV_NONE;
          oric->cpu.read = o16kread;
          oric->cpu.write = o16kwrite;
          oric->romdis = SDL_FALSE;
          oric->disksyms = NULL;
          break;
      }

      if( !load_rom( oric, oric1romfile, -16384, &oric->rom[0], &oric->romsyms, SYMF_ROMDIS0 ) ) return SDL_FALSE;
      load_patches( oric, oric1romfile );

      oric->cyclesperraster = 64;
      oric->vid_start = 65;
      oric->vid_maxrast = 312;
      oric->vid_end     = oric->vid_start + 224;
      oric->vid_raster  = 0;
      ula_powerup_default( oric );
      break;
    
    case MACH_ORIC1:
      hwopitems[0].name = "\x0e""Oric-1";
      hwopitems[1].name = " Oric-1 16K";
      hwopitems[2].name = " Atmos";
      hwopitems[3].name = " Telestrat";
      hwopitems[4].name = " Pravetz 8D";
      oric->mem = malloc( 65536 + 16384 );
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

        case DRV_PRAVETZ:
        default:
          oric->drivetype = DRV_NONE;
          oric->cpu.read = atmosread;
          oric->cpu.write = atmoswrite;
          oric->romdis = SDL_FALSE;
          oric->disksyms = NULL;
          break;
      }

      if( !load_rom( oric, oric1romfile, -16384, &oric->rom[0], &oric->romsyms, SYMF_ROMDIS0 ) ) return SDL_FALSE;
      load_patches( oric, oric1romfile );

      oric->cyclesperraster = 64;
      oric->vid_start = 65;
      oric->vid_maxrast = 312;
      oric->vid_end     = oric->vid_start + 224;
      oric->vid_raster  = 0;
      ula_powerup_default( oric );
      break;
    
    case MACH_ATMOS:
      hwopitems[0].name = " Oric-1";
      hwopitems[1].name = " Oric-1 16K";
      hwopitems[2].name = "\x0e""Atmos";
      hwopitems[3].name = " Telestrat";
      hwopitems[4].name = " Pravetz 8D";
      oric->mem = malloc( 65536 + 16384 );
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

        case DRV_PRAVETZ:
        default:
          oric->drivetype = DRV_NONE;
          oric->cpu.read = atmosread;
          oric->cpu.write = atmoswrite;
          oric->romdis = SDL_FALSE;
          oric->disksyms = NULL;
          break;
      }

      if( !load_rom( oric, atmosromfile, -16384, &oric->rom[0], &oric->romsyms, SYMF_ROMDIS0 ) ) return SDL_FALSE;
      load_patches( oric, atmosromfile );

      oric->cyclesperraster = 64;
      oric->vid_start = 65;
      oric->vid_maxrast = 312;
      oric->vid_end     = oric->vid_start + 224;
      oric->vid_raster  = 0;
      ula_powerup_default( oric );
      break;

    case MACH_TELESTRAT:
      hwopitems[0].name = " Oric-1";
      hwopitems[1].name = " Oric-1 16K";
      hwopitems[2].name = " Atmos";
      hwopitems[3].name = "\x0e""Telestrat";
      hwopitems[4].name = " Pravetz 8D";
      oric->mem = malloc( 65536+16384*7 );
      if( !oric->mem )
      {
        printf( "Out of memory\n" );
        return SDL_FALSE;
      }

      blank_ram( oric->rampattern, oric->mem, 65536+16384*7 );      

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
          if( !load_rom( oric, telebankfiles[i], -16384, oric->tele_bank[i].ptr, &oric->tele_banksyms[i], SYMF_TELEBANK0<<i ) ) return SDL_FALSE;
          load_patches( oric, telebankfiles[i] );
        } else {
          oric->tele_bank[i].type = TELEBANK_RAM;
        }
      }

      clear_patches( oric );

      oric->tele_currbank = 7;
      oric->tele_banktype = oric->tele_bank[7].type;
      oric->rom           = oric->tele_bank[7].ptr;

      oric->cyclesperraster = 64;
      oric->vid_start = 65;
      oric->vid_maxrast = 312;
      oric->vid_end     = oric->vid_start + 224;
      oric->vid_raster  = 0;
      ula_powerup_default( oric );
      break;

    case MACH_PRAVETZ:
      hwopitems[0].name = " Oric-1";
      hwopitems[1].name = " Oric-1 16K";
      hwopitems[2].name = " Atmos";
      hwopitems[3].name = " Telestrat";
      hwopitems[4].name = "\x0e""Pravetz 8D";
      oric->mem = malloc( 65536 + 16384 );
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

        case DRV_PRAVETZ:
        default:
          oric->drivetype = DRV_NONE;
          oric->cpu.read = atmosread;
          oric->cpu.write = atmoswrite;
          oric->romdis = SDL_FALSE;
          oric->disksyms = NULL;
          break;
      }

      if( !load_rom( oric, pravzromfile, -16384, &oric->rom[0], &oric->romsyms, SYMF_ROMDIS0 ) ) return SDL_FALSE;
      load_patches( oric, pravzromfile );

      oric->cyclesperraster = 64;
      oric->vid_start = 65;
      oric->vid_maxrast = 312;
      oric->vid_end     = oric->vid_start + 224;
      oric->vid_raster  = 0;
      ula_powerup_default( oric );
      break;
  }

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
  if( oric->mem ) { free( oric->mem ); oric->mem = NULL; oric->rom = NULL; }
  if( oric->prf ) { fclose( oric->prf ); oric->prf = NULL; }
  if( oric->tsavf ) tape_stop_savepatch( oric );
  if( oric->tapecap ) toggletapecap( oric, &mainitems[1], 0 );
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
    
    case DRV_PRAVETZ:
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

