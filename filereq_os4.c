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
**  Amiga file dialog
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/asl.h>

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "disk.h"
#include "gui.h"
#include "machine.h"
#include "monitor.h"
#include "filereq.h"

struct Library *AslBase = NULL;
struct AslIFace *IAsl = NULL;
static struct FileRequester *req = NULL;

SDL_bool init_filerequester( void )
{
  AslBase = IExec->OpenLibrary( "asl.library", 52 );
  if( !AslBase ) return SDL_FALSE;
  
  IAsl = (struct AslIFace *)IExec->GetInterface( AslBase, "main", 1, NULL );
  if( !IAsl ) return SDL_FALSE;

  req = (struct FileRequester *)IAsl->AllocAslRequestTags( ASL_FileRequest, TAG_DONE );
  if( !req ) return SDL_FALSE;

  return SDL_TRUE;
}

void shut_filerequester( void )
{
  if( req ) IAsl->FreeAslRequest( req );
  if( IAsl ) IExec->DropInterface( (struct Interface *)IAsl );
  if( AslBase ) IExec->CloseLibrary( AslBase );
}

SDL_bool filerequester( struct machine *oric, char *title, char *path, char *fname, int type )
{
  char *pat, ppat[6*2+2];
  
  switch( type )
  {
    case FR_DISKS:
      pat = "#?.dsk";
      break;
    
    case FR_TAPES:
      pat = "#?.tap";
      break;
    
    case FR_ROMS:
      pat = "#?.rom";
      break;
 
    default:
      pat = NULL;
      break;
  }
  
  if( pat ) IDOS->ParsePatternNoCase( pat, ppat, 6*2+2 );
  
  if( !IAsl->AslRequestTags( req,
         ASLFR_TitleText,     title,
         ASLFR_InitialDrawer, path,
         ASLFR_InitialFile,   fname,
         ASLFR_DoPatterns,    pat != NULL,
         ASLFR_RejectIcons,   TRUE,
         pat ? ASLFR_AcceptPattern : TAG_IGNORE, ppat,
         TAG_DONE ) )
    return SDL_FALSE;
  
  strncpy( path,  req->fr_Drawer, 4096 ); path[4095] = 0;
  strncpy( fname, req->fr_File,   512  ); path[511]  = 0;
  
  return SDL_TRUE;
}


