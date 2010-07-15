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
*/

SDL_bool read_config_string( char *buf, char *token, char *dest, Sint32 maxlen );
SDL_bool read_config_bool( char *buf, char *token, SDL_bool *dest );
SDL_bool read_config_option( char *buf, char *token, Sint32 *dest, char **options );
SDL_bool read_config_int( char *buf, char *token, int *dest, int min, int max );
