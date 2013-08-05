/*

Directory Opus 5
Original APL release version 5.82
Copyright 1993-2012 Jonathan Potter & GP Software

This program is free software; you can redistribute it and/or
modify it under the terms of the AROS Public License version 1.1.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
AROS Public License for more details.

The release of Directory Opus 5 under the GPL in NO WAY affects
the existing commercial status of Directory Opus for Windows.

For more information on Directory Opus for Windows please see:

                 http://www.gpsoft.com.au

*/

/**
 **	This file should contain any high level functions which
 **	connect and login a lister to an FTP site.
 **/

/*
 *	20-05-96	This file seperated from lister.c
 *	02-06-96	Now always does a PWD after logging in in case of a relative path

 *
 *	3.3.99		Fixed problem with ftp:// not scanning dir
 *			caused by redo_cache setting. Changre to TRUE
 *
 */

#include "ftp.h"
#include "ftp_ad_sockproto.h"
#include "ftp_ad_errno.h"
#include "ftp_arexx.h"
#include "ftp_ipc.h"
#include "ftp_lister.h"
#include "ftp_opusftp.h"
#include "ftp_util.h"
#include "ftp_module.h"
#include "ftp_addrsupp_protos.h"


#ifndef DEBUG
#define kprintf ;   /##/
#endif


#define SocketBase GETSOCKBASE(FindTask(0L))
#define errno GETGLOBAL(FindTask(NULL),g_errno)

int lister_retry_connect_requester( struct ftp_node *, struct connect_log_data * );

/********************************/

//
//	Check the startup message for system type in case the SYST command doesn't work.
//	There may be useful info in here that SYST doesn't tell us in any case.
//
static void lister_check_startup_msg( struct ftp_node *node )
{
strupr( node->fn_ftp.fi_iobuf );

if	(strstr( node->fn_ftp.fi_iobuf, "DG/UX" ))	// Data General Unix (no SYST)
	node->fn_systype = FTP_DGUX;			// (must come before unix check)
else if	(strstr( node->fn_ftp.fi_iobuf, "ULTRIX" ))
	node->fn_systype = FTP_ULTRIX;
else if	(strstr( node->fn_ftp.fi_iobuf, "UNIX" ))	// (Amiga Unix has no SYST)
	node->fn_systype = FTP_UNIX;			// (must come after specific unixes)

if	(strstr( node->fn_ftp.fi_iobuf, "WU-FTPD" ))	// Washington University FTP server
	node->fn_ftp.fi_flags |= FTP_IS_WUFTPD;		// Could use it's advanced features
}

/********************************/

//
//	Attempt to connect this node to the specified FTP host
//	the server's startup message is in the node's reply buffer.
//
static int lister_connect(struct ftp_node *node,int (*startupfn)(void *,int,char *),struct message_update_info *mu,char *host,int port)
{
int retval;

lst_addabort( node, SIGBREAKF_CTRL_C, 0 );

//kprintf("lister_connect %s %ld\n",host,port);

retval = connect_host( &node->fn_ftp, startupfn, mu, host, port );

lst_remabort( node );

if	(retval == 1)
	{
	node->fn_flags |= LST_CONNECTED;

	// Look for useful info in startup message
	lister_check_startup_msg( node );
	}


//kprintf("lister_connect returns %ld\n",retval);

return retval;
}

/********************************/

//
//	Attempt to log in to the FTP host we just connected to.
//
static int lister_login(
	struct ftp_node             *node,
	int                        (*startupfn)(void *,int,char *),
	struct message_update_info  *mu,
	char                        *user,
	char                        *pass )
{
char                     info[256+1];
int                      retval;

strcpy( info, GetString(locale,MSG_FTP_LOGGING_IN) );
strcat( info, " " );
strcat( info, user );
strcat( info, "..." );

lister_prog_info( node, info );

lst_addabort( node, SIGBREAKF_CTRL_C, 0 );

retval = login( &node->fn_ftp, startupfn, mu, user, pass );

lst_remabort( node );

if	(retval == 0)
	node->fn_flags |= LST_LOGGEDIN;
else
	{
	// Send QUIT even if we fail
	// TODO don't send commands if we're not connected
	// If connect() failed or we got a 421 etc already
	if	(node->fn_flags & LST_CONNECTED)
		logout( &node->fn_ftp );
	else
		kprintf( "** lst login not sending quit - not connected\n" );
	}

return retval;
}

/********************************/

static int systype_lookup( char *text )
{
struct ftpsystem
	{
	char *name;
	int type;
	};

static struct ftpsystem sys_types[] =
	{
	"UNIX",			FTP_UNIX,	// Check Unix before OS/2 - "Unix type OS/2"
	"AMIGA",		FTP_AMIGA,
	"WINDOWS_NT",		FTP_WINDOWSNT,
	"WIN32",		FTP_WIN32,	// NT or Win95/98
	"OS/2",			FTP_OS2,
	"MACOS",		FTP_MACOS,
	"VMS",			FTP_VAX_VMS,	// Check VMS before VM
	"VM",			FTP_VM_CMS,
	NULL,			FTP_UNKNOWN,
	};
struct ftpsystem *p = sys_types;
int               type = FTP_UNKNOWN;

while	(p->type != FTP_UNKNOWN)
	{
	if	(strstr( text, p->name ))
		{
		type = p->type;
		break;
		}
	p++;
	}

return type;
}

/********************************/

/*
 *	Attempt to determine what type of system the FTP host is.
 *	  There may already be a value in sys_type from what was seen in the startup message.
 *	  This function also initializes some stuff depending on the system type.
 */

static int lister_getsystype( struct ftp_node *node )
{
int reply, type;

// Can TIMEOUT
reply = ftp_syst( &node->fn_ftp );

if	(reply < 500 && reply != 421)
	{
	strupr( node->fn_ftp.fi_iobuf ); 

	if	(type = systype_lookup( node->fn_ftp.fi_iobuf ))
		node->fn_systype=type;
	else
		kprintf( "** Unknown system type: %s", node->fn_ftp.fi_iobuf + 4 );
	}

// Default ls conversion function different for NT
node->fn_ls_to_entryinfo = unix_line_to_entryinfo;

if	(node->fn_systype == FTP_DGUX)
	{
	strcpy( node->fn_lscmd, "NLST -alF" );
	node->fn_ftp.fi_flags |= FTP_IS_UNIX;
	}
else if	(node->fn_systype == FTP_ULTRIX)
	node->fn_ftp.fi_flags |= FTP_IS_UNIX;
else if	(node->fn_systype == FTP_UNIX)
	node->fn_ftp.fi_flags |= FTP_IS_UNIX;
else if	(node->fn_systype == FTP_OS2)
	strcpy( node->fn_lscmd, "LIST" );			/* It'll take more than this :) */
else if (node->fn_systype == FTP_WINDOWSNT)
	{
	strcpy( node->fn_lscmd, "LIST" );
	node->fn_ls_to_entryinfo = nt_line_to_entryinfo;
	}

#if 0
#ifdef DEBUG
	{
	char *sysname[] =
		{
		"FTP_UNKNOWN",		"FTP_UNIX",
		"FTP_AMIGA",		"FTP_WINDOWSNT",
		"FTP_WIN32",		"FTP_OS2",
		"FTP_MACOS",		"FTP_DGUX",
		"FTP_ULTRIX",		"FTP_VAX_VMS",
		"FTP_VM_CMS"
		};
	kprintf( "system type: '%s'%s\n", sysname[node->fn_systype], node->fn_ftp.fi_flags & FTP_IS_WUFTPD ? " (wu-ftpd)" : "" );
	}
#endif
#endif
return reply;
}

/********************************/

//
//	Attempt to CWD to the initial path specified.
//	If this fails, strip off the trailing part of the path and try again.
//
static int connect_cwd(struct ftp_node *ftpnode,int (*cwdfn)(void *,int,char *),struct message_update_info *mu,char *path )
{
int reply=0;
int pwd_done = 0;

while	(TRUE)
	{
	reply = ftp_cwd( &ftpnode->fn_ftp, cwdfn, mu, path );

	if	(reply / 100 == COMPLETE)
		{
		// Try synching without PWD
		if	(strlen( ftpnode->fn_ftp.fi_iobuf ) >= 5
			&& ftpnode->fn_ftp.fi_iobuf[4] == '"'
			&& strchr( ftpnode->fn_ftp.fi_iobuf + 5, '"' ))
			{
			stptok( ftpnode->fn_ftp.fi_iobuf + 5, ftpnode->fn_site.se_path, PATHLEN, "\"" );
			pwd_done = 1;
			}

		// Still need to work out new path?
		if	(!pwd_done)
			{
			if	(ftpnode->fn_site.se_env->e_safe_links)
				{
				// Can TIMEOUT
				if	(lister_synch_path( &ftpnode->fn_ftp, ftpnode->fn_site.se_path ))
					pwd_done = 1;
				}

			// Still need to work out new path?
			if	(!pwd_done)
				strcpy( ftpnode->fn_site.se_path, path );
			}
		break;
		}

	else if	(!parent_buffer( &ftpnode->fn_ftp, path, FALSE ))
		break;
	}

return reply;
}

/********************************/

struct connect_log_data
{
struct ftp_node    *cld_node;
ULONG               cld_handle;
struct connect_msg *cld_cm;
char               *cld_errmsg;
int                 cld_okay;
int                 cld_aborted;
ULONG               cld_flags;		// The flags field of the connect IPCMessage
};

//
//	Attempt to connect and login.
//
//	Returns 1 if succeeded
//	Returns 0 if failed
//
static int lister_connect_and_login( struct opusftp_globals *og, struct connect_log_data *cld )
{
int reply;
int connected = 0;
int logged_in = 0;
int loginerr;
int maxtries = 1;
int tries;
int attempt = 1;
int secs;
int prompt_userpass = 1;	// Need to prompt for new user/password
int need_countdown;		// Don't count down if user/password prompted for
int choice;
int need_query_path=0;

int (*startupfn)(void *,int,char *) = 0;
int (*cwdfn)(void *,int,char *) = 0;
char info[256] = "";
struct ftp_node *node = cld->cld_node;
struct message_update_info  *mu;
TimerHandle *th;
ULONG sigs;

kprintf( "lister_connect_and_login()\n" );

// Get number of retries from config
if	(node->fn_site.se_env->e_retry)
	maxtries = node->fn_site.se_env->e_retry_count + 1;

// It would be a bit silly to make no attempt to connect (:
if	(maxtries < 1)
	maxtries = 1;

tries = maxtries;

// Lister busy
rexx_lst_lock( cld->cld_cm->cm_opus, cld->cld_handle );

// Progress bar on
lister_prog_init(
	node,
	GetString( locale, cld->cld_flags & CONN_OPT_RECON
		? MSG_FTP_RECONNECTING
		: MSG_FTP_CONNECTING ),
	"",
	cld->cld_cm->cm_site.se_host,
	PROGRESS_FILE_OFF,
	PROGRESS_BAR_OFF );

// Allocate message update stuff
if	(mu = AllocVec( sizeof(*mu), MEMF_CLEAR ))
	{
	startupfn = message_update;
	mu->mu_node = node;

	if	(node->fn_site.se_env->e_show_dir)
		cwdfn = message_update;

	if	(node->fn_site.se_env->e_show_startup
		|| node->fn_site.se_env->e_show_dir)
		mu->mu_msg = Att_NewList( LISTF_POOL );
	}

// Loop until connected and logged in...
while	(tries--)
	{
	need_query_path = 0;
	need_countdown = 1;

	// Clear startup message from previous attempt
	if	(mu && mu->mu_msg)
		Att_RemList( mu->mu_msg, REMLIST_SAVELIST );

	// Try to connect lister to site
	connected = lister_connect(node,startupfn,mu,cld->cld_cm->cm_site.se_host,cld->cld_cm->cm_site.se_port );

	// Connect successful?
	if	(connected > 0)
		{
		// Log in with user name and password
		loginerr = lister_login(node,startupfn,mu,cld->cld_cm->cm_site.se_user,cld->cld_cm->cm_site.se_pass );

	//kprintf("lister_login of %s %s ->%ld\n",cld->cld_cm->cm_site.se_user,cld->cld_cm->cm_site.se_pass,loginerr);

		// Login successful?
		if	(loginerr == 0)
			{
			logged_in = 1;
			tries = 0;
			}

		// Login fatal error - don't retry
		// -1 : USER failed
		// -2 : PASS failed
		else
			{
			// User aborted?
			if	(errno == EINTR)
				{
				//kprintf( "** login abort (EINTR)\n" );
				cld->cld_aborted = 1;
				cld->cld_errmsg = 0;
				tries = 0;
				}
			// Set error message
			else if	(errno)
				cld->cld_errmsg = sockerr();
			else if	(*node->fn_ftp.fi_serverr)
				{
				// Remove \r\n from end of line
				if	(cld->cld_errmsg = strchr( node->fn_ftp.fi_serverr + 4, '\r' ))
					*cld->cld_errmsg = 0;

				// Include error number or message only?
				cld->cld_errmsg = node->fn_ftp.fi_serverr + (og->og_oc.oc_log_debug ? 0 : 4);
				}
			else if	(loginerr == -1)
				cld->cld_errmsg = GetString(locale,MSG_FTP_USERNAME_FAILED);
			else
				cld->cld_errmsg = GetString(locale,MSG_FTP_PASSWORD_FAILED);


			// Fatal or not?
//			if	(node->fn_ftp.fi_reply / 100 != TRANSIENT)
//				tries = 0;
//			else

			if	(prompt_userpass && tries)
				{
				choice = lister_retry_connect_requester( node, cld );

				need_countdown = 0;

				// Aborted?
				if	(cld->cld_aborted)
					tries = 0;

				// Keep trying?
				else if	(choice == 3)
					prompt_userpass = 0;
				}
			}
		}

	// Connect fatal error - don't retry
	else if	(connected < 0)
		{
		tries = 0;

		// User aborted?
		if	(errno == EINTR)
			{
			kprintf( "** connect abort (EINTR)\n" );
			cld->cld_aborted = 1;
			cld->cld_errmsg = 0;
			}

		// Set error message
		else if	(errno)
			cld->cld_errmsg = sockerr();
		else if	(connected == -2)
			cld->cld_errmsg = (char *)GetString(locale,MSG_SITE_NOT_FOUND);
		else
			cld->cld_errmsg = (char *)GetString(locale,MSG_FTP_COULD_NOT_CONNECT);
		}
	else if	(prompt_userpass)
		{
		// Set error message
		if	(errno)
			cld->cld_errmsg = sockerr();

		choice = lister_retry_connect_requester( node, cld );

		need_countdown = 0;

		// Aborted?
		if	(cld->cld_aborted)
			tries = 0;

		// Keep trying?
		else if	(choice == 3)
			prompt_userpass = 0;
		}

	// Delay a while, then retry
	if	(need_countdown && tries)
		{
		++attempt;

		lst_addabort( node, SIGBREAKF_CTRL_C, 0 );

		// Get a timer
		if	(th = AllocTimer( UNIT_VBLANK, NULL ))
			{
			for	(secs = node->fn_site.se_env->e_retry_delay; secs; --secs)
				{
				sprintf( info, GetString(locale,MSG_CONNECT_RETRY), attempt, maxtries, secs );

				lister_prog_info( node, info );

				StartTimer( th, 1, 0 );

				sigs = Wait( (1 << th->port->mp_SigBit) | SIGBREAKF_CTRL_C );

				// Abort pressed
				if	(sigs & SIGBREAKF_CTRL_C)
					{
					//kprintf( "** connect delay abort (CTRL_C)\n" );
					SetSignal( 0L, SIGBREAKF_CTRL_C );
					tries = 0;
					break;
					}
				}
			FreeTimer( th );
			}
		else
			{
			tries = 0;
			}

		lst_remabort( node );
		}

	// Non-fatal error, but maximum retries reached
	else if	(!cld->cld_errmsg)
		{
		// Set error message
		if	(errno)
			cld->cld_errmsg = sockerr();
		else
			cld->cld_errmsg = (char *)GetString(locale,MSG_FTP_COULD_NOT_CONNECT);
		}
	}

// Connected and logged in ok?  Do other initializations
if	(logged_in)
	{
	// Look for useful info via SYST command
	lister_prog_info( node, GetString(locale,MSG_QUERY_SYSTYPE) );

	reply = lister_getsystype( node );

	// For Windows NT servers, use DOS style list output
	if	(node->fn_systype == FTP_WINDOWSNT)
		{
		lister_prog_info( node, GetString(locale,MSG_ENABLE_NT) );

		if	(reply = ftp( &node->fn_ftp, "SITE DIRSTYLE\r\n" ) != 421)
			{
			if	(strstr( node->fn_ftp.fi_iobuf,"is off" ))
				reply = ftp( &node->fn_ftp, "SITE DIRSTYLE\r\n" );
			}
		}

	// Always use binary transfers
	lister_prog_info( node, GetString(locale,MSG_BINARY_MODE) );

	if	(reply != 421 && ftp_image( &node->fn_ftp ) != 421)
		{
		cld->cld_okay = TRUE;

		// Change to initial directory if specified
		if	(*node->fn_site.se_path)
			{
			lister_prog_info( node, GetString(locale,MSG_INITIAL_DIR) );

			// Can TIMEOUT
			reply = connect_cwd( node, cwdfn, mu, node->fn_site.se_path );

			kprintf("connect_cwd -> %ld\n",reply);

			if	(reply / 100 != COMPLETE)
				need_query_path = 1;
			}
		else
			need_query_path = 1;

		// Otherwise find out what the initial directory is
		if	(need_query_path)
			{
			lister_prog_info( node, GetString(locale,MSG_QUERY_PATH) );

			// Find out the initial path - Can TIMEOUT
			lister_synch_path( &node->fn_ftp, node->fn_site.se_path );
			}

		rexx_lst_set_path( node->fn_opus, node->fn_handle, node->fn_site.se_path );

		lister_prog_clear( node );
		rexx_lst_unlock( cld->cld_cm->cm_opus, cld->cld_handle );

		// Add all the FTP stuff to the lister now

		/*************************************************
		*	WAS LST_CLEAR but should be LST_EMPTY
		*
		*	rexx_lst_clear( cld->cld_cm->cm_opus, cld->cld_handle );
		*	send_rexxa( cld->cld_cm->cm_opus, REXX_REPLY_NONE, "lister set %lu title FTP:%s", cld->cld_handle, cld->cld_cm->cm_site.se_host );
		*
		*
		* NOTE: these are done by lister empty command
		*
		*	send_rexxa( cld->cld_cm->cm_opus, REXX_REPLY_NONE, "lister set %lu namelength 256", cld->cld_handle );
		*	send_rexxa( cld->cld_cm->cm_opus, REXX_REPLY_NONE, "lister set %lu case on", cld->cld_handle );
		*
		****************************************************/

		rexx_lst_empty( cld->cld_cm->cm_opus, cld->cld_handle );

		send_rexxa( cld->cld_cm->cm_opus, REXX_REPLY_NONE, "lister set %lu title FTP:%s", cld->cld_handle, cld->cld_cm->cm_site.se_host );

		send_rexxa( cld->cld_cm->cm_opus, REXX_REPLY_NONE, "lister refresh %lu full", cld->cld_handle );
		rexx_lst_label( cld->cld_cm->cm_opus, cld->cld_handle, "FTP:", cld->cld_cm->cm_site.se_host, NULL );

		// Scan initial directory - Can TIMEOUT
		if	(reply != 421 && !(cld->cld_flags & CONN_OPT_NOSCAN))
//			lister_list( og, node, FALSE );
			lister_list( og, node, TRUE );

		if	(reply == 421)
			{
			cld->cld_okay = FALSE;
			cld->cld_errmsg = GetString(locale,MSG_FTP_SERVER_CLOSED_CONN);
			}
		}
	}

if	(mu)
	{
	if	(mu->mu_msg)
		{
		if	(node->fn_site.se_env->e_show_startup || node->fn_site.se_env->e_show_dir)
			lister_long_message( node, mu->mu_msg, !node->fn_site.se_env->e_show_startup );

		Att_RemList( mu->mu_msg, 0 );
		}

	FreeVec( mu );
	}

// Connect or login failed (even after retrying)
if	(!cld->cld_okay)
	{
	// Disconnect
	if	(connected > 0)
		disconnect_host( &node->fn_ftp );

	lister_prog_clear( node );

	// Connect/login error requester
	if	(!cld->cld_aborted && !og->og_noreq)
		{
		char           buffer[1024+1];

		// Reason to attach?  Use brackets when appropriate
		if	(cld->cld_errmsg)
			sprintf(
				buffer,
				cld->cld_errmsg[0] == '('
					? "%s"
					: "(%s)",
				cld->cld_errmsg );

		lister_request(
			node,
			FR_FormatString,"%s %s\n%s",
			FR_MsgNum,	MSG_FTP_CANT_LOG_IN_TO,
			AR_Message,	cld->cld_cm->cm_site.se_host,
			AR_Message,	buffer,
			TAG_DONE );
		}

	// Remove handler
	send_rexxa( cld->cld_cm->cm_opus, REXX_REPLY_NONE, "lister set %lu handler ''", cld->cld_handle );
	}


kprintf( "lister_connect_and_login returns %ld\n",cld->cld_okay );

return cld->cld_okay;
}

/********************************/

//
//	Request the args for a new connection from the user if not specified in connect message
//	If there is no host prompt for host and user name
//	If there is a host but no user name, log in as anonymous
//	If there is a host and user name but no password, prompt for a password
//
static int lister_get_args(
	struct opusftp_globals *ogp,
	struct msg_loop_data   *mld,
	IPCMessage             *imsg )
{
struct connect_msg *cm = imsg->data_free;
char                path[PATHLEN+1] = "";
int                 gui = FALSE;
int                 active_gadget = GAD_CONNECT_NAME;	// Gadget in requester to acitvate
int                 okay = TRUE;
int                 noname = 0;

if	(imsg->flags & CONN_OPT_GUI)
	gui = TRUE;

// Look up site if we're supposed to
if	(imsg->flags & CONN_OPT_SITE)
	{
	if	(imsg->flags & CONN_OPT_PATH)
		stccpy( path, cm->cm_site.se_path, PATHLEN + 1 );

	get_site_entry( ogp, &cm->cm_site, mld->mld_ipc );

	if	(imsg->flags & CONN_OPT_PATH)
		stccpy( cm->cm_site.se_path, path, PATHLEN + 1 );
	}

// If we have no host, get host and user
if	(!*cm->cm_site.se_host)
	{
	active_gadget = GAD_CONNECT_HOST;
	gui = TRUE;
	}

// If we have host and user but not password, get password
if	(okay && *cm->cm_site.se_user && !*cm->cm_site.se_pass)
	{
	active_gadget = GAD_CONNECT_PASSWORD;
	gui = TRUE;
	}

// Need to use GUI?
if	(gui && ogp->og_addrproc)
	{

	/*************************************
	*	changed to keep it clear instead of last site all the time
	*	now has new gadget to get last site.
	*
	*	// Put last site in GUI if we're not reconnecting
	*	if	(!(imsg->flags & CONN_OPT_RECON) && ogp->og_valid_site)
	*		copy_site_entry( ogp, &cm->cm_site, &ogp->og_last_site );
	*
	*************************************/

	cm->cm_site.se_active_gadget = active_gadget;

	if	(!*cm->cm_site.se_name)
		noname = 1;

	okay = IPC_Command( ogp->og_addrproc, IPC_CONNECTVISUAL, 0, cm, 0, REPLY_NO_PORT );

	if	(noname)
		*cm->cm_site.se_name = 0;
	}

// If we have host but not user, use anonymous
if	(okay && (cm->cm_site.se_anon || !*cm->cm_site.se_user))
	{
	cm->cm_site.se_anon = TRUE;
	strcpy( cm->cm_site.se_user, "anonymous" );
	}

// If we have host and user is anonymous, use user's address as password
if	(okay && (cm->cm_site.se_anon || !strcmp( cm->cm_site.se_user, "anonymous" ) || !strcmp( cm->cm_site.se_user, "ftp" )))
	{
	// Has user defined an anonymous password?
	if	(*ogp->og_oc.oc_anonpass)
		strcpy( cm->cm_site.se_pass, ogp->og_oc.oc_anonpass );
	else
		getuseraddress( cm->cm_site.se_pass );
	}

return okay;
}

/********************************/

//
//	Attempt to allocate and initialize a new lister node and add it to the global list.
//
static struct ftp_node *lister_create_node( struct opusftp_globals *ogp, IPCData *ipc, struct connect_msg *cm )
{
struct ftp_node *node;

if	(node = AllocVec( sizeof(struct ftp_node), MEMF_CLEAR ))
	{
	node->fn_ipc = ipc;
	node->fn_ftp.fi_og = node->fn_og = ogp;	// MUST set this to access hook fns
	node->fn_ftp.fi_task = FindTask(0);	// To check against cross-task usage

	stccpy( node->fn_opus, cm->cm_opus, PORTNAMELEN + 1 );

	copy_site_entry(ogp,&node->fn_site,&cm->cm_site);

	//	ftp.ncr.com needs -l when using options or defaults to names only!
	//	for OS/2 no options should be used
	//	for DG/UX no options should be used but the SYST command does not work!

	// Default list command
	strcpy( node->fn_lscmd, LSCMD );
	ListLockAddHead( &ogp->og_listerlist, (struct Node *)node, &ogp->og_listercount );
	}

return node;
}

/********************************/

//
//	Attempt to create a new lister, connect to a site and log in
//
struct ftp_node *lister_new_connection( struct opusftp_globals *ogp, struct msg_loop_data *mld, IPCMessage *imsg )
{
struct connect_msg *cm = NULL;		// Connection message - this must be FreeVec'd
struct ftp_node *node = NULL;		// The new node
ULONG handle = 0;			// Lister handle
struct connect_log_data  cld = {0};

if	(imsg && (cm = imsg->data_free))
	{
	if	(cm->cm_opus && *cm->cm_opus && lister_get_args( ogp, mld, imsg ))
		{
		if	(node = lister_create_node( ogp, mld->mld_ipc, cm ))
			{
			// Some options need to be known at a lower level
			node->fn_ftp.fi_timeout = node->fn_site.se_env->e_timeout;

			mld->mld_node = node;

			if	(handle = rexx_lst_new(cm->cm_opus,cm->cm_handle,
						cm->cm_site.se_host,
						cm->cm_site.se_env->e_toolbar ))
				{
				//kprintf("lister_new_connection %lx\n",handle);

				node->fn_handle = handle;
				node->fn_flags |= LST_OPENED;

				// FTP shouldn't close listers that were already
				// open, but there's nasty side-effects when
				// there is a similar directory structure
				// on the FTP site and the computer itself
				// (FTP to localhost for example)

				if	(cm->cm_handle)
					node->fn_flags |= LST_LEAVEOPENIFFAIL;

				// Now we can fill in the handle for the sender to use
				cm->cm_handle = node->fn_handle;

				// Was lister opened invisible?
				if	(!rexx_lst_query_visible( cm->cm_opus, handle ))
					node->fn_flags |= LST_INVISIBLE;

				// Keep current format or use one passed by Greg?
				if	((((Lister *)node->fn_handle)->more_flags & LISTERF_LOCK_FORMAT)
					|| !node->fn_site.se_env->e_custom_format)
					{
					// Use current list format
					ftplister_read_listformat( node );
					}
				else
					{
					// Make lister use our list format
					ftplister_write_listformat( node, 1 );
					}

				cld.cld_cm     = cm;
				cld.cld_handle = handle;
				cld.cld_node   = node;
				cld.cld_errmsg = NULL;
				cld.cld_okay   = FALSE;
				cld.cld_flags  = imsg->flags;

				if	(!lister_connect_and_login( ogp, &cld ))
					{
					node->fn_flags |= LST_CONNECTFAILED;
					lister_disconnect( ogp, mld );
					mld->mld_node = 0;
					node = 0;
					}
				}

			if	(!cld.cld_okay && node)
				{
				lister_remove_node( ogp, node );
				node = NULL;
				}
			}
		}
	}

// Trigger connect script
if	(!cld.cld_aborted && ogp->og_hooks.dc_Script && handle)
	{
	char script_arg[13];

	sprintf( script_arg, "%lu", handle );

	if	(cm->cm_site.se_env->e_script_connect_ok && cld.cld_okay)
		ogp->og_hooks.dc_Script( "FTP connect success", script_arg );
	else if	(cm->cm_site.se_env->e_script_connect_fail && !cld.cld_okay)
		ogp->og_hooks.dc_Script( "FTP connect fail", script_arg );
	}

// Reply to message from main process?
if	(imsg)
	{
	imsg->command = cld.cld_okay;
	IPC_Reply( imsg );
	}

return node;
}

/********************************/

static int errno_known( void )
{
return (errno == EINTR
	|| errno == EACCES
	|| errno == EPROTONOSUPPORT
	|| errno == EADDRNOTAVAIL
	|| errno == ENETDOWN
	|| errno == ENETUNREACH
	|| errno == ECONNABORTED
	|| errno == ECONNRESET
	|| errno == ETIMEDOUT
	|| errno == ECONNREFUSED
	|| errno == EHOSTDOWN
	|| errno == EHOSTUNREACH)
	? TRUE : FALSE;
}

/*******************************{*/

//
//	Call here if the connection has been lost, usually a 421 response.
//	Displays a requester, shuts down and closes the control connection
//	Removes and frees the ftp node, and closes the lister if 'handle' is not NULL
//
//	Returns 1 if reconnect is desired
//	Returns 0 if not
//
static int opus_lostconn( struct opusftp_globals *og, struct ftp_node *ftpnode )
{
ULONG handle = ftpnode->fn_handle;
int   msg;
int   retval = 0;

#ifdef DEBUG
kprintf( "opus_lostconn() - %ld  ",ftpnode->fn_ftp.fi_errno );

	ftpnode->fn_ftp.fi_errno &= ~FTPERR_XFER_MASK;

	switch	(ftpnode->fn_ftp.fi_errno)
		{
		case FTPERR_TIMEOUT:
			kprintf(" FTPERR_TIMEOUT\n");
			break;

		case FTPERR_FAKE_421:
			kprintf(" FTPERR_FAKE_421\n");
			break;

		default:
			kprintf(" -unk\n");
			break;

		}
#endif


if	(handle)
	rexx_lst_lock( ftpnode->fn_opus, handle );

// Auto reconnect?
if	(ftpnode->fn_site.se_env->e_retry_lost)
	retval = 1;

// Show reconnect requester?
else if	(!og->og_noreq)
	{
	ftpnode->fn_ftp.fi_errno &= ~FTPERR_XFER_MASK;

	if	(ftpnode->fn_ftp.fi_errno == FTPERR_TIMEOUT)
		msg = MSG_FTP_LOST_TIMEOUT;
	else if	(ftpnode->fn_ftp.fi_errno == FTPERR_FAKE_421)
		msg = MSG_FTP_LOST_UNEXPECT;
	else
		msg = MSG_FTP_SERVER_CLOSED_CONN;

	retval = lister_request(
		ftpnode,
		FR_FormatString,"%s %s\n%s",
		AR_Message,	ftpnode->fn_site.se_host,
		FR_MsgNum,	MSG_FTP_NOT_AVAILABLE,
		AR_Message,	errno_known()
				? (unsigned char *)sockerr()
				: GetString(locale,msg),
		FR_ButtonNum,	MSG_FTP_RECONNECT,
		FR_ButtonNum,	MSG_FTP_CANCEL,
		TAG_DONE );
	}

if	(handle)
	rexx_lst_unlock( ftpnode->fn_opus, handle );

lostconn( &ftpnode->fn_ftp );

return retval;
}

/********************************/

//
//	Disconnect from the server if we're still connected.
//	If a new function has been started for the lister then get it going,
//	otherwise just close the lister.
//	Remove the lister's node from the list.
//
//	See opus_quitcommand() and opus_drop() for ScanDir and DeviceList stuff
//
void lister_disconnect( struct opusftp_globals *og, struct msg_loop_data *mld )
{
char                opus[PORTNAMELEN+1];
struct ftp_node    *ftpnode = mld->mld_node;
struct connect_msg *sm;
struct quit_msg    *qm;
ULONG               handle;
BOOL                do_script = 0;

kprintf( "lister_disconnect()\n" );

// save site entry

copy_site_entry( og,&og->og_last_site, &ftpnode->fn_site );

// set marker
og->og_valid_site=TRUE;

handle = ftpnode->fn_handle;

// Script must only be triggered for FTP site listers
// And only ones that had been successfully connected
if	((ftpnode->fn_flags & (LST_CONNECTED | LST_LOCAL)) == LST_CONNECTED
	&& ftpnode->fn_site.se_env->e_script_close
	&& og->og_hooks.dc_Script)
	do_script = 1;

// Need to remember last path?
if	((ftpnode->fn_flags & (LST_CONNECTED | LST_LOCAL)) == LST_CONNECTED
	&& ftpnode->fn_site.se_env->e_keep_last_dir)
	{
	if	(sm = AllocVec( sizeof(*sm), MEMF_CLEAR ))
		{
		copy_site_entry(og,&sm->cm_site,&ftpnode->fn_site);

		IPC_Command( og->og_main_ipc, IPC_REMEMBERPATH, 0, sm, 0, REPLY_NO_PORT );

		FreeVec( sm );
		}
	}

strcpy( opus, ftpnode->fn_opus );

// If the server already closed the connection
if	((!mld->mld_done && *mld->mld_ftpreply == 421)
	|| (mld->mld_node->fn_flags & LST_NOOP_QUIT))
	{
	// shut down the connection
	if	(!(mld->mld_reconnecting = opus_lostconn( og, ftpnode )))
		{
		// Flag to close lister if not reconnecting
		ftpnode->fn_flags &= ~LST_LEAVEOPENIFFAIL;
		ftpnode->fn_flags |= LST_CONNECTFAILED;
		}
	}

// Otherwise log out politely now
else if	(*mld->mld_ftpreply != 421)
	{
	if	(ftpnode->fn_flags & LST_LOGGEDIN)
		logout( &ftpnode->fn_ftp );

	if	(ftpnode->fn_flags & LST_CONNECTED)
		disconnect_host( &ftpnode->fn_ftp );
	}

// Just in case it's busy
rexx_lst_unlock( opus, ftpnode->fn_handle );

//	If lister open, execute 'DeviceList', or 'ScanDir' command if required
//	otherwise close the lister.

if	(ftpnode->fn_flags & LST_OPENED)
	{
	// Leave lister open but remove handler?
	if	((ftpnode->fn_flags & LST_LEAVEOPEN)
		|| ((ftpnode->fn_flags & LST_LEAVEOPENIFFAIL)
			&& (ftpnode->fn_flags & LST_CONNECTFAILED))
		|| ((ftpnode->fn_flags & LST_RECONNECTING)
			&& !(ftpnode->fn_flags & LST_CONNECTFAILED)))
		{
		//kprintf( "** leaving lister open\n" );
		// make sure the cache is dumped
		send_rexxa( opus, REXX_REPLY_NONE, "lister freecaches %lu "PORTNAME"", ftpnode->fn_handle);

		// Remove handler so we don't get 'inactive' msg 
		send_rexxa( opus, REXX_REPLY_NONE, "lister set %lu handler ''", ftpnode->fn_handle );

		send_rexxa( opus, REXX_REPLY_NONE, "lister set %lu source", ftpnode->fn_handle );

		// No command to issue after quitting?
		if	(!mld->mld_quitmsg || !mld->mld_quitmsg->flags)
			rexx_lst_refresh( opus, ftpnode->fn_handle, REFRESH_NODATE );
		}

	// Otherwise close lister
	else
		{
		//kprintf( "** closing lister\n" );
		// Now close it
		rexx_lst_close( opus, ftpnode->fn_handle );
		}
	}

lister_remove_node( og, ftpnode );

// Command to issue after quitting?
if	(mld->mld_quitmsg && (qm = (struct quit_msg *)mld->mld_quitmsg->flags))
	{
	// Command to send? (DeviceList, ScanDir, FTPConnect...)
	if	(qm->qm_command)
		send_rexxa( opus, REXX_REPLY_NONE, "command %s", qm->qm_command );
	}

// Reconnect command?
else if	(mld->mld_quit_command)
	{
	if	(mld->mld_reconnecting)
		send_rexxa( opus, REXX_REPLY_NONE, "command %s", mld->mld_quit_command );

	FreeVec( mld->mld_quit_command );
	mld->mld_quit_command = 0;
	}

// Script?
// Trigger script
if	(do_script)
	{
	char handlebuf[13];
	sprintf( handlebuf, "%lu", handle );

	og->og_hooks.dc_Script( "FTP close connection", handlebuf );
	}
}

/********************************/

void lister_reconnect( struct opusftp_globals *og, struct msg_loop_data *mld )
{
char command[1024+1];

//kprintf( "lister_reconnect()\n" );

// Valid?

if	(!og || !mld)
	return;

// Build command string user:pass@host:port/path
sprintf( command, "FTPConnect RECON LISTER=%lu DIR=\"%s\"",
	mld->mld_node->fn_handle,
	mld->mld_node->fn_site.se_path );

if	(mld->mld_node->fn_site.se_name && *mld->mld_node->fn_site.se_name)
	{
	sprintf( command + strlen(command), " SITE=\"%s\"", mld->mld_node->fn_site.se_name );
	}
else
	{
	strcat( command, " \"" );

	build_url(
		command + strlen(command),
		mld->mld_node->fn_site.se_user,
		mld->mld_node->fn_site.se_pass,
		mld->mld_node->fn_site.se_host,
		mld->mld_node->fn_site.se_port,
		mld->mld_node->fn_site.se_path,
		0 );

	strcat( command, "\"" );
	}

if	(mld->mld_quit_command = AllocVec( strlen(command) + 1, MEMF_CLEAR ))
	{
	//kprintf( "** reconnect allocated quit command\n" );
	strcpy( mld->mld_quit_command, command );

	// Since we'll still be using it, leave it open
	// TODO this is not perfect solution
	mld->mld_node->fn_flags |= LST_RECONNECTING;
	}
}

/********************************/

int lister_retry_connect_requester( struct ftp_node *node, struct connect_log_data *cld )
{
int  choice;
char buffer[1024+1] = "";

//kprintf( "lister_retry_connect_requester()\n" );

// Add reason in brackets
if	(cld->cld_errmsg)
	{
	sprintf(
		buffer,
		cld->cld_errmsg[0] == '('
		? "%s"
		: "(%s)",
		cld->cld_errmsg );
	}

choice = lister_request(
	node,
	FR_FormatString,*buffer ? "%s\n%s\n%s" : "%s\n%s",
	FR_MsgNum,	MSG_FTP_LOGIN_FAILED,
	*buffer ? AR_Message : TAG_IGNORE,	buffer,
	FR_MsgNum,	MSG_FTP_CHANGE_USERPASS,
	FR_ButtonNum,	MSG_FTP_YES,
	FR_ButtonNum,	MSG_FTP_NO,
	FR_ButtonNum,	MSG_FTP_KEEP,
	FR_ButtonNum,	MSG_FTP_CANCEL,
	TAG_DONE );

// Cancel?
if	(choice == 0)
	cld->cld_aborted = 1;

// Yes?
else if	(choice == 1)
	{
	// Activate 'User' gadget
	cld->cld_cm->cm_site.se_active_gadget = GAD_CONNECT_USER;

	if	(!IPC_Command( node->fn_og->og_addrproc, IPC_CONNECTVISUAL, 0, cld->cld_cm, 0, REPLY_NO_PORT ))
		cld->cld_aborted = 1;
	}

return choice;
}
