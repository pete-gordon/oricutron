
#define TWILIGHTE_CARD_ORIC_EXTENSION_REGISTER          0x342
#define TWILIGHTE_CARD_ORIC_EXTENSION_BANKING_REGISTER  0x343



#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>


#include "../../system.h"

extern SDL_bool read_config_string( char *buf, char *token, char *dest, Sint32 maxlen );
///extern SDL_bool load_rom( struct machine *oric, char *fname, int size, unsigned char *where, struct symboltable *stab, int symflags );
 
static SDL_bool load_rom_twilighte(  char *fname, int size, unsigned char *where )
{
  SDL_RWops *f;
  char *tmpname;

  // MinGW doesn't have asprintf :-(
  tmpname = malloc( strlen( fname ) + 10 );
  if( !tmpname ) return SDL_FALSE;

  sprintf( tmpname, "%s.rom", fname );
  f = SDL_RWFromFile( tmpname, "rb" );
  if( !f )
  {
    error_printf( "Unable to open '%s'\n", tmpname );
    free( tmpname );
    return SDL_FALSE;
  }

  if( size < 0 )
  {
    int filesize;
    SDL_RWseek( f, 0, SEEK_END );
    filesize = SDL_RWtell( f );
    SDL_RWseek( f, 0, SEEK_SET );

    if( filesize > -size )
    {
      error_printf( "ROM '%s' exceeds %d bytes.\n", fname, -size );
      SDL_RWclose( f );
      free( tmpname );
      return SDL_FALSE;
    }

    where += (-size)-filesize;
    size = filesize;
  }

  if( SDL_RWread( f, where, size, 1 ) != 1 )
  {
    error_printf( "Unable to read '%s'\n", tmpname );
    SDL_RWclose( f );
    free( tmpname );
    return SDL_FALSE;
  }

  SDL_RWclose( f );

  sprintf( tmpname, "%s.sym", fname );
  //mon_new_symbols( stab, oric, tmpname, symflags, SDL_FALSE, SDL_FALSE );
  free( tmpname );

  return SDL_TRUE;
}

struct twilbankinfo
{
  unsigned char type;
  unsigned char *ptr;
};

struct twilighte
{
  
  unsigned char t_register;
  unsigned char t_banking_register;
  char twilrombankfiles[32][1024];
  char twilrombankdata[32][16834];
  struct twilbankinfo rom_bank[32];
  //struct twilbankinfo ram_bank[32];
};

char twilrombankfiles[100][1024];

struct twilighte * twilighte_oric_init(void)
{
	//struct twilighte * temp;
	//temp=malloc(sizeof(struct twilighte *));
	char twilrombankfiles[32][1024];
	FILE *f;
  	unsigned int i, j;
	char tbtmp[32];
	char line[1024];
	struct twilighte *twilighte = malloc(sizeof(struct twilighte));
	twilighte->t_banking_register=0;
	twilighte->t_register=128+1;

	f = fopen( "plugins/twilighte_card/twilighte.cfg", "r" );
  	if( !f ) {
		  error_printf( "plugins/twilighte_card/twilighte.cfg not found\n" );
		  return NULL;
	}

	
	while( !feof( f ) )
	  {
		  fgets( line, 1024, f );
		  //error_printf( line );
		  for( j=1; j<31; j++ )
    	  {
      		sprintf( tbtmp, "twilbankrom%c", j+'0' );
      		if( read_config_string( line, tbtmp, twilighte->twilrombankfiles[j], 1024 ) ) 
			  {
				  //error_printf( line );
				 error_printf("File: %d %s",j,twilighte->twilrombankfiles[j]);
				 
				  break;
			  }
			
    	  }
       }

  fclose( f );

  // Now load rom 
    for( i=1; i<31; i++ )
      {

//        oric->rom = oric->tele_bank[i].ptr  = &oric->mem[0x0c000+(i*0x4000)];
        if( twilighte->twilrombankfiles[i][0] )
        {
          //oric->tele_bank[i].type = TELEBANK_ROM;
          if( !load_rom_twilighte( twilighte->twilrombankfiles[i], -16384, &twilighte->rom_bank[i].ptr  ) ) return SDL_FALSE;
          //load_patches( oric, telebankfiles[i] );
        //} else {
          //oric->tele_bank[i].type = TELEBANK_RAM;
        //}
      }
	  }
/* 
    
*/
	return  twilighte;
}


unsigned char 	twilighteboard_oric_read(struct twilighte *twilighte, uint16_t addr)
{
	if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_REGISTER)
	{
		return twilighte->t_register;
	}

	if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_BANKING_REGISTER)
	{
		return twilighte->t_banking_register;
	}

	return 0;
}

unsigned char 	twilighteboard_oric_write(struct twilighte *twilighte, uint16_t addr, unsigned char data)
{
	if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_REGISTER)
	{
		twilighte->t_register=data;
	}

	if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_BANKING_REGISTER)
	{
		twilighte->t_banking_register=data;
	}

	return 0;
}
