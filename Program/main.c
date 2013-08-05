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

#include "dopus.h"
#include "dopusmod:modules.h"
#include "eliza.h"
#include "scripts.h"

#define LIB_VER 68	// Minimum library version we need

#define INIT_STEPS	15

#define ExecLib		((struct ExecBase *)*((ULONG *)4))

void __asm __saveds rego_callback(register __a0 struct Window *window);
BOOL __asm __saveds rego_callback_2(
	register __a0 char *,
	register __a1 rego_data *,
	register __a2 rego_data *,
	register __a3 void **,
	register __d0 unsigned long);

struct Library *main_open_library(char *lib,short ver);

#pragma libcall ModuleBase LibExpunge 12 0
void LibExpunge(void);

#define PROG(x) main_bump_progress(main_status,(x),TRUE)


// Main entry point
void main(int argc,char **argv)
{
	IPCData *startup_pic;

	/******** Startup Stuff that happens before the progress bar is displayed ********/

	startup_misc_init();									// Miscellaneous startup stuff
	startup_check_assign();									// Check for DOPUS5: assignment
	startup_open_dopuslib();								// Open dopus5.library
	init_locale_data(&locale);								// Initialise locale info
	startup_check_duplicate();								// See if DOpus is already running
	startup_open_libraries();								// Open libraries
	startup_run_update();									// Run the update module
	startup_init_gui();										// Create a global memory pool and GUI structure
	startup_init_environment();								// Initialise environment structure
	startup_process_args(argc,argv);						// Get startup parameters
	startup_show_startup_picture(&startup_pic);				// Show startup picture

	/******** Startup Stuff that happens after the progress bar is displayed ********/

	startup_init_desktop();						PROG(1);	// Initialise desktop stuff
	startup_read_positions();					PROG(2);	// Initialise the position list
	startup_check_registration();				PROG(3);	// Check registration
	startup_init_ports();						PROG(4);	// Initialise message ports
	startup_read_openwith();					PROG(5);	// Read the OpenWith file
	startup_get_env();							PROG(6);	// Get environment variables
	startup_init_arexx_cx();					PROG(7);	// Initialise ARexx and the commodity
	startup_init_commands();					PROG(8);	// Initialise the commands
	startup_init_filetypes();					PROG(9);	// Initialise filetypes
	environment_open(environment,0,1,0);		PROG(10);	// Read initial environment
	startup_init_notification();				PROG(11);	// Initialise file notifications
	startup_init_icons();						PROG(12);	// Initialise icons and images
	startup_prestartup_script();				PROG(13);	// Pre-Startup script
	startup_init_display(startup_pic);			PROG(14);	// Initialise display
	startup_check_pirates();					PROG(15);	// Check for pirated version
	startup_misc_final();						PROG(16);	// Startup final miscellaneous steps

	// Run main event loop
	if (GUI->flags&GUIF_OK) event_loop();
	quit(0);
}


// Update environment variables
void env_update_settings(BOOL save)
{
	// Clear buffer
	GUI->work_buffer[0]=0;

	// Clock?
	if (GUI->flags&GUIF_CLOCK)
	{
		strcat(GUI->work_buffer,"-clock-");
	}

	// Create icons?
	if (GUI->flags&GUIF_SAVE_ICONS)
	{
		strcat(GUI->work_buffer,"-icons-");
	}

	// File filter?
	if (GUI->flags&GUIF_FILE_FILTER)
	{
		strcat(GUI->work_buffer,"-filter-");
	}

	// Def public screen?
	if (GUI->flags&GUIF_DEFPUBSCR)
	{
		strcat(GUI->work_buffer,"-defpub-");
	}

	// Show icons?
	if (GUI->flags&GUIF_VIEW_ICONS)
	{
		strcat(GUI->work_buffer,"-showicons-");
	}

	// Icon action?
	if (GUI->flags&GUIF_ICON_ACTION)
	{
		strcat(GUI->work_buffer,"-iconaction-");
	}

	// Show all?
	if (GUI->flags&GUIF_SHOW_ALL)
	{
		strcat(GUI->work_buffer,"-showall-");
	}

	// Set variable
	SetEnv("dopus/dopus",GUI->work_buffer,save);
}

void __asm __saveds rego_callback(register __a0 struct Window *window)
{
	struct Task *task;
	struct Window *status;
	TimerHandle *timer;
	short a=10,b=50;

	// Fix IPC
	(task=FindTask(0))->tc_UserData=&main_ipc;
	main_ipc.proc=(struct Process *)task;
	main_ipc.list=0;
	main_ipc.command_port=CreateMsgPort();

	// Display window and delay
	status=OpenStatusWindow(0,
		GetString(&locale,MSG_YOU_SHOULD_REGISTER),
		window->WScreen,0,
		WINDOW_NO_CLOSE);

	// Delay
	if (timer=AllocTimer(UNIT_VBLANK,0))
	{
		StartTimer(timer,10,0);
		Wait(1<<timer->port->mp_SigBit);
		FreeTimer(timer);
	}
	else Delay(a*b);

	// Close status window
	if (status) CloseConfigWindow(status);

	// Default serial number
	lsprintf(GUI->rego.serial_number,"%ld",GUI->backdrop);
}


// Open a library with warning
struct Library *main_open_library(char *lib,short ver)
{
	struct Library *library;
	char text[40];

	// Open it
	if (library=OpenLibrary(lib,ver))
		return library;

	// Build error text
	if (ver==0) strcpy(text,GetString(&locale,MSG_LIBRARY_ANY_VERSION));
	else lsprintf(text,"v%ld+",ver);

	// Show requester
	SimpleRequestTags(0,
		dopus_name,
		GetString(&locale,MSG_OKAY),
		GetString(&locale,MSG_LIBRARY_ERROR),
		lib,
		text);

	return 0;
}



/*************************** The startup functions ************************/

#define	AFF_68060	(1L<<7)

// Miscellaneous startup code
void startup_misc_init()
{
	// Set wildstar bit in dos
	DOSBase->dl_Root->rn_Flags|=RNF_WILDSTAR;

	// Get pointer to our Process structure, and hide requesters
	main_proc=(struct Process *)FindTask(0);
	main_proc->pr_WindowPtr=(APTR)-1;

	// Complement some strings
	compstr(register_name);
	compstr(about_1);
	compstr(about_2);
	eliza_decrypt();
}


// Check for DOPUS5: assignments
void startup_check_assign()
{
	BPTR lock;

	// See if we have a DOPUS5: lock
	if (lock=Lock("DOPUS5:",ACCESS_READ))
		UnLock(lock);

	// We don't; assign it to PROGDIR:
	else
	if (lock=DupLock(GetProgramDir()))
	{
		if (!(AssignLock("DOPUS5",lock)))
			UnLock(lock);
	}

	// See if we have a D5THEMES: lock
	if (lock=Lock("D5THEMES:",ACCESS_READ))
		UnLock(lock);

	// We don't; assign it to DOpus5:Themes
	else
	if (lock=Lock("DOpus5:Themes",ACCESS_READ))
	{
		if (!(AssignLock("D5THEMES",lock)))
			UnLock(lock);
	}
}


// Open dopus5.library
void startup_open_dopuslib()
{
	// Open the library
	if (!(DOpusBase=OpenLibrary("dopus5:libs/dopus5.library",LIB_VER)))
	{
		struct Library *IntuitionBase;

		// Get Intuition
		if (IntuitionBase=OpenLibrary("intuition.library",0))
		{
			struct EasyStruct easy;

			// Fill out EasyStruct
			easy.es_StructSize=sizeof(easy);
			easy.es_Flags=0;
			easy.es_Title=dopus_name;
			easy.es_TextFormat="Unable to open dopus5.library!";
			easy.es_GadgetFormat="Ok";

			// Show error
			EasyRequestArgs(0,&easy,0,0);

			// Close library
			CloseLibrary(IntuitionBase);
		}
		exit(0);
	}
}


// See if DOpus is already running
void startup_check_duplicate()
{
	Forbid();
	if (FindPort(dopus_name))
	{
		struct MsgPort *port;

		// Ask if we want to run another copy
		Permit();
		if (SimpleRequest(0,
			dopus_name,
			GetString(&locale,MSG_ALREADY_RUNNING_BUTTONS),
			GetString(&locale,MSG_ALREADY_RUNNING),0,0,0,0))
		{
			// Find port again (under Forbid)
			Forbid();
			if (port=FindPort(dopus_name))
			{
				// Signal program to wake up
				Signal((struct Task *)port->mp_SigTask,IPCSIG_SHOW);
			}
			Permit();

			// Quit this copy
			quit(0);
		}
	}
	Permit();
}


// Run the update module
void startup_run_update()
{
	struct Library *ModuleBase;

	// Try and get update.module
	if (ModuleBase=OpenLibrary("dopus5:modules/update.module",0))
	{
		// Launch update function
		Module_Entry(0,0,0,0,0,0);

		// Expunge library
		LibExpunge();

		// Close library
		CloseLibrary(ModuleBase);
	}
}


// Open libraries
void startup_open_libraries()
{
	BPTR file;
	long seed=0;

	// Open the other libraries that we need
	if (!(GfxBase=(struct GfxBase *)main_open_library("graphics.library",37)) ||
		!(IntuitionBase=(struct IntuitionBase *)main_open_library("intuition.library",37)) ||
		!(LayersBase=main_open_library("layers.library",37)) ||
		!(DiskfontBase=main_open_library("diskfont.library",36)) ||
		!(IconBase=main_open_library("icon.library",37)) ||
		!(WorkbenchBase=main_open_library("workbench.library",37)) ||
		!(CxBase=main_open_library("commodities.library",37)) ||
		!(UtilityBase=main_open_library("utility.library",37)) ||
		!(GadToolsBase=main_open_library("gadtools.library",37)) ||
		!(RexxSysBase=main_open_library("rexxsyslib.library",0)) ||
		!(AslBase=main_open_library("asl.library",37))) quit(0);

	// Some other useful libraries
	DataTypesBase=OpenLibrary("datatypes.library",0);
	AmigaGuideBase=OpenLibrary("amigaguide.library",0);
	NewIconBase=(struct NewIconBase *)OpenLibrary("newicon.library",0);

	// Is CyberGfx library already in system? If so, open it for ourselves
	if (FindName(&ExecLib->LibList,"cybergraphics.library"))
		CyberGfxBase=OpenLibrary("cybergraphics.library",0);

	// Get input.device base
	if (!OpenDevice("input.device",0,(struct IORequest *)&input_req,0))
		InputBase=(struct Library *)input_req.io_Device;

	// Get timer.device base
	if (!OpenDevice("timer.device",0,(struct IORequest *)&timer_req,0))
		TimerBase=(struct Library *)timer_req.io_Device;
	else quit(0);

	// Get console.device base
	if (!OpenDevice("console.device",-1,(struct IORequest *)&console_req,0))
		ConsoleDevice=(struct Library *)console_req.io_Device;
	else quit(0);

	// Seed random number with something interesting
	if (file=Open("dopus5:system/seed",MODE_OLDFILE))
	{
		char buf[20];
		Read(file,buf,20);
		Close(file);
		seed=atoi((buf[0]=='-'?buf+1:buf));
	}
	seed+=(long)IntuitionBase+(long)AslBase*(long)WorkbenchBase*IntuitionBase->Micros;
	Seed(seed);
}


// Create a global memory pool and GUI structure
void startup_init_gui()
{
	short a,proc=0;

	// Create a global memory pool and GUI structure
	if (!(global_memory_pool=NewMemHandle(1024,512,MEMF_CLEAR|MEMF_PUBLIC)) ||
		!(GUI=AllocMemH(global_memory_pool,sizeof(GUI_Glue))) ||
		!(GUI->screen_title=AllocMemH(global_memory_pool,256)) ||
		!(GUI->filter_string=AllocMemH(global_memory_pool,256)))
		quit(0);

	// Initialise lists
	InitListLock(&GUI->lister_list,0);
	InitListLock(&GUI->buffer_list,0);
	InitListLock(&GUI->buttons_list,0);
	InitListLock(&GUI->process_list,0);
	InitListLock(&GUI->group_list,0);
	InitListLock(&GUI->filetypes,0);
	InitListLock(&GUI->notify_process_list,0);
	InitListLock(&GUI->function_traps,0);
	InitListLock(&GUI->positions,0);
	InitListLock(&GUI->rexx_readers,0);
	InitListLock(&GUI->function_list,0);
	InitListLock(&GUI->rexx_apps,0);
	InitListLock(&GUI->command_list,0);
	InitListLock(&GUI->original_cmd_list,0);
	InitListLock(&GUI->modules_list,0);
	InitListLock(&GUI->popupext_list,0);
	InitListLock(&GUI->iconpos_list,0);
	InitListLock(&GUI->startmenu_list,0);
	InitListLock(&GUI->open_with_list,0);
	GUI->command_history=Att_NewList(LISTF_POOL|LISTF_LOCK);

	// Initialise locks
	InitSemaphore(&GUI->select_lock);
	InitSemaphore(&GUI->req_lock);
	InitSemaphore(&GUI->lister_menu_lock);
	InitSemaphore(&GUI->hotkeys_lock);
	InitSemaphore(&GUI->findfile_lock);
	InitSemaphore(&GUI->filter_lock);
	InitSemaphore(&GUI->scripts_lock);
	InitSemaphore(&GUI->user_menu_lock);
	InitSemaphore(&GUI->custom_pen_lock);

	// Signal for getting screen close
	GUI->screen_signal=AllocSignal(-1);

	// This pointer is cleared by the registration module; big crashes if that doesn't happen
	GUI->screen=(struct Screen *)1;
	GUI->def_filename_length=FILENAME_LEN;

	// Initialise filter string
	strcpy(GUI->filter_string,"#?");

	// Initial requester coordinates
	GUI->req_coords.Left=64;
	GUI->req_coords.Top=32;
	GUI->req_coords.Width=320;
	GUI->req_coords.Height=200;

	// Initialise selection data
	GUI->select_data.type=SELECT_SIMPLE;
	GUI->select_data.entry_type=SELECT_ENTRY_BOTH;
	strcpy(GUI->select_data.name,"*");
	GUI->select_data.name_match=SELECT_MATCH_MATCH;
	GUI->select_data.date_from[0]=0;
	GUI->select_data.date_to[0]=0;
	GUI->select_data.date_match=SELECT_MATCH_IGNORE;
	GUI->select_data.bits=0;
	GUI->select_data.bits_match=SELECT_MATCH_IGNORE;
	GUI->select_data.compare=0;
	GUI->select_data.compare_match=SELECT_MATCH_IGNORE;
	GUI->select_data.include=SELECT_INCLUDE;

	// Get decimal separator
	GUI->decimal_sep=(locale.li_Locale)?locale.li_Locale->loc_GroupSeparator[0]:',';

	// Locale patches installed?
	if (locale.li_LocaleBase &&
		((struct LocaleBase *)locale.li_LocaleBase)->lb_SysPatches) GUI->flags|=GUIF_LOCALE_OK;

	// Calculate width of date field
	if (locale.li_LocaleBase)
	{
		char *str;
		short day,len;
#define LocaleBase locale.li_LocaleBase

		// Get lengths of days of the week
		for (day=DAY_1;day<=DAY_7;day++)
		{
			if ((str=GetLocaleStr(locale.li_Locale,day)) &&
				(len=strlen(str))>GUI->date_length)
				GUI->date_length=len;
		}

		// Yesterday, etc
		for (day=YESTERDAYSTR;day<=FUTURESTR;day++)
		{
			if ((str=GetLocaleStr(locale.li_Locale,day)) &&
				(len=strlen(str))>GUI->date_length)
				GUI->date_length=len;
		}
	}

	// Otherwise, use default (Yesterday)
	else GUI->date_length=9;

	// See if SysIHack is running
	if (FindTask("� sysihack �")) GUI->flags|=GUIF_SYSIHACK;

	// Allocate a string for spaces, and the global undo buffer
	if (!(str_space_string=AllocMemH(global_memory_pool,MAXDISPLAYLENGTH)) ||
		!(GUI->global_undo_buffer=AllocMemH(global_memory_pool,1024)))
		quit(0);
	for (a=0;a<MAXDISPLAYLENGTH-1;a++) str_space_string[a]=' ';

	// Allocate backdrop patterns
	if (!(GUI->pattern=AllocMemH(global_memory_pool,sizeof(PatternData)*3)))
		quit(0);

	// Initialise requester pattern
	GUI->req_pattern.hook.h_Entry=(ULONG (*)())PatternBackfill;
	GUI->req_pattern.hook.h_Data=0;
	GUI->req_pattern.pattern=&GUI->pattern[PATTERN_REQ];
	GUI->req_pattern.disabled=FALSE;

	// Set requester pattern hook in library
	SetReqBackFill(&GUI->req_pattern.hook,&GUI->screen_pointer);
	GUI->flags2|=GUIF2_BACKFILL_SET;

	// Build kickstart version string
	if (GetVar("Kickstart",GUI->ver_kickstart,15,GVF_GLOBAL_ONLY)<1)
	{
		char *ptr3;
		ULONG ptr,*ptr2;
		UWORD ver,rev;
		
		ptr2=(ULONG *)0xffffec;
		ptr=0x1000000-(*ptr2);
		ptr3=(char *)ptr+12;
		ptr2=(ULONG *)ptr3;
		ptr=*ptr2;

		ver=ptr>>16;
		rev=ptr&(((1<<32)-(1<<16))-1);

		lsprintf(GUI->ver_kickstart,"%ld.%ld",ver,rev);
	}

	// Build workbench version string
	if (GetVar("Workbench",GUI->ver_workbench,15,GVF_GLOBAL_ONLY)<1)
	{
		struct Library *VersionBase;
		UWORD ver=0,rev=0;

		if (VersionBase=OpenLibrary("version.library",0))
		{
			ver=VersionBase->lib_Version;
			rev=VersionBase->lib_Revision;
			CloseLibrary(VersionBase);
		}
		lsprintf(GUI->ver_workbench,"%ld.%ld",ver,rev);
	}

	// Build CPU model string
	if (SysBase->AttnFlags&AFF_68010)
		if (SysBase->AttnFlags&AFF_68020)
			if (SysBase->AttnFlags&AFF_68030)
				if (SysBase->AttnFlags&AFF_68040)
					if (SysBase->AttnFlags&AFF_68060)
						proc=60;
					else
						proc=40;
				else
					proc=30;
			else
				proc=20;
		else
			proc=10;
	lsprintf(GUI->ver_cpu,"680%02ld",proc);

	// Build co-processor string
	if (SysBase->AttnFlags&AFF_68040 && SysBase->AttnFlags&AFF_FPU40)
	{
		strcpy(GUI->ver_fpu,"FPU");
	}
	else
	if (SysBase->AttnFlags&AFF_68881)
	{
		if (SysBase->AttnFlags&AFF_68882)
		{
			strcpy(GUI->ver_fpu,"68882");
		}
		else strcpy(GUI->ver_fpu,"68881");
	}
	else strcpy(GUI->ver_fpu,"No FPU");

	// Default CX priority
	GUI->cx_pri=100;

	// Default scroll lines
	GUI->wheel_lines=3;
}


// Process arguments
void startup_process_args(int argc,char **argv)
{
	BPTR lock;
	struct DiskObject *icon=0;
	char **arg_array=0,*ptr;
	BOOL check_env=0;

	// Run from Workbench?
	if (argc==0)
	{
		struct WBStartup *startup;
		short arg;

		// Get startup message
		startup=(struct WBStartup *)argv;

		// Any arguments?
		if (startup->sm_NumArgs>1)
		{
			BPTR lock;

			// Duplicate directory lock
			if (lock=DupLock(startup->sm_ArgList[1].wa_Lock))
			{
				// Change to that directory
				old_current_dir=CurrentDir(lock);
			}
			arg=1;
		}
		else arg=0;

		// Get icon
		icon=GetDiskObject(startup->sm_ArgList[arg].wa_Name);

		// Argument supplied?
		if (arg==1)
		{
			// Store name for environment file
			strcpy(environment->path,startup->sm_ArgList[arg].wa_Name);
			check_env=1;
		}

		// If we got icon, get tooltype array
		if (icon) arg_array=icon->do_ToolTypes;
	}

	// Otherwise, from CLI
	else arg_array=argv;

	// Got arguments?
	if (arg_array)
	{
		// Tool priority
		if (ptr=FindToolType(arg_array,"TOOLPRI"))
			SetTaskPri((struct Task *)main_proc,atoi(ptr));

		// Commodity priority
		if (ptr=FindToolType(arg_array,"CX_PRIORITY"))
			GUI->cx_pri=atoi(ptr);

		// Environment file
		if (ptr=FindToolType(arg_array,"ENVIRONMENT"))
		{
			strcpy(environment->path,ptr);
			check_env=1;
		}

		// Start hidden?
		if ((ptr=FindToolType(arg_array,"CX_POPUP")) &&
			MatchToolValue(ptr,"no"))
			GUI->flags|=GUIF_HIDE_START|GUIF_QUIET;

		// Quiet
		if (FindToolType(arg_array,"QUIET"))
			GUI->flags|=GUIF_QUIET;

		// No front
		if (FindToolType(arg_array,"NOSCREENTOFRONT"))
			GUI->flags2|=GUIF2_NO_SCREENFRONT;

		// Do Workbench startup drawer?
		if ((ptr=FindToolType(arg_array,"WBSTARTUP")) &&
			MatchToolValue(ptr,"yes"))
			GUI->flags|=GUIF_DO_WBSTARTUP;

		// Startup picture?
		if (ptr=FindToolType(arg_array,"STARTUPPIC"))
		{
			// Get picture name
			stccpy(GUI->startup_pic,ptr,79);

			// Delay specified?
			if (ptr=strchr(GUI->startup_pic,'|'))
			{
				// Get delay
				GUI->startup_pic_delay=atoi(ptr+1);
				*ptr=0;
			}
		}
	}

	// Free icon if we had one
	if (icon) FreeDiskObject(icon);

	// Need to check environment path?
	if (check_env)
	{
		APTR handle;

		// Valid OPUS file?
		if (!(handle=IFFOpen(environment->path,IFF_READ,ID_OPUS)))
		{
			BPTR dir,old;
			BOOL fail=1;

			// Get environment path
			if (dir=Lock("dopus5:environment",ACCESS_READ))
			{
				// Move there
				old=CurrentDir(dir);

				// Look for file
				if (lock=Lock(environment->path,ACCESS_READ))
				{
					// Store and unlock
					NameFromLock(lock,environment->path,256);
					UnLock(lock);
					fail=0;
				}

				// Restore directory and unlock
				CurrentDir(old);
				UnLock(dir);
			}

			// Failed?
			if (fail) strcpy(environment->path,"dopus5:environment/default");
		}
		else IFFClose(handle);
	}
}


// Show startup picture
void startup_show_startup_picture(IPCData **startup_pic)
{
	MiscStartup *startup;
	IPCData *ipc=0;
	BPTR lock;

	// Any startup picture defined?
	if (GUI->startup_pic[0])
	{
		// Check picture exists
		if (lock=Lock(GUI->startup_pic,ACCESS_READ))
		{
			// Unlock lock
			UnLock(lock);

			// Allocate startup
			if (startup=AllocVec(sizeof(MiscStartup),MEMF_CLEAR))
			{
				// Fill out startup
				startup->command=SHOW_PICTURE;
				startup->data=GUI->startup_pic;

				// Start process
				if (!(IPC_Launch(
					&GUI->process_list,
					&ipc,
					"dopus_show",
					(ULONG)misc_proc,
					STACK_DEFAULT,
					(ULONG)startup,(struct Library *)DOSBase)))
				{
					// Failed
					if (!ipc) FreeVec(startup);
				}

				// Delay?
				else
				if (GUI->startup_pic_delay)
				{
					Delay(GUI->startup_pic_delay*50);
				}
			}
		}
	}

	// Store IPC pointer
	*startup_pic=ipc;

	// Failed to show picture?
	if (!ipc)
	{
		// Check quiet startup flag isn't set
		if (!(GUI->flags&GUIF_QUIET))
		{
			ULONG flags;

			// Standard flags
			flags=PWF_FILENAME|PWF_GRAPH|PWF_NOIPC;

			// Numbers?
			if (GetVar("dopus/StartupNumbers",GUI->work_buffer,2,GVF_GLOBAL_ONLY)>0 && GUI->work_buffer[0]!='0')
				flags|=PWF_FILESIZE;

			// Open "loading" status window
			main_status=OpenProgressWindowTags(
				PW_Title,dopus_name,
				PW_FileName,GetString(&locale,MSG_PROGRAM_LOADING),
				PW_FileCount,INIT_STEPS,
				PW_FileSize,INIT_STEPS,
				PW_Flags,flags,
				TAG_END);
		}
	}
}


// Initialise desktop stuff
void startup_init_desktop()
{
	// Initialise desktop popup
	if (!(GUI->desktop_menu=PopUpNewHandle(0,0,&locale)))
		quit(0);

	// Add items to desktop popup
	PopUpNewItem(GUI->desktop_menu,MSG_DESKTOP_POPUP_LEFTOUT,MENU_DESKTOP_LEFTOUT,0);
	PopUpNewItem(GUI->desktop_menu,MSG_DESKTOP_POPUP_COPY,MENU_DESKTOP_COPY,0);
	PopUpNewItem(GUI->desktop_menu,MSG_DESKTOP_POPUP_MOVE,MENU_DESKTOP_MOVE,0);
	PopUpSeparator(GUI->desktop_menu);
	PopUpNewItem(GUI->desktop_menu,MSG_CANCEL,MENU_DESKTOP_CANCEL,0);
	GUI->desktop_menu->ph_Menu.flags|=POPUPMF_STICKY;

	// Allocate backdrop stuff
	if (!(GUI->backdrop=backdrop_new(&main_ipc,BDIF_MAIN_DESKTOP)))
		quit(0);

	// Cache some string pointers
	string_no_owner=GetString(&locale,MSG_NO_OWNER);
	string_no_group=GetString(&locale,MSG_NO_GROUP);
	string_empty=GetString(&locale,MSG_EMPTY);

	// Initialise scripts
	InitScripts();
}


// Initialise the position list
void startup_read_positions()
{
	// Allocate position memory
	if (!(GUI->position_memory=NewMemHandle(2048,512,MEMF_PUBLIC|MEMF_CLEAR)))
		quit(0);

	// Read position list
	GUI->position_name="dopus5:system/position-info";
	GetPositions(&GUI->positions,GUI->position_memory,GUI->position_name);
}


// Check registration
void startup_check_registration()
{
	struct Library *ModuleBase;

	// Try to open the registration module
	if (!(ModuleBase=OpenLibrary(register_name,68)) ||
		!(Module_Entry(
			(struct List *)register_name,
			(struct Screen *)rego_callback,
			(IPCData *)rego_callback_2,
			&main_ipc,
			(ULONG)&GUI->rego,
			(ULONG)&GUI->flags)))
	{
		CloseLibrary(ModuleBase);
		quit(0);
	}
	CloseLibrary(ModuleBase);
}


// Initialise message ports
void startup_init_ports()
{
	// Create some message ports
	if (!main_ipc.command_port ||
		!(main_ipc.reply_port=CreateMsgPort()) ||
		!(GUI->notify_port=CreateMsgPort()) ||
		!(GUI->appmsg_port=CreateMsgPort())) quit(0);

	// Create a message port for icon positioning
	GUI->iconpos_port=CreateMsgPort();

	// Make AppPort public
	GUI->appmsg_port->mp_Node.ln_Pri=1;
	GUI->appmsg_port->mp_Node.ln_Name=dopus_name;
	AddPort(GUI->appmsg_port);

	// Initialise public semaphore
	pub_semaphore.sem.ss_Link.ln_Name="DOpus Public Semaphore";
	pub_semaphore.sem.ss_Link.ln_Pri=0;
	pub_semaphore.main_ipc=&main_ipc;
	InitListLock(&pub_semaphore.modules,0);
	AddSemaphore((struct SignalSemaphore *)&pub_semaphore);

	// Add notify request
	GUI->notify_req=
		AddNotifyRequest(
			DN_OPEN_WORKBENCH|DN_CLOSE_WORKBENCH|DN_APP_MENU_LIST|DN_DISKCHANGE|DN_REXX_UP|DN_FLUSH_MEM,
			0,
			GUI->notify_port);
}


// Get environment variables
void startup_get_env(void)
{
	// Clear flags
	GUI->flags&=~(	GUIF_CLOCK|GUIF_SAVE_ICONS|GUIF_FILE_FILTER|GUIF_DEFPUBSCR|GUIF_VIEW_ICONS|GUIF_ICON_ACTION|
					GUIF_SHOW_ALL);
	GUI->flags2&=~(	GUIF2_WB_TITLE|GUIF2_ENABLE_SHORTCUTS|GUIF2_BENIFY);

	// Main system variable
	if (GetVar("dopus/dopus",GUI->work_buffer,sizeof(GUI->work_buffer),GVF_GLOBAL_ONLY)>0)
	{
		// Clock?
		if (strstr(GUI->work_buffer,"-clock-")) GUI->flags|=GUIF_CLOCK;

		// Create icons?
		if (strstr(GUI->work_buffer,"-icons-")) GUI->flags|=GUIF_SAVE_ICONS;

		// File filter?
		if (strstr(GUI->work_buffer,"-filter-")) GUI->flags|=GUIF_FILE_FILTER;

		// Def public screen?
		if (strstr(GUI->work_buffer,"-defpub-")) GUI->flags|=GUIF_DEFPUBSCR;

		// View icons?
		if (strstr(GUI->work_buffer,"-showicons-")) GUI->flags|=GUIF_VIEW_ICONS;

		// Icon action?
		if (strstr(GUI->work_buffer,"-iconaction-")) GUI->flags|=GUIF_ICON_ACTION;

		// Show all?
		if (strstr(GUI->work_buffer,"-showall-")) GUI->flags|=GUIF_SHOW_ALL;
	}

	GetVar("dopus/HardDiskCompression",GUI->work_buffer,2,GVF_GLOBAL_ONLY);
	GetVar("dopus/SchindlersList",GUI->work_buffer,2,GVF_GLOBAL_ONLY);

	// Get environment variables
	if (GetVar("dopus/TuEsUnTeapot",GUI->work_buffer,2,GVF_GLOBAL_ONLY)>0)
	{
		global_requester("Please do not set this variable again.");
		DeleteVar("dopus/TuEsUnTeapot",GVF_GLOBAL_ONLY);
	}
	if (GetVar("dopus/JeTePlumeraiLaTete",GUI->work_buffer,2,GVF_GLOBAL_ONLY)>0)
	{
		global_requester("Why do you want feathers in your head?");
		DeleteVar("dopus/JeTePlumeraiLaTete",GVF_GLOBAL_ONLY);
	}

	if (GetVar("dopus/WorkbenchTitle",GUI->work_buffer,2,GVF_GLOBAL_ONLY)>0 && GUI->work_buffer[0]!='0')
		GUI->flags2|=GUIF2_WB_TITLE;
	if (GetVar("dopus/EnableShortcuts",GUI->work_buffer,2,GVF_GLOBAL_ONLY)>0 && GUI->work_buffer[0]!='0')
		GUI->flags2|=GUIF2_ENABLE_SHORTCUTS;
	if (GetVar("dopus/HidePadlock",GUI->work_buffer,2,GVF_GLOBAL_ONLY)>0 && GUI->work_buffer[0]!='0')
		GUI->flags2|=GUIF2_NO_PADLOCK;
	if (GetVar("dopus/WheelScrollLines",GUI->work_buffer,4,GVF_GLOBAL_ONLY)>0 && isdigit(GUI->work_buffer[0]))
		GUI->wheel_lines=atoi(GUI->work_buffer);
	if (GetVar("dopus/ReturnOfBenify",GUI->work_buffer,2,GVF_GLOBAL_ONLY)>0 && GUI->work_buffer[0]!='0')
		GUI->flags2|=GUIF2_BENIFY;

	GetVar("dopus/JavaVM",GUI->work_buffer,2,GVF_GLOBAL_ONLY);
	GetVar("dopus/CPUTurboMode",GUI->work_buffer,2,GVF_GLOBAL_ONLY);

	// Icon spacing
	if (GetVar("dopus/IconSpaceX",GUI->work_buffer,4,GVF_GLOBAL_ONLY)>0)
	{
		GUI->icon_space_x=atoi(GUI->work_buffer);
		if (GUI->icon_space_x<1)
			GUI->icon_space_x=1;
	}
	else GUI->icon_space_x=CLEANUP_SPACE_X;

	GetVar("dopus/ColdChiselMode",GUI->work_buffer,2,GVF_GLOBAL_ONLY);

	// Icon spacing
	if (GetVar("dopus/IconSpaceY",GUI->work_buffer,4,GVF_GLOBAL_ONLY)>0)
	{
		GUI->icon_space_y=atoi(GUI->work_buffer);
		if (GUI->icon_space_y<1)
			GUI->icon_space_y=1;
	}
	else GUI->icon_space_y=CLEANUP_SPACE_Y;

	// Icon grid
	if (GetVar("dopus/IconGridX",GUI->work_buffer,4,GVF_GLOBAL_ONLY)>0)
	{
		GUI->icon_grid_x=atoi(GUI->work_buffer);
		if (GUI->icon_grid_x<1)
			GUI->icon_grid_x=1;
	}
	else GUI->icon_grid_x=1;

	// Icon grid
	if (GetVar("dopus/IconGridY",GUI->work_buffer,4,GVF_GLOBAL_ONLY)>0)
	{
		GUI->icon_grid_y=atoi(GUI->work_buffer);
		if (GUI->icon_grid_y<1)
			GUI->icon_grid_y=1;
	}
	else GUI->icon_grid_y=1;

	GetVar("dopus/DigitalConvergence",GUI->work_buffer,2,GVF_GLOBAL_ONLY);
	GetVar("dopus/PPCEnabled",GUI->work_buffer,2,GVF_GLOBAL_ONLY);
}


// Initialise the ARexx handler and the commodity
void startup_init_arexx_cx()
{
	// Check for valid serial number (GUI->backdrop pointer will be cleared if serial number not valid - crash!)
	SerialCheck(GUI->rego.serial_number,(ULONG *)&GUI->backdrop);

	// Launch the ARexx process
	IPC_Launch(
		&GUI->process_list,
		&GUI->rexx_proc,
		"dopus_rexx",
		(ULONG)rexx_proc,
		STACK_DEFAULT,0,(struct Library *)DOSBase);

	// Initialise the commodity
	if (!(GUI->cx=AllocMemH(global_memory_pool,sizeof(CxData))))
		quit(0);
	cx_install(GUI->cx);
}


// Initialise environment structure
void startup_init_environment()
{
	// Create main environment
	if (!(environment=environment_new()))
		quit(0);
	strcpy(environment->path,"dopus5:environment/default");
}


// Initialise commands
void startup_init_commands()
{
	short a;

	// Lock command list
	lock_listlock(&GUI->command_list,TRUE);

	// Go through internal command list
	for (a=0;commandlist_internal[a].name;a++)
	{
		// Add to list
		AddTail((struct List *)&GUI->command_list,(struct Node *)&commandlist_internal[a]);

		// Get description
		if (commandlist_internal[a].desc)
		{
			commandlist_internal[a].desc=(ULONG)GetString(&locale,commandlist_internal[a].desc);
		}
	}

	// Unlock command list
	unlock_listlock(&GUI->command_list);

	// Scan for normal modules
	init_commands_scan(SCAN_MODULES);

	// Scan for user commands
	init_commands_scan(SCAN_USER);

	// Initialise default functions
	def_function_leaveout=new_default_function("leaveout",global_memory_pool);
	def_function_iconinfo=new_default_function("iconinfo",global_memory_pool);
	def_function_format=new_default_function("format",global_memory_pool);
	def_function_diskcopy=new_default_function("diskcopy",global_memory_pool);
	def_function_select=new_default_function("select",global_memory_pool);
	def_function_diskinfo=new_default_function("diskinfo",global_memory_pool);
	def_function_devicelist=new_default_function("devicelist",global_memory_pool);
	def_function_cachelist=new_default_function("cachelist",global_memory_pool);
	def_function_all=new_default_function("all",global_memory_pool);
	def_function_rename=new_default_function("rename",global_memory_pool);
	def_function_delete=new_default_function("delete",global_memory_pool);
	def_function_delete_quiet=new_default_function("delete quiet",global_memory_pool);
	def_function_makedir=new_default_function("makedir",global_memory_pool);
	def_function_copy=new_default_function("copy",global_memory_pool);
	def_function_move=new_default_function("move",global_memory_pool);
	def_function_assign=new_default_function("assign",global_memory_pool);
	def_function_configure=new_default_function("configure",global_memory_pool);
	def_function_cli=new_default_function("cli",global_memory_pool);
	def_function_devicelist_full=new_default_function("devicelist full",global_memory_pool);
	def_function_devicelist_brief=new_default_function("devicelist brief",global_memory_pool);
	def_function_loadtheme=new_default_function("loadtheme",global_memory_pool);
	def_function_savetheme=new_default_function("savetheme",global_memory_pool);
	def_function_buildtheme=new_default_function("buildtheme",global_memory_pool);
}


// Initialise filetypes
void startup_init_filetypes()
{
	// Memory handle for the filetypes
	if (!(GUI->filetype_memory=NewMemHandle(4096,256,MEMF_CLEAR)))
		quit(0);

	// Get default filetypes
	filetype_default_list(GUI->filetype_memory,&GUI->filetypes);

	// Read filetypes
	filetype_read_list(GUI->filetype_memory,&GUI->filetypes);
}


// Initialise file notifications
void startup_init_notification()
{
	// Start notification of filetype directory
	GUI->filetype_notify=
		start_file_notify(
			"dopus5:filetypes",
			NOTIFY_FILETYPES_CHANGED,
			GUI->appmsg_port);

	// Start notification of modules directory
	GUI->modules_notify=
		start_file_notify(
			"dopus5:modules",
			NOTIFY_MODULES_CHANGED,
			GUI->appmsg_port);

	// Start notification of env:dopus directory
	GUI->env_notify=
		start_file_notify(
			"env:dopus",
			NOTIFY_ENV_CHANGED,
			GUI->appmsg_port);

	// Start notification of commands directory
	GUI->commands_notify=
		start_file_notify(
			"dopus5:commands",
			NOTIFY_COMMANDS_CHANGED,
			GUI->appmsg_port);

	// Start notification of font prefs
	GUI->font_notify=
		start_file_notify(
			"env:sys/font.prefs",
			NOTIFY_FONT_CHANGED,
			GUI->appmsg_port);
}


// Initialise icons, etc
void startup_init_icons()
{
	// Get arrow image for toolbars
	GUI->toolbar_arrow_image=OpenImage("dopus5:images/ToolbarArrow.image",0);

	// Get icon for listers
	GUI->lister_icon=GetCachedDiskObject("dopus5:Icons/Lister",GCDOF_NOCACHE);

	// Get icon for buttons
	GUI->button_icon=GetCachedDiskObject("dopus5:Icons/Buttons",GCDOF_NOCACHE);
}


// Run pre-startup script
void startup_prestartup_script()
{
	// Launch startup script
	RunScript(SCRIPT_PRESTARTUP,0);
}


// Initialise display
void startup_init_display(IPCData *startup_pic)
{
	// Close startup picture
	if (startup_pic)
		IPC_Quit(startup_pic,0,0);

	// If not hidden, open display
	if (!(GUI->flags&GUIF_HIDE_START))
		display_open(0);

	// Hidden startup
	else
	{
		// Initialise any listers
		IPC_ListCommand(&GUI->lister_list,LISTER_INIT,0,0,0);

		// Show iconified
		hide_display();
	}

	// Broadcast notify message to say Opus has started up
	SendNotifyMsg(DN_OPUS_START,GUI->dopus_copy,0,0,GUI->rexx_port_name,0);

	// Do Workbench startup
	if (GUI->flags&GUIF_DO_WBSTARTUP)
		wb_do_startup();
}


// Check for pirated version
void startup_check_pirates()
{
	// Pirated version? They can only run it 24 times...
	if (GUI->rego.pirate && GUI->rego.pirate_count>=23)
	{
		// Kill the mongrels
		misc_startup("foo",CMD_KILL_PIRATES,0,0,0);
	}
}


// Final startup steps
void startup_misc_final()
{
	// Launch startup script
	RunScript(SCRIPT_STARTUP,0);
	GUI->flags|=GUIF_DONE_STARTUP;

	// Is REXX running?
	Forbid();
	if (FindPort("REXX"))
	{
		// Set flag so we know it is
		GUI->flags|=GUIF_REXX;

		// Launch process to scan for modules
		misc_startup("dopus_module_sniffer",MAIN_SNIFF_MODULES,GUI->window,0,0);
	}
	Permit();

	// If main status window is open, close it
	if (main_status)
	{
		CloseProgressWindow(main_status);
		main_status=0;
	}
}


// Update the progress bar
BOOL main_bump_progress(APTR prog,short step,BOOL can_quit)
{
	BOOL ret=0;

	// Got progress indicator?
	if (prog)
	{
		// Update progress count
		SetProgressWindowTags(prog,PW_FileNum,step,PW_FileDone,step,TAG_END);

		// Get abort status
		if (ret=CheckProgressAbort(prog))
		{
			// Put up quit requester?
			if (can_quit)
			{
				if (ret=(!(SimpleRequest(0,
					dopus_name,
					GetString(&locale,MSG_ABORT_STARTUP_BUTTONS),
					GetString(&locale,MSG_ABORT_STARTUP),0,0,0,0))))
				{
					// We can quit from here
					quit(0);
				}
			}
		}
	}

	return ret;
}

