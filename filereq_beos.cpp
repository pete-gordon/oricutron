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
**  BeOS file dialog
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <FilePanel.h>
#include <Looper.h>
#include <Messenger.h>
#include <Path.h>

extern "C" {
#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "disk.h"
#include "gui.h"
#include "machine.h"
#include "monitor.h"
#include "filereq.h"
}


class PanelLooper : public BLooper {
public:
							PanelLooper();
	virtual					~PanelLooper();
	virtual	void			MessageReceived(BMessage* message);

	void					Wait();
	SDL_bool				DoIt() const { return fDoIt; };
	void					GetRef(entry_ref &ref) const { ref = fRef; };

private:
	sem_id		fSem;
	entry_ref	fRef;
	SDL_bool	fDoIt;
};


PanelLooper::PanelLooper()
	: BLooper("PanelLooper"),
	fDoIt(SDL_FALSE)
{
	fSem = create_sem(0, "PanelLooper lock");
}


PanelLooper::~PanelLooper()
{
	delete_sem(fSem);
}


void
PanelLooper::MessageReceived(BMessage* message)
{
	//message->PrintToStream();
	switch (message->what) {
		case B_SAVE_REQUESTED:
			message->FindRef("refs", &fRef);
			fDoIt = SDL_TRUE;
			break;
		case B_REFS_RECEIVED:
			message->FindRef("refs", &fRef);
			fDoIt = SDL_TRUE;
			break;
		case B_CANCEL:
		default:
			break;
	}
	release_sem(fSem);
}


void
PanelLooper::Wait()
{
	acquire_sem(fSem);
}


SDL_bool init_filerequester( void )
{
  return SDL_TRUE;
}

void shut_filerequester( void )
{
}

SDL_bool filerequester( struct machine *oric, char *title, char *path, char *fname, int type )
{
	BFilePanel *panel;
	PanelLooper *looper = new PanelLooper();
	looper->Run();
	SDL_bool ret;

  char *pat, ppat[6*2+2];
  bool dosavemode = false;
  
  switch( type )
  {
    case FR_DISKSAVE:
      dosavemode = true;
    case FR_DISKLOAD:
      pat = "*.dsk";
      break;
    
    case FR_TAPES:
      pat = "*.tap";
      break;
    
    case FR_ROMS:
      pat = "*.rom";
      break;

    case FR_SNAPSHOTSAVE:
      dosavemode = true;
    case FR_SNAPSHOTLOAD:
      pat = "*.sna";
      break;
 
    default:
      pat = NULL;
      break;
  }

	//XXX: use RefFilter

	panel = new BFilePanel(dosavemode ? B_SAVE_PANEL : B_OPEN_PANEL);
	panel->SetTarget(BMessenger(looper));

	panel->Show();

	looper->Wait();
	ret = looper->DoIt();
	entry_ref ref;
	looper->GetRef(ref);
	
	delete panel;
	looper->Lock();
	looper->Quit();
	
  if (ret) {
    BPath p(&ref);
    strncpy( fname, p.Leaf(),   512  ); path[511]  = 0;
    p.GetParent(&p);
    strncpy( path,  p.Path(), 4096 ); path[4095] = 0;
  }

  return ret;
}


