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
**  File requester
*/

enum
{
  FR_DISKLOAD=0,
  FR_DISKSAVE,
  FR_TAPELOAD,
  FR_TAPESAVE,
  FR_ROMS,
  FR_SNAPSHOTLOAD,
  FR_SNAPSHOTSAVE,
  FR_OTHER
};

SDL_bool init_filerequester( struct machine *oric );
void shut_filerequester( struct machine *oric );
SDL_bool filerequester( struct machine *oric, char *title, char *path, char *fname, int type );

