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
**  Dummy rendering
*/

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "disk.h"
#include "gui.h"
#include "monitor.h"
#include "machine.h"
#include "render_null.h"

void render_begin_null( struct machine *oric )
{
}

void render_end_null( struct machine *oric )
{
}

void render_textzone_alloc_null( struct machine *oric, int i )
{
}

void render_textzone_free_null( struct machine *oric, int i )
{
}

void render_textzone_null( struct machine *oric, int i )
{
}

void render_gimg_null( int i, Sint32 xp, Sint32 yp )
{
}

void render_gimgpart_null( int i, Sint32 xp, Sint32 yp, Sint32 ox, Sint32 oy, Sint32 w, Sint32 h )
{
}

void render_video_null( struct machine *oric, SDL_bool doublesize )
{
}

SDL_bool init_render_null( struct machine *oric )
{
  return SDL_TRUE;
}

void shut_render_null( struct machine *oric )
{
}
