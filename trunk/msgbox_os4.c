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
**  Amiga OS4.x message box
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <classes/requester.h>

#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/requester.h>

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "6551.h"
#include "machine.h"
#include "msgbox.h"

struct Library *IntuitionBase = NULL;
struct Library *RequesterBase = NULL;

struct IntuitionIFace *IIntuition = NULL;
struct RequesterIFace *IRequester = NULL;

SDL_bool init_msgbox( struct machine *oric )
{
  IntuitionBase = IExec->OpenLibrary( "intuition.library", 51 );
  if( !IntuitionBase )
  {
    printf( "Unable to open intuition.library v51+\n" );
    return SDL_FALSE;
  }

  IIntuition = (struct IntuitionIFace *)IExec->GetInterface( IntuitionBase, "main", 1, NULL );
  if( !IIntuition )
  {
    printf( "Unable to obtain main interface for intuition.library\n" );
    return SDL_FALSE;
  }

  RequesterBase = IExec->OpenLibrary( "requester.class", 51 );
  if( !RequesterBase )
  {
    printf( "Unable to open requester.library v51+\n" );
    return SDL_FALSE;
  }

  IRequester = (struct RequesterIFace *)IExec->GetInterface( RequesterBase, "main", 1, NULL );
  if( !IRequester )
  {
    printf( "Unable to obtain main interface for requester.library\n" );
    return SDL_FALSE;
  }

  return SDL_TRUE;
}

void shut_msgbox( struct machine *oric )
{
  if( IRequester ) IExec->DropInterface( (struct Interface *)IRequester );
  if( RequesterBase ) IExec->CloseLibrary( RequesterBase );
  if( IIntuition ) IExec->DropInterface( (struct Interface *)IIntuition );
  if( IntuitionBase ) IExec->CloseLibrary( IntuitionBase );
}

SDL_bool msgbox( struct machine *oric, int type, char *msg )
{
  Object *req_obj;
  int32 result, imgtype=REQIMAGE_INFO;
  STRPTR btns;

  switch( type )
  {
    case MSGBOX_YES_NO:
      btns = "Yes|No";
      imgtype = REQIMAGE_QUESTION;
      break;

    case MSGBOX_OK_CANCEL:
      btns = "OK|Cancel";
      imgtype = REQIMAGE_QUESTION;
      break;
    
    case MSGBOX_OK:
      btns = "OK";
      imgtype = REQIMAGE_INFO;
      break;
  }

  req_obj = (Object *)IIntuition->NewObject( IRequester->REQUESTER_GetClass(), NULL,
    REQ_TitleText,  "Oriculator Request",
    REQ_BodyText,   msg,
    REQ_GadgetText, "Yes|No",
    REQ_Image,      imgtype,
    TAG_DONE );

  if( !req_obj ) return SDL_TRUE; // Oh well

  result = IIntuition->IDoMethod( req_obj, RM_OPENREQ, NULL, NULL, NULL );
  IIntuition->DisposeObject( req_obj );

  if( type == MSGBOX_OK ) return SDL_TRUE;

  return result ? SDL_TRUE : SDL_FALSE;
}

