#define _GNU_SOURCE
#define PURPLE_PLUGINS

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

#include <string.h>

#include "compatability.h"

#include "pat.h"

#define _ gettext

#define PLUGIN_STATIC_NAME "nickserv"
#define PLUGIN_ID "super-unique-id-" PLUGIN_STATIC_NAME

#define PP_VERSION "2.6.0"

#define IRC_PLUGIN_ID "prpl-irc"

#define DESIRED_NICK PLUGIN_ID ".desired"
#define NICKSERV_NAME PLUGIN_ID ".nickserv"
#define NICKSERV_USE_PRIVMSG PLUGIN_ID ".privmsg"
#define PASSWORD PLUGIN_ID ".password"

struct pats {
    struct pat* ask_for_register;
    struct pat* was_identified;
    struct pat* use_recover;
    struct pat* was_killed;
    struct pat* unused;
    struct pat* owned;
} g_pats = {};

static void pats_setup(void) {
    // pats_init(); done need-based statically
  g_pats.ask_for_register = pat_setup("nickname is registered",pat_match);
  g_pats.was_identified = pat_setup("Password accepted|You are now identified",pat_pcre);
  g_pats.use_recover = pat_setup("Instead, use the RECOVER command",pat_plain);
  g_pats.was_killed = pat_setup("has been (?:killed|ghosted)",pat_pcre);
  g_pats.unused = pat_setup("isn't currently in use|is not online.",pat_pcre);
  g_pats.owned = pat_setup("is owned by someone else",pat_plain);
}
static void pats_cleanup(void) {
  pat_cleanup(&g_pats.ask_for_register);
  pat_cleanup(&g_pats.was_identified);
  pat_cleanup(&g_pats.use_recover);
  pat_cleanup(&g_pats.was_killed);
  pat_cleanup(&g_pats.unused);
  pat_cleanup(&g_pats.owned);
  pats_uninit();
}

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
  const char* desiredNick;
  PurpleConvIm* nick_conv;
  PurpleAccount* account;
  PurpleConversation* nick_conv_thingy;
  guint nick_checker;
  gboolean identified;
} account_context;

PurplePlugin* g_plugin = NULL;
FILE* thelog = NULL;

/*******************************************************************************************************/

GHashTable* contexts = NULL;

account_context* find_context(PurpleAccount* account) {
  account_context* context = g_hash_table_lookup(contexts,account);
  if(!context) {
    context = g_new0(account_context,1);
    context->nick_conv_thingy = NULL;
    context->account = account;
    g_hash_table_replace(contexts,account,context);
  }

  return context;
}

static void unset_nick_conv(PurpleConversation* conv, void* udata) {
  PurpleAccount* account = (PurpleAccount*) udata;
  // this context might have been freed if we signed off from this account!
  // conversations are not deleted on account sign-off!
  account_context* context = g_hash_table_lookup(contexts,account);
  if(context) {
    context->nick_conv_thingy = NULL;
    context->nick_conv = NULL;
  }
}

static void check_nick_conv(account_context* context) {
    PurpleAccount* account = context->account;
  if(!context->nick_conv_thingy) {
    const gchar* nickservnick = purple_account_get_string(account,NICKSERV_NAME,"NickServ");
    fprintf(thelog,"Nickserv nick '%s'",nickservnick);
    context->nick_conv_thingy = purple_conversation_new(PURPLE_CONV_TYPE_IM,account,nickservnick);
							
    purple_signal_connect(purple_conversations_get_handle(), "deleting-conversation",
            g_plugin, PURPLE_CALLBACK(unset_nick_conv), account);
    context->nick_conv = purple_conversation_get_im_data(context->nick_conv_thingy);
    return;
  }
  if(!context->nick_conv)
    context->nick_conv = purple_conversation_get_im_data(context->nick_conv_thingy);
  return;
}
// why can't I go va_append("nickserv",args)? x_x

inline void dolog(const char who[], const char* message) {
  fprintf(thelog,"%d ",time(NULL));
  fwrite(who,sizeof(who),1,thelog);
  fputc(' ',thelog);
  fwrite(message,strlen(message)-1,1,thelog);
  fputc('\n',thelog);
  fflush(thelog);
}

static void tell_nickserv(account_context* ctx, PurpleAccount *account, const char** args) {

  gchar* line = g_strjoinv(" ",(gchar**)args);
  dolog("nickserv ",line);
  if(purple_account_get_bool(account,NICKSERV_USE_PRIVMSG,FALSE)==FALSE) {
    gchar* var1434 = line;
    line = g_strconcat("nickserv ",var1434,NULL);
    g_free(var1434);

    gchar* error = NULL;
    fprintf(thelog,"Trying command %s\n",line);
    check_nick_conv(ctx);
    if(PURPLE_CMD_STATUS_OK!=purple_cmd_do_command(ctx->nick_conv_thingy,
					    line,line,&error)) {
      fprintf(stderr,"Nickserv command failed %s",line);
    }
  } else {
    const char* message = line;
    fprintf(thelog,"Trying privmsg %s %d\n",message,strlen(message));
    check_nick_conv(ctx);
    purple_conv_im_send_with_flags(ctx->nick_conv,
				   message,
				   PURPLE_MESSAGE_RAW |
				   PURPLE_MESSAGE_NO_LOG |
				   PURPLE_MESSAGE_INVISIBLE |
				   PURPLE_MESSAGE_AUTO_RESP);
  }
  g_free(line);
}

static void doIdentify(account_context* ctx, PurpleAccount* account, const char* password) {
    const char* var1434[] = {
        "IDENTIFY",
        password,
        NULL};
    tell_nickserv(ctx,account,var1434);
}


static void tell_user(account_context* ctx, const char** args) {
  gchar* message = g_strjoinv(" ",(gchar**)args);
  dolog("user ",message);
  check_nick_conv(ctx);
  purple_conversation_write(ctx->nick_conv_thingy,
			    "nickserv identifier",
			    message,
			    PURPLE_MESSAGE_SYSTEM,
			    time(NULL));
  g_free(message);
}

static void doGhost(account_context* ctx, PurpleAccount* account, const char* password, gboolean recover) {
  const char* var1435[] = {
      recover ? "RECOVER" : "GHOST",
      ctx->desiredNick,
      password,
      NULL};
  tell_nickserv(ctx,account,var1435);
}

static void setNick(account_context* ctx) {
  // then try to change your nickname.
  gchar* command = g_strconcat("nick ",ctx->desiredNick,NULL);
  gchar* error = NULL;
  check_nick_conv(ctx);
  purple_cmd_do_command(ctx->nick_conv_thingy,command,command,&error);
  g_free(command);
  if(error) {
    fprintf(stderr,"Umm got an error doing the nick %s\n",error);
  }
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

  account_context* ctx = find_context(account);
  if(conv != ctx->nick_conv_thingy) return FALSE;
  // if check_nick_conv creates a conversation, it will NEVER be existing conversation conv

  if(pat_check(g_pats.ask_for_register,*message) || pat_check(g_pats.owned,*message)) {
    doIdentify(ctx,account,password);
    const char* var1434[] = {
        "Identifying to nickserv as",
	    purple_connection_get_display_name(purple_conversation_get_connection(ctx->nick_conv_thingy)),
        NULL};
    tell_user(ctx,var1434);
    return TRUE;
  }
  if(pat_check(g_pats.was_identified,*message)) {
      ctx->identified = TRUE;
      const char* var1434[] = {
          "Successfully identified!",
          NULL};
      tell_user(ctx,var1434);
      return TRUE;
  }

  if(pat_check(g_pats.use_recover,*message)) {
      const char* csux[] = {
          "Oops, we need to RECOVER instead.",
          NULL
      };
      tell_user(ctx,csux);
      doGhost(ctx,account,password,TRUE);
      return TRUE;
  }

  if(pat_check(g_pats.was_killed,*message) || pat_check(g_pats.unused,*message)) {
      setNick(ctx);
  }

  // TODO: ghost response
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
    // always assign, in case settings changed.
    ctx->desiredNick = desiredNick;
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
  account_context* ctx = find_context(connection->account);
  // always assign, in case settings changed.
  ctx->desiredNick = desiredNick;
  const char* var1434[] = {
      "Ghosting",
      desiredNick,
      "so we can use it. (now at",
      purple_connection_get_display_name(connection),
      ")",
      NULL};
  tell_user(ctx,var1434);
  doGhost(ctx,connection->account,password,FALSE);
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

  // make sure we should do anything at all.
  const char* desiredNick = purple_account_get_string(account,DESIRED_NICK,NULL);
  if(!desiredNick || *desiredNick=='\0') return;

  const char* password = purple_account_get_string(account,PASSWORD,NULL);
  if(!password) return;

  purple_signal_connect(purple_conversations_get_handle(), "receiving-im-msg",
                        g_plugin, PURPLE_CALLBACK(check_for_nickserv), NULL);

  account_context* ctx = find_context(connection->account);
  // always assign, in case settings changed.
  ctx->desiredNick = desiredNick;

  if(TRUE==check_nick(connection)) {
    ctx->identified = FALSE;
    ctx->nick_checker = g_timeout_add_seconds(10,(GSourceFunc)check_nick,connection);
  } else {
      ctx->nick_checker = 0;
  }
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
        const char* var1434[] = {
            "No nickserv password for this network!",
            NULL};
        tell_user(ctx,var1434);
        return PURPLE_CMD_RET_FAILED;
    }
    const char* var1434[] = {
        "Identifying...",
        NULL};
    tell_user(ctx,var1434);
    doIdentify(ctx,account,password);
    return PURPLE_CMD_RET_OK;
}


struct {
    PurpleCmdId identify;
} id = {};


static gboolean plugin_load(PurplePlugin *plugin) {
  PurplePlugin *irc_prpl;
  PurplePluginProtocolInfo *prpl_info;
  void *conn_handle;

  gchar* dest = g_build_filename(purple_user_dir(),"nickserv.log",NULL);
  thelog = fopen(dest,"at");
  irc_prpl = purple_plugins_find_with_id(IRC_PLUGIN_ID);

  pats_setup();

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

  pats_cleanup();

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

  //g_hash_table_remove_all(contexts);
  g_hash_table_unref(contexts); contexts = NULL;

  fclose(thelog);
  thelog = NULL;

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
