
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

  switch( type )
  {
    case FR_DISKS:
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

    default:
      ofn.lpstrFilter = "All Files\0*.*";
      ofn.nFilterIndex = 1;
      break;
  }

  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = path;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  odir = getcwd( NULL, 0 );
  if( !GetOpenFileName( &ofn ) )
  {
    chdir( odir );
    free( odir );
    return SDL_FALSE;
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
