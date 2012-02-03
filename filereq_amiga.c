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
**  Amiga file dialog
*/

#define __USE_INLINE__

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
#include "monitor.h"
#include "6551.h"
#include "machine.h"
#include "filereq.h"

struct Library *AslBase = NULL;
static struct FileRequester *req = NULL;

#ifdef __amigaos4__
struct AslIFace *IAsl = NULL;
#endif

SDL_bool init_filerequester( struct machine *oric )
{
  AslBase = OpenLibrary( AslName, 39 );
  if( !AslBase ) return SDL_FALSE;

#ifdef __amigaos4__
  IAsl = (struct AslIFace *)GetInterface( AslBase, "main", 1, NULL );
  if( !IAsl ) return SDL_FALSE;
#endif

  req = (struct FileRequester *)AllocAslRequestTags( ASL_FileRequest, TAG_DONE );
  if( !req ) return SDL_FALSE;

  return SDL_TRUE;
}

void shut_filerequester( struct machine *oric )
{
  if( req ) FreeAslRequest( req );
#ifdef __amigaos4__
  if( IAsl ) DropInterface( (struct Interface *)IAsl );
#endif
  if( AslBase ) CloseLibrary( AslBase );
}

SDL_bool filerequester( struct machine *oric, char *title, char *path, char *fname, int type )
{
  char *pat, ppat[16*2+2];
  BOOL dosavemode = FALSE;
  
  switch( type )
  {
    case FR_DISKSAVE:
      dosavemode = TRUE;
    case FR_DISKLOAD:
      pat = "#?.dsk";
      break;
    
    case FR_TAPESAVETAP:
      dosavemode = TRUE;
      pat = "#?.tap";
      break;

    case FR_TAPESAVEORT:
      dosavemode = TRUE;
      pat = "#?.ort";
      break;

    case FR_TAPELOAD:
      pat = "#?.(tap|ort|wav)";
      break;
    
    case FR_ROMS:
      pat = "#?.rom";
      break;

    case FR_SNAPSHOTSAVE:
      dosavemode = TRUE;
    case FR_SNAPSHOTLOAD:
      pat = "#?.sna";
      break;
 
    default:
      pat = NULL;
      break;
  }
  
  if( pat ) ParsePatternNoCase( pat, ppat, 16*2+2 );
  
  if( !AslRequestTags( req,
         ASLFR_TitleText,     title,
         ASLFR_InitialDrawer, path,
         ASLFR_InitialFile,   fname,
         ASLFR_DoPatterns,    pat != NULL,
         ASLFR_RejectIcons,   TRUE,
         ASLFR_DoSaveMode,    dosavemode,
         pat ? ASLFR_AcceptPattern : TAG_IGNORE, ppat,
         pat ? ASLFR_InitialPattern : TAG_IGNORE, pat,
         TAG_DONE ) )
    return SDL_FALSE;
  
  strncpy( path,  req->fr_Drawer, 4096 ); path[4095] = 0;
  strncpy( fname, req->fr_File,   512  ); path[511]  = 0;
  
  return SDL_TRUE;
}
