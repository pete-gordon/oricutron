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
**  Monitor/Debugger
*/

#define SNAME_LEN 11              // Short name for symbols (with ...)
#define SSNAME_LEN (SNAME_LEN-3)  // Short short name :)

#define SYMB_ROMDIS0   0
#define SYMF_ROMDIS0   (1<<SYMB_ROMDIS0)
#define SYMB_ROMDIS1   1
#define SYMF_ROMDIS1   (1<<SYMB_ROMDIS1)
#define SYMB_MICRODISC 2
#define SYMF_MICRODISC (1<<SYMB_MICRODISC)
#define SYMB_JASMIN    3
#define SYMF_JASMIN    (1<<SYMB_JASMIN)
#define SYMB_TELEBANK0 4
#define SYMF_TELEBANK0 (1<<SYMB_TELEBANK0)
#define SYMB_TELEBANK1 5
#define SYMF_TELEBANK1 (1<<SYMB_TELEBANK1)
#define SYMB_TELEBANK2 6
#define SYMF_TELEBANK2 (1<<SYMB_TELEBANK2)
#define SYMB_TELEBANK3 7
#define SYMF_TELEBANK3 (1<<SYMB_TELEBANK3)
#define SYMB_TELEBANK4 8
#define SYMF_TELEBANK4 (1<<SYMB_TELEBANK4)
#define SYMB_TELEBANK5 9
#define SYMF_TELEBANK5 (1<<SYMB_TELEBANK5)
#define SYMB_TELEBANK6 10
#define SYMF_TELEBANK6 (1<<SYMB_TELEBANK6)
#define SYMB_TELEBANK7 11
#define SYMF_TELEBANK7 (1<<SYMB_TELEBANK7)


#define SYM_BESTGUESS  0xffff

struct msym
{
  unsigned short addr;       // Address
  unsigned short flags;
  char sname[SNAME_LEN+1];   // Short name
  char ssname[SSNAME_LEN+1]; // Short short name
  char *name;                // Full name
};

struct symboltable
{
  unsigned int numsyms, symspace;
  struct msym *syms;
};

SDL_bool isws( char c );
SDL_bool ishex( char c );
int hexit( char c );

void mon_warminit( struct machine *oric );
void mon_init( struct machine *oric );
void mon_render( struct machine *oric );
void mon_update_regs( struct machine *oric );
void mon_update( struct machine *oric );
SDL_bool mon_event( SDL_Event *ev, struct machine *oric, SDL_bool *needrender );
void dbg_printf( char *fmt, ... );
void mon_printf_above( char *fmt, ... );
void mon_enter( struct machine *oric );
void mon_shut( struct machine *oric );
void mon_init_symtab( struct symboltable *stab );
void mon_freesyms( struct symboltable *stab );
SDL_bool mon_new_symbols( struct symboltable *stab, struct machine *oric, char *fname, unsigned short flags, SDL_bool above, SDL_bool verbose );
SDL_bool mon_symsfromsnapshot( struct symboltable *stab, unsigned char *buffer, unsigned int len);
void mon_state_reset( struct machine *oric );
SDL_bool mon_getnum( struct machine *oric, unsigned int *num, char *buf, int *off, SDL_bool addrregs, SDL_bool nregs, SDL_bool viaregs, SDL_bool symbols );
SDL_bool mon_do_cmd( char *cmd, struct machine *oric, SDL_bool *needrender );

