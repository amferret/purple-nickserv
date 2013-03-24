#define _GNU_SOURCE
#define PURPLE_PLUGINS

#include "combineargs.h"

#include <stdio.h>

#include <plugin.h>
#include <account.h>
#include <accountopt.h>
#include <blist.h>
#include <connection.h>
#include <conversation.h>
#include <version.h>
#include <cmds.h>

#include <libintl.h>

#include <pcre.h>
#include <string.h>

#define _ gettext

#define PLUGIN_STATIC_NAME "nickserv"
#define PLUGIN_ID "super-unique-id-" PLUGIN_STATIC_NAME

#define PP_VERSION "2.6.0"

#define IRC_PLUGIN_ID "prpl-irc"

#define DESIRED_NICK PLUGIN_ID ".desired"
#define NICKSERV_NAME PLUGIN_ID ".nickserv"
#define NICKSERV_USE_PRIVMSG PLUGIN_ID ".privmsg"
#define PASSWORD PLUGIN_ID ".password"

static gboolean plugin_load(PurplePlugin *plugin);
static gboolean plugin_unload(PurplePlugin *plugin);
static GList* plugin_actions(PurplePlugin* plugin, gpointer context);

static PurplePluginInfo info =
{
	PURPLE_PLUGIN_MAGIC,
	PURPLE_MAJOR_VERSION,
	0,
	PURPLE_PLUGIN_STANDARD,          /**< type           */
	NULL,                            /**< ui_requirement */
	0,                               /**< flags          */
	NULL,                            /**< dependencies   */
	PURPLE_PRIORITY_DEFAULT,         /**< priority       */
	PLUGIN_ID,                       /**< id             */
	NULL,                            /**< name           */
	PP_VERSION,                      /**< version        */
	NULL,                            /**< summary        */
	NULL,                            /**< description    */
	"some hacker",	                 /**< author         */
	"http://lulz.net",                      /**< homepage       */
	plugin_load,                     /**< load           */
	plugin_unload,                   /**< unload         */
	NULL,                            /**< destroy        */

	NULL,                            /**< ui_info        */
	NULL,                            /**< extra_info     */
	NULL,                            /**< prefs_info     */
	NULL,                            /**< actions        */
	NULL,                            /**< reserved 1     */
	NULL,                            /**< reserved 2     */
	NULL,                            /**< reserved 3     */
	NULL                             /**< reserved 4     */
};

/*******************************************************************************************************/
/* fucking globals how do cthey work */

typedef struct account_context_s {
  PurpleConvIm* nick_conv;
  PurpleConversation* nick_conv_thingy;
  guint nick_checker;
  gboolean identified;
} account_context;

pcre* ask_for_register = NULL;
pcre_extra* ask_for_register_study = NULL;
pcre* was_identified = NULL;
pcre_extra* was_identified_study = NULL;
PurplePlugin* g_plugin = NULL;


/*******************************************************************************************************/

GHashTable* contexts = NULL;

account_context* find_context(PurpleAccount* account) {
  account_context* context = g_hash_table_lookup(contexts,account);
  if(!context) {
    context = g_new0(account_context,1);
    context->nick_conv_thingy = NULL;
    g_hash_table_replace(contexts,account,context);
  }

  return context;
}

// why can't I go args = "nickserv" + args? x_x

#define tell_nickserv(ctx,account,args...) tell_nickserv_c_sucks(ctx,account,"nickserv",args)

static void tell_nickserv_c_sucks(account_context* ctx, PurpleAccount *account, ...) {
  va_list args;
  va_start(args,account);
  char* line = vcombineargs(args);
  va_end(args);
  if(purple_account_get_bool(account,NICKSERV_USE_PRIVMSG,FALSE)==FALSE) {
    gchar* error = NULL;
    fprintf(stderr,"Trying command %s\n",line);
    if(PURPLE_CMD_STATUS_OK!=purple_cmd_do_command(ctx->nick_conv_thingy,
					    line,line,&error)) {
      fprintf(stderr,"Nickserv command failed %s",line);
    }
  } else {
    const char* message = line + 9;
    fprintf(stderr,"Trying privmsg %s %d\n",message,strlen(message));
    purple_conv_im_send_with_flags(ctx->nick_conv,
				   message,
				   PURPLE_MESSAGE_NO_LOG |
				   PURPLE_MESSAGE_INVISIBLE |
				   PURPLE_MESSAGE_AUTO_RESP);
  }
  free(line);
}

static void doIdentify(account_context* ctx, PurpleAccount* account, const char* password) {
    tell_nickserv(ctx,account,"IDENTIFY",password,NULL);
}

static account_context* check_nick_conv(PurpleAccount* account) {
  account_context* context = find_context(account);
  if(!context->nick_conv_thingy) {
    context->nick_conv_thingy = purple_conversation_new(PURPLE_CONV_TYPE_IM,account,purple_account_get_string(account,NICKSERV_NAME,"NickServ"));
    context->nick_conv = purple_conversation_get_im_data(context->nick_conv_thingy);
    return context;
  }
  if(!context->nick_conv)
    context->nick_conv = purple_conversation_get_im_data(context->nick_conv_thingy);
  return context;
}

static void tell_user(account_context* ctx,...) {
  va_list args;
  va_start(args,ctx);
  char* message = vcombineargs(args);
  va_end(args);
  purple_conversation_write(ctx->nick_conv_thingy,
			    "nickserv identifier",
			    message,
			    PURPLE_MESSAGE_SYSTEM,
			    time(NULL));
  g_free(message);
}


static gboolean check_for_nickserv(PurpleAccount *account,
                               char **sender,
                               char **message,
                               PurpleConversation *conv,
                               PurpleMessageFlags *flags) {
  g_assert(sender);
  if(strcmp(*sender,
	    purple_account_get_string(account,NICKSERV_NAME,"NickServ")))
    return FALSE;
  const char* password = purple_account_get_string(account,PASSWORD,NULL);
  if(password==NULL) return TRUE;

  account_context* ctx = check_nick_conv(account);
  if(!conv) {
    conv = ctx->nick_conv_thingy;
  } else if(conv != ctx->nick_conv_thingy) return FALSE;

  int rc = pcre_exec(ask_for_register,                   /* the compiled pattern */
		     ask_for_register_study,             /* no extra data - we didn't study the pattern */
		     *message,              /* the subject string */
		     strlen(*message),       /* the length of the subject */
		     0,                    /* start at offset 0 in the subject */
		     0,                    /* default options */
		     NULL,              /* output vector for substring information */
		     0);           /* number of elements in the output vector */

  if(rc >= 0) {
    tell_user(ctx,"Identifying to nickserv as",
	      purple_connection_get_display_name(purple_conversation_get_connection(conv)),NULL);
    doIdentify(ctx,account,password);
    return TRUE;
  }
  rc = pcre_exec(was_identified,
          was_identified_study,
          *message,
          strlen(*message),
          0,
          0,
          NULL,
          0);
  if(rc >= 0) {
      ctx->identified = TRUE;
      tell_user(ctx,"Successfully identified!",NULL);
      return TRUE;
  }
  //fprintf(stderr,"Message from ns %s\n",*message);
  return FALSE;
}

#define UP 0
#define DOWN 1

static void walk_blist(void (*handle)(PurpleBlistNode*)) {
  PurpleBlistNode* cur = purple_blist_get_root();
  if(!cur) return;
  short direction = DOWN;
  for(;;) {
    if(direction==DOWN) {
      handle(cur);
      PurpleBlistNode* child = purple_blist_node_get_first_child(cur);
      if(child) {
	cur = child;
      } else {
	direction = UP;
      }
    } else if(direction==UP) {
      PurpleBlistNode* sib = purple_blist_node_get_sibling_next(cur);
      if(sib) {
	direction = DOWN;
	cur = sib;
      } else {
	cur = purple_blist_node_get_parent(cur);
	if(!cur) return;
      }
    }
  }
}

static void check_blocked_channels(PurpleConnection* connection) {
  void each_node(PurpleBlistNode* node) {
    if(!PURPLE_BLIST_NODE_IS_CHAT(node)) return;
    PurpleChat* chat = PURPLE_CHAT(node);
    PurpleAccount* test = purple_chat_get_account(chat);
    /*fprintf(stderr,"%s == %s ? %s\n",
	    test->username,
	    connection->account->username,
	    test == connection->account ? "TRUE" : "FALSE");*/
    if(test != connection->account) return;

    gboolean autoJoin = purple_blist_node_get_bool(node,"gtk-autojoin");
    if(!autoJoin) return;

    // mostly ripped from pidgin/gtkblist.c gtk_blist_join_chat

    PurplePluginProtocolInfo* prpl = PURPLE_PLUGIN_PROTOCOL_INFO(purple_find_prpl(purple_account_get_protocol_id(connection->account)));
    if(!(prpl && prpl->get_chat_name)) return;
    GHashTable* components = purple_chat_get_components(chat);
    gchar* name = prpl->get_chat_name(components);
    if(!name) return;
    // This creates the conversation if it is not already there.
    PurpleConversation* conv =
      purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
					    name,
					    connection->account);
    g_free(name);
    if(conv) {
      // pidgin_conv_attach_conversation?
      purple_conversation_present(conv);
    }
    serv_join_chat(connection,components);
  }

  walk_blist(each_node);
}

/* we have to do this on a timer since there are NO HOOKS AT ALL ARGH for when IRC reports a conflicting nickname. */

static gboolean check_nick(gpointer udata) {
  PurpleConnection* connection = (PurpleConnection*) udata;
  const char* desiredNick = purple_account_get_string(connection->account,
					       DESIRED_NICK,NULL);

  if(desiredNick==NULL || *desiredNick=='\0') return FALSE;

  if(!strcmp(purple_connection_get_display_name(connection),desiredNick)) {
    // we have teh right nick now yay
    account_context* ctx = find_context(connection->account);
    if(!ctx->identified) {
        const char* password = purple_account_get_string(connection->account,
						   PASSWORD,NULL);
        if(!password) {
            // how did I GET here?
            return FALSE;
        }
        doIdentify(ctx,connection->account,password);
    }
    check_blocked_channels(connection);
    return FALSE;
  }

  g_assert(connection->account);

  const char* password = purple_account_get_string(connection->account,
						   PASSWORD,NULL);
  if(password==NULL) return FALSE;

  g_assert(connection && connection->account);
  account_context* ctx = check_nick_conv(connection->account);
  tell_user(ctx,"Ghosting",desiredNick,"so we can use it. (now at",
	    purple_connection_get_display_name(connection),
	    ")",NULL);
  tell_nickserv(ctx,connection->account,"GHOST",desiredNick,password,NULL);

  // then try to change your nickname.
  char* command = g_malloc(strlen(desiredNick)+6);
  ssize_t dlen = strlen(desiredNick);
  memcpy(command,"nick ",5);
  memcpy(command+5,desiredNick,dlen);
  command[5+dlen] = '\0';
  gchar* error = NULL;
  purple_cmd_do_command(ctx->nick_conv_thingy,command,command,&error);
  g_free(command);
  if(error) {
    fprintf(stderr,"Umm got an error doing the nick %s\n",error);
  }
  return TRUE;
}

static void free_context(account_context* ctx) {
  if(ctx->nick_checker)
    g_source_remove(ctx->nick_checker);
  /* This gets done automatically... seems to lead to a double free
  if(ctx->nick_conv_thingy)
    purple_conversation_destroy(ctx->nick_conv_thingy);
  */
  g_free(ctx);
}

static void signed_off(PurpleConnection *connection) {
  g_hash_table_remove(contexts,connection->account);
}

static void signed_on(PurpleConnection *connection, void* conn_handle) {

  PurpleAccount* account;
  PurpleConvIm* imconv;

  if(!connection) return;
  account = purple_connection_get_account(connection);

  if(!account) return;

  if (!g_str_equal(purple_account_get_protocol_id(account), IRC_PLUGIN_ID)) {
    // not an IRC connection
    return;
  }

  const char* desiredNick = purple_account_get_string(account,DESIRED_NICK,NULL);

  if(!desiredNick || *desiredNick=='\0') return;
  const char* password = purple_account_get_string(account,PASSWORD,NULL);
  if(!password) return;

  purple_signal_connect(purple_conversations_get_handle(), "receiving-im-msg",
                        g_plugin, PURPLE_CALLBACK(check_for_nickserv), NULL);

  account_context* ctx = find_context(connection->account);
  ctx->identified = FALSE;
  ctx->nick_checker = g_timeout_add_seconds(3,(GSourceFunc)check_nick,connection);
}

static PurpleCmdRet doIdentify_cb(PurpleConversation *conv,
        const gchar *cmd,
        gchar **args,
        gchar **error,
        void *data) {
    PurpleAccount* account = purple_conversation_get_account(conv);
    account_context* ctx = find_context(account);
    const char* password = purple_account_get_string(account,PASSWORD,NULL);
    if(password==NULL) {
        tell_user(ctx,"No nickserv password for this network!",NULL);
        return PURPLE_CMD_RET_FAILED;
    }
    tell_user(ctx,"Identifying...",NULL);
    doIdentify(ctx,account,password);
}


struct {
    PurpleCmdId identify;
} id = {};


static gboolean plugin_load(PurplePlugin *plugin) {
  PurplePlugin *irc_prpl;
  PurplePluginProtocolInfo *prpl_info;
  void *conn_handle;

  irc_prpl = purple_plugins_find_with_id(IRC_PLUGIN_ID);

  const char* err = NULL;
  int erroffset = 0;
  ask_for_register = pcre_compile("nickname is registered",0,
				  &err,&erroffset,NULL);
  if(!ask_for_register) {
    fprintf(stderr,"PCRE COMPILE ERROR %s\n",err);
    return FALSE;
  }

  ask_for_register_study = pcre_study(ask_for_register,0,&err);
  if(err) {
    fprintf(stderr,"Eh, study failed. %s\n",err);
  }

  was_identified = pcre_compile("Password accepted|You are now identified",0,
				  &err,&erroffset,NULL);
  if(!was_identified) {
    fprintf(stderr,"PCRE COMPILE ERROR %s\n",err);
    return FALSE;
  }

  was_identified_study = pcre_study(was_identified,0,&err);
  if(err) {
    fprintf(stderr,"Eh, study failed. %s\n",err);
  }

  if (NULL == irc_prpl)
    return FALSE;

  prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(irc_prpl);
  if (NULL == prpl_info)
    return FALSE;

  contexts = g_hash_table_new_full(g_direct_hash,g_direct_equal,
				   NULL,(GDestroyNotify)free_context);

  PurpleAccountOption* option;

  option = purple_account_option_string_new(_("Desired nick"),
					    DESIRED_NICK,"");
  prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);

  option = purple_account_option_string_new(_("Nickserv Nick"),
					    NICKSERV_NAME,"NickServ");
  prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);

  option = purple_account_option_bool_new(_("Use PRIVMSG?"),
					  NICKSERV_USE_PRIVMSG,FALSE);
  prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);

  option = purple_account_option_string_new(_("Nickserv password"),
					    PASSWORD,"");
  purple_account_option_string_set_masked(option,TRUE);
  prpl_info->protocol_options = g_list_append(prpl_info->protocol_options, option);

  g_plugin = plugin;

  conn_handle = purple_connections_get_handle();
  purple_signal_connect(conn_handle, "signed-on",
                        plugin, PURPLE_CALLBACK(signed_on), conn_handle);
  purple_signal_connect(conn_handle, "signed-off",
                        plugin, PURPLE_CALLBACK(signed_off), NULL);

  GList* l;
  for (l = purple_accounts_get_all(); l != NULL; l = l->next) {
    PurpleAccount* account = (PurpleAccount*)l->data;
    if(strcmp(purple_account_get_protocol_id(account),IRC_PLUGIN_ID))
      continue;
    PurpleConnection* gc = purple_account_get_connection(account);
    if(!gc)
      continue;
    signed_on(gc,conn_handle);
  }

  id.identify = purple_cmd_register(
          "identify",
          "",
          PURPLE_CMD_P_DEFAULT,
          PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PRPL_ONLY,
          "prpl-irc",
          doIdentify_cb,
          "identifies you to the current network",
          NULL);

  return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin) {

  PurplePlugin* irc_plugin;
  PurplePluginProtocolInfo* prpl_info;

  purple_cmd_unregister(id.identify);
  id.identify = 0;

  GList* l;
  for (l = purple_accounts_get_all(); l != NULL; l = l->next) {
    PurpleAccount* account = (PurpleAccount*)l->data;
    if(strcmp(purple_account_get_protocol_id(account),IRC_PLUGIN_ID))
      continue;
    PurpleConnection* gc = purple_account_get_connection(account);
    if(!gc)
      continue;
    signed_off(gc);
  }

  if(ask_for_register) {
    pcre_free(ask_for_register);
    ask_for_register = NULL;
  }
  if(ask_for_register_study) {
    pcre_free(ask_for_register_study);
    ask_for_register_study = NULL;
  }

  if(was_identified) {
    pcre_free(was_identified);
    was_identified = NULL;
  }
  if(was_identified_study) {
    pcre_free(was_identified_study);
    was_identified_study = NULL;
  }
  irc_plugin = purple_plugins_find_with_id(IRC_PLUGIN_ID);
  if(irc_plugin) {
    prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(irc_plugin);
    if(prpl_info) {
      for (l = prpl_info->protocol_options; l != NULL; l && (l = l->next)) {
	PurpleAccountOption* option = (PurpleAccountOption*)l->data;
    const char* pref_name = purple_account_option_get_text(option);
	if(!(strcmp(pref_name,DESIRED_NICK) &&
	     strcmp(pref_name,PASSWORD))) {
	  purple_account_option_destroy(option);
	  l->data = NULL;
	  l = g_list_delete_link(l,l);
	}
      }
    }
  }

  if(g_plugin)
    g_plugin = NULL;

  g_hash_table_remove_all(contexts);

  return TRUE;
}

static void plugin_init(PurplePlugin *plugin)
{
  info.dependencies = g_list_append(info.dependencies, IRC_PLUGIN_ID);

#ifdef ENABLE_NLS
  bindtextdomain(GETTEXT_PACKAGE, PP_LOCALEDIR);
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
#endif /* ENABLE_NLS */

  info.name = _("Nickserv");
  info.summary = _("slightly less cheap Nickserv hack");
  info.description = _("Auto-nick negotiation with nickserv.");
}

PURPLE_INIT_PLUGIN(PLUGIN_STATIC_NAME, plugin_init, info)