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

// ICONINFO internal function
DOPUS_FUNC(function_iconinfo)
{
	PathNode *path;
	FunctionEntry *entry;
	Att_List *list;
	Att_Node *node;
	struct Library *ModuleBase;
	short ret;

	// Create source list
	if (!(list=Att_NewList(0))) return 0;

	// Open show module
	if (!(ModuleBase=OpenModule("icon.module")))
	{
		Att_RemList(list,0);
		return 0;
	}

	// Get current path
	if (path=function_path_current(&handle->source_paths))

		// Tell this lister to update it's datestamp at the end
		path->flags|=LISTNF_UPDATE_STAMP;

	// Build list of sources
	while (entry=function_get_entry(handle))
	{
		// Build full name
		function_build_source(handle,entry,handle->work_buffer);

		// Add entry to list
		Att_NewNode(list,handle->work_buffer,(ULONG)entry,0);

		// Get next entry
		function_end_entry(handle,entry,1);
	}

	// Do IconInfo
	ret=
		Module_Entry(
			(struct List *)list,
			GUI->screen_pointer,
			handle->ipc,
			&main_ipc,
			0,
			(environment->env->desktop_flags&DESKTOPF_NO_REMAP)?1:0);

	// Go through list of files
	for (node=(Att_Node *)list->list.lh_Head;
		node->node.ln_Succ;
		node=(Att_Node *)node->node.ln_Succ)
	{
		// Was this icon saved?
		if (node->node.ln_Type==2)
		{
			Lister *lister;

			// Do we have a lister?
			if (lister=function_lister_current(&handle->source_paths))
			{
				// Get entry pointer
				entry=(FunctionEntry *)node->data;

				// Reload file and icon
				function_filechange_reloadfile(handle,handle->source_path,entry->name,0);
				function_filechange_reloadfile(handle,handle->source_path,entry->name,FFLF_ICON);
			}
		}
	}

	// Free file list
	Att_RemList(list,0);

	// Close show module
	CloseLibrary(ModuleBase);

	return ret;
}
