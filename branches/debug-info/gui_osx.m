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
**  Mac OS X GUI miscellaneous helpers.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#import <Cocoa/Cocoa.h>

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "6551.h"
#include "machine.h"

extern struct osdmenu menus[];

//XXX: I have no idea how allocation & gc is performed in ObjC, is this correct ?

//static NSMenu *appMenu;

static void build_native_menus( struct osdmenu *menu )
{
	struct osdmenuitem *item = menu->items;
	printf("menu '%s': {\n", menu->title);
	for (; item->name; item++) {
		const char *name = item->name;
		if (name == OSDMENUBAR)
			name = "------------";
		printf("item '%s' key '%s'\n", name, item->key);
		if (item->func == gotomenu && item->arg) {
			build_native_menus( &menus[item->arg] );
		}
	}
	printf("}\n");
}

SDL_bool init_gui_native( struct machine *oric )
{
	/*
	appMenu = [[NSApplication sharedApplication] mainMenu];
	printf("menu: %s\n", [[[appMenu itemAtIndex:0] title] UTF8String]);
	printf("menu: %s\n", [[[appMenu itemAtIndex:1] title] UTF8String]);
	printf("menu: %s\n", [[[appMenu itemAtIndex:2] title] UTF8String]);
	build_native_menus( menus );
	*/
	return SDL_TRUE;
}

void shut_gui_native( struct machine *oric )
{
}

void gui_open_url( const char *url )
{
  [[NSWorkspace sharedWorkspace] openURL: [NSURL URLWithString: [NSString stringWithUTF8String: url]]];
}


SDL_bool clipboard_copy_text( struct machine *oric )
{
	unsigned char *vidmem = (&oric->mem[oric->vid_addr]);
	int line, col;
	// TEXT
	NSMutableString *text = [NSMutableString stringWithCapacity:(40*28)];
	for (line = 0; line < 28; line++) {
		for (col = 0; col < 40; col++) {
			bool inverted = false;
			unsigned char c = vidmem[line * 40 + col];

			if (c > 127) {
				inverted = true;
				c -= 128;
			}

			if (c < 8) {
				//
				[text appendString: @" "];
			} else if (c < ' ' || c == 127) {
				[text appendString: @" "];
			} else if (c == 0x60) {
				[text appendString: @"©"];
			} else
				[text appendFormat: @"%c", c];
		}
		[text appendString: @"\n"];
	}

	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	[pasteboard clearContents];
	NSArray *copiedObjects = [NSArray arrayWithObject:text];
	[pasteboard writeObjects:copiedObjects];

	return SDL_TRUE;
}

SDL_bool clipboard_copy( struct machine *oric )
{
	unsigned char *vidmem = (&oric->mem[oric->vid_addr]);
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
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard]; 
	NSArray *classes = [[NSArray alloc] initWithObjects:[NSString class], nil];
	NSDictionary *options = [NSDictionary dictionary];
	NSArray *copiedItems = [pasteboard readObjectsForClasses:classes options:options];

	for (NSString *t in copiedItems) {
		t = [t stringByReplacingOccurrencesOfString: @"\n" withString: @"\r"];
		t = [t stringByReplacingOccurrencesOfString: @"\t" withString: @" "];
		queuekeys( (char *)[t UTF8String] );
	}

	return SDL_FALSE;
}
