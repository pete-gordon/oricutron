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
    msgbox(oric, MSGBOX_OK, "Snapshot failed: out of memory\n");
    return;
  }

  f = fopen(filename, "wb");
  if (!f)
  {
    msgbox(oric, MSGBOX_OK, "Unable to create snapshot file");
    free(buf);
    return;
  }

  NEWBLOCK("OSN\x00");
  PUTU8(oric->type);
  PUTU32(oric->overclockmult);
  PUTU32(oric->overclockshift);
  PUTU8(oric->vsync);
  PUTU8(oric->romdis);
  PUTU8(oric->romon);
  PUTU8(oric->vsynchack);
  PUTU8(oric->drivetype);
  PUTU32(oric->keymap);

  // Memory
  switch (oric->type)
  {
    case MACH_ORIC1_16K: DATABLOCK(oric->mem, 16384+16384); break;
    case MACH_ORIC1:     DATABLOCK(oric->mem, 65536+16384); break;
    case MACH_ATMOS:     DATABLOCK(oric->mem, 65536+16384); break;
    case MACH_TELESTRAT: DATABLOCK(oric->mem, 65536+16384*7); break;
    case MACH_PRAVETZ:   DATABLOCK(oric->mem, 65536+16384); break;
  }

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
    PUTU16(oric->ay.vol[3]);
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
    PUTU8((oric->wddisk.disk[0] != NULL));
    PUTU8((oric->wddisk.disk[1] != NULL));
    PUTU8((oric->wddisk.disk[2] != NULL));
    PUTU8((oric->wddisk.disk[3] != NULL));
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
        PUTU16(oric->wddisk.disk[i]->numsectors);
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

  if ((oric->disksyms) && (oric->disksyms->numsyms))
  {
    NEWBLOCK("SYD\x00");
    for (i=0; i<oric->disksyms->numsyms; i++)
    {
      PUTSTR(oric->disksyms->syms[i].name);
      PUTU16(oric->romsyms.syms[i].addr);
      PUTU16(oric->romsyms.syms[i].flags);
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
    msgbox(oric, MSGBOX_OK, "Snapshot failed!");
}

