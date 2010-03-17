
enum
{
  MSGBOX_YES_NO = 0,
  MSGBOX_OK_CANCEL,
  MSGBOX_OK
};

SDL_bool init_msgbox( void );
void shut_msgbox( void );
SDL_bool msgbox( struct machine *oric, int type, char *msg );
