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
*/

/*
**  Pravetz 8D disk drive emulation
**  Copyright (C) 2009-2013 iss
*/

#ifndef __disk_pravetz_h__
#define __disk_pravetz_h__

Uint8 disk_pravetz_read(struct machine *oric, Uint16 addr);
void  disk_pravetz_write(struct machine *oric, Uint16 addr, Uint8 data);
Uint8  disk_pravetz_image_raw_byte(struct machine *oric, int drive,  Uint16 t_idx, Uint16 s_idx, Uint16 b_idx);
void  disk_pravetz_write_image(struct pravetz_drive *d_ptr);
#endif /* __disk_pravetz_h__ */

