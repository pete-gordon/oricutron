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
**  SDL based message box
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include <gtk/gtk.h>

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

extern SDL_bool fullscreen;
void togglefullscreen( struct machine *oric, struct osdmenuitem *mitem, int dummy );

SDL_bool init_msgbox( struct machine *oric )
{
  // We rely on init_filerequester to call gtk_init()
  return SDL_TRUE;
}

void shut_msgbox( struct machine *oric )
{
}

SDL_bool msgbox( struct machine *oric, int type, char *msg )
{
  GtkWidget *dialog = NULL;
  GtkButtonsType btns = GTK_BUTTONS_OK;
  GtkMessageType mtyp = GTK_MESSAGE_INFO;
  SDL_bool result = SDL_FALSE;
  gint res;
  SDL_bool was_fullscreen = fullscreen;

  if (fullscreen)
    togglefullscreen(oric, NULL, 0);

  switch( type )
  {
    case MSGBOX_YES_NO:
      btns = GTK_BUTTONS_YES_NO;
      mtyp = GTK_MESSAGE_QUESTION;
      break;

    case MSGBOX_OK_CANCEL:
      btns = GTK_BUTTONS_OK_CANCEL;
      break;

    default:
      break;
  }

  dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, mtyp, btns, "%s", msg);
  res = gtk_dialog_run(GTK_DIALOG (dialog));
  if ((res == GTK_RESPONSE_OK) || (res == GTK_RESPONSE_YES) || (res == GTK_RESPONSE_ACCEPT))
    result = SDL_TRUE;
  
  gtk_widget_destroy(dialog);
  while (gtk_events_pending())
    gtk_main_iteration();

  if (was_fullscreen)
    togglefullscreen(oric, NULL, 0);

  return result;
}
