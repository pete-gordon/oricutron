#if SDL_MAJOR_VERSION == 1
# ifdef __SPECIFY_SDL_DIR__
# include <SDL/SDL.h>
# else
# include <SDL.h>
# endif
#else /* SDL_MAJOR_VERSION == 1 */
# ifdef __SPECIFY_SDL_DIR__
# include <SDL2/SDL.h>
# else
# include <SDL.h>
# endif
#endif

#include "ch376.h"
#include "stdio.h"

void dbg_printf(char *fmt, ...);

#define CH376_ORIC_EXTENSION_DATA_PORT		0x340
#define CH376_ORIC_EXTENSION_COMMAND_PORT	0x341


struct ch376 * ch376_oric_init()
{
	return  ch376_create(NULL);
}

void ch376_oric_reset(struct ch376 *ch376)
{
	ch376_reset(ch376);
}


void ch376_oric_destroy(struct ch376 *ch376)
{
	ch376_destroy(ch376);
}

void ch376_oric_config(struct ch376 *ch376)
{
	ch376_set_sdcard_drive_path(ch376, "sdcard/");
	ch376_set_usb_drive_path(ch376, "usbdrive/");
}

void	ch376_oric_write(struct ch376 *ch376, Uint16 addr, Uint8 data)
{
	if (addr == CH376_ORIC_EXTENSION_DATA_PORT)
	{
		ch376_write_data_port(ch376, data);
	}

	if (addr == CH376_ORIC_EXTENSION_COMMAND_PORT)
	{
		ch376_write_command_port(ch376, data);
	}
}
	

unsigned char 	ch376_oric_read(struct ch376 *ch376, Uint16 addr)
{
	unsigned char  temp;
	if (addr == CH376_ORIC_EXTENSION_DATA_PORT)
	{
		temp=ch376_read_data_port(ch376);
		return temp;
	}

	if (addr == CH376_ORIC_EXTENSION_COMMAND_PORT)
	{
		temp=ch376_read_command_port(ch376);
		return temp;
	}
	return 0;
}
