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

#include "dopuslib:dopusbase.h"
#include "dopuslib:dopuspragmas.h"

char *version="$VER: LoadDB 62.0 (14.7.97)";

enum
{
	ARG_DEBUG,
	ARG_DELAY,
	ARG_CLEANUP,
	ARG_NEWPATH,
	ARG_COUNT
};

BOOL workbench_running(void);

void __stdargs __main(char *arg_string)
{
	BOOL ok=0,run_wb=0,wb_startup=1,wb_running;
	BPTR lock;
	struct Process *proc;
	APTR wsave;
	char buf[256];
	struct Library *InputBase;
	struct Library *DOpusBase;
	struct IOStdReq input_req;
	short a;
	char *arg_ptr=0;
	struct RDArgs *args;
	ULONG arg_array[ARG_COUNT];

	// Initialise arguments
	for (a=0;a<ARG_COUNT;a++)
		arg_array[a]=0;

	// Parse arguments
	if (args=ReadArgs("-DEBUG/S,DELAY/S,CLEANUP/S,NEWPATH/S",(long *)arg_array,0))
	{
		// Free arguments
		FreeArgs(args);
	}

	// See if Workbench is running
	wb_running=workbench_running();

	// New path?
	if (arg_array[ARG_NEWPATH])
	{
		struct Library *DOpusBase;
		BOOL ok=0;
	
		// See if DOpus is running
		if (FindPort("Directory Opus"))
		{
			// Open library
			if (DOpusBase=OpenLibrary("dopus5:libs/dopus5.library",61))
			{
				// Update the path list from current process
				UpdatePathList();

				// Close library
				CloseLibrary(DOpusBase);

				// Set flag to say we got a new path
				ok=1;
			}
		}

		// Is Workbench also running?
		if (wb_running)
		{
			// Set flag to pass through to Workbench
			run_wb=1;
		}

		// Otherwise, exit now, unless Opus has to be launched
		else
		if (ok) exit(0);
	}

	// Open input device
	if (!run_wb && !(OpenDevice("input.device",0,(struct IORequest *)&input_req,0)))
	{
		// Get input base
		InputBase=(struct Library *)input_req.io_Device;

		// See if shift is held down
		if (PeekQualifier()&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT))
		{
			// Run Workbench instead
			run_wb=1;
		}

		// Is control held down?
		else
		if (PeekQualifier()&IEQUALIFIER_CONTROL)
		{
			// No WBStartup
			wb_startup=0;
		}

		// Close input device
		CloseDevice((struct IORequest *)&input_req);
	}

	// Find argument pointer
	for (a=0;arg_string[a] && arg_string[a]!=' ';a++);
	if (arg_string[a]) arg_ptr=arg_string+a;

	// Try to run DOpus?
	if (!run_wb)
	{
		// See if Workbench is already running; if so, we don't do WBStartup again
		if (wb_running) wb_startup=0;

		// Get this process, turn off requesters
		proc=(struct Process *)FindTask(0);
		wsave=proc->pr_WindowPtr;
		proc->pr_WindowPtr=(APTR)-1;

		// See if DOpus is there
		if (lock=Lock("DOpus5:DirectoryOpus",ACCESS_READ))
		{
			// Yup
			UnLock(lock);

			// Open library
			if (DOpusBase=OpenLibrary("dopus5:libs/dopus5.library",0))
			{
				CloseLibrary(DOpusBase);

				// Build command string
				strcpy(buf,"DOpus5:DirectoryOpus ENVIRONMENT=workbench ");
				if (wb_startup) strcat(buf," WBSTARTUP=YES ");
				if (arg_ptr) strcat(buf,arg_ptr);

				// Strip LF
				for (arg_ptr=buf;*arg_ptr;arg_ptr++)
					if (*arg_ptr=='\n') *arg_ptr=0;

				// Try launch
				ok=Execute(buf,0,0);
			}
		}

		// Restore requesters
		proc->pr_WindowPtr=wsave;
	}

	// Did we fail?
	if (!ok)
	{
		// Failed for some reason; start old-fashioned Workbench
		strcpy(buf,"c:LoadWB_Old");

		// Add Workbench arguments
		if (arg_array[ARG_DEBUG]) strcat(buf," -DEBUG");
		if (arg_array[ARG_DELAY]) strcat(buf," DELAY");
		if (arg_array[ARG_CLEANUP]) strcat(buf," CLEANUP");
		if (arg_array[ARG_NEWPATH]) strcat(buf," NEWPATH");

		// Run it
		Execute(buf,0,0);
	}

	exit(0);
}


// See if Workbench is running
BOOL workbench_running(void)
{
	struct Screen *screen;
	struct List *list;
	BOOL wb=0;

	// Lock public screen list
	if (list=LockPubScreenList())
	{
		struct PubScreenNode *pub;
		BOOL ok=0;

		// Go through screens
		for (pub=(struct PubScreenNode *)list->lh_Head;
			pub->psn_Node.ln_Succ;
			pub=(struct PubScreenNode *)pub->psn_Node.ln_Succ)
		{
			// Workbench screen?
			if (stricmp(pub->psn_Node.ln_Name,"Workbench")==0)
			{
				ok=1;
				break;
			}
		}

		// Unlock list
		UnlockPubScreenList();

		// If Workbench screen isn't open, don't try to lock it
		if (!ok) return 0;
	}

	// Lock Workbench screen
	if (screen=LockPubScreen("Workbench"))
	{
		struct Window *window;
		ULONG lock;

		// Lock Intuition
		lock=LockIBase(0);

		// Go through windows on screen
		for (window=screen->FirstWindow;
			window;
			window=window->NextWindow)
		{
			// Look for Workbench windows
			if (window->Flags&WFLG_WBENCHWINDOW)
			{
				wb=1;
				break;
			}
		}

		// Unlock Intuition, and screen
		UnlockIBase(lock);
		UnlockPubScreen(0,screen);
	}

	return wb;
}
