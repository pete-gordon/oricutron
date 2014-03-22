/*

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
**  GTK based file requester
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
#include "filereq.h"

extern SDL_bool fullscreen;
void togglefullscreen( struct machine *oric, struct osdmenuitem *mitem, int dummy );

SDL_bool init_filerequester( struct machine *oric )
{
  gtk_init(0, NULL);
  return SDL_TRUE;
}

void shut_filerequester( struct machine *oric )
{
}

// This routine displays a file requester and waits for the user to select a file
//   title = title at the top of the requester
//   path  = initial path to show
//   fname = initial contents of the filename textbox
// It returns true if the user selected a file, although the file is not
// guarranteed to exist.
SDL_bool filerequester( struct machine *oric, char *title, char *path, char *fname, int type )
{
  GtkWidget *dialog;
  SDL_bool result = SDL_FALSE;
  GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
  GtkFileFilter *filter = NULL, *allfilter = gtk_file_filter_new();
  SDL_bool was_fullscreen = fullscreen;

  if (fullscreen)
    togglefullscreen(oric, NULL, 0);

  gtk_file_filter_set_name(allfilter, "All files");
  gtk_file_filter_add_pattern(allfilter, "*");

  switch( type )
  {
    case FR_DISKSAVE:
      action = GTK_FILE_CHOOSER_ACTION_SAVE;
    case FR_DISKLOAD:
      filter = gtk_file_filter_new();
      gtk_file_filter_set_name(filter, "Disk images");
      gtk_file_filter_add_pattern(filter, "*.dsk");
      break;
    
    case FR_TAPESAVETAP:
      action = GTK_FILE_CHOOSER_ACTION_SAVE;
      filter = gtk_file_filter_new();
      gtk_file_filter_set_name(filter, ".tap files");
      gtk_file_filter_add_pattern(filter, "*.tap");
      break;

    case FR_TAPESAVEORT:
      action = GTK_FILE_CHOOSER_ACTION_SAVE;
      filter = gtk_file_filter_new();
      gtk_file_filter_set_name(filter, ".ort files");
      gtk_file_filter_add_pattern(filter, "*.ort");
      break;

    case FR_TAPELOAD:
      filter = gtk_file_filter_new();
      gtk_file_filter_set_name(filter, "Tape images");
      gtk_file_filter_add_pattern(filter, "*.tap");
      gtk_file_filter_add_pattern(filter, "*.wav");
      gtk_file_filter_add_pattern(filter, "*.ort");
      break;
    
    case FR_ROMS:
      filter = gtk_file_filter_new();
      gtk_file_filter_set_name(filter, "ROM images");
      gtk_file_filter_add_pattern(filter, "*.rom");
      break;

    case FR_SNAPSHOTSAVE:
      action = GTK_FILE_CHOOSER_ACTION_SAVE;
    case FR_SNAPSHOTLOAD:
      filter = gtk_file_filter_new();
      gtk_file_filter_set_name(filter, "Snapshot files");
      gtk_file_filter_add_pattern(filter, "*.sna");
      break;
      
    case FR_KEYMAPPINGSAVE:
       action = GTK_FILE_CHOOSER_ACTION_SAVE;
    case FR_KEYMAPPINGLOAD:
      filter = gtk_file_filter_new();
      gtk_file_filter_set_name(filter, "Keyboard Mapping files");
      gtk_file_filter_add_pattern(filter, "*.kma");
      break;
 
    default:
      break;
  }

  dialog = gtk_file_chooser_dialog_new(title,
				       NULL,
				       action,
				       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
				       NULL);

  gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), path);
  if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), fname);

  if (filter)
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), allfilter);

  if (gtk_dialog_run(GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
  {
    char *filename;
    int i;
    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
    strncpy( path,  filename, 4096 ); path[4095] = 0;
    i = strlen(path)-1;
    while (i >= 0)
    {
      if (path[i] == PATHSEP) break;
      i--;
    }
    strncpy( fname, &path[i+1], 512  ); fname[511]  = 0;
    path[i+1] = 0;
    g_free(filename);
    result = SDL_TRUE;
  }
  
  gtk_widget_destroy(dialog);
  while (gtk_events_pending())
    gtk_main_iteration();

  if (was_fullscreen)
    togglefullscreen(oric, NULL, 0);
    
  return result;
}
