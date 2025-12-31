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
**  Joystick definitions
*/

#define AXIS_THRESHOLD 8000       // How far the user needs to push an analog axis before it triggers a digital impulse. (was previously defined as 3200, but was too sensitive and made it almost impossible to accurately go in one direction)

// Joystick interface types
enum
{
  JOYIFACE_NONE = 0,
  JOYIFACE_ALTAI,
  JOYIFACE_IJK
};

// Joystick emulation modes
enum
{
  JOYMODE_NONE = 0,
  JOYMODE_KB1,
  JOYMODE_KB2,
  JOYMODE_SDL0,
  JOYMODE_SDL1,
  JOYMODE_SDL2,
  JOYMODE_SDL3,
  JOYMODE_SDL4,
  JOYMODE_SDL5,
  JOYMODE_SDL6,
  JOYMODE_SDL7,
  JOYMODE_SDL8,
  JOYMODE_SDL9,
  JOYMODE_MOUSE
};

enum
{
  JOYSTATE_UP = 0,
  JOYSTATE_DOWN,
  JOYSTATE_LEFT,
  JOYSTATE_RIGHT,
  JOYSTATE_FIRE1,
  JOYSTATE_FIRE2,
  JOYSTATE_FIRE3,
  JOYSTATE_TYPE,
  JOYSTATE_LAST
};

// Hacked joystick detection mode
enum
{
  JOYTYPE_NONE = 0,
  JOYTYPE_DEFAULT,
  JOYTYPE_DUALSENSE,
};

SDL_bool init_joy( struct machine *oric );
void shut_joy( struct machine *oric );

SDL_COMPAT_KEY joy_keyname_to_sym( char *name );
void joy_setup( struct machine *oric );
SDL_bool joy_filter_event( SDL_Event *ev, struct machine *oric );
void joy_buildmask( struct machine *oric );
