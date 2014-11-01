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
**  BeOS GUI miscellaneous helpers, clipboard
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <Alert.h>
#include <Clipboard.h>
#include <Font.h>
#include <InterfaceDefs.h>
#include <List.h>
#include <MenuItem.h>
#include <Mime.h>
#include <PopUpMenu.h>
#include <String.h>
#include <TextView.h>

extern "C" {
#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "6551.h"
#include "machine.h"
}

static rgb_color oric_colors[] = {
	{   0,   0,   0, 255 },
	{ 255,   0,   0, 255 },
	{   0, 255,   0, 255 },
	{ 255, 255,   0, 255 },
	{   0,   0, 255, 255 },
	{ 255,   0, 255, 255 },
	{   0, 255, 255, 255 },
	{ 255, 255, 255, 255 }
};

extern struct osdmenu menus[];
extern struct osdmenuitem aboutitems[];
//static BPopUpMenu *sPopup;

class OSDMenuItem : public BMenuItem {
public:
	OSDMenuItem(struct osdmenuitem *desc, const char *name, BMessage *message)
		: BMenuItem(name, message), fItem(desc)
		{
			if (desc->name[0] == '\x0e')
				SetMarked(true);
			if (desc->key)
				SetTrigger(desc->key[0]);
		};
	virtual ~OSDMenuItem() {};
	struct osdmenuitem *fItem;
};

static void build_native_menus( struct osdmenu *menu, BMenu *m )
{
	struct osdmenuitem *item = menu->items;

	printf("menu '%s': {\n", menu->title);
	for (; item->name; item++) {
		BMenuItem *mi;
		const char *name = item->name;
		if (name == OSDMENUBAR) {
			name = "------------";
			// skip separators before back since it will also be ignored
			if (item[1].sdlkey == SDLK_BACKSPACE)
				continue;
			mi = new BSeparatorItem();
		} else if (item->func == gotomenu) {
			if (item->arg == 0)
				continue;
			// about item is special
			if (menus[item->arg].items == aboutitems) {
				mi = new BMenuItem(name, new BMessage(B_ABOUT_REQUESTED));
			} else {
				BMenu *subMenu = new BMenu(name);
				mi = new BMenuItem(subMenu);
				build_native_menus( &menus[item->arg], subMenu );
			}
		} else {
			if (name[0] == ' ' || name[0] == '\x0e')
				name = &name[1];
			mi = new OSDMenuItem(item, name, NULL);
		}
			
		//printf("item '%s' key '%s'\n", name, item->key);

		m->AddItem(mi);
	}
	printf("}\n");
}

#if 0
static void run_native_about( void )
{
	struct osdmenuitem *item = aboutitems;
	BString text;
	for (; item->name; item++) {
		if (item->name == OSDMENUBAR)
			break;
		text << item->name;
		text << "\n";
	}
	BAlert *alert = new BAlert("About", text.String(), "Ok");
	alert->Go();
}
#endif

SDL_bool init_gui_native( struct machine *oric )
{
	BMimeType tapeType("application/x-oric-tape");
	if (!tapeType.IsInstalled())
		tapeType.Install();
	BMessage exts;
	if (tapeType.GetFileExtensions(&exts) != B_OK) {
		exts.AddString("extensions", "tap");
		tapeType.SetShortDescription("ORIC Tape");
		tapeType.SetLongDescription("ORIC Tape Image for emulators");
		tapeType.SetPreferredApp("application/x-vnd.oricutron");
		tapeType.SetFileExtensions(&exts);
		tapeType.SetSnifferRule("0.40      [0:8](\"\\026\\026\\026\\044\")");
	}
	exts.MakeEmpty();
	BMimeType diskType("application/x-oric-disk");
	if (!diskType.IsInstalled())
		diskType.Install();
	if (diskType.GetFileExtensions(&exts) != B_OK) {
		exts.AddString("extensions", "dsk");
		diskType.Install();
		diskType.SetShortDescription("ORIC Disk");
		diskType.SetLongDescription("ORIC Disk Image for emulators");
		diskType.SetPreferredApp("application/x-vnd.oricutron");
		diskType.SetFileExtensions(&exts);
		diskType.SetSnifferRule("0.80      (\"MFM_DISK\")");
	}
	//BMimeType romType("application/x-oric-rom");

	//sPopup = new BPopUpMenu("main");
	//build_native_menus( menus, sPopup );
	//TEST
	//sPopup->Go( BPoint(0, 0), false, true);

	//run_native_about();

	return SDL_TRUE;
}

void shut_gui_native( struct machine *oric )
{
}

extern "C" SDL_bool clipboard_copy( struct machine *oric );
extern "C" SDL_bool clipboard_paste( struct machine *oric );

text_run *new_run(int32 offset, int color)
{
	BFont font(be_fixed_font);
	text_run *run = new text_run;
	run->offset = offset;
	run->font = font;
	run->color = oric_colors[color];
	return run;
}


SDL_bool clipboard_copy_text( struct machine *oric )
{
	unsigned char *vidmem = (&oric->mem[oric->vid_addr]);
	int line, col;
	// TEXT
	BString text;
	BList textruns;

	textruns.AddItem(new_run(0, 0));

	for (line = 0; line < 28; line++) {
		for (col = 0; col < 40; col++) {
			bool inverted = false;
			unsigned char c = vidmem[line * 40 + col];

			if (c > 127) {
				inverted = true;
				c -= 128;
			}

			if (c < 8) {
				textruns.AddItem(new_run(text.Length(), c));
				text << ' ';
			} else if (c < ' ' || c == 127) {
				text << ' ';
			} else if (c == 0x60) {
				text << B_UTF8_COPYRIGHT;
			} else
				text << (char)c;
		}
		text << '\n';
	}
	//printf("%s\n", text.String());
	

	BMessage *clip = NULL;
	if (be_clipboard->Lock()) {
		be_clipboard->Clear();
		clip = be_clipboard->Data();
		if (clip) {
			clip->AddData("text/plain", B_MIME_TYPE, text.String(), text.Length());

			int arraySize = sizeof(text_run_array)
				+ textruns.CountItems() * sizeof(text_run);
			text_run_array *array = (text_run_array *)malloc(arraySize);
			array->count = textruns.CountItems();
			for (int i = 0; i < array->count; i++) {
				memcpy(&array->runs[i], textruns.ItemAt(i), sizeof(text_run));
			}
			clip->AddData("application/x-vnd.Be-text_run_array", B_MIME_TYPE, 
				array, arraySize);
			free(array);

			be_clipboard->Commit();
		}
		be_clipboard->Unlock();
	}
	
	for (int i = 0; i < textruns.CountItems(); i++) {
		delete (text_run *)(textruns.ItemAt(i));
	}
	textruns.MakeEmpty();

	return SDL_TRUE;
}


SDL_bool clipboard_copy( struct machine *oric )
{
	//unsigned char *vidmem = (&oric->mem[oric->vid_addr]);
	if (oric->vid_addr == oric->vidbases[0]) {
		// HIRES
	} else if (oric->vid_addr == oric->vidbases[2]) {
		// TEXT
		return clipboard_copy_text( oric );
	}

	return SDL_TRUE;
}

SDL_bool clipboard_paste( struct machine *oric )
{
	const char *text;
	int32 textLen;
	BMessage *clip = NULL;
	
	if (be_clipboard->Lock()) {
		clip = be_clipboard->Data();
		if (clip && clip->FindData("text/plain", B_MIME_TYPE, (const void **)&text, &textLen) == B_OK) {
			printf("clip: tlen %ld\n", textLen);
			BString t(text, textLen);
			t.ReplaceAll('\n', '\r');
			t.ReplaceAll('\t', ' ');
			queuekeys( (char *)t.String() );
		}
		be_clipboard->Unlock();
	}
  return SDL_TRUE;
}


// SDL version doesn't work due to cpu speed
SDL_bool clipboard_paste_sdl( struct machine *oric )
{
	const char *text;
	int32 textLen;
	BMessage *clip = NULL;
	SDL_Event ev;
	ev.key.keysym.scancode = 0;
	ev.key.keysym.mod = (SDLMod)0;
	ev.key.keysym.sym = (SDLKey)0;
	
	if (be_clipboard->Lock()) {
		clip = be_clipboard->Data();
		if (clip && clip->FindData("text/plain", B_MIME_TYPE, (const void **)&text, &textLen) == B_OK) {
			for (int i = 0; i < textLen; i++) {
				fprintf(stderr, "pushing key '%c'\n", text[i]);
				ev.key.keysym.sym = (SDLKey)text[i];
				ev.key.keysym.unicode = text[i];
				ev.type = SDL_KEYDOWN;
				ev.key.state = SDL_PRESSED;
				SDL_PushEvent(&ev);
				ev.type = SDL_KEYUP;
				ev.key.state = SDL_RELEASED;
				SDL_PushEvent(&ev);
			}
		}
		be_clipboard->Unlock();
	}
  return SDL_TRUE;
}

void gui_open_url( const char *url )
{
	// BUrl is not really public yet, and I'm lazy.
	// It's a bit dangerous but we trust the caller.
	BString command("/bin/open '");
	command << url << "'";
	system(command.String());
}

