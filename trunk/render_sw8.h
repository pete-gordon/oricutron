/*
**  Oricutron
**  Copyright (C) 2009-2014 Peter Gordon
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
**  Software rendering
*/

void render_begin_sw8( struct machine *oric );
void render_end_sw8( struct machine *oric );
void render_textzone_alloc_sw8( struct machine *oric, int i );
void render_textzone_free_sw8( struct machine *oric, int i );
void render_textzone_sw8( struct machine *oric, int i );
void render_gimg_sw8( int i, Sint32 xp, Sint32 yp );
void render_gimgpart_sw8( int i, Sint32 xp, Sint32 yp, Sint32 ox, Sint32 oy, Sint32 w, Sint32 h );
void render_video_sw8( struct machine *oric, SDL_bool doublesize );
void render_clear_area_sw8( int x, int y, int w, int h );
SDL_bool render_togglefullscreen_sw8( struct machine *oric );
void preinit_render_sw8( struct machine *oric );
SDL_bool init_render_sw8( struct machine *oric );
void shut_render_sw8( struct machine *oric );

