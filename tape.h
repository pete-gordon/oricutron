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
*/

#define TAPE_0_PULSE 416
#define TAPE_1_PULSE 208

#define TAPE_DECODE_0_MIN (((TAPE_0_PULSE-TAPE_1_PULSE)/2)+TAPE_1_PULSE)
#define TAPE_DECODE_0_MAX (TAPE_0_PULSE*2)
#define TAPE_DECODE_1_MIN (TAPE_1_PULSE/2)
#define TAPE_DECODE_1_MAX (TAPE_DECODE_0_MIN)

#define TIME_TO_BIT(t) ((t<TAPE_DECODE_1_MIN)||(t>TAPE_DECODE_0_MAX))?-1:((t<TAPE_DECODE_0_MIN)?1:0)

void tape_eject( struct machine *oric );
void tape_rewind( struct machine *oric );
SDL_bool tape_load_tap( struct machine *oric, char *fname );
void tape_ticktock( struct machine *oric, int cycles );
void tape_setmotor( struct machine *oric, SDL_bool motoron );
void tape_patches( struct machine *oric );
void toggletapecap( struct machine *oric, struct osdmenuitem *mitem, int dummy );
void tape_orbchange(struct via *via);
void tape_stop_savepatch( struct machine *oric );
