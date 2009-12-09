/*
**  Oriculator
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
**  Monitor/Debugger
*/

void mon_init( struct machine *oric );
void mon_render( struct machine *oric );
void mon_update_regs( struct machine *oric );
SDL_bool mon_event( SDL_Event *ev, struct machine *oric, SDL_bool *needrender );
void dbg_printf( char *fmt, ... );
void mon_enter( struct machine *oric );
void mon_shut( void );
SDL_bool mon_new_symbols( char *fname, SDL_bool above );
void mon_watch_reset( struct machine *oric );
