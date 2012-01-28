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
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __amigaos4__
#include <proto/exec.h>
#include <proto/dos.h>
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
#include "filereq.h"
#include "msgbox.h"
#include "avi.h"
#include "main.h"
#include "ula.h"
#include "render_sw.h"
#include "render_gl.h"
#include "joystick.h"

#define FRAMES_TO_AVERAGE 15

SDL_bool need_sdl_quit = SDL_FALSE;
SDL_bool fullscreen, hwsurface;
extern SDL_bool warpspeed, soundon;
Uint32 lastframetimes[FRAMES_TO_AVERAGE], frametimeave;
extern char mon_bpmsg[];
extern struct avi_handle *vidcap;
extern struct symboltable usersyms;
extern char tapepath[], diskpath[], telediskpath[];
extern char atmosromfile[];
extern char oric1romfile[];
extern char mdiscromfile[];
extern char jasmnromfile[];
extern char pravzromfile[];
extern char telebankfiles[8][1024];

#ifdef __amigaos4__
int32 timersigbit = -1;
uint32 timersig;
struct Task *maintask;
char __attribute__((used)) stackcookie[] = "$STACK: 1000000";
#endif

#if defined(__amigaos4__) || defined(__MORPHOS__)
char __attribute__((used)) versiontag[] = "$VER: " APP_NAME_FULL " (20.9.2010)";
#endif

struct start_opts
{
  char     lctmp[2048];
  Sint32   start_machine;
  Sint32   start_disktype;
  Sint32   start_rendermode;
  SDL_bool start_debug;
  char     start_disk[1024];
  char     start_tape[1024];
  char     start_syms[1024];
  char    *start_breakpoint;
};

static char *machtypes[] = { "oric1",
                             "oric1-16k",
                             "atmos",
                             "telestrat",
                             "pravetz|pravetz8d",
                             NULL };

static char *disktypes[] = { "none",
                             "jasmin",
                             "microdisc",
                             "pravetz",
                             NULL };

static char *joyifacetypes[] = { "none",
                                 "altai|pase",
                                 "ijk",
                                 NULL };

static char *joymodes[] = { "none",
                            "kbjoy1",
                            "kbjoy2",
                            "sdljoy0",
                            "sdljoy1",
                            "sdljoy2",
                            "sdljoy3",
                            "sdljoy4",
                            "sdljoy5",
                            "sdljoy6",
                            "sdljoy7",
                            "sdljoy8",
                            "sdljoy9",
                            "mouse",
                            NULL };

static char *rendermodes[] = { "{{INVALID}}",
                               "soft",
                               "opengl",
                               NULL };

static char *swdepths[] = { "16", "32", NULL };

static SDL_bool istokend( char c )
{
  if( isws( c ) ) return SDL_TRUE;
  if( ( c == 0 ) || ( c == 10 ) || ( c == 13 ) ) return SDL_TRUE;
  return SDL_FALSE;
}

SDL_bool read_config_string( char *buf, char *token, char *dest, Sint32 maxlen )
{
  Sint32 i, toklen, d;

  // Get the token length
  toklen = strlen( token );

  // Is this the token?
  if( strncasecmp( buf, token, toklen ) != 0 ) return SDL_FALSE;
  i = toklen;

  // Check for whitespace, equals, whitespace, single quote
  while( isws( buf[i] ) ) i++;
  if( buf[i] != '=' ) return SDL_TRUE;
  i++;
  while( isws( buf[i] ) ) i++;
  if( buf[i] != '\'' ) return SDL_TRUE;
  i++;

  // Copy and un-escape the string
  d=0;
  while( buf[i] != '\'' )
  {
    if( d >= (maxlen-1) ) break;
    if( !buf[i] ) break;

    if( ( buf[i] == '\\' ) && ( buf[i+1] == '\'' ) )
    {
      dest[d++] = '\'';
      i+=2;
      continue;
    }

    if( ( buf[i] == '\\' ) && ( buf[i+1] == '\\' ) )
    {
      dest[d++] = '\\';
      i+=2;
      continue;
    }

    dest[d++] = buf[i++];
  }

  dest[d] = 0;
  return SDL_TRUE;
}

SDL_bool read_config_bool( char *buf, char *token, SDL_bool *dest )
{
  Sint32 i, toklen;

  // Get the token length
  toklen = strlen( token );

  // Is this the token?
  if( strncasecmp( buf, token, toklen ) != 0 ) return SDL_FALSE;
  i = toklen;

  // Check for whitespace, equals, whitespace, single quote
  while( isws( buf[i] ) ) i++;
  if( buf[i] != '=' ) return SDL_TRUE;
  i++;
  while( isws( buf[i] ) ) i++;

  (*dest) = SDL_FALSE;
  if( strncasecmp( &buf[i], "true", 4 ) == 0 ) (*dest) = SDL_TRUE;
  if( strncasecmp( &buf[i], "yes", 3 ) == 0 )  (*dest) = SDL_TRUE;
  return SDL_TRUE;
}

SDL_bool read_config_option( char *buf, char *token, Sint32 *dest, char **options )
{
  Sint32 i, j, len;
  char *thisopt;

  // Get the token length
  len = strlen( token );

  // Is this the token?
  if( strncasecmp( buf, token, len ) != 0 ) return SDL_FALSE;
  i = len;

  // Check for whitespace, equals, whitespace, single quote
  while( isws( buf[i] ) ) i++;
  if( buf[i] != '=' ) return SDL_TRUE;
  i++;
  while( isws( buf[i] ) ) i++;

  // Huh!?
  if( strncmp( &buf[i], "{{INVALID}}", 11 ) == 0 )
    return SDL_FALSE;

  for( j=0; options[j]; j++ )
  {
    thisopt = options[j];
    while( thisopt[0] )
    {
      len = 0;
      while( (thisopt[len]!=0) && (thisopt[len]!='|') ) len++;

      if( strncasecmp( &buf[i], thisopt, len ) == 0 )
      {
        if( istokend( buf[i+len] ) )
        {
          *dest = j;
          return SDL_TRUE;
        }
      }
      thisopt += len;
      if( thisopt[0] == '|' ) thisopt++;
    }
  }

  return SDL_TRUE;
}

SDL_bool read_config_int( char *buf, char *token, int *dest, int min, int max )
{
  Sint32 i, toklen;
  int val, hv;

  // Get the token length
  toklen = strlen( token );

  // Is this the token?
  if( strncasecmp( buf, token, toklen ) != 0 ) return SDL_FALSE;
  i = toklen;

  // Check for whitespace, equals, whitespace
  while( isws( buf[i] ) ) i++;
  if( buf[i] != '=' ) return SDL_TRUE;
  i++;
  while( isws( buf[i] ) ) i++;

  val = 0;
  if( buf[i] == '$' )
  {
    i++;
    while( (hv=hexit(buf[i])) != -1 )
    {
      val = (val<<4) + hv;
      i++;
    }
  } else {
    val = atoi( &buf[i] );
  }

  if( val < min ) val = min;
  if( val > max ) val = max;

  (*dest) = val;
  return SDL_TRUE;
}

SDL_bool read_config_joykey( char *buf, char *token, Sint16 *dest )
{
  Sint32 i, toklen, d;
  Sint16 thekey;
  char keyname[32];

  // Get the token length
  toklen = strlen( token );

  // Is this the token?
  if( strncasecmp( buf, token, toklen ) != 0 ) return SDL_FALSE;
  i = toklen;

  // Check for whitespace, equals, whitespace, single quote
  while( isws( buf[i] ) ) i++;
  if( buf[i] != '=' ) return SDL_TRUE;
  i++;
  while( isws( buf[i] ) ) i++;
  if( buf[i] != '\'' ) return SDL_TRUE;
  i++;

  // Copy and un-escape the string
  d=0;
  while( buf[i] != '\'' )
  {
    if( d >= 31 ) break;
    if( !buf[i] ) break;

    if( ( buf[i] == '\\' ) && ( buf[i+1] == '\'' ) )
    {
      keyname[d++] = '\'';
      i+=2;
      continue;
    }

    if( ( buf[i] == '\\' ) && ( buf[i+1] == '\\' ) )
    {
      keyname[d++] = '\\';
      i+=2;
      continue;
    }

    keyname[d++] = buf[i++];
  }

  keyname[d] = 0;

  thekey = joy_keyname_to_sym( keyname );

  if( thekey )
  {
    *dest = thekey;
    return SDL_TRUE;
  }

  return SDL_FALSE;
}

static void load_config( struct start_opts *sto, struct machine *oric )
{
  FILE *f;
  Sint32 i, j;
  char tbtmp[32];

  f = fopen( FILEPREFIX"oricutron.cfg", "r" );
  if( !f ) return;

  while( !feof( f ) )
  {
    if( !fgets( sto->lctmp, 2048, f ) ) break;

    for( i=0; isws( sto->lctmp[i] ); i++ ) ;

    if( read_config_option( &sto->lctmp[i], "machine",      &sto->start_machine, machtypes ) ) continue;
    if( read_config_option( &sto->lctmp[i], "disktype",     &sto->start_disktype, disktypes ) ) continue;
    if( read_config_bool(   &sto->lctmp[i], "debug",        &sto->start_debug ) ) continue;
    if( read_config_bool(   &sto->lctmp[i], "fullscreen",   &fullscreen ) ) continue;
    if( read_config_bool(   &sto->lctmp[i], "hwsurface",    &hwsurface ) ) continue;
    if( read_config_bool(   &sto->lctmp[i], "scanlines",    &oric->scanlines ) ) continue;
    if( read_config_bool(   &sto->lctmp[i], "hstretch",     &oric->hstretch ) ) continue;
    if( read_config_string( &sto->lctmp[i], "diskimage",    sto->start_disk, 1024 ) ) continue;
    if( read_config_string( &sto->lctmp[i], "tapeimage",    sto->start_tape, 1024 ) ) continue;
    if( read_config_string( &sto->lctmp[i], "symbols",      sto->start_syms, 1024 ) ) continue;
    if( read_config_string( &sto->lctmp[i], "tapepath",     tapepath, 1024 ) ) continue;
    if( read_config_string( &sto->lctmp[i], "diskpath",     diskpath, 1024 ) ) continue;
    if( read_config_string( &sto->lctmp[i], "telediskpath", telediskpath, 1024 ) ) continue;
    if( read_config_string( &sto->lctmp[i], "atmosrom",     atmosromfile, 1024 ) ) continue;
    if( read_config_string( &sto->lctmp[i], "oric1rom",     oric1romfile, 1024 ) ) continue;
    if( read_config_string( &sto->lctmp[i], "mdiscrom",     mdiscromfile, 1024 ) ) continue;
    if( read_config_string( &sto->lctmp[i], "jasminrom",    jasmnromfile, 1024 ) ) continue;
    if( read_config_string( &sto->lctmp[i], "pravetzrom",   pravzromfile, 1024 ) ) continue;
    if( read_config_int(    &sto->lctmp[i], "rampattern",   &oric->rampattern, 0, 1 ) ) continue;
    if( read_config_option( &sto->lctmp[i], "swdepth",      &oric->sw_depth, swdepths ) ) continue;
    if( read_config_option( &sto->lctmp[i], "rendermode",   &sto->start_rendermode, rendermodes ) )
    {
#ifndef __OPENGL_AVAILABLE__
      if( sto->start_rendermode == RENDERMODE_GL )
        sto->start_rendermode = RENDERMODE_SW;
#endif
      continue;
    }
    for( j=0; j<8; j++ )
    {
      sprintf( tbtmp, "telebank%c", j+'0' );
      if( read_config_string( &sto->lctmp[i], tbtmp, telebankfiles[j], 1024 ) ) break;
    }
    if( read_config_bool(   &sto->lctmp[i], "lightpen",     &oric->lightpen ) ) continue;
    if( read_config_option( &sto->lctmp[i], "joyinterface", &oric->joy_iface, joyifacetypes ) ) continue;
    if( read_config_option( &sto->lctmp[i], "joystick_a",   &oric->joymode_a,     joymodes ) ) continue;
    if( read_config_option( &sto->lctmp[i], "joystick_b",   &oric->joymode_b,     joymodes ) ) continue;
    if( read_config_option( &sto->lctmp[i], "telejoy_a",    &oric->telejoymode_a, joymodes ) ) continue;
    if( read_config_option( &sto->lctmp[i], "telejoy_b",    &oric->telejoymode_b, joymodes ) ) continue;
    if( read_config_joykey( &sto->lctmp[i], "kbjoy1_up",    &oric->kbjoy1[0] ) ) continue;
    if( read_config_joykey( &sto->lctmp[i], "kbjoy1_down",  &oric->kbjoy1[1] ) ) continue;
    if( read_config_joykey( &sto->lctmp[i], "kbjoy1_left",  &oric->kbjoy1[2] ) ) continue;
    if( read_config_joykey( &sto->lctmp[i], "kbjoy1_right", &oric->kbjoy1[3] ) ) continue;
    if( read_config_joykey( &sto->lctmp[i], "kbjoy1_fire1", &oric->kbjoy1[4] ) ) continue;
    if( read_config_joykey( &sto->lctmp[i], "kbjoy1_fire2", &oric->kbjoy1[5] ) ) continue;
    if( read_config_joykey( &sto->lctmp[i], "kbjoy2_up",    &oric->kbjoy2[0] ) ) continue;
    if( read_config_joykey( &sto->lctmp[i], "kbjoy2_down",  &oric->kbjoy2[1] ) ) continue;
    if( read_config_joykey( &sto->lctmp[i], "kbjoy2_left",  &oric->kbjoy2[2] ) ) continue;
    if( read_config_joykey( &sto->lctmp[i], "kbjoy2_right", &oric->kbjoy2[3] ) ) continue;
    if( read_config_joykey( &sto->lctmp[i], "kbjoy2_fire1", &oric->kbjoy2[4] ) ) continue;
    if( read_config_joykey( &sto->lctmp[i], "kbjoy2_fire2", &oric->kbjoy2[5] ) ) continue;
  }

  fclose( f );
}

static void usage( int ret )
{
  printf( VERSION_COPYRIGHTS "\n\n"
          "Usage:\toricutron [-a|--arg [option]] [disk file] [tape file]\n"
          "  -m / --machine    = Specify machine type. Valid types are:\n"
          "\n"
          "                      \"atmos\" or \"a\" for Oric Atmos\n"
          "                      \"oric1\" or \"1\" for Oric-1\n"
          "                      \"o16k\" for Oric-1 16k\n"
          "                      \"telestrat\" or \"t\" for Telestrat\n"
          "                      \"pravetz\", \"pravetz8d\" or \"p\" for Pravetz 8D\n"
          "\n"
          "  -d / --disk       = Specify a disk image to use in drive 0\n"
          "  -t / --tape       = Specify a tape image to use\n"
          "  -k / --drive      = Specify a disk drive controller. Valid types are:\n"
          "  \n"
          "                      \"microdisc\" or \"m\" for Microdisc\n"
          "                      \"jasmin\" or \"j\" for Jasmin\n"
//          "                      \"pravetz\" or \"p\" for Pravetz\n"
          "\n"
          "  -s / --symbols    = Load symbols from a file\n"
          "  -f / --fullscreen = Run oricutron fullscreen\n"
          "  -w / --window     = Run oricutron in a window\n"
#ifdef __OPENGL_AVAILABLE__
          "  -R / --rendermode = Render mode. Valid modes are:\n"
          "\n"
          "                      \"soft\" for software rendering\n"
          "                      \"opengl\" for OpenGL\n"
          "\n"
#endif
          "  -b / --debug      = Start oricutron in the debugger\n"
          "  -r / --breakpoint = Set a breakpoint\n" );
  exit(ret);
}

SDL_bool init( struct machine *oric, int argc, char *argv[] )
{
  Sint32 i;
  struct start_opts *sto;
  char opt_type, *opt_arg, *tmp;

  sto = malloc( sizeof( struct start_opts ) );
  if( !sto ) return SDL_FALSE;

  // Defaults
  sto->start_machine  = MACH_ATMOS;
  sto->start_disktype = DRV_NONE;
  sto->start_debug    = SDL_FALSE;
  sto->start_rendermode = RENDERMODE_SW;
  sto->start_disk[0]  = 0;
  sto->start_tape[0]  = 0;
  sto->start_syms[0]  = 0;
  sto->start_breakpoint = NULL;
  fullscreen          = SDL_FALSE;
#ifdef WIN32
  hwsurface           = SDL_TRUE;
#else
  hwsurface           = SDL_FALSE;
#endif

  preinit_ula( oric );
  preinit_machine( oric );
  preinit_render_sw( oric );
#ifdef __OPENGL_AVAILABLE__
  preinit_render_gl( oric );
#endif
  preinit_gui( oric );

  // Go SDL!
  if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO ) < 0 )
  {
    printf( "SDL init failed\n" );
    return SDL_FALSE;
  }
  need_sdl_quit = SDL_TRUE;

  SDL_WM_SetIcon( SDL_LoadBMP( IMAGEPREFIX"winicon.bmp" ), NULL );

  render_sw_detectvideo( oric );

  load_config( sto, oric );

  for( i=1; i<argc; i++ )
  {
    opt_type = 0;
    if( argv[i][0] == '-' )
    {
      switch( argv[i][1] )
      {
        case '-':  // Long argument types
          opt_arg = NULL;
          tmp = &argv[i][2];

          if( strcasecmp( tmp, "fullscreen" ) == 0 ) { opt_type = 'f'; break; }
          if( strcasecmp( tmp, "window"     ) == 0 ) { opt_type = 'w'; break; }
          if( strcasecmp( tmp, "debug"      ) == 0 ) { opt_type = 'b'; break; }
          if( strcasecmp( tmp, "hwsurface"  ) == 0 ) { hwsurface = SDL_TRUE; break; }
          if( strcasecmp( tmp, "swsurface"  ) == 0 ) { hwsurface = SDL_FALSE; break; }
          if( strcasecmp( tmp, "help"       ) == 0 ) { opt_type = 'h'; break; }

          if( i<(argc-1) )
            opt_arg = argv[i+1];
          i++;

          if( strcasecmp( tmp, "machine"    ) == 0 ) { opt_type = 'm'; break; }
          if( strcasecmp( tmp, "disk"       ) == 0 ) { opt_type = 'd'; break; }
          if( strcasecmp( tmp, "tape"       ) == 0 ) { opt_type = 't'; break; }
          if( strcasecmp( tmp, "drive"      ) == 0 ) { opt_type = 'k'; break; }
          if( strcasecmp( tmp, "symbols"    ) == 0 ) { opt_type = 's'; break; }
          if( strcasecmp( tmp, "breakpoint" ) == 0 ) { opt_type = 'r'; break; }
#ifdef __OPENGL_AVAILABLE__
          if( strcasecmp( tmp, "rendermode" ) == 0 ) { opt_type = 'R'; break; }
#endif
          break;
        
        default:
          opt_type = argv[i][1];
          opt_arg = NULL;
          if (argv[i][2])
            opt_arg = &argv[i][2];
          else if ( i<(argc-1))
          {
            switch (opt_type)
            {
              case 'm':
              case 'd':
              case 't':
              case 'k':
              case 's':
              case 'r':
#ifdef __OPENGL_AVAILABLE__
              case 'R':
#endif
              opt_arg = argv[i+1];
                i++;
            }
          }
          break;
      }

      switch( opt_type )
      {
        case 'm':  // Machine type
          if( opt_arg )
          {
            if(( strcasecmp( opt_arg, "atmos"     ) == 0 ) ||
               ( strcasecmp( opt_arg, "a"         ) == 0 )) { sto->start_machine = MACH_ATMOS;     break; }
            if(( strcasecmp( opt_arg, "oric1"     ) == 0 ) ||
               ( strcasecmp( opt_arg, "1"         ) == 0 )) { sto->start_machine = MACH_ORIC1;     break; }
            if(  strcasecmp( opt_arg, "o16k"      ) == 0 )  { sto->start_machine = MACH_ORIC1_16K; break; }
            if(( strcasecmp( opt_arg, "telestrat" ) == 0 ) ||
               ( strcasecmp( opt_arg, "t"         ) == 0 )) { sto->start_machine = MACH_TELESTRAT; break; }
            if(( strcasecmp( opt_arg, "pravetz"   ) == 0 ) ||
               ( strcasecmp( opt_arg, "pravetz8d" ) == 0 ) ||
               ( strcasecmp( opt_arg, "p"         ) == 0 )) { sto->start_machine = MACH_PRAVETZ;   break; }
          }
          
          printf( "Invalid machine type\n" );
          break;
        
        case 'd':  // Disk image
          if( opt_arg )
          {
            strncpy( sto->start_disk, opt_arg, 1024 );
            sto->start_disk[1023] = 0;
          } else {
            printf( "No disk image specified\n" );
          }
          break;
        
        case 't':  // Tape image
          if( opt_arg )
          {
            strncpy( sto->start_tape, opt_arg, 1024 );
            sto->start_tape[1023] = 0;
          } else {
            printf( "No tape image specified\n" );
          }          
          break;
   
        case 'k':  // Drive controller type
          if( opt_arg )
          {
            if(( strcasecmp( opt_arg, "microdisc" ) == 0 ) ||
               ( strcasecmp( opt_arg, "m"         ) == 0 )) { sto->start_disktype = DRV_MICRODISC; break; }
            if(( strcasecmp( opt_arg, "jasmin"    ) == 0 ) ||
               ( strcasecmp( opt_arg, "j"         ) == 0 )) { sto->start_disktype = DRV_JASMIN;    break; }
            if(( strcasecmp( opt_arg, "pravetz"   ) == 0 ) ||
               ( strcasecmp( opt_arg, "p"         ) == 0 )) { sto->start_disktype = DRV_PRAVETZ;   break; }
          }

          printf( "Invalid drive type\n" );
          break;
        
        case 's':  // Pre-load symbols file
          if( opt_arg )
          {
            strncpy( sto->start_syms, opt_arg, 1024 );
            sto->start_syms[1023] = 0;
          } else {
            printf( "No symbols file specified\n" );
          }          
          break;
        
        case 'f':
          fullscreen = SDL_TRUE;
          break;
        
        case 'w':
          fullscreen = SDL_FALSE;
          break;
        
        case 'b':
          sto->start_debug = SDL_TRUE;
          break;
        
        case 'r': // Breakpoint
          if( opt_arg )
            sto->start_breakpoint = opt_arg;
          else
            printf( "Breakpoint address or symbol expected\r\n" );
          break;

#ifdef __OPENGL_AVAILABLE__
        case 'R': // Render mode
          if( opt_arg )
          {
            if( strcasecmp( opt_arg, "soft" ) == 0 )   { sto->start_rendermode = RENDERMODE_SW; break; }
            if( strcasecmp( opt_arg, "opengl" ) == 0 ) { sto->start_rendermode = RENDERMODE_GL; break; }
          }

          printf( "Invalid render mode\n" );
          break;
#endif

        case 'h':
          usage( EXIT_SUCCESS );
          break;
      }        
    }
    else
    {
      char *p = strrchr(argv[i], '.');
      if( p && ( strcasecmp(p, ".dsk") == 0 ) )
      {
        strncpy( sto->start_disk, argv[i], 1024 );
        sto->start_disk[1023] = 0;
      }
      else if( p && ( strcasecmp(p, ".tap") == 0 ) )
      {
        strncpy( sto->start_tape, argv[i], 1024 );
        sto->start_tape[1023] = 0;
      }
      else
        usage( EXIT_FAILURE );
    }
  }

  load_diskroms( oric );

  if( ( sto->start_disk[0] ) && ( sto->start_disktype == DRV_NONE ) )
    sto->start_disktype = DRV_MICRODISC;

  for( i=0; i<8; i++ ) lastframetimes[i] = 0;
  frametimeave = 0;

#ifdef __amigaos4__
  timersigbit = IExec->AllocSignal( -1 );
  if( timersigbit == -1 ) { free( sto ); return SDL_FALSE; }
  timersig = 1L<<timersigbit;
  
  maintask = IExec->FindTask( NULL );
#endif

  if( !init_gui( oric, sto->start_rendermode ) ) { free( sto ); return SDL_FALSE; }
  if( !init_filerequester( oric ) ) { free( sto ); return SDL_FALSE; }
  if( !init_msgbox( oric ) ) { free( sto ); return SDL_FALSE; }
  oric->drivetype = sto->start_disktype;
  if( !init_ula( oric ) ) { free( sto ); return SDL_FALSE; }
  if( !init_joy( oric ) ) { free( sto ); return SDL_FALSE; }
  if( !init_machine( oric, sto->start_machine, SDL_TRUE ) ) { free( sto ); return SDL_FALSE; }

  if( sto->start_disk[0] ) diskimage_load( oric, sto->start_disk, 0 );
  if( sto->start_tape[0] )
  {
    if( tape_load_tap( oric, sto->start_tape ) )
      queuekeys( "CLOAD\"\"\x0d" );
  }

  mon_init( oric );
  if( sto->start_syms[0] )
    mon_new_symbols( &usersyms, oric, sto->start_syms, SYM_BESTGUESS, SDL_TRUE, SDL_TRUE );

  if( sto->start_breakpoint )
  {
    int i;
    unsigned int addr;
    if( !mon_getnum( oric, &addr, sto->start_breakpoint, &i, SDL_FALSE, SDL_FALSE, SDL_FALSE, SDL_TRUE ) )
    {
      printf( "Invalid breakpoint\n" );
      free( sto );
      return SDL_FALSE;
    }
    
    oric->cpu.breakpoints[0] = addr;
    oric->cpu.anybp = SDL_TRUE;
  }

  if( sto->start_debug )
    setemumode( oric, NULL, EM_DEBUG );

  free( sto );
  return SDL_TRUE;
}

void shut( struct machine *oric )
{
  if( vidcap ) avi_close( &vidcap );
  if( oric )
  {
    shut_machine( oric );
    shut_joy( oric );
    shut_ula( oric );
    mon_shut();
    shut_filerequester( oric );
    shut_msgbox( oric );
    shut_gui( oric );
  }
  if( need_sdl_quit ) SDL_Quit();
#ifdef __amigaos4__
  IExec->FreeSignal( timersigbit );
#endif
}

// Stupid 10ms granularity bastard.
static Uint32 time50[] = { 20, 20, 20 };
static Uint32 time60[] = { 20, 20, 10 };
static int tmoffs = 0;

Uint32 systemtiming( Uint32 interval, void *userdata )
{
  struct machine *oric = (struct machine *)userdata;
  if( oric->emu_mode == EM_RUNNING )
  {
#if __amigaos4__
    IExec->Signal( maintask, timersig );
#else
    SDL_Event     event;
    SDL_UserEvent userevent;

    userevent.type  = SDL_USEREVENT;
    userevent.code  = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;
    
    event.type = SDL_USEREVENT;
    event.user = userevent;
    
    SDL_PushEvent( &event );
#endif
  }

  tmoffs = (tmoffs+1)%3;
  return oric->vid_freq ? time50[tmoffs] : time60[tmoffs];
}

int main( int argc, char *argv[] )
{
  SDL_bool isinit;
  void *thetimer;
  Sint32 i, foo;
  struct machine oric;
  Uint32 framestart;

  if( ( isinit = init( &oric, argc, argv ) ) )
  {
    SDL_bool done, needrender, framedone;

    thetimer = SDL_AddTimer( 1000/50, (SDL_NewTimerCallback)systemtiming, (void *)&oric );
    foo = 0;
    framestart = 0;

    done = SDL_FALSE;
    needrender = SDL_TRUE;
    framedone = SDL_FALSE;
    while( !done )
    {
      SDL_Event event;

      if( oric.emu_mode == EM_PLEASEQUIT )
        break;

      if( needrender )      {
        render( &oric );
        needrender = SDL_FALSE;
      }
      
      if( oric.emu_mode == EM_RUNNING )
      {
        if( !framedone )
        {
          if( framestart != 0 )
          {
            frametimeave = 0;
            for( i=(FRAMES_TO_AVERAGE-1); i>0; i-- )
            {
              lastframetimes[i] = lastframetimes[i-1];
              frametimeave += lastframetimes[i];
            }
            lastframetimes[0] = SDL_GetTicks() - framestart;
            frametimeave = (frametimeave+lastframetimes[0])/FRAMES_TO_AVERAGE;
          }

          framestart = SDL_GetTicks();

          while( ( !framedone ) && ( !needrender ) )
          {
            while( oric.cpu.rastercycles > 0 )
            {
              if( m6502_set_icycles( &oric.cpu, SDL_TRUE, mon_bpmsg ) )
              {
                setemumode( &oric, NULL, EM_DEBUG );
                needrender = SDL_TRUE;
                break;
              }

              via_clock( &oric.via, oric.cpu.icycles );
              ay_ticktock( &oric.ay, oric.cpu.icycles );
              if( oric.drivetype ) wd17xx_ticktock( &oric.wddisk, oric.cpu.icycles );
              if( oric.type == MACH_TELESTRAT )
              {
                via_clock( &oric.tele_via, oric.cpu.icycles );
                acia_clock( &oric.tele_acia, oric.cpu.icycles );
              }
              m6502_inst( &oric.cpu );
            }

            if( oric.cpu.rastercycles <= 0 )
            {
              framedone = ula_doraster( &oric );
              oric.cpu.rastercycles += oric.cyclesperraster;
            }
          }
        }

        ay_unlockaudio( &oric.ay );

        if( warpspeed )
        {
          if( framedone )
          {
            if( (oric.emu_mode == EM_RUNNING) && ((oric.frames&3)!=0) )
            {
              framedone = SDL_FALSE;
              continue;
            }
            needrender = SDL_TRUE;
            framedone  = SDL_FALSE;
          }
          if( !SDL_PollEvent( &event ) ) continue;
        } else {
          if( needrender )
          {
            if( !SDL_PollEvent( &event ) ) continue;
          } else {
#ifdef __amigaos4__
            uint32 gotsigs;

            gotsigs = IExec->Wait( timersig | SIGBREAKF_CTRL_C );
            if( gotsigs & SIGBREAKF_CTRL_C ) done = TRUE;
            if( gotsigs & timersig )
            {
              needrender = SDL_TRUE;
              framedone  = SDL_FALSE;
              if( !SDL_PollEvent( &event ) ) continue;
            }
#else
            if( !SDL_WaitEvent( &event ) ) break;
#endif
          }
        }
      } else {
        ay_unlockaudio( &oric.ay );
        if( !SDL_WaitEvent( &event ) ) break;
      }

      switch( event.type )
      {
        case SDL_USEREVENT:
          needrender = SDL_TRUE;
          framedone  = SDL_FALSE;
          break;

        case SDL_QUIT:
          done = SDL_TRUE;
          break;
        
        default:
          switch( oric.emu_mode )
          {
            case EM_MENU:
              done |= menu_event( &event, &oric, &needrender );
              break;
 
            case EM_RUNNING:
              done |= emu_event( &event, &oric, &needrender );
              break;

            case EM_DEBUG:
              done |= mon_event( &event, &oric, &needrender );
              break;
          }
      }
    }
    ay_unlockaudio( &oric.ay );
  }
  shut( &oric );

  return isinit ? EXIT_SUCCESS : EXIT_FAILURE;
}
