#if PURPLE_MAJOR_VERSION == 2

#define purple_account_option_string_set_masked(a,b) (a->masked = b)
#define purple_conversation_get_connection(a) purple_account_get_connection((a)->account)
#define purple_connection_get_display_name(a) ((a)->display_name)
//#define purple_conversation_get_connection(a) (a->connection)

#endif
