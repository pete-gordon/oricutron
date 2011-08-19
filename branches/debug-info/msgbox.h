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
**  Message Box
*/

enum
{
  MSGBOX_YES_NO = 0,
  MSGBOX_OK_CANCEL,
  MSGBOX_OK
};

SDL_bool init_msgbox( struct machine *oric );
void shut_msgbox( struct machine *oric );
SDL_bool msgbox( struct machine *oric, int type, char *msg );
