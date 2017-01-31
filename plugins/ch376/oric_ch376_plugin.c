#include <SDL.h>
#include "ch376.h"
#include "stdio.h"

void dbg_printf(char *fmt, ...);

#define CH376_ORIC_EXTENSION_DATA_PORT		0x340
#define CH376_ORIC_EXTENSION_COMMAND_PORT	0x341


struct ch376 * ch376_oric_init()
{
	//struct *ch376;
	return  ch376_create(NULL);
	
	/*


	dbg_printf("Output :  %s", ch376_get_usb_drive_path(ch376));
	*/
	//dbg_printf("[INFO][DATA][CH376_CMD_FILE_CLOSE] Init oric ch376");
	//dbg_printf("[WRITE][DATA][CH376_CMD_FILE_CLOSE] ch376_set_usb_drive_path: 
	
	

/*
	// Configuration
	
	
	const char * ch376_get_sdcard_drive_path(struct ch376 *ch376);
	const char * ch376_get_usb_drive_path(struct ch376 *ch376);
	*/
//	return ch376;
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

void	ch376_oric_write(struct  ch376 *ch376, Uint16 addr, Uint8 data)
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
	

unsigned char 	ch376_oric_read(struct  ch376 *ch376, Uint16 addr)
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

