
void mon_init( struct machine *oric );
void mon_render( struct machine *oric );
void mon_update_regs( struct machine *oric );
SDL_bool mon_event( SDL_Event *ev, struct machine *oric, SDL_bool *needrender );
void dbg_printf( char *fmt, ... );
