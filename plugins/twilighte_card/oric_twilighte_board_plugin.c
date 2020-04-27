
#define TWILIGHTE_CARD_ORIC_EXTENSION_VDDRA            0x323
#define TWILIGHTE_CARD_ORIC_EXTENSION_DDRA             0x321

#define TWILIGHTE_CARD_ORIC_EXTENSION_REGISTER          0x342
#define TWILIGHTE_CARD_ORIC_EXTENSION_BANKING_REGISTER  0x343



#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>


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
 
  unsigned char twilrombankdata[32][16834];


  //unsigned char twilrambankdata[32][16834];

  //struct twilbankinfo rom_bank[32];
  unsigned char VDDRA;
  unsigned char DDRA;
  unsigned char current_bank;
  //struct twilbankinfo ram_bank[32];
};





#include "../../system.h"

extern SDL_bool read_config_string( char *buf, char *token, char *dest, Sint32 maxlen );
///extern SDL_bool load_rom( struct machine *oric, char *fname, int size, unsigned char *where, struct symboltable *stab, int symflags );
 
static SDL_bool load_rom_twilighte(  char *fname, int size, unsigned char where[] )
{
  SDL_RWops *f;
  char *tmpname;

  // MinGW doesn't have asprintf :-(
  tmpname = malloc( strlen( fname ) + 10 );
  if( !tmpname ) {

  return SDL_FALSE;
  }

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

    //where += (-size)-filesize;
    size = filesize;
  }

  if( SDL_RWread( f, &where, size, 1 ) != 1 )
  {
    error_printf( "Unable to read '%s'\n", tmpname );
    SDL_RWclose( f );
    free( tmpname );
    return SDL_FALSE;
  }

  SDL_RWclose( f );
  free( tmpname );
  return SDL_TRUE;
}

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

 for( j=1; j<31; j++ ) twilighte->twilrombankfiles[j][0]=0;

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
    for( i=0; i<31; i++ )
      {

//        oric->rom = oric->tele_bank[i].ptr  = &oric->mem[0x0c000+(i*0x4000)];
 
      if( twilighte->twilrombankfiles[i][0]!=0 )
        { 
            if( !load_rom_twilighte( twilighte->twilrombankfiles[i], -16384, twilighte->twilrombankdata[i]  ) ) {
			  error_printf("Cannot load %s",twilighte->twilrombankfiles[i]);
		  return NULL;
		  }
	
		}
		for (j=0;j<16384;j++) twilighte->twilrombankdata[i][j]=0x57;   
	  }
/* 
    
*/
    //twilighte->DDRA=7;
	//twilighte->VDDRA=7;
	//twilighte->current_bank=7;
	return  twilighte;
}

unsigned char 	twilighteboard_oric_ROM_RAM_read(struct twilighte *twilighte, uint16_t addr) {
	unsigned char data;
//	error_printf( "%d",twilighte->current_bank); 
//	return 0;
//	 if (twilighte->current_bank==0) 
 	//data=twilighte->twilrombankdata[0][0];
//	 else
//twilighte->twilrombankdata[0][0]=0;
data=twilighte->twilrombankdata[1][1];
error_printf( "addr : %u c000 : %x  value : %d\n",addr,0xc000+addr,data); 
//data=(unsigned char)twilighte->twilrombankdata[1][0];

		
	
	return data;
}

unsigned char 	twilighteboard_oric_ROM_RAM_write(struct twilighte *twilighte, uint16_t addr, unsigned char data) {

	 //if (twilighte->current_bank==0) 
		//twilighte->twilrambankdata[twilighte->current_bank][addr]=data;
	
	error_printf( "addr : %x %d\n",0xc000+addr,data);
	return 0;
}


unsigned char 	twilighteboard_oric_read(struct twilighte *twilighte, uint16_t addr)
{
	if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_REGISTER)
	{
		return twilighte->t_register;
	}

	if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_VDDRA)
	{
		return twilighte->VDDRA;
	}

	if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_DDRA)
	{
		return twilighte->DDRA;
	}

	if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_BANKING_REGISTER)
	{
		return twilighte->t_banking_register;
	}

	return 0;
}

unsigned char 	twilighteboard_oric_write(struct twilighte *twilighte, uint16_t addr, unsigned char data)
{
	if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_VDDRA)
	{
		twilighte->VDDRA=data;
		
	}

	if (addr == TWILIGHTE_CARD_ORIC_EXTENSION_DDRA)
	{
	    twilighte->DDRA=data;
		//twilighte->current_bank=data&0b00000111;
	}


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
