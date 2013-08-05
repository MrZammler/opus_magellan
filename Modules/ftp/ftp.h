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

#ifndef _FTP_H
#define _FTP_H

#define CATCOMP_NUMBERS
#include "ftp.strings"
#include "modules_lib.h"
#include "hooks.h"

#include "ftp_ad_internet.h"

#define CMDBUFSIZE 256
#define REPLYTIMEOUT 60

#define IOBUFSIZE	(8* 1024)	// What is the most effective size for this
					// depends on speed of link. Largest observed
					// recv() size localhost is 8K
					// for agnus, largest is 536 bytes !

#define	BUFIOBUFSIZE	1024		// size for sgetc buffer for each socket/ftp_info

#define	VERSION_DOPUSLIB	60	// min dopus library version required

#define	INDEX_INDEX 	(1 << 0)
#define	INDEX_FILES 	(1 << 1)
#define	INDEX_BIG 	(1 << 2)	// index file exceeds limit
#define	INDEX_FILES_BIG (1 << 3)	// files.bbs exceeds limit

#define	NAME_INDEX "INDEX"
#define	NAME_FBBS  "Files.BBS"

//
//	Every FTP server has an ftp_info structure:
//
struct ftp_info
{
struct opusftp_globals *fi_og;			// Global data pointer - we really shouldn't need this at this low level!!
struct Task            *fi_task;		// Task who owns this socket
struct sockaddr_in      fi_addr;		// Socket address
int                     fi_cs;			// Control socket
LONG                    fi_port;		// Port 21 or User selected port
int                     fi_timeout;
ULONG                   fi_flags;		// Server info: which commands are not supported, OS type, etc
ULONG                   fi_abortsignals;	// Signal the process (ipc->proc) with this when abort pressed (CTRL_C or CTRL_D)
char                    fi_iobuf[IOBUFSIZE + 1];	// I/O buffer for control socket and get, put, list
char                    fi_bufiobuf[BUFIOBUFSIZE + 1];	// Buffered I/O buffer used by iread & friends
int                     fi_reply;		// FTP reply code
int                     fi_errno;		// Last error code (for stuff other than ftp replies)
int                     fi_aborted;		// Did we abort the last get, put, or getput?
LONG                    fi_ioerr;		// Result from IoErr() on dos error
char                    fi_serverr[IOBUFSIZE + 1];	// Copy off error server error reply

int                     fi_found_index;		// GP Market flags for index, files.bbs etc
int                     fi_found_index_size;	// GP Size of index file found
int                     fi_found_fbbs_size;	// GP Size of Files.BBS
int                     fi_doing_noop;		// GP Flag to suppress log output
int                     fi_buffer_left;		// GP
char                   *fi_buffer_pos;		// GP
};

//	FTP flags
enum
{
FTP_NO_PASV	= 1 << 31,	// Doesn't support or allow 'PASV' command - passive transfers
FTP_NO_REST	= 1 << 30,	// Doesn't support 'REST' command - restart transer
FTP_NO_SIZE	= 1 << 29,	// Doesn't support 'SIZE' command - size of file
FTP_NO_MDTM	= 1 << 28,	// Doesn't support 'MDTM' command - last modified time of file
FTP_NO_CHMOD	= 1 << 27,	// Doesn't support 'SITE CHMOD' command - set protection bits

FTP_IS_UNIX	= 1 << 15,	// Operating system is a version of Unix (DG/UX)
FTP_IS_WUFTPD	= 1 << 14,	// FTP server is wu-ftpd

FTP_PASSIVE	= 1 << 7,	// Use passive transfers
};

//	Different FTP sytem types we might eventually recognise
enum
{
FTP_UNKNOWN,
FTP_UNIX,
FTP_AMIGA,
FTP_WINDOWSNT,			// Use SITE command to enable Unix style ls format
FTP_WIN32,			// NT or Windows95/98 - works like Unix
FTP_OS2,			// Different ls options and ls format
FTP_MACOS,			// ls output has no 'links' field
FTP_DGUX,			// LIST can't use options but NLST -alF is fine
FTP_ULTRIX,
FTP_VAX_VMS,			// Incompatible list format
FTP_VM_CMS			// Incompatible list format
};

// FTP error codes (other than FTP replies)
enum
{
FTPERR_NO_MEM = 1,		// Out of memory
FTPERR_SOCKET_FAIL,		// A call to socket() failed
FTPERR_BIND_FAIL,		// A call to bind() failed
FTPERR_GETSOCKNAME_FAIL,	// A call to getsockname() failed
FTPERR_LISTEN_FAIL,		// A call to listen() failed
FTPERR_TIMEOUT,			// sgetc() timed out
FTPERR_FAKE_421,		// getreply() returns 421 but didn't read it in

FTPERR_XFER_SRCERR	= 1 << 15,	// Error on source end of get(), put(), getput()
FTPERR_XFER_DSTERR	= 1 << 16,	// Error on destination end of get(), put(), getput()
FTPERR_XFER_RARERR	= 1 << 17,	// Other error during get(), put(), list()

FTPERR_XFER_MASK	= FTPERR_XFER_SRCERR | FTPERR_XFER_DSTERR | FTPERR_XFER_RARERR,
};

// PORT command flags
#define PORT_QUIET	(1<<0)	// Don't leave reply in reply buffer

// Internal ftp() flags
#define FTPFLAG_ASYNCH	(1<<0)	// Don't get reply

// Internal getreply() flags
#define GETREPLY_QUIET	(1<<0)	// Don't overwrite iobuf

int  gethost   ( struct opusftp_globals *, struct sockaddr_in *, char * );

int  ftp       ( struct ftp_info *, const char *cmd );
int  ftpa      ( struct ftp_info *, const char *fmt, ... );
int  getreply  ( struct ftp_info * );

// Internal version of some commands supporting flags
int _ftpa      ( struct ftp_info *, unsigned long flags, const char *fmt, ... );
int _getreply  ( struct ftp_info *, unsigned long flags, int (*updatefn)(void *,int,char *), void *updateinfo );

void ftp_abor  ( struct ftp_info * );

int  ftp_cdup  ( struct ftp_info *, int (*updatefn)(void *,int,char *), void *updateinfo );
int  ftp_chmod ( struct ftp_info *, unsigned int mode, char *name );
int  ftp_cwd   ( struct ftp_info *, int (*updatefn)(void *,int,char *), void *updateinfo, char *path );
int  ftp_dele  ( struct ftp_info *, char *name );
int  ftp_image ( struct ftp_info * );
int  ftp_mdtm  ( struct ftp_info *, char *name );
int  ftp_mkd   ( struct ftp_info *, char *name );
int  ftp_pasv  ( struct ftp_info * );
int  ftp_port  ( struct ftp_info *, unsigned long flags, unsigned char *host, unsigned char *port );
int  ftp_pwd   ( struct ftp_info * );
int  ftp_rename( struct ftp_info *, char *oldname, char *newname );
int  ftp_rest  ( struct ftp_info *, unsigned int offset );
int  ftp_rmd   ( struct ftp_info *, char *name );
int  ftp_size  ( struct ftp_info *, char *name );
int  ftp_syst  ( struct ftp_info * );

int  connect_host   ( struct ftp_info *, int (*updatefn)(void *,int,char *), void *updateinfo, char *host, int port );
void disconnect_host( struct ftp_info * );
void lostconn       ( struct ftp_info * );

int  login ( struct ftp_info *, int (*updatefn)(void *,int,char *), void *updateinfo, char *user, char *passwd );
void logout( struct ftp_info * );

int          list( struct ftp_info *, int (*updatefn)(void *,const char *line), void *updateinfo, const char *cmd, const char *pathname );
unsigned int get ( struct ftp_info *, int (*updatefn)(void *,unsigned int,unsigned int), void *updateinfo, char *remote, char *local, BOOL restart );
unsigned int put ( struct ftp_info *, int (*updatefn)(void *,unsigned int,unsigned int), void *updateinfo, char *local, char *remote, unsigned int restart );

int do_lister_srcport( IPCMessage *msg );

// Convert PASV reply to address
BOOL pasv_to_address( struct sockaddr_in *address, const char *buf );

#endif
