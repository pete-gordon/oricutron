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
**  SDL based file requester
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "machine.h"
#include "filereq.h"

// Externs
extern SDL_Surface *screen;
static struct textzone *tz = NULL;

// A directory entry in the file requester
struct frq_ent
{
  char name[512];
  char showname[40];
  SDL_bool isdir;
};

// A text input box (used in the file requester)
struct frq_textbox
{
  char *buf;
  int x, y, w;
  int maxlen, slen, cpos, vpos;
};

// The actual text box structures used by the file requester
static struct frq_textbox freqf_tbox[] = { { NULL, 7, 28, 32, 4096, 0, 0, 0 },
                                           { NULL, 7, 30, 32,  512, 0, 0, 0 } };

// File requester state
static int freqf_size=0, freqf_used=0, freqf_cgad=2;
static struct frq_ent *freqfiles=NULL;
static int freqf_clicktime=0;

SDL_bool init_filerequester( void )
{
  tz = alloc_textzone( 160, 48, 40, 32, "Files" );
  if( !tz ) return SDL_FALSE;

  return SDL_TRUE;
}

void shut_filerequester( void )
{
  if( tz ) free_textzone( tz );
}

// Render the filerequester
static void filereq_render( struct machine *oric )
{
  if( SDL_MUSTLOCK( screen ) )
    SDL_LockSurface( screen );

  video_show( oric );
  draw_textzone( tz );

  if( SDL_MUSTLOCK( screen ) )
    SDL_UnlockSurface( screen );

  SDL_Flip( screen );
}

// Add a file to the list of files to show in the file requester
static void filereq_addent( SDL_bool isdir, char *name, char *showname )
{
  struct frq_ent *tmpf;
  int i, j;

  // No file buffer yet, or file buffer full?
  if( ( !freqfiles ) || ( freqf_used >= freqf_size ) )
  {
    // (re)allocate it!
    tmpf = (struct frq_ent *)realloc( freqfiles, sizeof( struct frq_ent ) * (freqf_size+8) );
    if( !tmpf ) return;

    freqfiles   = tmpf;
    freqf_size += 8;
  }

  // Get the point to insert it
  j = 0;
  if( freqf_used > 0 )
  {
    if( isdir )
    {
      for( j=1; j<freqf_used; j++ )
      {
        if( strcasecmp( name, freqfiles[j].name ) <= 0 ) break;
        if( !freqfiles[j].isdir ) break;
      }
    } else {
      for( j=1; j<freqf_used; j++ )
        if( !freqfiles[j].isdir ) break;

      for( ; j<freqf_used; j++ )
        if( strcasecmp( name, freqfiles[j].name ) <= 0 ) break;
    }

    // Move everything down to make space
    if( j < freqf_used )
    {
      for( i=freqf_used; i>j; i-- )
        freqfiles[i] = freqfiles[i-1];
    }
  }

  // Fill in this structure
  freqfiles[j].isdir = isdir;
  strncpy( freqfiles[j].name, name, 512 );
  strncpy( freqfiles[j].showname, showname, 38 );
  freqfiles[j].name[511] = 0;
  freqfiles[j].showname[37] = 0;

  // I dun it!!11
  freqf_used++;
}

// Scan a directory to show in the filerequester
static SDL_bool filereq_scan( char *path )
{
  DIR *dh;
  struct dirent *de;
  struct stat sb;
  char *odir, *tpath;

  tpath = path;
#ifndef __amigaos4__
  if( !path[0] )
    tpath = ".";
#endif

  freqf_used = 0;

  dh = opendir( tpath );
  if( !dh ) return SDL_FALSE;

  odir = getcwd( NULL, 0 );
  chdir( tpath );

  filereq_addent( SDL_TRUE, "", "[Parent]" );

  while( ( de = readdir( dh ) ) )
  {
    if( ( strcmp( de->d_name, "." ) == 0 ) ||
        ( strcmp( de->d_name, ".." ) == 0 ) )
      continue;
    stat( de->d_name, &sb );
    filereq_addent( S_ISDIR(sb.st_mode), de->d_name, de->d_name );
  }

  closedir( dh );

  chdir( odir );
  free( odir );
  return SDL_TRUE;
}

// Render the file list into the filerequester textzone
static void filereq_showfiles( int offset, int cfile )
{
  int i, j, o;

  // If the listview isn't selected, don't show a current file
  if( freqf_cgad != 2 )
    cfile = -1;

  for( i=0; i<26; i++ )
  {
    // Set the colours
    if( (i+offset) < freqf_used )
      tzsetcol( tz, freqfiles[i+offset].isdir ? 1 : 0, (i+offset)==cfile ? 7 : 6 );
    else
      tzsetcol( tz, 0, 6 );

    // Clear this line
    o = (i+1)*tz->w+1;
    for( j=0; j<38; j++, o++ )
    {
      tz->fc[o] = tz->cfc;
      tz->bc[o] = tz->cbc;
      tz->tx[o] = 32;
    }

    if( (i+offset) >= freqf_used )
      continue;

    // Print the name
    tzstrpos( tz, 1, i+1, freqfiles[i+offset].showname );
  }
}

// Set a filerequester textbox contents to the specified string
static void filereq_settbox( struct frq_textbox *tb, char *buf )
{
  // Set up the buffer
  tb->buf  = buf;
  tb->slen = strlen( buf );

  // Make sure the cursor is in view
  tb->cpos = tb->slen;
  tb->vpos = tb->slen > (tb->w-1) ? tb->slen-(tb->w-1) : 0;
}

// Draw a textbox into the filerequester textzone
static void filereq_drawtbox( struct frq_textbox *tb, SDL_bool active )
{
  int i, j, o;

  tzsetcol( tz, 0, active ? 7 : 6 );
  o = (tb->y*tz->w)+tb->x;
  for( i=0,j=tb->vpos; i<tb->w; i++, o++, j++ )
  {
    if( ( active ) && ( j==tb->cpos ) )
    {
      tz->fc[o] = tz->cbc;
      tz->bc[o] = tz->cfc;
    } else {
      tz->fc[o] = tz->cfc;
      tz->bc[o] = tz->cbc;
    }
    if( j < tb->slen )
      tz->tx[o] = tb->buf[j];
    else
      tz->tx[o] = 32;
  }
}

// This sets the file textbox to the currently highlighted file
// It also stores the filename in "fname"
static void filereq_setfiletbox( int cfile, char *fname )
{
  if( ( cfile < 0 ) || ( cfile >= freqf_used ) ||
      ( freqfiles[cfile].isdir ) )
  {
    fname[0] = 0;
    freqf_tbox[1].cpos=0;
    freqf_tbox[1].vpos=0;
    freqf_tbox[1].slen=0;
    return;
  }

  strncpy( fname, freqfiles[cfile].name, 512 );
  fname[511] = 0;
  filereq_settbox( &freqf_tbox[1], fname );
}

enum
{
  ACTION_NONE = 0,
  ACTION_SELECTITEM,
  ACTION_PAGEUP,
  ACTION_PAGEDOWN
};

// This routine displays a file requester and waits for the user to select a file
//   title = title at the top of the requester
//   path  = initial path to show
//   fname = initial contents of the filename textbox
// It returns true if the user selected a file, although the file is not
// guarranteed to exist.
SDL_bool filerequester( struct machine *oric, char *title, char *path, char *fname, int type )
{
  SDL_Event event;
  struct frq_textbox *tb;
  int top=0,cfile=0,i,mx,my,mclick;
  SDL_bool shifty = SDL_FALSE, wasunicode;
  int doaction;

  wasunicode = SDL_EnableUNICODE( SDL_TRUE );
  SDL_EnableKeyRepeat( SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL );

  filereq_settbox( &freqf_tbox[0], path );
  filereq_settbox( &freqf_tbox[1], fname );

  tzsettitle( tz, title );
  tzsetcol( tz, 2, 3 );
  tzstrpos( tz, 1, 28, "Path:" );
  tzstrpos( tz, 1, 30, "File:" );

  if( !filereq_scan( path ) )
  {
    if( path[0] )
    {
      path[0] = 0;
      filereq_settbox( &freqf_tbox[0], path );
      if( !filereq_scan( path ) )
      {
        SDL_EnableUNICODE( wasunicode );
        SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
        return SDL_FALSE;
      }
    } else {
      SDL_EnableUNICODE( wasunicode );
      SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
      return SDL_FALSE;
    }
  }
  filereq_showfiles( top, cfile );
  filereq_drawtbox( &freqf_tbox[0], freqf_cgad==0 );
  filereq_drawtbox( &freqf_tbox[1], freqf_cgad==1 );
  filereq_render( oric );

  for( ;; )
  {
    if( !SDL_WaitEvent( &event ) )
    {
      SDL_EnableUNICODE( wasunicode );
      SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
      return SDL_FALSE;
    }

    mx = -1;
    my = -1;
    mclick = 0;
    doaction = ACTION_NONE;
    switch( event.type )
    {
      case SDL_MOUSEMOTION:
        mx = (event.motion.x - tz->x)/8;
        my = (event.motion.y - tz->y)/12;
        break;

      case SDL_MOUSEBUTTONDOWN:
        if( event.button.button == SDL_BUTTON_LEFT )
        {
          mx = (event.button.x - tz->x)/8;
          my = (event.button.y - tz->y)/12;
          mclick = SDL_GetTicks();
        }
        break;
    }

    switch( event.type )
    {
      case SDL_MOUSEBUTTONDOWN:
        if( ( mx < 1 ) || ( mx > 38 ) )
          break;
       
        if( ( my == 28 ) || ( my == 30 ) )
        {
          freqf_cgad = (my-28)/2;
          filereq_showfiles( top, cfile );
          filereq_drawtbox( &freqf_tbox[0], freqf_cgad==0 );
          filereq_drawtbox( &freqf_tbox[1], freqf_cgad==1 );
          filereq_render( oric );
          break;
        }

        if( ( my < 1 ) || ( my > 26 ) )
          break;

        freqf_cgad = 2;

        i = (my-1)-top;
        if( i >= freqf_used ) i = freqf_used-1;

        if( cfile != i )
        {
          cfile = i;
          filereq_setfiletbox( cfile, fname );
          filereq_showfiles( top, cfile );
          filereq_drawtbox( &freqf_tbox[1], 0 );
          filereq_render( oric );
          freqf_clicktime = mclick;
          break;
        }

        if( ( freqf_clicktime == 0 ) ||
            ( (mclick-freqf_clicktime) >= 2000 ) )
        {
          freqf_clicktime = mclick;
          break;
        }

        freqf_clicktime = 0;
        doaction = ACTION_SELECTITEM;
        break;

      case SDL_KEYUP:
        switch( event.key.keysym.sym )
        {
          case SDLK_LSHIFT:
          case SDLK_RSHIFT:
            shifty = SDL_FALSE;
            break;

          case SDLK_ESCAPE:
            SDL_EnableUNICODE( wasunicode );
            SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
            return SDL_FALSE;

          case SDLK_RETURN:
            switch( freqf_cgad )
            {
              case 0:
                cfile = top = 0;
                if( !filereq_scan( path ) )
                {
                  if( path[0] )
                  {
                    path[0] = 0;
                    filereq_settbox( &freqf_tbox[0], path );
                    if( !filereq_scan( path ) )
                    {
                      SDL_EnableUNICODE( wasunicode );
                      SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
                      return SDL_FALSE;
                    }
                  } else {
                    SDL_EnableUNICODE( wasunicode );
                    SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
                    return SDL_FALSE;
                  }
                }
                filereq_showfiles( top, cfile );
                filereq_drawtbox( &freqf_tbox[0], freqf_cgad==0 );
                filereq_drawtbox( &freqf_tbox[1], freqf_cgad==1 );
                filereq_render( oric );
                break;
              
              case 1:
                SDL_EnableUNICODE( wasunicode );
                SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
                return SDL_TRUE;
                
              case 2:
                doaction = ACTION_SELECTITEM;
                break;
            }
            break;
          
          default:
            break;
        }
        break;

      case SDL_KEYDOWN:
        switch( event.key.keysym.sym )
        {          
          case SDLK_LSHIFT:
          case SDLK_RSHIFT:
            shifty = SDL_TRUE;
            break;

          case SDLK_TAB:
            if( shifty )
              freqf_cgad = (freqf_cgad+2)%3;
            else
              freqf_cgad = (freqf_cgad+1)%3;
            filereq_showfiles( top, cfile );
            filereq_drawtbox( &freqf_tbox[0], freqf_cgad==0 );
            filereq_drawtbox( &freqf_tbox[1], freqf_cgad==1 );
            filereq_render( oric );
            break;
          
          case SDLK_UP:
            if( freqf_cgad != 2 )
            {
              freqf_cgad = 2;
              filereq_showfiles( top, cfile );
              filereq_drawtbox( &freqf_tbox[0], freqf_cgad==0 );
              filereq_drawtbox( &freqf_tbox[1], freqf_cgad==1 );
              filereq_render( oric );
              break;
            }

            if( cfile <= 0 ) break;
            if( shifty )
            {
              doaction = ACTION_PAGEUP;
              break;
            }
            cfile--;
            if( cfile < top ) top = cfile;
            filereq_setfiletbox( cfile, fname );
            filereq_showfiles( top, cfile );
            filereq_drawtbox( &freqf_tbox[1], SDL_FALSE );
            filereq_render( oric );
            break;
          
          case SDLK_DOWN:
            if( freqf_cgad != 2 )
            {
              freqf_cgad = 2;
              filereq_showfiles( top, cfile );
              filereq_drawtbox( &freqf_tbox[0], freqf_cgad==0 );
              filereq_drawtbox( &freqf_tbox[1], freqf_cgad==1 );
              filereq_render( oric );
              break;
            }

            if( cfile >= (freqf_used-1) ) break;
            if( shifty )
            {
              doaction = ACTION_PAGEDOWN;
              break;
            }
            cfile++;
            if( cfile > (top+25) ) top = cfile-25;
            filereq_setfiletbox( cfile, fname );
            filereq_showfiles( top, cfile );
            filereq_drawtbox( &freqf_tbox[1], SDL_FALSE );
            filereq_render( oric );
            break;
          
          case SDLK_PAGEUP:
            if( freqf_cgad != 2 )
            {
              freqf_cgad = 2;
              filereq_showfiles( top, cfile );
              filereq_drawtbox( &freqf_tbox[0], freqf_cgad==0 );
              filereq_drawtbox( &freqf_tbox[1], freqf_cgad==1 );
              filereq_render( oric );
              break;
            }

            doaction = ACTION_PAGEUP;
            break;
          
          case SDLK_PAGEDOWN:
            if( freqf_cgad != 2 )
            {
              freqf_cgad = 2;
              filereq_showfiles( top, cfile );
              filereq_drawtbox( &freqf_tbox[0], freqf_cgad==0 );
              filereq_drawtbox( &freqf_tbox[1], freqf_cgad==1 );
              filereq_render( oric );
              break;
            }

            doaction = ACTION_PAGEDOWN;
            break;
          
          case SDLK_BACKSPACE:
            if( freqf_cgad == 2 ) break;
            tb = &freqf_tbox[freqf_cgad];
            if( tb->cpos < 1 ) break;
            for( i=tb->cpos-1; i<tb->slen; i++ )
              tb->buf[i] = tb->buf[i+1];
            tb->slen--;
            if( tb->slen < 0 )
            {
              tb->slen = 0;
              tb->cpos = 0;
            }

          case SDLK_LEFT:
            if( freqf_cgad == 2 ) break;
            tb = &freqf_tbox[freqf_cgad]; 
            if( tb->cpos > 0 ) tb->cpos--;
            if( tb->cpos < tb->vpos ) tb->vpos = tb->cpos;
            if( tb->cpos >= (tb->vpos+tb->w) ) tb->vpos = tb->cpos-(tb->w-1);
            if( tb->vpos < 0 ) tb->vpos = 0;
            filereq_drawtbox( tb, SDL_TRUE );
            filereq_render( oric );
            break;

          case SDLK_RIGHT:
            if( freqf_cgad == 2 ) break;
            tb = &freqf_tbox[freqf_cgad]; 
            if( tb->cpos < tb->slen ) tb->cpos++;
            if( tb->cpos < tb->vpos ) tb->vpos = tb->cpos;
            if( tb->cpos >= (tb->vpos+tb->w) ) tb->vpos = tb->cpos-(tb->w-1);
            if( tb->vpos < 0 ) tb->vpos = 0;
            filereq_drawtbox( tb, SDL_TRUE );
            filereq_render( oric );
            break;          

          case SDLK_DELETE:
            if( freqf_cgad == 2 ) break;
            tb = &freqf_tbox[freqf_cgad];
            if( tb->cpos >= tb->slen ) break;
            for( i=tb->cpos; i<tb->slen; i++ )
              tb->buf[i] = tb->buf[i+1];
            tb->slen--;
            if( tb->slen < 0 )
            {
              tb->slen = 0;
              tb->cpos = 0;
            }
            filereq_drawtbox( tb, SDL_TRUE );
            filereq_render( oric );
            break;

          default:
            break;
        }

        switch( event.key.keysym.unicode )
        {  
          default:
            if( freqf_cgad == 2 ) break;

            if( ( event.key.keysym.unicode > 31 ) && ( event.key.keysym.unicode < 127 ) )
            {
              tb = &freqf_tbox[freqf_cgad];
              if( tb->slen >= (tb->maxlen-1) ) break;
              for( i=tb->slen; i>=tb->cpos; i-- )
                tb->buf[i] = tb->buf[i-1];
              tb->buf[tb->cpos] = event.key.keysym.unicode;
              tb->slen++;
              tb->cpos++;
              tb->buf[tb->cpos] = 0;
              if( tb->cpos < tb->vpos ) tb->vpos = tb->cpos;
              if( tb->cpos >= (tb->vpos+tb->w) ) tb->vpos = tb->cpos-(tb->w-1);
              if( tb->vpos < 0 ) tb->vpos = 0;
              filereq_drawtbox( tb, SDL_TRUE );
              filereq_render( oric );
            }
            break;
        }
        break;
      
      case SDL_QUIT:
        setemumode( oric, NULL, EM_PLEASEQUIT );
        return SDL_FALSE;
      
      default:
        break;
    }

    switch( doaction )
    {
      case ACTION_PAGEUP:
        if( cfile > top )
        {
          cfile = top;
        } else {
          top -= 25;
          if( top < 0 ) top = 0;
          cfile = top;
        }
        filereq_showfiles( top, cfile );
        filereq_drawtbox( &freqf_tbox[0], freqf_cgad==0 );
        filereq_drawtbox( &freqf_tbox[1], freqf_cgad==1 );
        filereq_render( oric );
        break;
      
      case ACTION_PAGEDOWN:
        if( cfile < (top+25) )
        {
          cfile = top+25;
        } else {
          cfile += 25;
        }
        if( cfile > (freqf_used-1) ) cfile = freqf_used-1;
        top = cfile-25;
        if( top < 0 ) top = 0;
        filereq_showfiles( top, cfile );
        filereq_drawtbox( &freqf_tbox[0], freqf_cgad==0 );
        filereq_drawtbox( &freqf_tbox[1], freqf_cgad==1 );
        filereq_render( oric );
        break;

      case ACTION_SELECTITEM:
        if( freqf_used <= 0 ) break;
        if( cfile<=0 ) // Parent
        {
          i = strlen( path )-1;
          if( i<=0 ) break;
          if( path[i] == PATHSEP ) i--;
          while( i > -1 )
          {
            if( path[i] == PATHSEP ) break;
            i--;
          }
          if( i==-1 ) i++;
          path[i] = 0;
        } else if( freqfiles[cfile].isdir ) {
          i = strlen( path )-1;
          if( i < 0 )
          {
            i++;
          } else {
            if( path[i] != PATHSEP )
              i++;
          }
          if( i > 0 ) path[i++] = PATHSEP;
          strncpy( &path[i], freqfiles[cfile].name, 4096-i );
          path[4095] = 0;
        } else {
          SDL_EnableUNICODE( wasunicode );
          SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
          return SDL_TRUE;
        }
        cfile = top = 0;
        filereq_settbox( &freqf_tbox[0], path );
        if( !filereq_scan( path ) )
        {
          if( path[0] )
          {
            path[0] = 0;
            filereq_settbox( &freqf_tbox[0], path );
            if( !filereq_scan( path ) )
            {
              SDL_EnableUNICODE( wasunicode );
              SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
              return SDL_FALSE;
            }
          } else {
            SDL_EnableUNICODE( wasunicode );
            SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
            return SDL_FALSE;
          }
        }
        filereq_showfiles( top, cfile );
        filereq_drawtbox( &freqf_tbox[0], freqf_cgad==0 );
        filereq_drawtbox( &freqf_tbox[1], freqf_cgad==1 );
        filereq_render( oric );
        break;
    }
  }

  SDL_EnableUNICODE( wasunicode );
  SDL_EnableKeyRepeat( wasunicode ? SDL_DEFAULT_REPEAT_DELAY : 0, wasunicode ? SDL_DEFAULT_REPEAT_INTERVAL : 0 );
  return SDL_FALSE;
}
