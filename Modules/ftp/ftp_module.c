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

/*
 *	 1-09-95	callback function now used to get Opus port name
 *	25-09-95	FTPCopy + FTPRename now work
 *	17-10-97	Added forbid/permit around findport.
 */

#include "ftp.h"
#include "ftp_arexx.h"
#include "ftp_module.h"
#include "ftp_ipc.h"
#include "ftp_opusftp.h"
#include "ftp_util.h"
#include "ftp.strings"
#include "ftp_addrsupp_protos.h"

#ifndef DEBUG
#define kprintf ;   /##/
#endif

#define min(a,b) (((a)<(b))?(a):(b))

void dopus_ftp( void );

// WARNING - This initialises the structure to 0 the first time only!
// struct opusftp_globals og = { 0 };
//
// But this initialises it every time.
//
// (Some weird behavioural differences between DATA and BSS initialization
//  in shared libraries)

struct opusftp_globals og;

//
//	Setup global variables and launch main task
//	Must be done only once for the life of the module
//
static int mod_init( EXT_FUNC(func_callback), IPCData *opus_ipc, IPCData *function_ipc )
{
struct modlaunch_data *mldata;		// Startup data we send to new process
DOpusCallbackInfo register *hooks;
int okay = FALSE;

// Setup global constants
og.og_opus_ipc		= opus_ipc;	// ** GJP IMPORTANT Must be set 

og.og_ftp_launched	= TRUE;
og.og_noreq		= FALSE;

// main
og.og_socketlibname[0]	= "bsdsocket.library";
og.og_socketlibname[1]	= "socket.library";
og.og_socketlibver[0]	= 3;
og.og_socketlibver[1]	= 0;

// Setup global variables

og.og_socketlib		= AMITCPSOCK;
og.og_listercount	= 0;

// addressbook
og.og_addrbook_open	= FALSE;

// Fill in callback hooks
hooks = &og.og_hooks;
hooks->dc_Count = DOPUS_HOOK_COUNT;

// What is this doing??
hooks->dc_RemoveFileEntry = 0;

IPC_Command( opus_ipc, HOOKCMD_GET_CALLBACKS, 0, &og.og_hooks, 0, REPLY_NO_PORT );

// No callback hooks, no play
if	(!hooks->dc_CheckDesktop) // new 2/5/97
	{
	display_msg( &og, function_ipc, NULL, 0, GetString(locale,MSG_BADVER) );
	return FALSE;
	}

// Global data now set up

// Allocate data for the new process
if	(mldata = AllocVec( sizeof(struct modlaunch_data), MEMF_CLEAR ))
	{
	mldata->mld_a4            = getreg(REG_A4);	// Instead of __saveds
	mldata->mld_screen        = og.og_screen;	// Opus's screen (should be passed to support multiple Opuses)
	mldata->mld_function_ipc  = function_ipc;	// Module IPC for new callback hooks
	mldata->mld_func_callback = func_callback;	// Opus's old callback hook
	mldata->mld_og            = &og;		// Global info

	// Launch a new task - returns TRUE if CreateNewProc() succeeds
	if	(IPC_Launch(
		NULL,				// List to add task to (optional, but useful)
		&og.og_main_ipc, 		// IPCData ** to store task IPC pointer in (optional)
		"dopus_ftp",			// Name
		(ULONG)dopus_ftp,		// Code
		4096 * 2,			// Stack size
		(ULONG)mldata,			// Data passed to task
		(struct Library *)DOSBase ))	// Needs pointer to dos.library
		{
		if	(mldata->mld_okay)
			okay = 1;
		}

	if	(!okay)
		FreeVec( mldata );
	}

return okay;
}

//
//	Send an addressbook message
//
static int mod_addrbook( IPCData *function_ipc, char *args, int arglen )
{
struct addrbook_msg *am;
int                  okay = FALSE;

if	(am = AllocVec( sizeof(*am), MEMF_CLEAR ))
	{
	am->am_function_handle = IPCDATA(function_ipc);
	stccpy( am->am_opus, og.og_opusname, PORTNAMELEN + 1 );

	okay = IPC_Command( og.og_main_ipc, IPC_ADDRBOOK, 0, am, 0, REPLY_NO_PORT );

	FreeVec( am );
	}

kprintf( "module addrbook done (%ld)\n", okay );

return okay;
}

//
//	Build and send a connect message
//	Function is called from a direct command FTPCONNECT (like AmiNet..)
//
static int mod_connect( IPCData *function_ipc, char *args, int arglen )
{
FuncArgs           *fa;
struct connect_msg *cm;
ULONG               flags = 0;
int                 okay = FALSE;

kprintf( "mod_connect()\n" );

// Workaround weird jon-ism
if	(!args || !*args)
	args = " ";

if	(fa = ParseArgs( CONNECT_TEMPLATE, args ))
	{
	if	(cm = get_blank_connectmsg( &og ))
		{
		cm->cm_handle = 0;

		stccpy( cm->cm_opus, og.og_opusname, PORTNAMELEN + 1 );

		if	(fa->FA_Arguments[D_OPT_HOST])
			stccpy( cm->cm_site.se_host, (char *)fa->FA_Arguments[D_OPT_HOST], HOSTNAMELEN + 1 );
		if	(fa->FA_Arguments[D_OPT_PORT])
			cm->cm_site.se_port = *(int *)fa->FA_Arguments[D_OPT_PORT];
		if	(fa->FA_Arguments[D_OPT_USER])
			stccpy( cm->cm_site.se_user, (char *)fa->FA_Arguments[D_OPT_USER], USERNAMELEN + 1 );
		if	(fa->FA_Arguments[D_OPT_PASS])
			stccpy( cm->cm_site.se_pass, (char *)fa->FA_Arguments[D_OPT_PASS], PASSWORDLEN + 1 );
		if	(fa->FA_Arguments[D_OPT_PATH])
			{
			stccpy( cm->cm_site.se_path, (char *)fa->FA_Arguments[D_OPT_PATH], PATHLEN + 1 );
			flags |= CONN_OPT_PATH;
			}
		if	(fa->FA_Arguments[D_OPT_HANDLE])
			cm->cm_handle = *(int *)fa->FA_Arguments[D_OPT_HANDLE];

		if	(fa->FA_Arguments[D_OPT_SITE])
			{
			stccpy( cm->cm_site.se_name, (char *)fa->FA_Arguments[D_OPT_SITE], HOSTNAMELEN + 1 );
			flags |= CONN_OPT_SITE;
			}

		if	(fa->FA_Arguments[D_OPT_GUI])
			flags |= CONN_OPT_GUI;

		if	(fa->FA_Arguments[D_OPT_NOSCAN])
			flags |= CONN_OPT_NOSCAN;

		if	(fa->FA_Arguments[D_OPT_RECON])
			flags |= CONN_OPT_RECON;

		// Check for official style URL (RFC 1738)
		// ftp://<user>:<password>@<host>:<port>/<url-path>
		//        [user[:password]@]host[:port][/path]

		// When Opus calls us for 'FTP://', the path has already been converted to DIR=xxx
		if	(fa->FA_Arguments[D_OPT_HOST]
			&&!(fa->FA_Arguments[D_OPT_PORT]
			||fa->FA_Arguments[D_OPT_USER]
			||fa->FA_Arguments[D_OPT_PASS]
			/*||fa->FA_Arguments[D_OPT_PATH]*/))
			{
			// Remove hostname copied above and start afresh
			stccpy( cm->cm_site.se_host, "", HOSTNAMELEN + 1 );

			split_url(
				(char *)fa->FA_Arguments[D_OPT_HOST],
				cm->cm_site.se_user,
				cm->cm_site.se_pass,
				cm->cm_site.se_host,
				&cm->cm_site.se_port,
				cm->cm_site.se_path );
			}

		okay = IPC_Command( og.og_main_ipc, IPC_CONNECT, flags, 0, cm, REPLY_NO_PORT );
		}

	DisposeArgs( fa );
	}

kprintf( "module connect done (%ld)\n", okay );

return okay;
}


//
//	These need space to add a LF for ReadArgs()
//
static int mod_setvar( IPCData *function_ipc, char *args, int arglen )
{
struct ftp_msg *fm;
int             okay = FALSE;

// Workaround weird jon-ism
if	(!args || !*args)
	{
	args = " ";
	arglen = 1;
	}

if	(fm = AllocVec( sizeof(struct ftp_msg) + arglen + 1, MEMF_CLEAR ))
	{
	fm->fm_function_handle = IPCDATA(function_ipc);

	if	(args && *args)
		{
		fm->fm_names = (char *)(fm + 1);
		strcpy( fm->fm_names, args );
		}

	okay = IPC_Command( og.og_main_ipc, IPC_SETVAR, 0, fm, 0, REPLY_NO_PORT );

	FreeVec( fm );
	}

kprintf( "module setvar done (%ld)\n", okay );

return okay;
}

//
//	Add an entry for the current FTP lister to the address book
//
static int mod_ftpadd( IPCData *function_ipc, char *args, int arglen )
{
struct ftp_msg *fm;
int             okay = FALSE;

if	(fm = AllocVec( sizeof(*fm), MEMF_CLEAR ))
	{
	fm->fm_function_handle = IPCDATA(function_ipc);

	okay = IPC_Command( og.og_main_ipc, IPC_ADD, 0, fm, 0, REPLY_NO_PORT );

	FreeVec( fm );
	}

kprintf( "module ftpadd done (%ld)\n", okay );

return okay;
}

//
//	Send a literal command to the FTP server
//

enum
{
COMMAND_OPT_QUIET,
COMMAND_OPT_LONG,
COMMAND_OPT_COMMAND,
NUM_COMMAND_OPTS
};

static int mod_ftpcommand( IPCData *function_ipc, char *args, int arglen )
{
FuncArgs       *fa;
struct ftp_msg *fm;
ULONG           flags = 0;
int             len = 0;
int             okay = FALSE;

// Workaround weird jon-ism
if	(!args || !*args)
	args = " ";

if	(fa = ParseArgs( COMMAND_TEMPLATE, args ))
	{
	if	(fa->FA_Arguments[COMMAND_OPT_QUIET])
		flags |= CMD_OPT_QUIET;
	if	(fa->FA_Arguments[COMMAND_OPT_LONG])
		flags |= CMD_OPT_LONG;
	if	(fa->FA_Arguments[COMMAND_OPT_COMMAND])
		{
		len = strlen((char *)fa->FA_Arguments[COMMAND_OPT_COMMAND]);
		}

	if	(fm = AllocVec( sizeof(*fm) + len + 1, MEMF_CLEAR ))
		{
		fm->fm_function_handle = IPCDATA(function_ipc);
		fm->fm_flags = flags;

		if	(fa->FA_Arguments[COMMAND_OPT_COMMAND])
			{
			fm->fm_names = (char *)(fm + 1);
			sprintf( fm->fm_names, "%s", fa->FA_Arguments[COMMAND_OPT_COMMAND] );
			}

		okay = IPC_Command(
			og.og_main_ipc,
			IPC_FTPCOMMAND,
			fm->fm_names ? CMD_HAS_ARGS : 0,
			fm,
			0,
			REPLY_NO_PORT );

		FreeVec( fm );
		}
	DisposeArgs( fa );
	}

kprintf( "module ftpcommand done (%ld)\n", okay );

return okay;
}

//
//	FTPOptions
//

enum
{
OPT_OPT_DEFAULT,
NUM_OPT_OPTS
};

static int mod_options( IPCData *function_ipc, char *args, int arglen )
{
FuncArgs           *fa;
struct connect_msg *sm;
int                 isdefault = 0;
int                 okay = 0;

// Workaround weird jon-ism
if	(!args || !*args)
	args = " ";

if	(fa = ParseArgs( OPTIONS_TEMPLATE, args ))
	{
	if	(fa->FA_Arguments[OPT_OPT_DEFAULT])
		isdefault = 1;

	if	(sm = AllocVec( sizeof(*sm), MEMF_CLEAR ))
		{
		sm->cm_function_handle = IPCDATA(function_ipc);
		okay = IPC_Command( og.og_main_ipc, IPC_OPTIONS, isdefault, sm, 0, REPLY_NO_PORT );

		FreeVec( sm );
		}

	DisposeArgs( fa );
	}

kprintf( "module options done (%ld)\n", okay );

return okay;
}

//
//	Quit
//

enum
{
OPT_FORCE,
NUM_QUIT_OPTS
};

static int mod_quit( IPCData *function_ipc, char *args, int arglen )
{
FuncArgs *fa;
int       okay = FALSE;

//kprintf( "mod_quit()\n" );

// Workaround weird jon-ism
if	(!args || !*args)
	args = " ";

if	(fa = ParseArgs( QUIT_TEMPLATE, args ))
	{
	IPC_Quit( og.og_main_ipc, fa->FA_Arguments[OPT_FORCE], TRUE );
	okay = TRUE;

	DisposeArgs( fa );
	}

// og.og_ftp_launched = FALSE; // Marker for invocation test GJP
return okay;
}

struct module_command_info
{
ULONG   mci_id;
int   (*mci_function)(IPCData*,char*args,int arglen);
int     mci_need_port;
};

struct module_command_info module_command_table[] =
{
ID_ADDRESSBOOK,	mod_addrbook,	0,
ID_CONNECT,	mod_connect,	0,
ID_SETVAR,	mod_setvar,	0,
ID_COMMAND,	mod_ftpcommand,	0,
ID_QUIT,	mod_quit,	1,
ID_ADD,		mod_ftpadd,	0,
ID_OPTIONS,	mod_options,	0,
0xffffffff
};

//
//	The Module Entry Routine
//
//	The args passed to a module will be either:
//		1. When no arguments are supplied, it will be an empty string ""
//		2. When arguments are supplied, a string ending in a LF character "\n"
//
int __asm __saveds L_Module_Entry(
	register __a0 char          *args,
	register __a1 struct Screen *screen,
	register __a2 IPCData       *function_ipc,	// 'dopus_function' - the spawned module process
	register __a3 IPCData       *opus_ipc,		// 'DOpus5' - the main Opus process
	register __d0 ULONG          mod_id,
	register __d1 EXT_FUNC(func_callback))
{
struct Library             *DOpusBase, *DOSBase;	// Avoid stomping on the global bases!
struct MsgPort             *ftpport;			// If we find this OpusFTP is already running
struct module_command_info *mci;
int	                    arglen;			// length of args (including null terminator)
int                         okay = FALSE;		// Was this call to the module successful?
char                       *oldtaskname;

oldtaskname = FindTask(0)->tc_Node.ln_Name;
FindTask(0)->tc_Node.ln_Name = "dopus_ftp_function";

{
void *m = AllocMem( 101, MEMF_ANY );
FreeMem( m, 101 );
}

// Screen may be different because of mode change or hide/show
// Screen pointer should not be global to support multiple Opuses
og.og_screen = screen;

if	(DOpusBase = OpenLibrary( "dopus5:libs/dopus5.library", VERSION_DOPUSLIB ))
	{
	if	(DOSBase = OpenLibrary( "dos.library", 0 ))
		{
		// Create a ReadArgs() compatible copy of the arguments
		if	(!args || !*args)
			{
			// length of args - including LF
			arglen = 1;
	
			// args must not end with LF
			args = "";
			}
		else
			{
			// length of args - including LF
			arglen = strlen(args);
	
			// args must not end with LF
			args[arglen - 1] = 0;
			}
	
		Forbid();
		ftpport = FindPort( PORTNAME );
		Permit();

		// Get Opus's ARexx port name
		func_callback( EXTCMD_GET_PORT, IPCDATA(function_ipc), og.og_opusname );

		if	(mod_id == ID_QUIT || ftpport )
			{
			//kprintf( "not launching dopus_ftp\n" );
			if	(ftpport)
				;//kprintf( "\tbecause it's already running\n" );
			if	(mod_id == ID_QUIT)
				;//kprintf( "\tbecause function is quit\n" );

			okay = TRUE;
			}
		else
			{
			// Have we started and are waiting for response from user
			// to bad version arexx request ?
			//
			// if so the ipc ptr should be set
			// so okay should remain false so we do not re-start prog
			// og.og_ftp_launched is initialised by mod_init and
			// cleared when process quits mod_quit and at end of
			// fn dopus_ftp()

			if	(!og.og_ftp_launched)
				{
				if	(!(okay = mod_init( func_callback, opus_ipc, function_ipc )))
					og.og_ftp_launched = FALSE;
				}
			}
			
		// If OpusFTP process already existed or launched successfully, send a command to it
		if	(okay)
			{
			// kprintf( "  sending message for running process\n" );

			// Send a message to the main FTP process
			for	(mci = module_command_table; mci->mci_id != 0xffffffff; ++mci)
				{
				if	(mci->mci_id == mod_id && (!mci->mci_need_port || ftpport))
					{
					okay = mci->mci_function( function_ipc, args, arglen );
					break;
					}
				}

			if	(mci->mci_id == 0xffffffff)
				kprintf( "unknown mod_id:%ld\n", mod_id );

			}

		CloseLibrary( DOSBase );
		}
	
	CloseLibrary( DOpusBase );
	}

FindTask(0)->tc_Node.ln_Name = oldtaskname;

return okay;
}
