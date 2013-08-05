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
 *	20-05-96	This file seperated from lister.c
 *	13-06-96	Now strips trailing '#' from filenames as well as '/', '*', and '@'
 */

#include "ftp.h"
#include "ftp_ad_sockproto.h"
#include "ftp_ad_errno.h"
#include "ftp_lister.h"
#include "ftp_arexx.h"
#include "ftp_ipc.h"
#include "ftp_opusftp.h"
#include "ftp_util.h"
#include "ftp_list.h"

#define   min(a,b)    (((a) <= (b)) ? (a) : (b))

#ifndef DEBUG
#define kprintf ;   /##/
#endif

#define SocketBase GETSOCKBASE(FindTask(0L))

extern const char *months[];

/********************************/

//
//	Gets called for each line returned by the 'LIST' FTP command
//	Translates the info for each entry and adds it to the lister
//
static int list_update( struct update_info *ui, char *line )
{
struct entry_info entry = {0};

// Valid?
if	(!ui || !line)
	return 1;

// Is this needed any more?
ObtainSemaphore( &ui->ui_sem );


if	(ui->ui_ftpnode->fn_ls_to_entryinfo( &entry, line, ui->ui_flags))
	{
	lister_add(
		ui->ui_ftpnode,
		entry.ei_name,
		entry.ei_size,
		entry.ei_type,
		entry.ei_seconds,
		entry.ei_prot,
		entry.ei_comment );

	GetSysTime( &ui->ui_curr );

	if	(ui->ui_curr.tv_secs - ui->ui_last.tv_secs >= ui->ui_ftpnode->fn_site.se_env->e_list_update)
		{
		ftplister_refresh( ui->ui_ftpnode, REFRESH_NODATE );
		ui->ui_last = ui->ui_curr;
		}
	}

ReleaseSemaphore( &ui->ui_sem );

return 1;
}

/***************************/

//
//	Ask if user wishes to grab index files
//
static int ask_index( struct ftp_node *ftpnode, char *name, int size )
{
Lister *lister;
int     result = 0;
char   *bytes;

if	(!ftpnode->fn_og->og_noreq && (lister = (Lister *)ftpnode->fn_handle))
	{
	if	(size > 1024)
		{
		size = size/1024;
		bytes = GetString(locale,MSG_KILOBYTES);
		}
	else
		bytes = GetString(locale,MSG_FTP_XFER_BYTES);		
	
	result = lister_request(
		ftpnode,
		FR_FormatString,GetString(locale,MSG_QUERYINDEX),
		AR_Message,	name,
		AR_Message,	size,
		AR_Message,	bytes,
		FR_ButtonNum,	MSG_FTP_OK,
		FR_ButtonNum,	MSG_FTP_CANCEL,
		TAG_DONE );
	}

return result;
}

#define	FNAMESIZE	32

/***********************************/

//
//	split details from index file and update Lister
//	This ASSUMES a standard format followed by
//	AMINET index and Files.BBS where data starts on column 40
//	In some cases this will not work so a simple back check is done
//	for a ' ' or '+' as a starting marker
//
static void update_lister_comments( struct ftp_node *ftpnode, char *indexname )
{
char  fname[FNAMESIZE];
char  comment[COMMENTLEN];
char  buf[256];
BPTR  cf;
ULONG lister;

lister = ftpnode->fn_handle;

// lock list.  lister is still busy from previous fn call
ftpnode->fn_og->og_hooks.dc_LockFileList( (ULONG)lister, TRUE );

// open file
if	(cf = Open( indexname, MODE_OLDFILE ))
	{
	while	(FGets( cf, buf, 256 ))
		{
		*fname = *comment = NULL;

		if	(*buf == '|' || *buf == '\n')
			continue;

		// strip,name and comment

		stptok( buf, fname, sizeof(fname), " " );

		if	(strlen(buf) > 39)
			{
			// check if we have a ' ' or + on pos 39
			// if not then search backwards to up to 20 chars
			register char *x;
			register int limit=20;

			x=&buf[39];
			
			while	(--limit && *x != ' ' && *x != '+')
				--x;

			stptok( x + 1, comment, sizeof(comment), "\"\r\n" );
			}

		// update the lister
		if	(*fname && *comment)
			ftpnode->fn_og->og_hooks.dc_SetFileComment( (ULONG)lister, fname, comment );
		}
	Close( cf );
	}

// unlock list
ftpnode->fn_og->og_hooks.dc_UnlockFileList( (ULONG)lister );

// refresh lister
ftplister_refresh( ftpnode, REFRESH_NODATE );
}


/***********************************
 * set index type and get index fromm remote site
 * and update lister
 */

static void get_index( struct ftp_node *ftpnode )
{
char  localname[80];
char *remotename;
int   size;
BPTR  file;

if	(ftpnode->fn_ftp.fi_found_index & INDEX_INDEX)
	{
	remotename = NAME_INDEX;
	size = ftpnode->fn_ftp.fi_found_index_size;
	}
else
	{
	remotename = NAME_FBBS;
	size = ftpnode->fn_ftp.fi_found_fbbs_size;
	}

if	((size / 1024) > ftpnode->fn_site.se_env->e_indexsize)
	{
	if	(ftpnode->fn_site.se_env->e_index_auto 	// if auto and >max then skip
		|| !ask_index( ftpnode, remotename, size )) // or user cancels
		return;
	}

//sprintf( localname, "T:do_idx#%lu", ftpnode->fn_handle );
if	(file = open_temp_file( localname, ftpnode->fn_ipc ))
	Close( file );

if	(lister_xferindex( ftpnode, localname, remotename, size )) // Get OK
	update_lister_comments( ftpnode, localname );

// NOTE: lister_xferindex left lister locked
rexx_lst_unlock( ftpnode->fn_opus, ftpnode->fn_handle );

//delete index or part index
DeleteFile( localname );
}

/********************************/

//
//	Load directory into a lister, updates the path from the ftp_node structure
//
int lister_list( struct opusftp_globals *ogp, struct ftp_node *ftpnode, BOOL redo_cache )
{
int retval = 0, buffered = 0;
int lsresult;
BOOL abort = FALSE;
struct update_info *ui;
ULONG handle;
struct Library *TimerBase = GetTimerBase();
int progress = ftpnode->fn_site.se_env->e_progress_window;

kprintf( "lister_list()\n" );

// Prepare for index if found
ftpnode->fn_ftp.fi_found_index = 0;
ftpnode->fn_ftp.fi_found_index_size = 0;
ftpnode->fn_ftp.fi_found_fbbs_size = 0;

// Passive mode required?
ftpnode->fn_ftp.fi_flags &= ~FTP_PASSIVE;
if	(ftpnode->fn_site.se_env->e_passive)
	ftpnode->fn_ftp.fi_flags |= FTP_PASSIVE;

if	(ui = AllocVec( sizeof(struct update_info), MEMF_CLEAR ))
	{
	handle = ftpnode->fn_handle;

	rexx_lst_lock( ftpnode->fn_opus, handle );

	if	(!redo_cache)
		buffered = rexx_lst_findcache( ftpnode->fn_opus, handle, ftpnode->fn_site.se_path );

	if	(!buffered || redo_cache)
		{
		// No need to do an empty if entry is cached, use clear
		if	(redo_cache)
			rexx_lst_clear( ftpnode->fn_opus, handle );
		else
			rexx_lst_empty( ftpnode->fn_opus, handle ); 

		// We just cleared the path - put it back
		rexx_lst_set_path( ftpnode->fn_opus, handle, ftpnode->fn_site.se_path );

		InitSemaphore( &ui->ui_sem );

		if	(ftpnode->fn_systype == FTP_MACOS)
			ui->ui_flags |= UI_NO_LINK_FIELD;

		if	(ftpnode->fn_ftp.fi_flags & FTP_IS_UNIX)
			ui->ui_flags |= UI_DOT_HIDDEN;

		if	(ftpnode->fn_site.se_env->e_unk_links_file)
			ui->ui_flags |= UI_LINKS_ARE_FILES;

/*gp changed to use flag and not use fred_hack env var */

		if	(ftpnode->fn_site.se_env->e_special_dir)
			ui->ui_flags |= UI_SPECIAL_DIR;

		ui->ui_og      = ogp;
		ui->ui_ftpnode = ftpnode;
		ui->ui_handle  = handle;
		ui->ui_opus    = ftpnode->fn_opus;
		ui->ui_abort   = &abort;

		if	(progress)
			{
			lister_prog_init(
				ftpnode,
				ftpnode->fn_site.se_host,
				GetString(locale,MSG_READING_DIR),
				ftpnode->fn_site.se_path,
				PROGRESS_FILE_OFF,
				PROGRESS_BAR_OFF );
			}
		else
			{
			rexx_lst_title( ftpnode->fn_opus, handle, GetString(locale,MSG_READING_DIR) );
			send_rexxa( ftpnode->fn_opus, FALSE, "lister refresh %lu full", ftpnode->fn_handle );
			}

		lst_addabort( ftpnode, SIGBREAKF_CTRL_D, 0 );

		GetSysTime( &ui->ui_last );

		// Send the LIST command
		lsresult = list( &ftpnode->fn_ftp, list_update, ui, ftpnode->fn_lscmd, "" );
		
		//kprintf("list command result |n",lsresult);

		ftplister_refresh( ftpnode, REFRESH_NODATE );

		// LIST command failed?
		if	(lsresult == -2)
			{
			if	(!stricmp( ftpnode->fn_lscmd, LSCMD ))	/* If we haven't tried yet */
				{
				strcpy( ftpnode->fn_lscmd, "LIST" );	/* Get rid of all options */
				lsresult = list( &ftpnode->fn_ftp, list_update, ui, ftpnode->fn_lscmd, "" );
								/* and try again... */
				}				/* (should change to NLST?) */
			}					/* (dg allows NLST opts) */
		
		retval = lsresult ? FALSE : TRUE;

		lst_remabort( ftpnode );

		if	(progress)
			lister_prog_clear( ftpnode );
		}

	rexx_lst_label( ftpnode->fn_opus, handle, "FTP:", ftpnode->fn_site.se_host, NULL );
	rexx_lst_title( ftpnode->fn_opus, handle, ftpnode->fn_site.se_host );
	send_rexxa( ftpnode->fn_opus, FALSE, "lister refresh %lu full", handle );

	rexx_lst_unlock( ftpnode->fn_opus, handle );

	FreeVec( ui );
	}

// Get index if option is enabled and it exists
if	(ftpnode->fn_site.se_env->e_index_enable && ftpnode->fn_ftp.fi_found_index)
	get_index( ftpnode );

return retval;
}
