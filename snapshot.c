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
**  Snapshot saving & loading
**
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "system.h"
#include "6502.h"
#include "via.h"
#include "8912.h"
#include "gui.h"
#include "disk.h"
#include "monitor.h"
#include "6551.h"
#include "machine.h"
#include "avi.h"
#include "filereq.h"
#include "main.h"
#include "ula.h"
#include "joystick.h"
#include "tape.h"
#include "msgbox.h"

extern char diskpath[];

#define MAX_BLOCK (262144)
static unsigned char *buf;
static int offs = 0;

#define PUTU32(val) ok=putu32(ok, (Uint32)val)
static SDL_bool putu32(SDL_bool stillok, Uint32 val)
{
  if (!stillok) return SDL_FALSE;
  if ((offs+4) > MAX_BLOCK) return SDL_FALSE;
  buf[offs++] = (val>>24)&0xff;
  buf[offs++] = (val>>16)&0xff;
  buf[offs++] = (val>>8)&0xff;
  buf[offs++] = val&0xff;
  return SDL_TRUE;
}

#define PUTU16(val) ok=putu16(ok, (Uint16)val)
static SDL_bool putu16(SDL_bool stillok, Uint16 val)
{
  if (!stillok) return SDL_FALSE;
  if ((offs+2) > MAX_BLOCK) return SDL_FALSE;
  buf[offs++] = (val>>8)&0xff;
  buf[offs++] = val&0xff;
  return SDL_TRUE;
}

#define PUTU8(val) ok=putu8(ok, (Uint8)val)
static SDL_bool putu8(SDL_bool stillok, Uint8 val)
{
  if (!stillok) return SDL_FALSE;
  if (offs >= MAX_BLOCK) return SDL_FALSE;
  buf[offs++] = val&0xff;
  return SDL_TRUE;
}

#define PUTDATA(data, size) ok=putdata(ok, data, size)
static SDL_bool putdata(SDL_bool stillok, unsigned char *data, Uint32 size)
{
  if (!stillok) return SDL_FALSE;
  if ((offs+size) > MAX_BLOCK) return SDL_FALSE;
  memcpy( &buf[offs], data, size );
  offs += size;
  return SDL_TRUE;
}

#define PUTSTR(str) ok=putstr(ok, str)
static SDL_bool putstr(SDL_bool stillok, char *str)
{
  if (!stillok) return SDL_FALSE;
  stillok = putu32(stillok, strlen(str)+1);
  return putdata(stillok, (unsigned char *)str, strlen(str)+1);
}

#define WRITEBLOCK() ok=writeblock(ok, f)
static SDL_bool writeblock(SDL_bool stillok, FILE *f)
{
  if (!stillok) return SDL_FALSE;
  if (offs <= 8) return SDL_TRUE; // No block to write

  buf[4] = ((offs-8)>>24)&0xff;
  buf[5] = ((offs-8)>>16)&0xff;
  buf[6] = ((offs-8)>>8)&0xff;
  buf[7] = (offs-8)&0xff;

  stillok = (fwrite(buf, offs, 1, f) == 1);
  offs = 0;
  return stillok;
}

// ID must be 4 bytes long!
#define NEWBLOCK(id) ok=newblock(ok, id, f)
static SDL_bool newblock(SDL_bool stillok, char *id, FILE *f)
{
  if (!stillok) return SDL_FALSE;
  // Got an old block?
  if (!writeblock(stillok, f)) return SDL_FALSE;

  // Start a new one!
  memcpy(buf, id, 4);
  offs = 8;
  return SDL_TRUE;
}

#define DATABLOCK(data, len) ok = datablock(ok, data, len, f)
static SDL_bool datablock(SDL_bool stillok, unsigned char *data, Uint32 len, FILE *f)
{
  Uint32 lenbe = _BE32(len);
  if (!stillok) return SDL_FALSE;
  // Anything to write?
  if (len == 0) return SDL_TRUE;
  // Got an old block?
  if (!writeblock(stillok, f)) return SDL_FALSE;

  stillok  = (fwrite("DATA", 4, 1, f) == 1);
  stillok &= (fwrite(&lenbe, 4, 1, f) == 1);
  stillok &= (fwrite(data, len, 1, f) == 1);

  offs = 0;
  return stillok;
}

void save_snapshot(struct machine *oric, char *filename)
{
  SDL_bool ok = SDL_TRUE;
  struct m6502 *cpu = &oric->cpu;
  SDL_bool do_wd17xx = SDL_FALSE;
  int i, j;
  FILE *f = NULL;

  buf = malloc(MAX_BLOCK);
  if (!buf)
  {
    msgbox(oric, MSGBOX_OK, "Snapshot failed: out of memory (1)\n");
    return;
  }

  f = fopen(filename, "wb");
  if (!f)
  {
    msgbox(oric, MSGBOX_OK, "Unable to create snapshot file (2)");
    free(buf);
    return;
  }

  NEWBLOCK("OSN\x00");
  PUTU8(oric->type);            //  0
  PUTU32(oric->overclockmult);  //  1
  PUTU32(oric->overclockshift); //  5
  PUTU8(oric->vsync);           //  9
  PUTU8(oric->romdis);          // 10
  PUTU8(oric->romon);           // 11
  PUTU8(oric->vsynchack);       // 12
  PUTU8(oric->drivetype);       // 13
  PUTU32(oric->keymap);         // 14 = 18
  DATABLOCK(oric->mem, oric->memsize);

  NEWBLOCK("TAP\x00");
  PUTU8(oric->tapebit);
  PUTU8(oric->tapeout);
  PUTU8(oric->tapeparity);
  PUTU32(oric->tapelen);
  PUTU32(oric->tapeoffs);
  PUTU32(oric->tapecount);
  PUTU32(oric->tapetime);
  PUTU32(oric->tapedupbytes);
  PUTU32(oric->tapehdrend);
  PUTU32(oric->tapedelay);
  PUTU8(oric->tapemotor);
  PUTU8(oric->tapeturbo_forceoff);
  PUTU8(oric->rawtape);
  PUTU32(oric->nonrawend);
  PUTU32(oric->tapehitend);
  PUTU32(oric->tapeturbo_syncstack);
  DATABLOCK(oric->tapebuf, oric->tapelen);

  // Patches
  NEWBLOCK("PCH\0x00");
  PUTU32(oric->pch_fd_cload_getname_pc);
  PUTU32(oric->pch_fd_csave_getname_pc);
  PUTU32(oric->pch_fd_store_getname_pc);
  PUTU32(oric->pch_fd_recall_getname_pc);
  PUTU32(oric->pch_fd_getname_addr);
  PUTU8(oric->pch_fd_available);
  PUTU32(oric->pch_tt_getsync_pc);
  PUTU32(oric->pch_tt_getsync_end_pc);
  PUTU32(oric->pch_tt_getsync_loop_pc);
  PUTU32(oric->pch_tt_readbyte_pc);
  PUTU32(oric->pch_tt_readbyte_end_pc);
  PUTU32(oric->pch_tt_readbyte_storebyte_addr);
  PUTU32(oric->pch_tt_readbyte_storezero_addr);
  PUTU32(oric->pch_tt_putbyte_pc);
  PUTU32(oric->pch_tt_putbyte_end_pc);
  PUTU32(oric->pch_tt_csave_end_pc);
  PUTU32(oric->pch_tt_store_end_pc);
  PUTU32(oric->pch_tt_writeleader_pc);
  PUTU32(oric->pch_tt_writeleader_end_pc);

  PUTU8(oric->pch_tt_readbyte_setcarry);
  PUTU8(oric->pch_tt_available);
  PUTU8(oric->pch_tt_save_available);

  // CPU
  NEWBLOCK("CPU\x00");
  PUTU32(cpu->cycles);
  PUTU16(cpu->pc);
  PUTU16(cpu->lastpc);
  PUTU16(cpu->calcpc);
  PUTU16(cpu->calcint);
  PUTU8(cpu->nmi);
  PUTU8(cpu->a);
  PUTU8(cpu->x);
  PUTU8(cpu->y);
  PUTU8(cpu->sp);
  PUTU8(MAKEFLAGS);
  PUTU8(cpu->irq);
  PUTU8(cpu->nmicount);
  PUTU8(cpu->calcop);

  // AY
  NEWBLOCK("AY\x00\x00");
  PUTU8(oric->ay.bmode);
  PUTU8(oric->ay.creg);
  PUTDATA(&oric->ay.eregs[0], NUM_AY_REGS);
  for (i=0; i<8; i++)
    PUTU8(oric->ay.keystates[i]);
  for (i=0; i<3; i++)
    PUTU32(oric->ay.toneper[i]);
  PUTU32(oric->ay.noiseper);
  PUTU32(oric->ay.envper);
  for (i=0; i<3; i++)
  {
    PUTU16(oric->ay.tonebit[i]);
    PUTU16(oric->ay.noisebit[i]);
    PUTU16(oric->ay.vol[i]);
  }
  PUTU16(oric->ay.newout);
  for (i=0; i<3; i++)
    PUTU32(oric->ay.ct[i]);
  PUTU32(oric->ay.ctn);
  PUTU32(oric->ay.cte);
  for (i=0; i<3; i++)
  {
    PUTU32(oric->ay.tonepos[i]);
    PUTU32(oric->ay.tonestep[i]);
    PUTU32(oric->ay.sign[i]);
    PUTU32(oric->ay.out[i]);
  }
  PUTU32(oric->ay.envpos);
  PUTU32(oric->ay.currnoise);
  PUTU32(oric->ay.rndrack);
  PUTU32(oric->ay.keybitdelay);
  PUTU32(oric->ay.currkeyoffs);

  // VIA
  NEWBLOCK("VIA\x00");
  PUTU8(oric->via.ifr);
  PUTU8(oric->via.irb);
  PUTU8(oric->via.orb);
  PUTU8(oric->via.irbl);
  PUTU8(oric->via.ira);
  PUTU8(oric->via.ora);
  PUTU8(oric->via.iral);
  PUTU8(oric->via.ddra);
  PUTU8(oric->via.ddrb);
  PUTU8(oric->via.t1l_l);
  PUTU8(oric->via.t1l_h);
  PUTU16(oric->via.t1c);
  PUTU8(oric->via.t2l_l);
  PUTU8(oric->via.t2l_h);
  PUTU16(oric->via.t2c);
  PUTU8(oric->via.sr);
  PUTU8(oric->via.acr);
  PUTU8(oric->via.pcr);
  PUTU8(oric->via.ier);
  PUTU8(oric->via.ca1);
  PUTU8(oric->via.ca2);
  PUTU8(oric->via.cb1);
  PUTU8(oric->via.cb2);
  PUTU8(oric->via.srcount);
  PUTU8(oric->via.t1reload);
  PUTU8(oric->via.t2reload);
  PUTU16(oric->via.srtime);
  PUTU8(oric->via.t1run);
  PUTU8(oric->via.t2run);
  PUTU8(oric->via.ca2pulse);
  PUTU8(oric->via.cb2pulse);
  PUTU8(oric->via.srtrigger);
  PUTU32(oric->via.irqbit);


  switch (oric->drivetype)
  {
    case DRV_JASMIN:
      NEWBLOCK("JSM\x00");
      PUTU8(oric->jasmin.olay);
      PUTU8(oric->jasmin.romdis);
      do_wd17xx = SDL_TRUE;
      break;

    case DRV_MICRODISC:
      NEWBLOCK("MDC\x00");
      PUTU8(oric->md.status);
      PUTU8(oric->md.intrq);
      PUTU8(oric->md.drq);
      PUTU8(oric->md.diskrom);
      do_wd17xx = SDL_TRUE;
      break;
  }

  if( do_wd17xx )
  {
    NEWBLOCK("WDD\x00");
    PUTU8(oric->wddisk.r_status);
    PUTU8(oric->wddisk.r_track);
    PUTU8(oric->wddisk.r_sector);
    PUTU8(oric->wddisk.r_data);
    PUTU8(oric->wddisk.c_drive);
    PUTU8(oric->wddisk.c_side);
    PUTU8(oric->wddisk.c_track);
    PUTU8(oric->wddisk.c_sector);
    PUTU8(oric->wddisk.sectype);
    PUTU8(oric->wddisk.last_step_in);
    PUTU32(oric->wddisk.currentop);
    PUTU32(oric->wddisk.curroffs);
    PUTU32(oric->wddisk.delayedint);
    PUTU32(oric->wddisk.delayeddrq);
    PUTU32(oric->wddisk.distatus);
    PUTU32(oric->wddisk.ddstatus);
    PUTU32(oric->wddisk.crc);

    for (i=0; i<4; i++)
    {
      if( oric->wddisk.disk[i] )
      {
        NEWBLOCK("DSK\x00");
        PUTU16(oric->wddisk.disk[i]->drivenum);
        PUTU16(oric->wddisk.disk[i]->numtracks);
        PUTU16(oric->wddisk.disk[i]->numsides);
        PUTU16(oric->wddisk.disk[i]->geometry);
        PUTU16(oric->wddisk.disk[i]->cachedtrack);
        PUTU16(oric->wddisk.disk[i]->cachedside);
        PUTU32(oric->wddisk.disk[i]->rawimagelen);
        DATABLOCK(oric->wddisk.disk[i]->rawimage, oric->wddisk.disk[i]->rawimagelen);
      }
    }
  }

  if (oric->type == MACH_TELESTRAT)
  {
    // Bank info
    NEWBLOCK("BNK\x00");
    for (i=0; i<8; i++)
      PUTU8(oric->tele_bank[i].type);
    PUTU8(oric->tele_currbank);

    // ACIA
    NEWBLOCK("ACI\x00");
    PUTDATA(&oric->tele_acia.regs[0], ACIA_LAST);

    // VIA
    NEWBLOCK("TVA\x00");
    PUTU8(oric->tele_via.ifr);
    PUTU8(oric->tele_via.irb);
    PUTU8(oric->tele_via.orb);
    PUTU8(oric->tele_via.irbl);
    PUTU8(oric->tele_via.ira);
    PUTU8(oric->tele_via.ora);
    PUTU8(oric->tele_via.iral);
    PUTU8(oric->tele_via.ddra);
    PUTU8(oric->tele_via.ddrb);
    PUTU8(oric->tele_via.t1l_l);
    PUTU8(oric->tele_via.t1l_h);
    PUTU16(oric->tele_via.t1c);
    PUTU8(oric->tele_via.t2l_l);
    PUTU8(oric->tele_via.t2l_h);
    PUTU16(oric->tele_via.t2c);
    PUTU8(oric->tele_via.sr);
    PUTU8(oric->tele_via.acr);
    PUTU8(oric->tele_via.pcr);
    PUTU8(oric->tele_via.ier);
    PUTU8(oric->tele_via.ca1);
    PUTU8(oric->tele_via.ca2);
    PUTU8(oric->tele_via.cb1);
    PUTU8(oric->tele_via.cb2);
    PUTU8(oric->tele_via.srcount);
    PUTU8(oric->tele_via.t1reload);
    PUTU8(oric->tele_via.t2reload);
    PUTU16(oric->tele_via.srtime);
    PUTU8(oric->tele_via.t1run);
    PUTU8(oric->tele_via.t2run);
    PUTU8(oric->tele_via.ca2pulse);
    PUTU8(oric->tele_via.cb2pulse);
    PUTU8(oric->tele_via.srtrigger);
    PUTU32(oric->tele_via.irqbit);
  }

  // Symbols
  if (oric->romsyms.numsyms)
  {
    NEWBLOCK("SYR\x00");
    for (i=0; i<oric->romsyms.numsyms; i++)
    {
      PUTSTR(oric->romsyms.syms[i].name);
      PUTU16(oric->romsyms.syms[i].addr);
      PUTU16(oric->romsyms.syms[i].flags);
    }
  }

  if (oric->usersyms.numsyms)
  {
    NEWBLOCK("SYU\x00");
    for (i=0; i<oric->usersyms.numsyms; i++)
    {
      PUTSTR(oric->usersyms.syms[i].name);
      PUTU16(oric->usersyms.syms[i].addr);
      PUTU16(oric->usersyms.syms[i].flags);
    }
  }

  if (oric->type == MACH_TELESTRAT)
  {
    for (j=0; j<8; j++)
    {
      if (oric->tele_banksyms[j].numsyms)
      {
        char name[8];
        sprintf(name, "SY%d%c", j, 0);
        NEWBLOCK(name);
        for (i=0; i<oric->tele_banksyms[j].numsyms; i++)
        {
          PUTSTR(oric->tele_banksyms[j].syms[i].name);
          PUTU16(oric->romsyms.syms[i].addr);
          PUTU16(oric->romsyms.syms[i].flags);
        }
      }
    }
  }

  // Breakpoints
  if ((cpu->anybp) || (cpu->anymbp))
  {
    NEWBLOCK("BKP\x00");
    for (i=0; i<16; i++)
      PUTU32(cpu->breakpoints[i]);
    for (i=0; i<16; i++)
    {
      PUTU8(cpu->membreakpoints[i].flags);
      PUTU8(cpu->membreakpoints[i].lastval);
      PUTU16(cpu->membreakpoints[i].addr);
    }
  }

  WRITEBLOCK();
  fclose(f);
  free(buf);

  if (!ok)
    msgbox(oric, MSGBOX_OK, "Snapshot failed! (3)");
}



struct blockheader
{
  unsigned char       id[4];
  unsigned int        offset;
  unsigned int        size;
  unsigned char      *buf;
  struct blockheader *datablock;
  int                 offs;
};

int numhdrs = 0;
static struct blockheader *bkh = NULL;

static SDL_bool getheaders(struct machine *oric, FILE *f)
{
  int i;
  unsigned int size, offset, filesize;
  unsigned char hdr[8];

  fseek(f, 0, SEEK_END);
  filesize = ftell(f);
  fseek(f, 0, SEEK_SET);

  /* First, find all the blocks in the file */
  /* and make sure the file structure is sane. */
  offset = 0;
  numhdrs = 0;
  while (offset < filesize)
  {
    if (fread(hdr, 8, 1, f) != 1)
    {
      msgbox(oric, MSGBOX_OK, "Snapshot load failed: Read error (4)");
      numhdrs = 0;
      return SDL_FALSE;
    }

    size = (hdr[4]<<24)|(hdr[5]<<16)|(hdr[6]<<8)|hdr[7];
    if ((size == 0) || ((offset+size) > filesize))
    {
      msgbox(oric, MSGBOX_OK, "Snapshot load failed: Invalid file structure (5)");
      numhdrs = 0;
      return SDL_FALSE;
    }
    
    numhdrs++;
    fseek(f, size, SEEK_CUR);
    offset += size+8;
  }

  /* Allocate memory to store all the block headers */
  bkh = (struct blockheader *)malloc(numhdrs*sizeof(struct blockheader));
  if (!bkh)
  {
    msgbox(oric, MSGBOX_OK, "Snapshot load failed: Out of memory (6)");
    numhdrs = 0;
    return SDL_FALSE;
  }
  memset(bkh, 0, numhdrs*sizeof(struct blockheader));

  /* Now read in all the block headers */
  fseek(f, 0, SEEK_SET);
  offset = 0;
  i = 0;
  while (offset < filesize)
  {
    if (fread(hdr, 8, 1, f) != 1)
    {
      msgbox(oric, MSGBOX_OK, "Snapshot load failed: Read error (7)");
      free(bkh);
      bkh = NULL;
      numhdrs = 0;
      return SDL_FALSE;
    }

    size = (hdr[4]<<24)|(hdr[5]<<16)|(hdr[6]<<8)|hdr[7];
    if ((size == 0) || ((offset+size) > filesize))
    {
      msgbox(oric, MSGBOX_OK, "Snapshot load failed: Invalid file structure (8)");
      free(bkh);
      bkh = NULL;
      numhdrs = 0;
      return SDL_FALSE;
    }
    offset+=8;

    memcpy(bkh[i].id, hdr, 4);
    bkh[i].offset = offset;
    bkh[i].size   = size;
    
    //printf("Block: %c%c%c%c (@ %d, size %d)\n", bkh[i].id[0], bkh[i].id[1], (bkh[i].id[2]>31)?bkh[i].id[2]:'.', (bkh[i].id[3]>31)?bkh[i].id[3]:'.', bkh[i].offset, bkh[i].size);
    
    if ((i>0) && (memcmp(bkh[i].id, "DATA", 4)==0))
    {
      bkh[i-1].datablock = &bkh[i];
    }

    fseek(f, size, SEEK_CUR);
    offset += size;
    i++;
  }

  return SDL_TRUE;
}

static void free_block(struct blockheader *blk)
{
  if ((!blk)||(!blk->buf)) return;
  free(blk->buf);
  blk->buf = NULL;
}

static void free_blockheaders(void)
{
  int i;

  if (!bkh) return;

  for (i=0; i<numhdrs; i++)
  {
    if (bkh[i].buf) free(bkh[i].buf);
  }
  free(bkh);
}

static struct blockheader *load_block(struct machine *oric, char *id, FILE *f, SDL_bool required, int expectedsize, SDL_bool datarequired)
{
  int i;

  for (i=0; i<numhdrs; i++)
  {
    if (memcmp(bkh[i].id, id, 4) == 0)
      break;
  }

  if (i==numhdrs)
  {
    //printf("Unable to find %c%c%c%c\n", id[0], id[1], (id[2]>31)?id[2]:'.', (id[3]>31)?id[3]:'.');
    if (required) msgbox(oric, MSGBOX_OK, "Snapshot load failed: Invalid file (9)");
    return NULL;
  }

  if ((datarequired) && (bkh[i].datablock == NULL))
  {
    if (required) msgbox(oric, MSGBOX_OK, "Snapshot load failed: Invalid file (10)");
    return NULL;
  }

  if ((expectedsize != -1) && (bkh[i].size != expectedsize))
  {
    //printf("Size for %c%c%c%c is %d, expected %d\n", id[0], id[1], (id[2]>31)?id[2]:'.', (id[3]>31)?id[3]:'.', bkh[i].size, expectedsize);
    if (required) msgbox(oric, MSGBOX_OK, "Snapshot load failed: Invalid file (11)");
    return NULL;
  }

  /* Already loaded it?! */
  if (bkh[i].buf)
  {
    bkh[i].offs = 0;
    return &bkh[i];
  }

  bkh[i].buf = malloc(bkh[i].size);
  if (!bkh[i].buf)
  {
    if (required) msgbox(oric, MSGBOX_OK, "Snapshot load failed: Out of memory (12)");
    return NULL;
  }

  fseek(f, bkh[i].offset, SEEK_SET);
  if (fread(bkh[i].buf, bkh[i].size, 1, f) != 1)
  {
    if (required) msgbox(oric, MSGBOX_OK, "Snapshot load failed: Read error (13)");
    return NULL;
  }

  bkh[i].offs = 0;
  return &bkh[i];
}

static SDL_bool read_block(struct machine *oric, struct blockheader *blk, FILE *f, SDL_bool required, unsigned char *destbuf)
{
  if (!blk)
  {
    if (required) msgbox(oric, MSGBOX_OK, "Snapshot load failed: Internal error (14)");
    return SDL_FALSE;
  }

  if (blk->buf)
  {
    memcpy(destbuf, blk->buf, blk->size);
    return SDL_TRUE;
  }

  fseek(f, blk->offset, SEEK_SET);
  if (fread(destbuf, blk->size, 1, f) != 1)
  {
    if (required) msgbox(oric, MSGBOX_OK, "Snapshot load failed: Read error (15)");
    return SDL_FALSE;
  }

  return SDL_TRUE;
}

static Uint32 getu32(struct blockheader *blk)
{
  Uint32 val = (blk->buf[blk->offs]<<24)|(blk->buf[blk->offs+1]<<16)|(blk->buf[blk->offs+2]<<8)|blk->buf[blk->offs+3];
  blk->offs+=4;
  return val;
}

static Sint32 gets32(struct blockheader *blk)
{
  Sint32 val = (blk->buf[blk->offs]<<24)|(blk->buf[blk->offs+1]<<16)|(blk->buf[blk->offs+2]<<8)|blk->buf[blk->offs+3];
  blk->offs+=4;
  return val;
}

static Uint16 getu16(struct blockheader *blk)
{
  Uint16 val = (blk->buf[blk->offs]<<8)|blk->buf[blk->offs+1];
  blk->offs+=2;
  return val;
}

static Sint16 gets16(struct blockheader *blk)
{
  Sint16 val = (blk->buf[blk->offs]<<8)|blk->buf[blk->offs+1];
  blk->offs+=2;
  return val;
}

static Uint8 getu8(struct blockheader *blk)
{
  return blk->buf[blk->offs++];
}

static void getdata(struct blockheader *blk, unsigned char *dest, unsigned int size)
{
  memcpy(dest, &blk->buf[blk->offs], size);
  blk->offs += size;
}

void load_snapshot(struct machine *oric, char *filename)
{
  struct m6502 *cpu = &oric->cpu;
  int i;
  SDL_bool do_wd17xx = SDL_FALSE;
  unsigned int type, drivetype;
  FILE *f = NULL;
  struct blockheader *blk = NULL;

  f = fopen(filename, "rb");
  if (!f)
  {
    msgbox(oric, MSGBOX_OK, "Unable to open snapshot file (16)");
    return;
  }

  if(!getheaders(oric, f))
  {
    fclose(f);
    return;
  }

  /* Get the main block */
  blk = load_block(oric, "OSN\x00", f, SDL_TRUE, 18, SDL_TRUE);
  if (!blk)
  {
    free_blockheaders();
    fclose(f);
    return;
  }

  /* Set up the emulation for the required machine type */
  type      = blk->buf[0];
  drivetype = blk->buf[13];

  if (type >= MACH_LAST)
  {
    msgbox(oric, MSGBOX_OK, "Snapshot load failed: Invalid file (17)");
    free_blockheaders();
    fclose(f);
    return;
  }

  swapmach(oric, NULL, (drivetype<<16)|type);

  /* Perform some validation */
  if((oric->drivetype != drivetype) ||             // Unrecognised or invalid drive type 
     (oric->memsize   != blk->datablock->size))    // Memsize incorrect for type
  {
    msgbox(oric, MSGBOX_OK, "Snapshot load failed: Invalid file (18)");
    free_blockheaders();
    fclose(f);
    return;
  }

  /* Read in the memory */
  if (!read_block(oric, blk->datablock, f, SDL_TRUE, oric->mem))
  {
    free_blockheaders();
    fclose(f);
    return;
  }

  /* Clear things that will get replaced */
  clear_patches( oric );
  if (oric->tapebuf)
  {
    free(oric->tapebuf);
    oric->tapebuf = NULL;
    oric->tapelen = 0;
  }

  blk->offs = 1;
  oric->overclockmult  = getu32(blk);
  oric->overclockshift = getu32(blk);
  oric->vsync          = getu8 (blk);
  oric->romdis         = getu8 (blk);
  oric->romon          = getu8 (blk);
  oric->vsynchack      = getu8 (blk);
  blk->offs++; // Skip drivetype (already got it)
  oric->keymap         = getu32(blk);
  
  // Finished with this one
  free_block(blk);

  /* Get the CPU block */
  blk = load_block(oric, "CPU\x00", f, SDL_TRUE, 21, SDL_FALSE);
  if (!blk)
  {
    free_blockheaders();
    fclose(f);
    return;
  }

  cpu->cycles   = getu32(blk);
  cpu->pc       = getu16(blk);
  cpu->lastpc   = getu16(blk);
  cpu->calcpc   = getu16(blk);
  cpu->calcint  = getu16(blk);
  cpu->nmi      = getu8 (blk);
  cpu->a        = getu8 (blk);
  cpu->x        = getu8 (blk);
  cpu->y        = getu8 (blk);
  cpu->sp       = getu8 (blk);
  i             = getu8 (blk);
  SETFLAGS(i);
  cpu->irq      = getu8 (blk);
  cpu->nmicount = getu8 (blk);
  cpu->calcop   = getu8 (blk);

  // Finished with this one
  free_block(blk);

  /* Get the AY block */
  blk = load_block(oric, "AY\x00\x00", f, SDL_TRUE, 153, SDL_FALSE);
  if (!blk)
  {
    free_blockheaders();
    fclose(f);
    return;
  }

  oric->ay.bmode        = getu8(blk);
  oric->ay.creg         = getu8(blk);
  getdata(blk, &oric->ay.eregs[0], NUM_AY_REGS);
  for (i=0; i<8; i++)
    oric->ay.keystates[i] = getu8(blk);
  for (i=0; i<3; i++)
    oric->ay.toneper[i] = getu32(blk);
  oric->ay.noiseper     = getu32(blk);
  oric->ay.envper       = getu32(blk);
  for (i=0; i<3; i++)
  {
    oric->ay.tonebit[i]  = getu16(blk);
    oric->ay.noisebit[i] = getu16(blk);
    oric->ay.vol[i]      = getu16(blk);
  }
  oric->ay.newout = getu16(blk);
  for (i=0; i<3; i++)
    oric->ay.ct[i]      = getu32(blk);
  oric->ay.ctn          = getu32(blk);
  oric->ay.cte          = getu32(blk);
  for (i=0; i<3; i++)
  {
    oric->ay.tonepos[i]  = getu32(blk);
    oric->ay.tonestep[i] = getu32(blk);
    oric->ay.sign[i]     = getu32(blk);
    oric->ay.out[i]      = getu32(blk);
  }
  oric->ay.envpos       = getu32(blk);
  oric->ay.currnoise    = getu32(blk);
  oric->ay.rndrack      = getu32(blk);
  oric->ay.keybitdelay  = getu32(blk);
  oric->ay.currkeyoffs  = getu32(blk);

  // Finished with this one
  free_block(blk);

  /* Get the VIA block */
  blk = load_block(oric, "VIA\x00", f, SDL_TRUE, 39, SDL_FALSE);
  if (!blk)
  {
    free_blockheaders();
    fclose(f);
    return;
  }

  oric->via.ifr      = getu8 (blk);
  oric->via.irb      = getu8 (blk);
  oric->via.orb      = getu8 (blk);
  oric->via.irbl     = getu8 (blk);
  oric->via.ira      = getu8 (blk);
  oric->via.ora      = getu8 (blk);
  oric->via.iral     = getu8 (blk);
  oric->via.ddra     = getu8 (blk);
  oric->via.ddrb     = getu8 (blk);
  oric->via.t1l_l    = getu8 (blk);
  oric->via.t1l_h    = getu8 (blk);
  oric->via.t1c      = getu16(blk);
  oric->via.t2l_l    = getu8 (blk);
  oric->via.t2l_h    = getu8 (blk);
  oric->via.t2c      = getu16(blk);
  oric->via.sr       = getu8 (blk);
  oric->via.acr      = getu8 (blk);
  oric->via.pcr      = getu8 (blk);
  oric->via.ier      = getu8 (blk);
  oric->via.ca1      = getu8 (blk);
  oric->via.ca2      = getu8 (blk);
  oric->via.cb1      = getu8 (blk);
  oric->via.cb2      = getu8 (blk);
  oric->via.srcount  = getu8 (blk);
  oric->via.t1reload = getu8 (blk);
  oric->via.t2reload = getu8 (blk);
  oric->via.srtime   = getu16(blk);
  oric->via.t1run    = getu8 (blk);
  oric->via.t2run    = getu8 (blk);
  oric->via.ca2pulse = getu8 (blk);
  oric->via.cb2pulse = getu8 (blk);
  oric->via.srtrigger= getu8 (blk);
  oric->via.irqbit   = getu32(blk);

  // Finished with this one
  free_block(blk);

  /* Get the tape block */
  blk = load_block(oric, "TAP\x00", f, SDL_TRUE, 46, SDL_TRUE);
  if (!blk)
  {
    free_blockheaders();
    fclose(f);
    return;
  }

  oric->tapebit     = getu8 (blk);
  oric->tapeout     = getu8 (blk);
  oric->tapeparity  = getu8 (blk);
  oric->tapelen     = getu32(blk);
  oric->tapeoffs    = getu32(blk);
  oric->tapecount   = getu32(blk);
  oric->tapetime    = getu32(blk);
  oric->tapedupbytes= getu32(blk);
  oric->tapehdrend  = getu32(blk);
  oric->tapedelay   = getu32(blk);
  oric->tapemotor   = getu8 (blk);
  oric->tapeturbo_forceoff= getu8(blk);
  oric->rawtape     = getu8 (blk);
  oric->nonrawend   = getu32(blk);
  oric->tapehitend  = getu32(blk);
  oric->tapeturbo_syncstack = getu32(blk);

  if (!blk->datablock)
  {
    oric->tapelen = 0;
    oric->tapeoffs = 0;
  }
  else
  {
    oric->tapebuf = malloc(blk->datablock->size);
    if (!oric->tapebuf)
    {
      msgbox(oric, MSGBOX_OK, "Snapshot load failed: Out of memory (19)");
      free_blockheaders();
      fclose(f);
      return;
    }

    if (!read_block(oric, blk->datablock, f, SDL_TRUE, oric->tapebuf))
    {
      free_blockheaders();
      fclose(f);
      return;
    }
  }

  // Finished with this one
  free_block(blk);

  /* Get the patch block */
  if ((blk = load_block(oric, "PCH\x00", f, SDL_FALSE, 76, SDL_FALSE)))
  {
    oric->pch_fd_cload_getname_pc        = getu32(blk);
    oric->pch_fd_csave_getname_pc        = getu32(blk);
    oric->pch_fd_store_getname_pc        = getu32(blk);
    oric->pch_fd_recall_getname_pc       = getu32(blk);
    oric->pch_fd_getname_addr            = getu32(blk);
    oric->pch_fd_available               = getu8(blk);
    oric->pch_tt_getsync_pc              = getu32(blk);
    oric->pch_tt_getsync_end_pc          = getu32(blk);
    oric->pch_tt_getsync_loop_pc         = getu32(blk);
    oric->pch_tt_readbyte_pc             = getu32(blk);
    oric->pch_tt_readbyte_end_pc         = getu32(blk);
    oric->pch_tt_readbyte_storebyte_addr = getu32(blk);
    oric->pch_tt_readbyte_storezero_addr = getu32(blk);
    oric->pch_tt_putbyte_pc              = getu32(blk);
    oric->pch_tt_putbyte_end_pc          = getu32(blk);
    oric->pch_tt_csave_end_pc            = getu32(blk);
    oric->pch_tt_store_end_pc            = getu32(blk);
    oric->pch_tt_writeleader_pc          = getu32(blk);
    oric->pch_tt_writeleader_end_pc      = getu32(blk);
    oric->pch_tt_readbyte_setcarry       = getu8 (blk);
    oric->pch_tt_available               = getu8 (blk);
    oric->pch_tt_save_available          = getu8 (blk);

    // Finished with this one
    free_block(blk);
  }

  switch (oric->drivetype)
  {
    case DRV_JASMIN:
      /* Get the jasmin block */
      blk = load_block(oric, "JSM\x00", f, SDL_TRUE, 2, SDL_FALSE);
      if (!blk)
      {
        free_blockheaders();
        fclose(f);
        return;
      }

      oric->jasmin.olay   = getu8(blk);
      oric->jasmin.romdis = getu8(blk);

      // Finished with this one
      free_block(blk);

      do_wd17xx = SDL_TRUE;
      break;

    case DRV_MICRODISC:
      /* Get the microdisc block */
      blk = load_block(oric, "MDC\x00", f, SDL_TRUE, 4, SDL_FALSE);
      if (!blk)
      {
        free_blockheaders();
        fclose(f);
        return;
      }

      oric->md.status  = getu8(blk);
      oric->md.intrq   = getu8(blk);
      oric->md.drq     = getu8(blk);
      oric->md.diskrom = getu8(blk);

      // Finished with this one
      free_block(blk);

      do_wd17xx = SDL_TRUE;
      break;
  }

  if (do_wd17xx)
  {
    Uint8 disks[4];

    /* Get the wd17xx block */
    blk = load_block(oric, "WDD\x00", f, SDL_TRUE, 38, SDL_FALSE);
    if (!blk)
    {
      free_blockheaders();
      fclose(f);
      return;
    }
    
    oric->wddisk.r_status     = getu8 (blk);
    oric->wddisk.r_track      = getu8 (blk);
    oric->wddisk.r_sector     = getu8 (blk);
    oric->wddisk.r_data       = getu8 (blk);
    oric->wddisk.c_drive      = getu8 (blk);
    oric->wddisk.c_side       = getu8 (blk);
    oric->wddisk.c_track      = getu8 (blk);
    oric->wddisk.c_sector     = getu8 (blk);
    oric->wddisk.sectype      = getu8 (blk);
    oric->wddisk.last_step_in = getu8 (blk);
    getdata(blk, disks, 4);
    oric->wddisk.currentop    = getu32(blk);
    oric->wddisk.curroffs     = getu32(blk);
    oric->wddisk.delayedint   = getu32(blk);
    oric->wddisk.delayeddrq   = getu32(blk);
    oric->wddisk.distatus     = getu32(blk);
    oric->wddisk.ddstatus     = getu32(blk);
    oric->wddisk.crc          = getu32(blk);

    // Finished with this one
    free_block(blk);

    // Get the disk images
    while ((blk = load_block(oric, "DSK\x00", f, SDL_FALSE, -1, SDL_FALSE)))
    {
      int track_to_cache=-1, side_to_cache=-1;

      // Get the drive associated with this disk image
      i = getu16(blk);
      if ((i<0) || (i>3) || (oric->wddisk.disk[i] != NULL) || (!blk->datablock) || (blk->size != 16))
      {
        msgbox(oric, MSGBOX_OK, "Snapshot load failed: Invalid file (20)");
        free_blockheaders();
        fclose(f);
        return;
      }

      // Allocate a diskimage header
      oric->wddisk.disk[i] = (struct diskimage *)malloc(sizeof(struct diskimage));
      if (!oric->wddisk.disk[i])
      {
        msgbox(oric, MSGBOX_OK, "Snapshot load failed: Out of memory (21)");
        free_blockheaders();
        fclose(f);
        return;
      }

      // Allocate space for the disk image
      oric->wddisk.disk[i]->rawimage = malloc(blk->datablock->size);
      if (!oric->wddisk.disk[i])
      {
        free(oric->wddisk.disk[i]);
        oric->wddisk.disk[i] = NULL;
        msgbox(oric, MSGBOX_OK, "Snapshot load failed: Out of memory (22)");
        free_blockheaders();
        fclose(f);
        return;
      }

      // Read in the disk image
      if (!read_block(oric, blk->datablock, f, SDL_TRUE, oric->wddisk.disk[i]->rawimage))
      {
        free(oric->wddisk.disk[i]->rawimage);
        free(oric->wddisk.disk[i]);
        oric->wddisk.disk[i] = NULL;
        free_blockheaders();
        fclose(f);
        return;
      }

      memset(oric->wddisk.disk[i], 0, sizeof(struct diskimage));
      oric->wddisk.disk[i]->cachedtrack = -1;
      oric->wddisk.disk[i]->cachedside  = -1;

      // Fill out the disk image header
      oric->wddisk.disk[i]->drivenum    = i;
      oric->wddisk.disk[i]->numtracks   = getu16(blk);
      oric->wddisk.disk[i]->numsides    = getu16(blk);
      oric->wddisk.disk[i]->geometry    = getu16(blk);
      track_to_cache                    = gets16(blk);
      side_to_cache                     = gets16(blk);
      oric->wddisk.disk[i]->rawimagelen = getu32(blk);
      
      if ((track_to_cache != -1) && (side_to_cache != -1))
        diskimage_cachetrack(oric->wddisk.disk[i], track_to_cache, side_to_cache);

      oric->wddisk.currsector = wd17xx_find_sector(&oric->wddisk, oric->wddisk.r_sector);

      sprintf(oric->wddisk.disk[i]->filename, "%s%cSNAPDISK%d.DSK", diskpath, PATHSEP, i);

      blk->id[0] = 0; // Don't find this one again
      free_block(blk);
    }
  }

  if (oric->type == MACH_TELESTRAT)
  {
    /* Get the tape block */
    blk = load_block(oric, "BNK\x00", f, SDL_TRUE, 9, SDL_FALSE);
    if (!blk)
    {
      free_blockheaders();
      fclose(f);
      return;
    }

    for (i=0; i<8; i++)
      oric->tele_bank[i].type = getu8(blk);
    oric->tele_currbank = getu8(blk);

    // Finished with this one
    free_block(blk);

    /* Get the ACIA block */
    blk = load_block(oric, "ACI\x00", f, SDL_TRUE, ACIA_LAST, SDL_FALSE);
    if (!blk)
    {
      free_blockheaders();
      fclose(f);
      return;
    }
    
    getdata(blk, &oric->tele_acia.regs[0], ACIA_LAST);

    // Finished with this one
    free_block(blk);

    /* Get the VIA block */
    blk = load_block(oric, "TVA\x00", f, SDL_TRUE, 39, SDL_FALSE);
    if (!blk)
    {
      free_blockheaders();
      fclose(f);
      return;
    }
  
    oric->tele_via.ifr      = getu8 (blk);
    oric->tele_via.irb      = getu8 (blk);
    oric->tele_via.orb      = getu8 (blk);
    oric->tele_via.irbl     = getu8 (blk);
    oric->tele_via.ira      = getu8 (blk);
    oric->tele_via.ora      = getu8 (blk);
    oric->tele_via.iral     = getu8 (blk);
    oric->tele_via.ddra     = getu8 (blk);
    oric->tele_via.ddrb     = getu8 (blk);
    oric->tele_via.t1l_l    = getu8 (blk);
    oric->tele_via.t1l_h    = getu8 (blk);
    oric->tele_via.t1c      = getu16(blk);
    oric->tele_via.t2l_l    = getu8 (blk);
    oric->tele_via.t2l_h    = getu8 (blk);
    oric->tele_via.t2c      = getu16(blk);
    oric->tele_via.sr       = getu8 (blk);
    oric->tele_via.acr      = getu8 (blk);
    oric->tele_via.pcr      = getu8 (blk);
    oric->tele_via.ier      = getu8 (blk);
    oric->tele_via.ca1      = getu8 (blk);
    oric->tele_via.ca2      = getu8 (blk);
    oric->tele_via.cb1      = getu8 (blk);
    oric->tele_via.cb2      = getu8 (blk);
    oric->tele_via.srcount  = getu8 (blk);
    oric->tele_via.t1reload = getu8 (blk);
    oric->tele_via.t2reload = getu8 (blk);
    oric->tele_via.srtime   = getu16(blk);
    oric->tele_via.t1run    = getu8 (blk);
    oric->tele_via.t2run    = getu8 (blk);
    oric->tele_via.ca2pulse = getu8 (blk);
    oric->tele_via.cb2pulse = getu8 (blk);
    oric->tele_via.srtrigger= getu8 (blk);
    oric->tele_via.irqbit   = getu32(blk);

    // Finished with this one
    free_block(blk);
  }

  /* Get the rom symbols block */
  if ((blk = load_block(oric, "SYR\x00", f, SDL_FALSE, -1, SDL_FALSE)))
  {
    mon_symsfromsnapshot(&oric->romsyms, blk->buf, blk->size);
    free_block(blk);
  }

  /* Get the user symbols block */
  if ((blk = load_block(oric, "SYU\x00", f, SDL_FALSE, -1, SDL_FALSE)))
  {
    mon_symsfromsnapshot(&oric->usersyms, blk->buf, blk->size);
    free_block(blk);
  }

  if (oric->type == MACH_TELESTRAT)
  {
    for (i=0; i<8; i++)
    {
      char name[8];
      sprintf(name, "SY%d%c", i, 0);
      if ((blk = load_block(oric, name, f, SDL_FALSE, -1, SDL_FALSE)))
      {
        mon_symsfromsnapshot(&oric->tele_banksyms[i], blk->buf, blk->size);
        free_block(blk);
      }
    } 
  }

  /* ... and finally, breakpoints! */
  if ((blk = load_block(oric, "SYU\x00", f, SDL_FALSE, -1, SDL_FALSE)))
  {
    cpu->anybp = SDL_FALSE;
    cpu->anymbp = SDL_FALSE;

    for (i=0; i<16; i++)
    {
      cpu->breakpoints[i] = gets32(blk);
      if (cpu->breakpoints[i] != -1) cpu->anybp = SDL_TRUE;
    }

    for (i=0; i<16; i++)
    {
      cpu->membreakpoints[i].flags   = getu8(blk);
      cpu->membreakpoints[i].lastval = getu8(blk);
      cpu->membreakpoints[i].addr    = getu16(blk);
      if (cpu->membreakpoints[i].flags) cpu->anymbp = SDL_TRUE;
    }

    free_block(blk);
  }
    
  free_blockheaders();
  fclose(f);
}

