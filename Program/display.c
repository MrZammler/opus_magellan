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

#define ExecLib		((struct ExecBase *)*((ULONG *)4))

// Open the display
BOOL display_open(long flags)
{
	struct Gadget *gadget=0;
	struct TextFont *font=0;

	// Clear "display-closed" flag
	GUI->flags&=~GUIF_CLOSED;

	// Kill "hidden" clock if it's open
	IPC_QuitName(&GUI->process_list,NAME_HIDDEN_CLOCK,0);

	// Is there a hide appicon?
	if (GUI->hide_appicon)
	{
		// Remove app icon
		RemoveAppIcon(GUI->hide_appicon);
		FreeCachedDiskObject(GUI->hide_diskobject);
		GUI->hide_appicon=0;
	}

	// Or an appmenuitem?
	if (GUI->hide_appitem)
	{
		RemoveAppMenuItem(GUI->hide_appitem);
		GUI->hide_appitem=0;
	}

	// See if we need to open the screen
	if (!GUI->screen &&
		!GUI->window &&
		environment->env->screen_mode!=MODE_WORKBENCHUSE &&
		environment->env->screen_mode!=MODE_PUBLICSCREEN)
	{
		USHORT pens[1];

		// Screen mode
		ULONG screen_mode;

		// Workbench clone?
		if (environment->env->screen_mode==MODE_WORKBENCHCLONE)
		{
			struct Screen *wbscreen;

			// Get Workbench screen
			if (wbscreen=LockPubScreen("Workbench"))
			{
				screen_mode=GetVPModeID(&wbscreen->ViewPort);
				UnlockPubScreen(0,wbscreen);
			}
			else screen_mode=HIRES_KEY;
		}
		else screen_mode=environment->env->screen_mode;

		// Initialise pen array
		pens[0]=(USHORT)~0;

		// Want a font?
		if (environment->env->font_name[FONT_SCREEN][0] &&
			environment->env->font_size[FONT_SCREEN]>4)
		{
			// Copy font name
			strcpy(GUI->screen_font_name,environment->env->font_name[FONT_SCREEN]);

			// Add .font if necessary
			if (!(strstri(GUI->screen_font_name,".font"))) strcat(GUI->screen_font_name,".font");

			// Fill out TextAttr
			GUI->screen_font.ta_Name=GUI->screen_font_name;
			GUI->screen_font.ta_YSize=environment->env->font_size[FONT_SCREEN];
			GUI->screen_font.ta_Style=0;
			GUI->screen_font.ta_Flags=0;

			// Open font
			font=OpenDiskFont(&GUI->screen_font);
		}

		// Open screen
		GUI->screen=OpenScreenTags(0,
			SA_LikeWorkbench,TRUE,
			SA_Width,
				(environment->env->screen_flags&SCRFLAGS_DEFWIDTH)?
					STDSCREENWIDTH:
					environment->env->screen_width,
			SA_Height,
				(environment->env->screen_flags&SCRFLAGS_DEFHEIGHT)?
					STDSCREENHEIGHT:
					environment->env->screen_height,
			SA_Depth,environment->env->screen_depth,
			SA_Overscan,OSCAN_TEXT,
			SA_Title,dopus_name,
			SA_DisplayID,screen_mode,
			SA_PubName,(ULONG)GUI->rexx_port_name,
			SA_AutoScroll,TRUE,
			SA_Interleaved,TRUE,
			SA_SharePens,TRUE,
			(font)?SA_Font:SA_SysFont,(font)?(ULONG)&GUI->screen_font:1,
			SA_Pens,(ULONG)pens,
			SA_PubSig,GUI->screen_signal,
			TAG_END);

		// Had a font?
		if (font) CloseFont(font);

		// Screen open?
		if (GUI->screen)
		{
			// Make it public
			PubScreenStatus(GUI->screen,0);

			// Save pointer to it
			GUI->screen_pointer=GUI->screen;

			// Load palette
			load_screen_palette();

			// Default public screen?
			if (GUI->flags&GUIF_DEFPUBSCR)
				display_set_shanghai(TRUE);
		}

		// Otherwise failed
		else
		{
			super_request_args(
				0,
				GetString(&locale,MSG_UNABLE_TO_OPEN_SCREEN),
				SRF_SCREEN_PARENT,
				GetString(&locale,MSG_CONTINUE),0);
		}
	}


	// See if we need to open the window
	if (!GUI->window)
	{
		struct Screen *use_screen,*lock_screen=0;
		struct IBox dims;

		// Do we have our own custom screen?
		if (GUI->screen) use_screen=GUI->screen;

		// Otherwise
		else
		{
			// Workbench use?
			if (environment->env->screen_mode==MODE_WORKBENCHUSE)
				lock_screen=LockPubScreen("Workbench");

			// Public screen
			else
			if (environment->env->screen_mode==MODE_PUBLICSCREEN)
				lock_screen=LockPubScreen(environment->env->pubscreen_name);

			// Default?
			if (!lock_screen)
			{
				// Try to lock default screen
				while (!(lock_screen=LockPubScreen(0)))
				{
					short pri;

					// Try to lock Workbench
					if (lock_screen=LockPubScreen("Workbench"))
						break;

					// Get old priority, set us to priority 1
					pri=FindTask(0)->tc_Node.ln_Pri;
					SetTaskPri(FindTask(0),1);

					// Open Workbench and wait a bit
					OpenWorkBench();
					Delay(25);

					// Restore priority
					SetTaskPri(FindTask(0),pri);
				}
			}

			// Use this one
			use_screen=lock_screen;
		}

		// Is window a backdrop window?
		if (environment->env->env_flags&ENVF_BACKDROP) GUI->backdrop_window=1;
		else GUI->backdrop_window=0;

		// Get window dimensions
		if (GUI->backdrop_window ||
			(environment->env->window_pos.Width<1 ||
			environment->env->window_pos.Height<1))
		{
			// Default size and positioning
			dims.Left=0;
			dims.Top=use_screen->BarHeight+1;
			dims.Width=use_screen->Width;
			dims.Height=use_screen->Height-use_screen->BarHeight-1;
		}

		// Otherwise, use configured dimensions
		else
		{
			dims=environment->env->window_pos;
			if (dims.Width<100) dims.Width=100;
			if (dims.Height<40) dims.Height=40;
		}

		// Store pointer to screen
		GUI->screen_pointer=use_screen;

		// Lock backdrop window pointer
		GetSemaphore(&GUI->backdrop->window_lock,SEMF_EXCLUSIVE,0);

		// Initialise backfill hook
		GUI->main_pattern.hook.h_Entry=(ULONG (*)())PatternBackfill;
		GUI->main_pattern.hook.h_Data=GUI->backdrop;
		GUI->main_pattern.pattern=&GUI->pattern[PATTERN_MAIN];

		// Default title
		strcpy(GUI->screen_title,dopus_name);

		// Initialise boopsi list
		NewList(&GUI->backdrop->boopsi_list);

		// Create iconify gadget unless backdrop
		if (!GUI->backdrop_window)
			gadget=create_iconify_gadget(use_screen,&GUI->backdrop->boopsi_list,FALSE);

		// Open window
		GUI->window=OpenWindowTags(0,
			WA_Left,dims.Left,
			WA_Top,dims.Top,
			WA_Width,dims.Width,
			WA_Height,dims.Height,
			WA_MinWidth,100,
			WA_MinHeight,40,
			WA_MaxWidth,-1,
			WA_MaxHeight,-1,
			WA_IDCMP,
					IDCMP_ACTIVEWINDOW|
					IDCMP_CLOSEWINDOW|
					IDCMP_DISKINSERTED|
					IDCMP_DISKREMOVED|
					IDCMP_GADGETDOWN|
					IDCMP_GADGETUP|
					IDCMP_INACTIVEWINDOW|
					IDCMP_MENUHELP|
					IDCMP_MENUPICK|
					IDCMP_MENUVERIFY|
					IDCMP_MOUSEBUTTONS|
					IDCMP_MOUSEMOVE|
					IDCMP_NEWSIZE|
					IDCMP_RAWKEY|
					IDCMP_REFRESHWINDOW,
			WA_Activate,TRUE,
			WA_Backdrop,GUI->backdrop_window,
			WA_Borderless,GUI->backdrop_window,
			WA_CloseGadget,!GUI->backdrop_window,
			WA_DepthGadget,!GUI->backdrop_window,
			WA_DragBar,!GUI->backdrop_window,
			WA_MenuHelp,TRUE,
			WA_SimpleRefresh,TRUE,
			WA_SizeGadget,!GUI->backdrop_window,
			WA_SizeBBottom,!GUI->backdrop_window,
			WA_SizeBRight,!GUI->backdrop_window,
			WA_NewLookMenus,TRUE,
			(GUI->backdrop_window)?TAG_IGNORE:WA_Title,dopus_name,
			WA_PubScreen,use_screen,
			WA_PubScreenFallBack,TRUE,
			WA_ScreenTitle,GUI->screen_title,
			WA_Gadgets,gadget,
			WA_BackFill,&GUI->main_pattern,
			TAG_END);

		// Unlock backdrop window pointer
		FreeSemaphore(&GUI->backdrop->window_lock);

		// Window open ok?
		if (GUI->window)
		{
			// Build chipset string
			strcpy(GUI->ver_chips,
					(GfxBase->ChipRevBits0&GFXF_AA_ALICE)?"AGA":
					(GfxBase->ChipRevBits0&GFXF_HR_AGNUS)?"ECS":"OCS");

			// Is window on a custom screen?
			if (GfxBase->LibNode.lib_Version>=39 &&
				!(GetBitMapAttr(GUI->window->RPort->BitMap,BMA_FLAGS)&BMF_STANDARD))
			{
				// Picasso?
				if (FindName(&ExecLib->LibList,"rtg.library"))
					strcpy(GUI->ver_chips,"P96");

				// CyberGfx
				else
				if (FindName(&ExecLib->LibList,"cgxsystem.library") ||
					FindName(&ExecLib->LibList,"cyberintuition.library"))
					strcpy(GUI->ver_chips,"CGX");
			}

			// Get visual info
			GUI->visual_info=GetVisualInfoA(GUI->window->WScreen,0);

			// Fix title gadgets
			if (!GUI->backdrop_window) FixTitleGadgets(GUI->window);

			// Initialise ID
			SetWindowID(GUI->window,&GUI->id,WINDOW_BACKDROP,(struct MsgPort *)&main_ipc);

			// No screen to front?
			if (GUI->flags2&GUIF2_NO_SCREENFRONT)
				GUI->flags2&=~GUIF2_NO_SCREENFRONT;

			// Bring window's screen to front
			else
			if (IntuitionBase->FirstScreen!=GUI->window->WScreen)
				ScreenToFront(GUI->window->WScreen);

			// Set busy pointer
			SetBusyPointer(GUI->window);

			// Store pointer to screen
			GUI->screen_pointer=GUI->window->WScreen;

			// Get screen information
			GUI->screen_info=ScreenInfo(GUI->screen_pointer);

			// Get DrawInfo
			if (!GUI->draw_info) GUI->draw_info=GetScreenDrawInfo(GUI->screen_pointer);

			// Turn on appicon notification
			SetNotifyRequest(GUI->notify_req,DN_APP_ICON_LIST,DN_APP_ICON_LIST);

			// Initialise backdrop
			backdrop_init_info(GUI->backdrop,GUI->window,0);

			// Try to add AppWindow
			GUI->appwindow=AddAppWindowA(
				WINDOW_BACKDROP,
				(ULONG)&main_ipc,
				GUI->window,
				GUI->appmsg_port,0);

			// Set menu
			display_reset_menus(1,0);

			// Remap rexx icons
			rexx_remap_icons(FALSE);

			// Get palette
			get_colour_table();

			// Get all icons
			backdrop_get_objects(
				GUI->backdrop,
				BGOF_ALL|BGOF_SHOW|BGOF_FIRST|(flags&BGOF_DESKTOP));
		}

		// Unlock the screen if we'd locked it
		if (lock_screen) UnlockPubScreen(0,lock_screen);
	}

	// Failed?
	if (!GUI->window) return 0;

	// Set "display open" flag
	GUI->flags|=GUIF_OPEN;

	// Get screen information
	GUI->screen_info=ScreenInfo(GUI->screen_pointer);

	// Do remap?
	if (!(flags&DSPOPENF_NO_REMAP))
	{
		// Remap toolbar arrow
		RemapImage(GUI->toolbar_arrow_image,GUI->screen_pointer,&GUI->remap);

		// Update toolbar cache
		GetToolBarCache(GUI->toolbar,TRUE);
	}

	// If clock isn't open, launch it
	if (!(IPC_FindProc(&GUI->process_list,NAME_CLOCK,0,0)))
	{
		// Launch clock
		IPC_Launch(
			&GUI->process_list,
			0,
			NAME_CLOCK,
			(ULONG)clock_proc,
			STACK_DEFAULT,
			0,(struct Library *)DOSBase);
	}

	// Remap lister and button icons
	RemapIcon(GUI->lister_icon,GUI->window->WScreen,FALSE);
	RemapIcon(GUI->button_icon,GUI->window->WScreen,FALSE);

	// Open buttons
	IPC_ListCommand(&GUI->buttons_list,BUTTONS_OPEN,0,(ULONG)GUI->screen_pointer,0);

	// Open StartMenus
	IPC_ListCommand(&GUI->startmenu_list,IPC_SHOW,0,(ULONG)GUI->screen_pointer,0);

	// Tile listers?
	if (environment->env->flags&ENVRF_AUTO_TILE)
	{
		// Tile 'em
		lister_tile(LISTER_TILE_ALL);

		// Clear tile flag
		environment->env->flags&=~ENVRF_AUTO_TILE;
	}

	// Open listers and groups
	IPC_ListCommand(&GUI->lister_list,LISTER_INIT,0,(ULONG)GUI->screen_pointer,0);
	IPC_ListCommand(&GUI->group_list,IPC_SHOW,0,(ULONG)GUI->screen_pointer,0);

	// Tell all processes to show themselves
	IPC_ListCommand(&GUI->process_list,IPC_SHOW,0,(ULONG)GUI->screen_pointer,FALSE);
	IPC_ListCommand(&GUI->function_list,IPC_SHOW,0,(ULONG)GUI->screen_pointer,FALSE);

	// Broadcast notify message
	SendNotifyMsg(DN_OPUS_SHOW,(ULONG)GUI->window,0,0,0,0);

	// Check source/destination
	lister_smart_source(0);

	// Get backdrop patterns
	if (!GUI->pattern[PATTERN_MAIN].valid)
	{
		// Launch process to read pattern
		misc_startup(NAME_PATTERNS,CMD_REMAP_PATTERNS,0,0,TRUE);
	}

	// Clear busy pointer
	ClearPointer(GUI->window);

/*	// V44 icon stuff
	if (IconBase->lib_Version>=44)
		IconControl(NULL,ICONCTRLA_SetGlobalScreen,GUI->screen_pointer,TAG_DONE);
*/

	// Success
	return 1;
}


// Shut the display down
void close_display(int close_flags,BOOL quit_all)
{
	struct Screen *screen_lock=0;

	// Activate main window
	if (GUI->window)
	{
		// Activate window
		ActivateWindow(GUI->window);
		SetBusyPointer(GUI->window);

		// Get a lock on our screen
		if (FindPubScreen(GUI->window->WScreen,TRUE))
			screen_lock=GUI->window->WScreen;
	}

	// Close child windows?
	if (close_flags&(CLOSE_SCREEN|CLOSE_CHILDREN))
	{
		// Close listers
		if (quit_all) IPC_ListQuit(&GUI->lister_list,&main_ipc,0,TRUE);
		else IPC_ListCommand(&GUI->lister_list,LISTER_CLOSE,0,0,TRUE);

		// Close buttons
		if (quit_all) IPC_ListQuit(&GUI->buttons_list,&main_ipc,0,TRUE);
		else IPC_ListCommand(&GUI->buttons_list,BUTTONS_CLOSE,0,0,TRUE);

		// Close StartMenus
		if (quit_all) IPC_ListQuit(&GUI->startmenu_list,&main_ipc,0,TRUE);
		else IPC_ListCommand(&GUI->startmenu_list,IPC_HIDE,0,0,TRUE);

		// Tell all processes to hide
		IPC_ListCommand(&GUI->process_list,IPC_HIDE,0,0,FALSE);
		IPC_ListCommand(&GUI->group_list,IPC_HIDE,0,0,FALSE);
		IPC_ListCommand(&GUI->function_list,IPC_HIDE,0,0,FALSE);

		// Broadcast notify message
		SendNotifyMsg(DN_OPUS_HIDE,0,0,0,0,0);
	}

	// Close main window?
	if (close_flags&CLOSE_WINDOW)
	{
		// Lock backdrop window pointer
		GetSemaphore(&GUI->backdrop->window_lock,SEMF_EXCLUSIVE,0);

		// Clear "display open" flag
		GUI->flags&=~GUIF_OPEN;

		// Store main window position (if not backdrop)
		if (close_flags&CLOSE_STOREPOS) display_store_pos();

		// Turn off appicon notification
		SetNotifyRequest(GUI->notify_req,0,DN_APP_ICON_LIST);

		// Close window
		if (GUI->window)
		{
			// Remove AppWindow
			if (GUI->appwindow)
			{
				RemoveAppWindow(GUI->appwindow);
				GUI->appwindow=0;
			}

			// Clear menus and clipping
			display_free_menu(GUI->window);
			InstallClipRegion(GUI->window->WLayer,0);

			// Free visual info
			FreeVisualInfo(GUI->visual_info);
			GUI->visual_info=0;

			// Free icon remapping
			backdrop_free_remap(GUI->backdrop,GUI->window);

			// Free lister and button remapping
			RemapIcon(GUI->lister_icon,GUI->window->WScreen,TRUE);
			RemapIcon(GUI->button_icon,GUI->window->WScreen,TRUE);

			// Free rexx icon remapping
			rexx_remap_icons(TRUE);

			// Close window
			CloseWindow(GUI->window);
			GUI->window=0;

			// Free backdrop stuff
			backdrop_free_info(GUI->backdrop);
		}

		// Unlock backdrop window pointer
		FreeSemaphore(&GUI->backdrop->window_lock);

		// Free backdrop patterns
		if (!(close_flags&CLOSE_KEEP_PATTERN))
			display_free_pattern();

		// Free palette if children closing too
		if (close_flags&CLOSE_CHILDREN)
		{
			// Free palette
			free_colour_table();
		}

		// No remap?
		if (!(close_flags&CLOSE_KEEP_REMAP))
		{
			// Free toolbar cache
			FreeToolBarCache(GUI->toolbar);

			// Free toolbar arrow remapping
			FreeRemapImage(GUI->toolbar_arrow_image,&GUI->remap);
			FreeImageRemap(&GUI->remap);
		}
	}
	else
	if (GUI->window) ClearPointer(GUI->window);

	// Public screen to unlock?
	if (screen_lock) UnlockPubScreen(0,screen_lock);

/*
	if (IconBase->lib_Version>=44)
		IconControl(NULL,ICONCTRLA_SetGlobalScreen,NULL,TAG_DONE);
*/
	
	// Close the screen?
	if (close_flags&CLOSE_SCREEN)
	{
		// Free palette
		free_colour_table();

		// Initialise custom pens
		InitCustomPens();

		// Free DrawInfo
		if (GUI->draw_info)
		{
			FreeScreenDrawInfo(GUI->screen_pointer,GUI->draw_info);
			GUI->draw_info=0;
		}

		// Clear screen signal
		if (GUI->screen_signal!=-1)
			SetSignal(0,1<<GUI->screen_signal);

		// Default public screen?
		if (GUI->flags&GUIF_DEFPUBSCR)
			display_set_shanghai(FALSE);

		// Try to close screen
		if (GUI->screen &&
			GUI->screen>(struct Screen *)1 &&
			!(CloseScreen(GUI->screen)))
		{
			TimerHandle *handle;

			// Open timer
			if (handle=AllocTimer(UNIT_VBLANK,0))
			{
				// Wait for a second or so
				StartTimer(handle,1,0);

				// Wait for timer or screen signal
				Wait(	1<<handle->port->mp_SigBit|
						((GUI->screen_signal!=-1)?(1<<GUI->screen_signal):0));

				// Free timer
				FreeTimer(handle);
			}

			// Loop until we can close it
			while (GUI->screen)
			{
				// Try to close screen
				if (CloseScreen(GUI->screen)) break;

				// Put up requester
				super_request_args(
					GUI->screen,
					GetString(&locale,MSG_UNABLE_TO_CLOSE_SCREEN),
					SRF_SCREEN_PARENT|SRF_SIGNAL,
					GUI->screen_signal,
					GetString(&locale,MSG_TRY_AGAIN),0);

				// Clear signals
				SetSignal(0,1<<GUI->screen_signal);
			}
		}

		GUI->screen=0;
		GUI->screen_pointer=0;
	}
}

void display_box_to_rect(struct IBox *box,struct Rectangle *rect)
{
	// Convert a box to a rectangle
	rect->MinX=box->Left;
	rect->MinY=box->Top;
	rect->MaxX=box->Left+box->Width-1;
	rect->MaxY=box->Top+box->Height-1;
}

void display_gui_complete(GUI_Element *gui,struct RastPort *rp)
{
	// Set visible flag
	gui->visible=1;

	// Convert IBox to Rectangle
	display_box_to_rect(&gui->box,&gui->rect);

	// Copy RastPort
	gui->rast=*rp;

	// Set font
	if (gui->font) SetFont(&gui->rast,gui->font);
	else gui->font=gui->rast.Font;

	// Initialise pens and style
	SetAPen(&gui->rast,gui->fg_pen);
	SetBPen(&gui->rast,gui->bg_pen);
	SetDrMd(&gui->rast,JAM2);
}


// Draw border for a GUI element
void display_gui_border(GUI_Element *gui)
{
	struct Rectangle border;

	// Calculate element border
	border.MinX=gui->rect.MinX-1;
	border.MaxX=gui->rect.MaxX+1;
	border.MinY=gui->rect.MinY-1;
	border.MaxY=gui->rect.MaxY+1;

	// Draw box
	do3dbox(&gui->rast,&border,0);
}


// Clear a GUI element
void display_gui_clear(GUI_Element *gui)
{
	SetAPen(&gui->rast,gui->bg_pen);
	lister_clear_title_area(gui);
	SetAPen(&gui->rast,gui->fg_pen);
}


// See if a point is within a GUI element
BOOL point_in_element(GUI_Element *element,USHORT x,USHORT y)
{
	return (BOOL)(x>=element->rect.MinX &&
			x<=element->rect.MaxX &&
			y>=element->rect.MinY &&
			y<=element->rect.MaxY);
}


// Lock all windows on the screen
void display_lock_all(int state)
{
	// Lock all listers
	IPC_ListCommand(&GUI->lister_list,LISTER_BUSY,1,state,0);

	// Lock all buttons
	IPC_ListCommand(&GUI->buttons_list,BUTTONS_BUSY,0,state,0);
}

// Pop the display up
void display_popup(short type)
{
	// Screen open?
	if (GUI->screen)
	{
		// Already active?
		if (IntuitionBase->FirstScreen==GUI->screen &&
			GUI->screen->TopEdge==0 &&
			(!IntuitionBase->ActiveWindow || IntuitionBase->ActiveWindow->WScreen==GUI->screen))
		{
			// Move to the back
			ScreenToBack(GUI->screen);

			// If main window has a parent on the front screen, activate it
			if (GUI->window->Parent && GUI->window->Parent->WScreen==IntuitionBase->FirstScreen)
				ActivateWindow(GUI->window->Parent);

			// Otherwise, activate first window on this screen
			else
			if (IntuitionBase->FirstScreen->FirstWindow)
				ActivateWindow(IntuitionBase->FirstScreen->FirstWindow);
		}

		// Not active
		else
		{
			// Bring screen to front	
			if (IntuitionBase->FirstScreen!=GUI->screen)
				ScreenToFront(GUI->screen);

			// Make main window active
			if (IntuitionBase->ActiveWindow->WScreen!=GUI->screen)
				ActivateWindow(GUI->window);

			// Move screen to top of the display
			if (GUI->screen->TopEdge>0)
				MoveScreen(GUI->screen,0,-GUI->screen->TopEdge);
		}
	}

	// No screen; is window open?
	else
	if (GUI->window)
	{
		// Is window active?
		if (IntuitionBase->ActiveWindow==GUI->window &&
			IntuitionBase->FirstScreen==GUI->screen_pointer &&
			GUI->screen_pointer->LayerInfo.top_layer==GUI->window->RPort->Layer)
		{
			struct Window *window;

			// Activate parent if it has one
			if (window=GUI->window->Parent)
			{
				// Activate it and bring it to the front
				ActivateWindow(window);
				WindowToFront(window);

				// On a different screen to Opus?
				if (window->WScreen!=GUI->window->WScreen)
				{
					// Bring screen to the front
					if (IntuitionBase->FirstScreen!=window->WScreen)
						ScreenToFront(window->WScreen);
				}

				// Otherwise, push DOpus to the back
				else WindowToBack(GUI->window);
			}

			// Push DOpus to the back
			else WindowToBack(GUI->window);
		}

		// Activate it
		else
		{
			// Move its screen to the front
			if (IntuitionBase->FirstScreen!=GUI->screen_pointer)
				ScreenToFront(GUI->screen_pointer);

			// Move window to front and activate it
			WindowToFront(GUI->window);
			ActivateWindow(GUI->window);
		}
	}

	// Window not open
	else
	{
		// Launch reveal script
		RunScript(SCRIPT_REVEAL,0);

		// Open display
		display_open(0);
	}
}


// Store window position
void display_store_pos(void)
{
	// Window open (and not backdrop)?
	if (GUI->window && !(GUI->window->Flags&WFLG_BACKDROP))
	{
		// Is window positioned in "default" position?
		if (GUI->window->LeftEdge==0 &&
			GUI->window->TopEdge==GUI->window->WScreen->BarHeight+1 &&
			GUI->window->Width==GUI->window->WScreen->Width &&
			GUI->window->Height==GUI->window->WScreen->Height-GUI->window->WScreen->BarHeight-1)
		{
			// Set default position
			environment->env->window_pos.Width=(WORD)-1;
			environment->env->window_pos.Height=(WORD)-1;
		}

		// Otherwise, store position
		else environment->env->window_pos=*((struct IBox *)&GUI->window->LeftEdge);
	}
}


// Do hide
void hide_display(void)
{
	struct TagItem tags[2];

	// If pattern thing is open, don't do this
	if (IPC_FindProc(&GUI->process_list,NAME_PATTERNS,0,0))
		return;

	// Launch hide script
	RunScript(SCRIPT_HIDE,0);

	// Kill clock if it's open
	IPC_QuitName(&GUI->process_list,NAME_CLOCK,0);

	// Shut display down
	close_display(CLOSE_ALL|CLOSE_STOREPOS,0);

	// Iconification tags
	tags[0].ti_Tag=DAE_Local;
	tags[0].ti_Data=0;
	tags[1].ti_Tag=TAG_END;

	// Look at hide type
	switch (environment->env->settings.hide_method)
	{
		// Clock
		case HIDE_CLOCK:
			{
				IPCData *ipc;

				// Launch clock
				if ((IPC_Launch(
					&GUI->process_list,
					&ipc,
					NAME_HIDDEN_CLOCK,
					(ULONG)clock_proc,
					STACK_DEFAULT,
					0,(struct Library *)DOSBase)))
				{
					// Tell it to show itself
					IPC_Command(ipc,IPC_SHOW,1,0,0,0);
				}
			}
			break;


		// Icon
		case HIDE_ICON:

			// Get disk object
			if (!(GUI->hide_diskobject=GetCachedDiskObject("dopus5:Icons/AppIcon",0)))
			{
				GUI->hide_diskobject=GetCachedDiskObjectNew("dopus5:DirectoryOpus",1);
			}

			// Got one?
			if (GUI->hide_diskobject)
			{
				// Add AppIcon
				if (GUI->hide_appicon=AddAppIconA(
					0x12345678,0,
					"Opus 5",
					GUI->appmsg_port,
					0,
					GUI->hide_diskobject,tags)) break;

				// Failed, try menu
				FreeCachedDiskObject(GUI->hide_diskobject);
			}

		// Menu
		case HIDE_MENU:

			// Add AppMenuItem
			GUI->hide_appitem=AddAppMenuItemA(
				0x12345678,0,
				"Directory Opus 5",
				GUI->appmsg_port,tags);
			break;
	}
}


// Fix default public screen bit
void display_set_shanghai(BOOL set)
{
	// Want default public screen?
	if (set)
	{
		// Make this the default public screen
		SetDefaultPubScreen(GUI->rexx_port_name);

		// Set shanghai bit
		if (((GUI->old_pub_modes=SetPubScreenModes(SHANGHAI))&~SHANGHAI))
			SetPubScreenModes(GUI->old_pub_modes|SHANGHAI);
	}

	// Don't
	else
	{
		// Reset default public screen
		SetDefaultPubScreen(0);

		// Reset modes
		SetPubScreenModes(GUI->old_pub_modes);
	}
}
