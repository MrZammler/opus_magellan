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

#include "dopuslib.h"

// Status window
static ConfigWindow
	_status_window={
		{POS_CENTER,POS_CENTER,36,3},
		{0,0,8,6}};


// Status objects
static ObjectDef
	_status_objects[]={

		// Display area
		{OD_AREA,
			TEXTPEN,
			{0,0,SIZE_MAXIMUM,SIZE_MAXIMUM},
			{2,2,-2,-2},
			0,
			AREAFLAG_RECESSED|TEXTFLAG_CENTER|AREAFLAG_OPTIM|AREAFLAG_THIN,
			1,
			0},

		{OD_END}};
		

// Open status window
struct Window *__asm __saveds L_OpenStatusWindow(
	register __a0 char *title,
	register __a1 char *text,
	register __a2 struct Screen *screen,
	register __d1 LONG graph,
	register __d0 ULONG flags,
	register __a6 struct MyLibrary *libbase)
{
	struct Window *window;
	NewConfigWindow newwin;
	ObjectList *list;

	// Initialise new window
	newwin.parent=screen;
	newwin.dims=&_status_window;
	newwin.title=(title)?title:"Directory Opus";
	newwin.locale=0;
	newwin.port=0;
	newwin.flags=WINDOW_SCREEN_PARENT|WINDOW_REQ_FILL|flags;
	newwin.font=0;

	// Open new window
	if (!(window=L_OpenConfigWindow(&newwin,libbase)))
		return 0;

	// Add objects
	if (list=L_AddObjectList(window,_status_objects,libbase))
	{
		// Display text
		if (text) L_SetGadgetValue(list,1,(ULONG)text);
	}

	// Make window busy
	L_SetWindowBusy(window);

	return window;
}


// Change status text
void __asm __saveds L_SetStatusText(
	register __a0 struct Window *window,
	register __a1 char *text)
{
	if (window)
	{
		// Update text
		L_SetGadgetValue(OBJLIST(window),1,(ULONG)text);
	}
}


// Update bar graph
void __asm __saveds L_UpdateStatusGraph(
	register __a0 struct Window *window,
	register __a1 char *text,
	register __d0 ULONG total,
	register __d1 ULONG count)
{
}
