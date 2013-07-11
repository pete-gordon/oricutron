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

/*
**  Pravetz 8D disk drive emulation
**  Copyright (C) 2009-2013 iss
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "system.h"
#include "disk.h"
#include "disk_pravetz.h"

#define disk_pravetz_offset      0x310

#define SECTORS_PER_TRACK        16
#define BYTES_PER_SECTOR         256
#define RAW_BYTES_PER_SECTOR     374
#define TRACKS_PER_DISK          35
#define RAW_TRACK_SIZE           6200

typedef
struct disk_image_s
{
    int num;
    FILE * file;

    SDL_bool dirty;
    SDL_bool protected;

    Uint8  volume;
    Uint8  select;
    Uint8  motor_on;
    Uint8  write_ready;

    Uint16 byte;
    Uint16 half_track;
    Uint8  sector_buf[256];    

    Uint8  image[TRACKS_PER_DISK][RAW_TRACK_SIZE];
} 
disk_image_t, *disk_image_p;

static disk_image_t  disk_drive[MAX_DRIVES];
static disk_image_p  current_drive;

static Uint8  disk_pravetz_read_selected(void);
static void   disk_pravetz_write_selected(Uint8 data);
static void   disk_pravetz_switch(Uint16 addr);

void disk_pravetz_init(void)
{
    int i;
    for( i=0; i<MAX_DRIVES; i++ )
    {
      current_drive = &disk_drive[i];
      current_drive->write_ready = 0x80;
      current_drive->volume = 254;
      current_drive->select = 0;
      current_drive->motor_on = 0;
      current_drive->num = i;
    }
    current_drive = &disk_drive[0];
}

Uint8 disk_pravetz_read(Uint16 addr)
{
    disk_pravetz_switch(addr-disk_pravetz_offset);
    /* odd addresses don't produce anything */
    return (!(addr & 1))? disk_pravetz_read_selected() : 0;
}

void disk_pravetz_write(Uint16 addr, Uint8 data)
{
    disk_pravetz_switch(addr-disk_pravetz_offset);
    /* even addresses don't write anything */
    if(addr & 1)
    {
        disk_pravetz_write_selected(data);
    }
}

/* Skewing-Table */
static int skewing[16] = 
{   
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15 
};

/* Translation Table */
static int  translate[256] =
{
    0x96, 0x97, 0x9a, 0x9b, 0x9d, 0x9e, 0x9f, 0xa6,
    0xa7, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb9, 0xba, 0xbb, 0xbc,
    0xbd, 0xbe, 0xbf, 0xcb, 0xcd, 0xce, 0xcf, 0xd3,
    0xd6, 0xd7, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
    0xdf, 0xe5, 0xe6, 0xe7, 0xe9, 0xea, 0xeb, 0xec,
    0xed, 0xee, 0xef, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
    0xf7, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x01,
    0x80, 0x80, 0x02, 0x03, 0x80, 0x04, 0x05, 0x06,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x07, 0x08,
    0x80, 0x80, 0x80, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
    0x80, 0x80, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13,
    0x80, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x1b, 0x80, 0x1c, 0x1d, 0x1e,
    0x80, 0x80, 0x80, 0x1f, 0x80, 0x80, 0x20, 0x21,
    0x80, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x29, 0x2a, 0x2b,
    0x80, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32,
    0x80, 0x80, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
    0x80, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f
};

static void  disk_pravetz_update_position (void)
{
    if (current_drive->motor_on)
    {
        current_drive->byte = (1 + current_drive->byte) % RAW_TRACK_SIZE;
    }
}

static Uint8  disk_pravetz_image_raw_byte (disk_image_p d_ptr, Uint16 t_idx, Uint16 s_idx, Uint16 b_idx)
{
    long  f_pos;

    static Uint8  old;
    static Uint8  eor;
    static Uint8  raw;
    static Uint8  check;

    raw = 0xFF;

    switch (b_idx)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
        /* sync bytes */
        return 0xFF;

    case 6:
    case 25:
        /* sector header byte #1 */
        return 0xD5;

    case 7:
    case 26:
        /* sector header byte #2 */
        return 0xAA;

    case 8:
        /* address header byte */
        return 0x96;

    case 9:
        /* volume byte #1 */
        check = current_drive->volume;
        return 0xAA | (current_drive->volume >> 1);

    case 10:
        /* volume */
        return 0xAA | current_drive->volume;

    case 11:
        /* track byte #1 */
        check ^= t_idx;
        return 0xAA | (t_idx >> 1);

    case 12:
        /* track byte #2 */
        return 0xAA | t_idx;

    case 13:
        /* sector byte #1 */
        check ^= s_idx;
        return 0xAA | (s_idx >> 1);

    case 14:
        /* sector byte #2 */
        return 0xAA | s_idx;

    case 15:
        /* checksum byte #1 */
        return 0xAA | (check >> 1);

    case 16:
        /* checksum byte #2 */
        return 0xAA | check;

    case 17:
    case 371:
        /* address/data trailer byte #1 */
        return 0xDE;

    case 18:
    case 372:
        /* address/data trailer byte #2 */
        return 0xAA;

    case 19:
    case 373:
        /* address/data trailer byte #3 */
        return 0xEB;

    case 27:
        /* data header */
        eor = 0;

        /* Read the coming sector */
        f_pos = (256 * 16 * t_idx) + (256 * skewing[s_idx]);

        fseek(d_ptr->file, f_pos, SEEK_SET);
        fread(current_drive->sector_buf, 1, 256, d_ptr->file);

        return 0xAD;

    case 370:
        /* checksum */
        return translate[eor & 0x3F];

    default:
        b_idx -= 28;

        if (b_idx >= 0x56)
        {
            /* 6 Bit */
            old  = current_drive->sector_buf[b_idx - 0x56];
            old  = old >> 2;
            eor ^= old;
            raw  = translate[eor & 0x3F];
            eor  = old;
        }
        else
        {
            /* 3 * 2 Bit */
            old  = (current_drive->sector_buf[b_idx] & 0x01) << 1;
            old |= (current_drive->sector_buf[b_idx] & 0x02) >> 1;
            old |= (current_drive->sector_buf[b_idx + 0x56] & 0x01) << 3;
            old |= (current_drive->sector_buf[b_idx + 0x56] & 0x02) << 1;
            old |= (current_drive->sector_buf[b_idx + 0xAC] & 0x01) << 5;
            old |= (current_drive->sector_buf[b_idx + 0xAC] & 0x02) << 3;
            eor ^= old;
            raw  = translate[eor & 0x3F];
            eor  = old;
        }
        break;
    }

    return raw;
}


/**
** Returns the next byte from the 'disk'
*/

static Uint8  disk_pravetz_readbyte (void)
{
    disk_pravetz_update_position();
    /** disk_pravetz_load_images(); */

    /* make sure there's a 'disk in the drive' */
    if (0 == current_drive->file)
        return 0xFF;
    /*
    printf("{R} fdu: T:%.2d%c S:%.2X.%.3d [%.2X]\n",
        current_drive->half_track/2, (current_drive->half_track & 1) ? '+' : ' ',
        current_drive->byte/RAW_BYTES_PER_SECTOR,
        current_drive->byte%RAW_BYTES_PER_SECTOR,
        current_drive->image[current_drive->half_track / 2][current_drive->byte]);
    */
    return current_drive->image[current_drive->half_track / 2][current_drive->byte];
}

/**
** Returns the appropriate disk controller (IWM) register based on the
** state of the Q6 and Q7 select bits.
*/

static Uint8 disk_pravetz_read_selected(void)
{
    if (!current_drive->write_ready)
    {
        current_drive->write_ready = 0x80;
    }

    switch (current_drive->select)
    {
    case 0:
        return disk_pravetz_readbyte();

    case 1:
        return current_drive->protected | current_drive->motor_on;

    case 2:
        return current_drive->write_ready;
    }

    return 0;
}

static void  disk_pravetz_write_image (disk_image_p  d_ptr)
{
    Uint8   eor;
    Uint8   s_idx;

    int  sector_search_count;

    int  t_idx;
    int  b_idx;
    int  tb_idx;
    int  sector_count;

    Uint8 temp_sector_buffer[BYTES_PER_SECTOR + 2];

    if (!d_ptr->dirty)
        return;

    tb_idx = 0;
    for (t_idx = 0; t_idx < TRACKS_PER_DISK; t_idx++)
    {
        sector_count = 16;

find_sector:

        /* look for the sector header */

        sector_search_count = 0;
        while (0xD5 != d_ptr->image[t_idx][tb_idx])
        {
            tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
            if (++sector_search_count > RAW_TRACK_SIZE) return;
        }

        tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
        if (0xAA != d_ptr->image[t_idx][tb_idx])
        {
            tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
            goto find_sector;
        }

        tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
        if (0x96 != d_ptr->image[t_idx][tb_idx])
        {
            tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
            goto find_sector;
        }

        /* found the sector header */

        /* skip 'volume' and 'track' */
        tb_idx = (tb_idx + 5) % RAW_TRACK_SIZE;

        /* sector byte #1 */
        s_idx  = 0x55 | (d_ptr->image[t_idx][tb_idx] << 1);
        tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
        s_idx &= d_ptr->image[t_idx][tb_idx];
        tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;

        /* look for the sector data */

        while (0xD5 != d_ptr->image[t_idx][tb_idx])
            tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;

        tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
        if (0xAA != d_ptr->image[t_idx][tb_idx])
        {
            tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
            goto find_sector;
        }

        tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
        if (0xAD != d_ptr->image[t_idx][tb_idx])
        {
            tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
            goto find_sector;
        }

        /* found the sector data */

        tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;
        eor = 0;

        for (b_idx = 0; b_idx < 342; b_idx++)
        {
            eor ^= translate[d_ptr->image[t_idx][tb_idx]];
            tb_idx = (tb_idx + 1) % RAW_TRACK_SIZE;

            if (b_idx >= 0x56)
            {
                /* 6 Bit */
                temp_sector_buffer[b_idx - 0x56] |= (eor << 2);
            }
            else
            {
                /* 3 * 2 Bit */
                temp_sector_buffer[b_idx]         = (eor & 0x01) << 1;
                temp_sector_buffer[b_idx]        |= (eor & 0x02) >> 1;
                temp_sector_buffer[b_idx + 0x56]  = (eor & 0x04) >> 1;
                temp_sector_buffer[b_idx + 0x56] |= (eor & 0x08) >> 3;
                temp_sector_buffer[b_idx + 0xAC]  = (eor & 0x10) >> 3;
                temp_sector_buffer[b_idx + 0xAC] |= (eor & 0x20) >> 5;
            }
        }

        /* write the sector */

        fseek(d_ptr->file,
              (BYTES_PER_SECTOR * SECTORS_PER_TRACK * t_idx) +
              (BYTES_PER_SECTOR * skewing[s_idx]), SEEK_SET);

        fwrite(temp_sector_buffer, 1, BYTES_PER_SECTOR, d_ptr->file);

        sector_count--;

        if (sector_count)
            goto find_sector;
    }

    d_ptr->dirty = SDL_FALSE;
}

/**
** Writes a byte to the next position on the 'disk'
*/

static void  disk_pravetz_writebyte (Uint8  w_byte)
{
    disk_pravetz_update_position();

    /* make sure there's a 'disk in the drive' */
    if (0 == current_drive->file)
        return;

    if (current_drive->protected)
        return;

    /* don't allow impossible bytes */
    if (w_byte < 0x96)
        return;

    current_drive->dirty = SDL_TRUE;
    current_drive->image[current_drive->half_track / 2][current_drive->byte] = w_byte;

    /*
    printf("{W} fdu: T:%.2d%c S:%.2X.%.3d [%.2X]\n",
        current_drive->half_track/2, (current_drive->half_track & 1) ? '+' : ' ',
        current_drive->byte/RAW_BYTES_PER_SECTOR,
        current_drive->byte%RAW_BYTES_PER_SECTOR,
        w_byte);
    */
}

/**
** Writes to the appropriate disk controller (IWM) register based on the
** state of the Q6 and Q7 select bits.
*/

static void  disk_pravetz_write_selected (Uint8  w_byte)
{
    if (3 == current_drive->select)
    {
        if (current_drive->motor_on)
        {
            if (current_drive->write_ready)
            {
                disk_pravetz_writebyte(w_byte);
                current_drive->write_ready = 0;
            }
        }
    }
}

static void  disk_pravetz_switch (Uint16  addr)
{
    switch (addr & 0xF)
    {
    case 0x08:
        current_drive->motor_on = 0;
        disk_pravetz_write_image(current_drive);
        break;

    case 0x09:
        current_drive->motor_on = 0x20;
        break;

    case 0x0A:
        /* select drive 0 */
        if (current_drive != disk_drive)
        {
            disk_pravetz_write_image(current_drive);
            current_drive = disk_drive;
        }
        break;

    case 0x0B:
        /* select drive 1 */
        if (current_drive == disk_drive)
        {
            disk_pravetz_write_image(current_drive);
            current_drive = disk_drive + 1;
        }
        break;

    case 0x0C:
        current_drive->select &= 0xFE;
        break;

    case 0x0D:
        current_drive->select |= 0x01;
        break;

    case 0x0E:
        current_drive->select &= 0xFD;
        break;

    case 0x0F:
        current_drive->select |= 0x02;
        break;

    default:
    {
        int phase = (addr & 0x06) >> 1;

        /* move the head in and out by stepping motor */
        if (addr & 0x01)
        {
            phase += 4;
            phase -= (current_drive->half_track % 4);
            phase %= 4;

            if (1 == phase)
            {
                if (current_drive->half_track < (2 * TRACKS_PER_DISK - 2))
                {
                    current_drive->half_track++;
                }
            }
            else if (3 == phase)
            {
                if (current_drive->half_track > 0)
                {
                    current_drive->half_track--;
                }
            }
        }
    }
    break;
    }
}

SDL_bool disk_pravetz_load(const char* filename, int drive, SDL_bool readonly)
{
    disk_pravetz_free(drive);

    if(readonly)
    {
        disk_drive[drive].file = fopen(filename, "rb");
        if (disk_drive[drive].file)
            disk_drive[drive].protected = SDL_TRUE;
        else
        {
            disk_pravetz_free(drive);
        }
    }
    else
    {
        /* Try to open for read/write */
        disk_drive[drive].file = fopen(filename, "r+b");
        if (disk_drive[drive].file)
            disk_drive[drive].protected = SDL_FALSE;
        else
        {
            disk_pravetz_free(drive);

            /* Try to create the file */
            disk_drive[drive].file = fopen(filename, "wb");
            if (disk_drive[drive].file)
            {
                fclose(disk_drive[drive].file);
                disk_drive[drive].file = fopen(filename, "r+b");

                if (disk_drive[drive].file)
                    disk_drive[drive].protected = SDL_FALSE;
                else
                    disk_pravetz_free(drive);
            }
        }
    }

    if (disk_drive[drive].file)
    {
        long  size;

        fseek(disk_drive[drive].file, 0, SEEK_END);
        size = ftell(disk_drive[drive].file);

        if (0 == size)
        {
            /* new file... make a blank disk */
            long  b_idx;

            fseek(disk_drive[drive].file, 0, SEEK_SET);
            for (b_idx = 0; b_idx < 143360; b_idx++)
                fputc(0, disk_drive[drive].file);

            fseek(disk_drive[drive].file, 0, SEEK_END);
            size = ftell(disk_drive[drive].file);

            if (143360 != size)
            {
                fclose(disk_drive[drive].file);
                disk_drive[drive].file = 0;
                remove(filename);
                size = 0;
            }
        }

        if (143360 == size)
        {
            Uint16  t_idx;
            Uint16  s_idx;
            Uint16  sb_idx;
            Uint16  tb_idx;

            /* ### */
            for (t_idx = 0; t_idx < TRACKS_PER_DISK; t_idx++)
            {
                tb_idx = 0;

                for (s_idx = 0; s_idx < SECTORS_PER_TRACK; s_idx++)
                {
                    for (sb_idx = 0; sb_idx < RAW_BYTES_PER_SECTOR; sb_idx++, tb_idx++)
                    {
                        disk_drive[drive].image[t_idx][tb_idx] =
                            disk_pravetz_image_raw_byte(disk_drive + drive, t_idx, s_idx, sb_idx);
                    }
                }

                while (tb_idx < RAW_TRACK_SIZE)
                {
                    disk_drive[drive].image[t_idx][tb_idx] = 0xFF;
                    tb_idx++;
                }
            }
        }
        else
            disk_pravetz_free(drive);
    }

    disk_drive[drive].dirty      = SDL_FALSE;
    disk_drive[drive].byte       = 0;
    disk_drive[drive].half_track = 0;
    
    return (disk_drive[drive].file)? SDL_TRUE : SDL_FALSE;
}

void disk_pravetz_free(int drive)
{
    /* write out and close the old disk image, if any */
    if (disk_drive[drive].file)
    {
        disk_pravetz_write_image(disk_drive + drive);
        fflush(disk_drive[drive].file);
        fclose(disk_drive[drive].file);
    }

    disk_drive[drive].file       = 0;
    disk_drive[drive].byte       = 0;
    disk_drive[drive].dirty      = SDL_FALSE;
    disk_drive[drive].half_track = 0;
}

int disk_pravetz_drive( void )
{
    return current_drive->num;
}

SDL_bool disk_pravetz_active( void )
{
  return current_drive->motor_on ? SDL_TRUE : SDL_FALSE;
}
