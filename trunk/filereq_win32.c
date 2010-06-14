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
**  Windows file dialog
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <windows.h>

#define WANT_WMINFO

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "disk.h"
#include "gui.h"
#include "machine.h"
#include "monitor.h"
#include "filereq.h"

SDL_bool init_filerequester( void )
{
  return SDL_TRUE;
}

void shut_filerequester( void )
{
}

SDL_bool filerequester( struct machine *oric, char *title, char *path, char *fname, int type )
{
  SDL_SysWMinfo wmi;
  OPENFILENAME ofn;
  HWND hwnd;
  char tmp[4096];
  int i;
  char *odir;

  strcpy( tmp, fname );

  hwnd = NULL;
  SDL_VERSION(&wmi.version);
  if( SDL_GetWMInfo( &wmi ) )
    hwnd = (HWND)wmi.window;

  ZeroMemory( &ofn, sizeof( ofn ) );
  ofn.lStructSize = sizeof( ofn );
  ofn.hwndOwner   = hwnd;
  ofn.lpstrFile   = fname;
  ofn.nMaxFile    = 4096;

  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
  switch( type )
  {
    case FR_DISKSAVE:
      ofn.Flags = OFN_PATHMUSTEXIST;
    case FR_DISKLOAD:
      ofn.lpstrFilter = "All Files\0*.*\0Disk Images (*.dsk)\0*.DSK\0";
      ofn.nFilterIndex = 2;
      break;
    
    case FR_TAPES:
      ofn.lpstrFilter = "All Files\0*.*\0Tape Images (*.tap)\0*.TAP\0";
      ofn.nFilterIndex = 2;
      break;

    case FR_ROMS:
      ofn.lpstrFilter = "All Files\0*.*\0Rom Images (*.rom)\0*.ROM\0";
      ofn.nFilterIndex = 2;
      break;

    case FR_SNAPSHOTSAVE:
      ofn.Flags = OFN_PATHMUSTEXIST;
    case FR_SNAPSHOTLOAD:
      ofn.lpstrFilter = "All Files\0*.*\0Snapshot files (*.sna)\0*.SNA\0";
      ofn.nFilterIndex = 2;
      break;

    default:
      ofn.lpstrFilter = "All Files\0*.*";
      ofn.nFilterIndex = 1;
      break;
  }

  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = path;

  odir = getcwd( NULL, 0 );

  switch( type )
  {
    case FR_SNAPSHOTSAVE:
    case FR_DISKSAVE:
      if( !GetSaveFileName( &ofn ) )
      {
        chdir( odir );
        free( odir );
        return SDL_FALSE;
      }
      break;
    
    default:
      if( !GetOpenFileName( &ofn ) )
      {
        chdir( odir );
        free( odir );
        return SDL_FALSE;
      }
      break;
  }

  chdir( odir );
  free( odir );

  i = strlen( ofn.lpstrFile );
  if( !i ) return SDL_FALSE;

  i--;
  while( ofn.lpstrFile[i] != PATHSEP )
  {
    if( i == 0 ) break;
    i--;
  }

  if( ( ofn.lpstrFile[i] == PATHSEP ) && ( i < (strlen(ofn.lpstrFile)-1) ) )
  {
    ofn.lpstrFile[i] = 0;
    strcpy( path, ofn.lpstrFile );
    strncpy( fname, &ofn.lpstrFile[i+1], 512 );
    fname[511] = 0;
  } else {
    path[0] = 0;
    strncpy( fname, ofn.lpstrFile, 512 );
    fname[511] = 0;
  }

  return SDL_TRUE;
}
