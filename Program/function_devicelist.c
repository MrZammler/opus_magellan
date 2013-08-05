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

struct device_data
{
	char				name[80];	// Disk name
	char				full[8];	// Percent full string
	char				free[16];	// Free space string
	char				used[16];	// Used space string
	struct DosList		*dos;		// DOS list pointer
	struct DateStamp	date;		// Date
	short				valid;		// Validating?
	char				fsys[6];	// File system
};

enum
{
	ARG_NEW,
	ARG_FULL,
	ARG_BRIEF
};

// DEVICELIST internal function
DOPUS_FUNC(function_devicelist)
{
	Lister *lister;
	short type;
	struct DosList *doslist,*list_entry;
	static short type_order[3]={LDF_DEVICES,LDF_VOLUMES,LDF_ASSIGNS};
	DirEntry *add_after=0;
	char device_name[80];
	struct List list[3];
	struct Node *node;
	DirBuffer *custom=0;

	// New lister?
	if (instruction->funcargs &&
		instruction->funcargs->FA_Arguments[ARG_NEW] &&
		!(handle->result_flags&FRESULTF_MADE_LISTER))
	{
		// Open new lister
		if (lister=lister_new(0))
		{
			// Set flag for devicelist if appropriate
			if (command->function==FUNC_DEVICELIST)
			{
				// Set device list flag
				lister->lister->lister.flags|=DLSTF_DEVICE_LIST;

				// Which sort of device-list?
				if (instruction->funcargs)
				{
					// Full?
					if (instruction->funcargs->FA_Arguments[ARG_FULL])
						lister->lister->lister.flags|=DLSTF_DEV_FULL;

					// Brief?
					else
					if (instruction->funcargs->FA_Arguments[ARG_BRIEF])
						lister->lister->lister.flags|=DLSTF_DEV_BRIEF;
				}
			}

			// Or cache list
			else lister->lister->lister.flags|=DLSTF_CACHE_LIST;

			// Tell it to open
			IPC_Command(lister->ipc,LISTER_INIT,0,GUI->screen_pointer,0,0);
		}

		// Return, lister will generate its own list
		return 1;
	}

	// Get current lister
	if (!(lister=function_lister_current(&handle->source_paths)))
		return 1;

	// Make window into a "special" buffer
	IPC_Command(
		lister->ipc,
		LISTER_SHOW_SPECIAL_BUFFER,
		0,
		GetString(&locale,(command)?MSG_BUFFER_LIST:MSG_DEVICE_LIST),
		0,
		(struct MsgPort *)-1);

	// Does lister have a handler?
	if (*lister->old_buffer->buf_CustomHandler)
		custom=lister->old_buffer;

	// Set "device list" flag
	lister->flags|=LISTERF_DEVICE_LIST;

	// Clear flag
	lister->cur_buffer->flags&=~DWF_VALID;
	lister->cur_buffer->more_flags&=~(DWF_DEVICE_LIST|DWF_DEV_FULL|DWF_DEV_BRIEF|DWF_CACHE_LIST);

	// Buffer list?
	if (command->function==FUNC_BUFFERLIST)
	{
		DirBuffer *buffer;
		Att_List *list;

		// Mark as a cache list
		lister->cur_buffer->more_flags|=DWF_CACHE_LIST;

		// Create temporary list
		if (list=Att_NewList(0))
		{
			Att_Node *node;
			DirEntry *entry=0;

			// Lock buffer list
			lock_listlock(&GUI->buffer_list,FALSE);

			// Go through buffers
			for (buffer=(DirBuffer *)GUI->buffer_list.list.lh_Head;
				buffer->node.ln_Succ;
				buffer=(DirBuffer *)buffer->node.ln_Succ)
			{
				// Valid directory?
				if (buffer->flags&DWF_VALID)
				{
					char *ptr;

					// Looking for custom buffer?
					if (custom)
					{
						// If this buffer doesn't match the handler, we ignore it
						if (stricmp(custom->buf_CustomHandler,buffer->buf_CustomHandler)!=0)
							continue;

						// Or, if the lister doesn't match the handler, we ignore it
						else
						if (custom->buf_OwnerLister!=buffer->buf_OwnerLister)
							continue;
					}

					// Otherwise, if this is a custom buffer we ignore it
					else
					if (*buffer->buf_CustomHandler) continue;

					// Copy path, strip /
					strcpy(handle->work_buffer,buffer->buf_Path);
					if (*(ptr=handle->work_buffer+strlen(handle->work_buffer)-1)=='/')
						*ptr=0;

					// Add to list
					if (handle->work_buffer[0])
						Att_NewNode(list,handle->work_buffer,(ULONG)buffer,ADDNODE_SORT|ADDNODE_EXCLUSIVE);
				}
			}

			// Unlock buffer list
			unlock_listlock(&GUI->buffer_list);

			// Go through sorted list
			for (node=(Att_Node *)list->list.lh_Head;
				node->node.ln_Succ;
				node=(Att_Node *)node->node.ln_Succ)
			{
				// Add entry
				if (entry=add_file_entry(
					lister->cur_buffer,
					create_file_entry(
						lister->cur_buffer,0,
						FilePart(node->node.ln_Name),
						-1,
						0,0,0,0,
						SUBENTRY_BUFFER,
						node->node.ln_Name,0,
						0),
					entry))
				{
					// set userdata to point to buffer
					entry->de_UserData=(ULONG)node->data;
				}
			}

			// Free temporary list
			Att_RemList(list,0);
		}
	}

	// Device list
	else
	if (doslist=LockDosList(LDF_DEVICES|LDF_VOLUMES|LDF_ASSIGNS|LDF_READ))
	{
		short max_name_width=0,max_dev_width=0,max_full_width=0,max_free_width=0,max_used_width=0,need_vol=0;
		char *dev_format,*asn_format,*dev_val_format;
		short asn_first_x,asn_multi_x;
		BOOL full=0,brief=0;
		short num;

		// Mark as a device list
		lister->cur_buffer->more_flags|=DWF_DEVICE_LIST;

		// Which sort of device-list?
		if (instruction->funcargs)
		{
			// Full?
			if (instruction->funcargs->FA_Arguments[ARG_FULL])
			{
				full=1;
				lister->cur_buffer->more_flags|=DWF_DEV_FULL;
			}

			// Brief?
			else
			if (instruction->funcargs->FA_Arguments[ARG_BRIEF])
			{
				brief=1;
				lister->cur_buffer->more_flags|=DWF_DEV_BRIEF;
			}
		}

		// Brief device list?
		if (brief) num=2;
		else num=3;

		// Go through the types of entries
		for (type=0;type<num;type++)
		{
			// Initialise list
			NewList(&list[type]);

			// Scan list
			list_entry=doslist;
			while (list_entry=NextDosEntry(list_entry,type_order[type]))
			{
				// Valid device?
				if (list_entry->dol_Type==DLT_DIRECTORY || list_entry->dol_Task)
				{
					// Convert name
					BtoCStr(list_entry->dol_Name,device_name,32);
					strcat(device_name,":");

					// Get pathname
					if (full) DevNameFromLock(list_entry->dol_Lock,handle->work_buffer,256);
					else handle->work_buffer[0]=0;

					// Create entry
					if (node=AllocMemH(handle->entry_memory,sizeof(struct Node)+32+strlen(handle->work_buffer)+1))
					{
						node->ln_Type=list_entry->dol_Type;
						strcpy((char *)(node+1),device_name);
						strcpy(((char *)(node+1))+32,handle->work_buffer);
						AddTail(&list[type],node);
					}

					// Multi-path assign?
					if (type_order[type]==LDF_ASSIGNS &&
						list_entry->dol_misc.dol_assign.dol_List &&
						full)
					{
						struct AssignList *assign;

						// Go through assign list
						for (assign=list_entry->dol_misc.dol_assign.dol_List;
							assign;
							assign=assign->al_Next)
						{
							// Get pathname
							DevNameFromLock(assign->al_Lock,handle->work_buffer,256);

							// Create entry
							if (node=AllocMemH(handle->entry_memory,sizeof(struct Node)+strlen(handle->work_buffer)+1))
							{
								node->ln_Type=255;
								strcpy((char *)(node+1),handle->work_buffer);
								AddTail(&list[type],node);
							}
						}
					}
				}
			}
		}

		// Unlock DOS list
		UnLockDosList(LDF_DEVICES|LDF_VOLUMES|LDF_ASSIGNS|LDF_READ);

		// Go through the lists to get maximum name width
		for (type=0;type<num;type++)
		{
			// Go through the list we created
			for (node=list[type].lh_Head;
				node->ln_Succ;
				node=node->ln_Succ)
			{
				// Device or volume?
				if (type==0 || type==1)
				{
					struct InfoData __aligned info;
					struct DevProc *proc;
					struct DosList *dos;
					struct device_data *data;

					// Get device process
					if (!(proc=GetDeviceProc((char *)(node+1),0)))
						return 0;

					// Get current information, check it's valid
					if (!(DoPkt(proc->dvp_Port,ACTION_DISK_INFO,MKBADDR(&info),0,0,0,0)) ||
						!(dos=(struct DosList *)BADDR(info.id_VolumeNode)) ||
						info.id_DiskType==ID_NO_DISK_PRESENT ||
						info.id_DiskType==ID_UNREADABLE_DISK ||
						info.id_DiskType==ID_NOT_REALLY_DOS ||
						info.id_DiskType==ID_KICKSTART_DISK)
					{
						// Free device process and proceed to next entry
						FreeDeviceProc(proc);
						continue;
					}

					// If this is a volume, see if we already have it in the list
					if (type==1)
					{
						struct Node *test;
						struct device_data *data;

						// Go through device list
						for (test=list[0].lh_Head;
							test->ln_Succ;
							test=test->ln_Succ)
						{
							// Get device data
							if (data=(struct device_data *)test->ln_Name)
							{
								// Compare dos list pointer
								if (data->dos==dos) break;
							}
						}

						// Did it exist?
						if (test->ln_Succ)
						{
							// Free device process and proceed to next entry
							FreeDeviceProc(proc);
							continue;
						}
					}

					// Allocate some data
					if (data=AllocMemH(handle->entry_memory,sizeof(struct device_data)))
					{
						short len;

						// Convert name, store doslist pointer
						BtoCStr(dos->dol_Name,data->name,32);
						data->dos=dos;

						// Save date
						data->date=dos->dol_misc.dol_volume.dol_VolumeDate;

						// Is disk validating?
						if (info.id_DiskState==ID_VALIDATING)
						{
							data->valid=1;
						}

						// Nope, it's ok
						else
						{
							ULONG disktype;

							// Get percent used
							if (info.id_NumBlocks==0)
							{
								strcpy(data->full,"100");
							}
							else
							{
								DivideToString(
									data->full,
									info.id_NumBlocksUsed*100,
									info.id_NumBlocks,
									0,
									(environment->env->settings.date_flags&DATE_1000SEP)?GUI->decimal_sep:0);
							}

							// Get space free and used
							BytesToString(
								(info.id_NumBlocks-info.id_NumBlocksUsed)*info.id_BytesPerBlock,
								data->free,1,
								(environment->env->settings.date_flags&DATE_1000SEP)?GUI->decimal_sep:0);
							BytesToString(
								info.id_NumBlocksUsed*info.id_BytesPerBlock,
								data->used,1,
								(environment->env->settings.date_flags&DATE_1000SEP)?GUI->decimal_sep:0);

							// Check widths
							if ((len=lister_get_length(lister,data->full))>max_full_width)
								max_full_width=len;
							if ((len=lister_get_length(lister,data->free))>max_free_width)
								max_free_width=len;
							if ((len=lister_get_length(lister,data->used))>max_used_width)
								max_used_width=len;

							// Get disk type from DOS list if it's set
							disktype=dos->dol_misc.dol_volume.dol_DiskType;

							// If it's not, get it from Info
							if (!disktype) disktype=info.id_DiskType;

							// Get filesystem string
							data->fsys[0]=(char)((disktype>>24)&0xff);
							data->fsys[1]=(char)((disktype>>16)&0xff);
							data->fsys[2]=(char)((disktype>>8)&0xff);

							// Last character might be either a number or a letter
							data->fsys[3]=(char)(disktype&0xff);
							if (data->fsys[3]<10) data->fsys[3]+='0';
							data->fsys[4]=0;
						}

						// Store pointer to data
						node->ln_Name=(char *)data;

						// Check widths
						if ((len=lister_get_length(lister,data->name))>max_name_width)
							max_name_width=len;
						if (type==0 && (len=lister_get_length(lister,(char *)(node+1)))>max_dev_width)
							max_dev_width=len;
						else
						if (type!=0) need_vol=1;
					}

					// Unlock device process
					FreeDeviceProc(proc);
				}

				// Assign
				else
				{
					short len;

					// Check name width
					if (node->ln_Type!=255 &&
						(len=lister_get_length(lister,(char *)(node+1)))>max_name_width)
						max_name_width=len;
				}
			}
		}

		// Get format string pointers
		dev_format=handle->work_buffer+256;
		dev_val_format=handle->work_buffer+384;
		asn_format=handle->work_buffer+512;

		// Need to use volume string in device list?
		if (need_vol)
		{
			// Check maximum device width against volume string
			if (max_dev_width<(type=lister_get_length(lister,GetString(&locale,MSG_VOLUME))))
				max_dev_width=type;
			else
				need_vol=0;
		}

		// Build format strings
		{
			short name_w,dev_w,full_w,free_w,used_w,pad;

			// Calculate column positions
			name_w=max_name_width+(pad=lister_get_length(lister,"   "));
			dev_w=name_w+max_dev_width+((need_vol)?pad:(lister_get_length(lister,"()")+pad));
			full_w=dev_w+max_full_width+lister_get_length(lister,"% , ")+lister_get_length(lister,GetString(&locale,MSG_FULL));
			free_w=full_w+max_free_width+lister_get_length(lister," , ")+lister_get_length(lister,GetString(&locale,MSG_FREE));
			used_w=free_w+max_used_width+pad+lister_get_length(lister,GetString(&locale,MSG_USED));

			// Build format for main device list
			lsprintf(dev_format,
				"%%s\t\b%ld\b%%s\t\b%ld\b%%s%%%% %s,\t\b%ld\b%%s %s,\t\b%ld\b%%s %s\t\b%ld\b[%%s]",
				name_w,
				dev_w,
				GetString(&locale,MSG_FULL),
				full_w,
				GetString(&locale,MSG_FREE),
				free_w,
				GetString(&locale,MSG_USED),
				used_w);

			// Used if a device is being validated
			lsprintf(dev_val_format,
				"%%s\t\b%ld\b%%s\t\b%ld\b%%s",
				name_w,
				dev_w,
				GetString(&locale,MSG_VALIDATING));

			// Used for assign list
			lsprintf(asn_format,
				"%%s\t\b%ld\b%s",
				name_w,
				GetString(&locale,MSG_ASSIGN));

			// Get assign coordinates
			asn_first_x=dev_w;
			asn_multi_x=dev_w-lister_get_length(lister,"+ ");
		}

		// Go through the lists again to build device list
		for (type=0;type<num;type++)
		{
			DirEntry *last_entry=0;

			// Go through the list we created
			for (node=list[type].lh_Head;
				node->ln_Succ;
				node=node->ln_Succ)
			{
				char *comment=0;
				struct DateStamp *date=0;

				// Device?
				if (type==0 || type==1)
				{
					struct device_data *data;

					// Valid data entry?
					if ((data=(struct device_data *)node->ln_Name))
					{
						char buf[120];

						// Build device name
						if (type==0)
						{
							lsprintf(buf,"(%s",(char *)(node+1));
							buf[strlen(buf)-1]=')';
						}
						else strcpy(buf,GetString(&locale,MSG_VOLUME));

						// Valid?
						if (!data->valid)
						{
							// Build display string
							lsprintf(handle->work_buffer,
								dev_format,
								data->name,
								buf,
								data->full,
								data->free,
								data->used,
								data->fsys);
						}

						// Validating
						else
						{
							// Build display string
							lsprintf(handle->work_buffer,
								dev_val_format,
								data->name,
								buf);
						}

						// Use device name as comment
						comment=data->name;

						// Get date pointer
						date=&data->date;
					}

					// No valid entry, continue to next
					else continue;
				}

				// Assign
				else
				{
					// Multi-directory assign?
					if (node->ln_Type==255)
					{
						lsprintf(handle->work_buffer,"\t\b%ld\b+ %s",asn_multi_x,(char *)(node+1));
						if (comment=AllocMemH(handle->entry_memory,strlen((char *)(node+1))+1))
							strcpy(comment,(char *)(node+1));
					}

					// Build display string
					else
					{
						lsprintf(handle->work_buffer,
							asn_format,
							(char *)(node+1));
						if (full)
						{
							lsprintf(handle->work_buffer+strlen(handle->work_buffer),"\t\b%ld\b%s",asn_first_x,((char *)(node+1))+32);
						}
					}
				}

				// Create an entry for this node
				last_entry=add_file_entry(
					lister->cur_buffer,
					create_file_entry(
						lister->cur_buffer,0,
						(char *)(node+1),
						(node->ln_Type==DLT_DIRECTORY || node->ln_Type==255)?DLT_DIRECTORY:DLT_DEVICE,
						ENTRY_DEVICE,
						date,
						comment,
						0,
						(node->ln_Type==255)?SUBENTRY_BUFFER:0,
						handle->work_buffer,0,
						0),
					(node->ln_Type==255 && last_entry)?last_entry:add_after);
			}

			// Find entry at the end of the list
			add_after=(DirEntry *)lister->cur_buffer->entry_list.mlh_TailPred;
			if (!add_after->de_Node.dn_Pred) add_after=0;
		}
	}

	// Get icons
	if (lister->flags&LISTERF_VIEW_ICONS)
		lister_get_icons(handle,lister,0,GETICONSF_CLEAN|GETICONSF_NO_REFRESH);

	return 1;
}
