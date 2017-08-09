struct ch376 * ch376_oric_init(void);
void ch376_oric_reset(struct ch376 *ch376);
void	ch376_oric_write(struct  ch376 *ch376, Uint16 addr, Uint8 data);
unsigned char 	ch376_oric_read(struct  ch376 *ch376, Uint16 addr);
void ch376_oric_config(struct ch376 *ch376);
void ch376_oric_destroy(struct ch376 *ch376);
void ch376_oric_reset(struct ch376 *ch376);
