
#define TWILIGHTE_CARD_ORIC_EXTENSION_REGISTER		    0x342
#define TWILIGHTE_CARD_ORIC_EXTENSION_BANKING_REGISTER	0x343

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
  struct twilbankinfo rom_bank[32];
  struct twilbankinfo ram_bank[32];
};

struct twilighte * twilighte_oric_init(void)
{
	//struct twilighte * temp;
	//temp=malloc(sizeof(struct twilighte *));
	struct twilighte *twilighte = malloc(sizeof(struct twilighte));
	twilighte->t_banking_register=0;
	twilighte->t_register=128+1;
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
