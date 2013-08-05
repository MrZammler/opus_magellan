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

// Add path 
PathNode *function_add_path(FunctionHandle *handle,PathList *list,Lister *lister,char *path)
{
	PathNode *node;

	// Allocate node
	if (node=AllocMemH(handle->memory,sizeof(PathNode)))
	{
		// Copy path
		if (path) strcpy(node->path_buf,path);

		// Store lister pointer
		node->lister=lister;

		// Add to path list
		AddHead((struct List *)list,(struct Node *)node);

		// Make this the current path
		list->current=node;

		// Set appropriate flag
		if (list==&handle->source_paths)
			handle->func_flags|=FUNCF_GOT_SOURCE;
		else handle->func_flags|=FUNCF_GOT_DEST;
	}

	return node;
}


// Build source path
void function_build_source(
	FunctionHandle *handle,
	FunctionEntry *entry,
	char *buffer)
{
	// Is entry a device?
	if (entry->type>0 && entry->name[strlen(entry->name)-1]==':')
	{
		// This is the new source path
		strcpy(handle->source_path,entry->name);
	}

	// Build source path
	strcpy(buffer,handle->source_path);
	AddPart(buffer,entry->name,256);
}


// Build destination path
void function_build_dest(
	FunctionHandle *handle,
	FunctionEntry *entry,
	char *buffer)
{
	strcpy(buffer,handle->dest_path);
	AddPart(buffer,entry->name,256);
}


// Get the current path
PathNode *function_path_current(PathList *list)
{
	// Is there a current one?
	if (list->current &&
		list->current->node.mln_Succ)
	{
		// Make sure the path is right
		if (list->current->lister && !(list->current->flags&LISTNF_CHANGED))
			list->current->path=list->current->lister->cur_buffer->buf_Path;
		else list->current->path=list->current->path_buf;

		// Return current path node
		return list->current;
	}
	return 0;
}


// Get the lister associated with the current path
Lister *function_lister_current(PathList *list)
{
	// Is there a current one?
	if (list->current &&
		list->current->node.mln_Succ)
	{
		return list->current->lister;
	}
	return 0;
}


// Get the next path to use
PathNode *function_path_next(PathList *list)
{
	PathNode *node=0;

	// While there's a valid path
	while (list->current && list->current->node.mln_Succ)
	{
		// Is this lister valid?
		if (!(list->current->flags&LISTNF_INVALID))
		{
			node=list->current;
			break;
		}

		// Get next
		list->current=(PathNode *)list->current->node.mln_Succ;
	}

	// If no path found, reset to first in the list
	if (!node)
	{
		if (!(IsListEmpty((struct List *)&list->list)))
			list->current=(PathNode *)list->list.mlh_Head;
		else list->current=0;
	}

	// Check current path
	function_path_current(list);

	return node;
}


// Finished with a lister; move on to the next
void function_path_end(FunctionHandle *handle,PathList *list,int cleanup)
{
	// Valid lister?
	if (list->current && list->current->node.mln_Succ && list->current->lister)
	{
		// Call cleanup for this list
		if (cleanup==1)
		{
			// Do cleanup for lister
			function_cleanup(handle,list->current,1);
		}

		// Or is this lister now invalid?
		else
		if (cleanup==-1)
		{
			list->current->flags|=LISTNF_INVALID;
		}
	}

	// Loop until we get a valid path or reach the end of the list
	while (list->current)
	{
		// Is the current one valid?
		if (list->current->node.mln_Succ)
		{
			// Get next lister
			list->current=(PathNode *)list->current->node.mln_Succ;

			// Is this lister ok?
			if (list->current->node.mln_Succ &&
				!(list->current->flags&LISTNF_INVALID))
				return;
		}

		// Otherwise, no more paths
		else list->current=0;
	}
}

// See if a path is valid
BOOL function_valid_path(PathNode *path)
{
	// Is path node valid?
	if (!path) return 0;

	// Valid path?
	if (path->path && path->path[0]) return 1;

	// Lister pointer?
	if (path->lister)
	{
		// Get path from lister
		path->path=path->lister->cur_buffer->buf_Path;
		return 1;
	}

	return 0;
}

