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

void lister_get_device_icons(Lister *,BOOL);

// Get icons for display in a lister
void lister_get_icons(FunctionHandle *handle,Lister *lister,char *add_name,short flags)
{
	BackdropObject *object;
	DirBuffer *buffer;
	DirEntry *entry=0;
	char name[256];
	BPTR dir,lock;
	short iter=0;
	long count=0;
	Att_List *icon_list;
	Att_Node *icon_node=0;
	BackdropInfo *info=lister->backdrop_info;
	BOOL shown=0;

	// Initialise positions
	info->last_x_pos=0;
	info->last_y_pos=6;
	info->flags|=BDIF_LAST_POS_OK;

	// Device or cache list?
	if (lister->cur_buffer->more_flags&(DWF_DEVICE_LIST|DWF_CACHE_LIST))
	{
		// Get icons for device/cache list
		lister_get_device_icons(lister,(flags&GETICONSF_CLEAN));
		return;
	}

	// CD to directory
	if (!(lock=Lock(lister->cur_buffer->buf_Path,ACCESS_READ)))
		return;
	dir=CurrentDir(lock);

	// Lock lister buffer
	buffer=lister->cur_buffer;
	if (!add_name) buffer_lock(buffer,0);

	// Create a list for icons
	icon_list=Att_NewList(0);

	// If not adding a specific file, get first entry
	if (!add_name &&
		!((entry=(DirEntry *)buffer->entry_list.mlh_Head)->de_Node.dn_Succ))
	{
		// Nothing in main list, look in reject list (if showing all)
		if (lister->flags&LISTERF_SHOW_ALL)
		{
			entry=(DirEntry *)buffer->reject_list.mlh_Head;
			iter=1;
		}
	}

	// Go through entries
	if (add_name || (entry && entry->de_Node.dn_Succ))
	do
	{
		char *ptr;
		BOOL ok=1;

		// Check for abort
		if (function_check_abort(handle)) break;

		// Name pointer
		ptr=(add_name)?add_name:entry->de_Node.dn_Name;

		// Skip .info
		if (stricmp(ptr,".info")==0)
			ok=0;

		// Name supplied?
		else
		if (add_name)
		{
			// Copy name
			strcpy(name,ptr);
		}

		// If showing all, only get if NOT an icon
		else
		if ((lister->flags&LISTERF_SHOW_ALL) && !icon_node)
		{
			// If it's an icon, skip this
			if (isicon(ptr))
			{
				// Add to icon list for later
				Att_NewNode(icon_list,0,(ULONG)entry,0);
				ok=0;
			}

			// Copy name
			else strcpy(name,ptr);
		}

		// Otherwise, only get if it IS an icon
		else
		{
			char *iptr;

			// Copy name
			strcpy(name,ptr);

			// Not an icon?
			if (!(iptr=isicon(name))) ok=0;

			// Strip .info
			else *iptr=0;
		}

		// Ok to add?
		if (ok)
		{
			// Lock icon list
			lock_listlock(&info->objects,0);

			// Look for object
			if (object=(BackdropObject *)FindNameI(&info->objects.list,name))
			{
				// If found, mark as ok
				object->flags|=BDOF_OK;
			}

			// Unlock icons
			unlock_listlock(&info->objects);

			// Don't already have it?
			if (!object)
			{
				// Lock icon list for writing
				lock_listlock(&info->objects,1);

				// Create icon
				if (object=backdrop_leftout_new(info,name,lister->cur_buffer->buf_Path,0))
				{
					BPTR file;
					struct FileInfoBlock __aligned fib;
					DirEntry *fent;

					// Try and find entry in buffer
					if (!add_name &&
						((fent=find_entry(&buffer->entry_list,name,0,0)) ||
						 (fent=find_entry(&buffer->reject_list,name,0,0))))
					{
						// Fill out date and size
						object->date=fent->de_Date;
						object->size=fent->de_Size;

						// Link?
						if (fent->de_Flags&ENTF_LINK) object->flags|=BDOF_LINK_ICON;
					}

					// Try to lock file
					else
					if (file=Lock(name,ACCESS_READ))
					{
						// Get info
						Examine(file,&fib);
						UnLock(file);

						// Fill out date and size
						object->date=fib.fib_Date;
						object->size=fib.fib_Size;

						// Link?
						if (fib.fib_DirEntryType==ST_SOFTLINK ||
							fib.fib_DirEntryType==ST_LINKDIR ||
							fib.fib_DirEntryType==ST_LINKFILE) object->flags|=BDOF_LINK_ICON;
					}

					// Set appropriate flag
					object->flags|=BDOF_ICON_VIEW|BDOF_OK;

					// Get object icon
					backdrop_get_icon(info,object,0);

					// If no icon, or it's a disk, remove it again
					if (!object->icon || object->icon->do_Type==WBDISK)
					{
						backdrop_remove_object(info,object);
						object=0;
					}

					// Otherwise, increment count
					else ++count;
				}

				// Unlock list
				unlock_listlock(&info->objects);

				// Tell lister to show icon
				if (object && !(object->flags&BDOF_NO_POSITION))
				{
					if (handle)
						IPC_Command(lister->ipc,LISTER_SHOW_ICON,0,object,0,REPLY_NO_PORT);
					else
						lister_show_icon(lister,object);
				}
			}
		}

		// If adding a specific name, break out
		if (add_name) break;

		// Get next entry
		if (icon_node)
		{
			icon_node=(Att_Node *)icon_node->node.ln_Succ;
			if (!icon_node->node.ln_Succ) entry=(DirEntry *)icon_node;
			else
			{
				entry=(DirEntry *)icon_node->data;
			}
		}
		else entry=(DirEntry *)entry->de_Node.dn_Succ;

		// Is this the last in the list?
		if (!entry->de_Node.dn_Succ)
		{
			// Increment iteration count
			++iter;

			// First iteration; jump to reject list
			if (iter==1)
			{
				// Reject list empty (or not in show all mode)?
				if (IsListEmpty((struct List *)&buffer->reject_list) ||
					!(lister->flags&LISTERF_SHOW_ALL))
					iter=2;

				// Get first reject
				else entry=(DirEntry *)buffer->reject_list.mlh_Head;
			}

			// Second iteration; jump to icon list
			if (iter==2)
			{
				if (!(IsListEmpty((struct List *)icon_list)))
				{
					icon_node=(Att_Node *)icon_list->list.lh_Head;
					entry=(DirEntry *)icon_node->data;
				}
			}
		}
	} while (entry->de_Node.dn_Succ);

	// Free temporary icon list
	Att_RemList(icon_list,0);

	// Check for redundant icons?
	if (!add_name)
	{
		short rem;

		// Check icons for OK flag
		rem=backdrop_check_icons_ok(info);

		// Use value if nothing added to trigger recalc
		if (!count) count=-rem;
	}

	// Unlock buffer
	if (!add_name) buffer_unlock(buffer);

	// Fix selection count
	else backdrop_fix_count(info,0);

	// Clear position flag
	info->flags&=~BDIF_LAST_POS_OK;

	// Any new objects?
	if (count>0)
	{
		// Arrange unpositioned icons
		if (!backdrop_cleanup(info,BSORT_NORM,CLEANUPF_CHECK_POS))
		{
			// Icons weren't shown by the cleanup, so show now
			backdrop_show_objects(info,BDSF_RECALC);
			shown=1;
		}
	}

	// Recalculate after a remove?
	else
	if (count<0)
	{
		// Recalculate virtual size
		backdrop_calc_virtual(info);
	}

	// Need to show?
	if (flags&GETICONSF_SHOW && !shown)
	{
		// Show icons
		backdrop_show_objects(info,0);
	}

	// Restore current dir
	UnLock(CurrentDir(dir));
}


// Get icons for device/cache list
void lister_get_device_icons(Lister *lister,BOOL clean)
{
	DirBuffer *buffer;
	DirEntry *entry;
	struct List *search;
	BackdropObject *object;
	BackdropInfo *info=lister->backdrop_info;
	BOOL new=1;

	// Lock lister buffer
	buffer_lock((buffer=lister->cur_buffer),0);

	// Go through buffer
	for (entry=(DirEntry *)buffer->entry_list.mlh_Head;
		entry->de_Node.dn_Succ;
		entry=(DirEntry *)entry->de_Node.dn_Succ)
	{
		// Cache list?
		if (buffer->more_flags&DWF_CACHE_LIST)
		{
			char *name=0,*ptr;

			// Get filename pointer
			if (ptr=(char *)GetTagData(DE_DisplayString,0,entry->de_Tags))
				name=FilePart(ptr);

			// If no filename, use device name
			if (!name || !*name) name=ptr;

			// Lock icon list
			lock_listlock(&lister->backdrop_info->objects,0);

			// See if directory is in list
			search=&info->objects.list;
			while (object=(BackdropObject *)FindName(search,name))
			{
				// Match cache name
				if (object->device_name &&
					strcmp(object->device_name,ptr)==0)
				{
					object->flags|=BDOF_OK;
					new=0;
					break;
				}

				// Keep searching
				search=(struct List *)object;
			}

			// Unlock icon list
			unlock_listlock(&lister->backdrop_info->objects);

			// Found?
			if (object) continue;

			// Lock icon list for writing
			lock_listlock(&lister->backdrop_info->objects,1);

			// Allocate a new object
			if (object=backdrop_new_object(
				info,
				name,
				ptr,
				BDO_DISK))
			{
				// Mark object as ok
				object->flags=BDOF_OK|BDOF_CACHE|BDOF_NO_POSITION;

				// Add to backdrop list
				AddTail(&info->objects.list,&object->node);

				// Get icon
				backdrop_get_icon(info,object,GETICON_CD|GETICON_SAVE_POS|GETICON_DEFDIR);
			}

			// Unlock icon list
			unlock_listlock(&lister->backdrop_info->objects);
		}

		// Disk?
		else
		if (entry->de_Size==DLT_DEVICE)
		{
			char *ptr;

			// Lock icon list
			lock_listlock(&lister->backdrop_info->objects,0);

			// Get device name
			ptr=(char *)GetTagData(DE_Comment,0,entry->de_Tags);

			// See if disk is in list
			search=&info->objects.list;
			while (object=(BackdropObject *)FindName(search,ptr))
			{
				// Compare dates
				if (CompareDates(&object->date,&entry->de_Date)==0)
				{
					// Mark as ok
					object->flags|=BDOF_OK;
					new=0;
					break;
				}

				// Keep searching
				search=(struct List *)object;
			}

			// Unlock icon list
			unlock_listlock(&lister->backdrop_info->objects);

			// If object was in the list, continue
			if (object) continue;

			// Lock icon list for writing
			lock_listlock(&lister->backdrop_info->objects,1);

			// Allocate a new object
			if (object=backdrop_new_object(
				info,
				ptr,
				entry->de_Node.dn_Name,
				BDO_DISK))
			{
				// Get object date
				object->date=entry->de_Date;

				// Mark object as ok
				object->flags=BDOF_OK|BDOF_NO_POSITION;

				// Add to backdrop list
				AddTail(&info->objects.list,&object->node);

				// Get icon
				backdrop_get_icon(info,object,GETICON_CD|GETICON_SAVE_POS);
			}

			// Unlock icon list
			unlock_listlock(&lister->backdrop_info->objects);
		}

		// Assign
		else
		if (entry->de_SubType!=SUBENTRY_BUFFER)
		{
			// Lock icon list
			lock_listlock(&lister->backdrop_info->objects,0);

			// See if assign is in list
			if (object=(BackdropObject *)FindName(&info->objects.list,entry->de_Node.dn_Name))
			{
				object->flags|=BDOF_OK;
				new=0;
			}

			// Unlock icon list
			unlock_listlock(&lister->backdrop_info->objects);

			// Already in list?
			if (object) continue;

			// Lock icon list for writing
			lock_listlock(&lister->backdrop_info->objects,1);

			// Allocate a new object
			if (object=backdrop_new_object(
				info,
				entry->de_Node.dn_Name,
				entry->de_Node.dn_Name,
				BDO_DISK))
			{
				// Get object date
				object->date=entry->de_Date;

				// Mark object as ok
				object->flags=BDOF_OK|BDOF_ASSIGN|BDOF_NO_POSITION;

				// Add to backdrop list
				AddTail(&info->objects.list,&object->node);

				// Get icon
				backdrop_get_icon(info,object,GETICON_CD|GETICON_SAVE_POS|GETICON_DEFDIR);
			}

			// Unlock icon list
			unlock_listlock(&lister->backdrop_info->objects);
		}
	}

	// Unlock buffer
	buffer_unlock(buffer);

	// Check icons for OK flag
	backdrop_check_icons_ok(info);

	// Arrange backdrop objects
	if (new || clean) backdrop_cleanup(lister->backdrop_info,BSORT_NORM,0);
	else backdrop_arrange_objects(info);

	// Show backdrop objects (including newly arranged icons)
	backdrop_show_objects(lister->backdrop_info,BDSF_CLEAR|BDSF_RECALC);
}


// Check icons, remove any not marked OK
short backdrop_check_icons_ok(BackdropInfo *info)
{
	BackdropObject *object;
	short count=0;

	// Lock list
	lock_listlock(&info->objects,1);

	// Go through icon list
	for (object=(BackdropObject *)info->objects.list.lh_Head;
		object->node.ln_Succ;)
	{
		BackdropObject *next=(BackdropObject *)object->node.ln_Succ;

		// Anything not marked as ok gets removed
		if (!(object->flags&BDOF_OK))
		{
			// Erase object
			backdrop_erase_icon(info,object,0);

			// Free object
			backdrop_remove_object(info,object);

			// Increment count
			++count;
		}

		// Otherwise clear ok flag
		else object->flags&=~BDOF_OK;

		// Get next object
		object=next;
	}

	// Unlock list
	unlock_listlock(&info->objects);

	return count;
}


// Show a specific icon
void lister_show_icon(Lister *lister,BackdropObject *object)
{
	BackdropObject *test;

	// Is the window open, or not in icon mode?
	if (!lister_valid_window(lister) || !(lister->flags&LISTERF_VIEW_ICONS)) return;

	// Lock icon list
	lock_listlock(&lister->backdrop_info->objects,0);

	// Look for icon in list
	for (test=(BackdropObject *)lister->backdrop_info->objects.list.lh_Head;
		test->node.ln_Succ;
		test=(BackdropObject *)test->node.ln_Succ)
	{
		// Found?
		if (test==object)
		{
			// Show the icon
			backdrop_render_object(lister->backdrop_info,object,BRENDERF_CLIP);
			break;
		}
	}

	// Unlock list
	unlock_listlock(&lister->backdrop_info->objects);
}
