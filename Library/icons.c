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
#include <proto/newicon.h>

/*	
	Modified 3 nov 99 gjp to now call puticontags if V44


*/

// Patched PutDiskObject()
BOOL __asm __saveds L_WB_PutDiskObject(
	register __a0 char *name,
	register __a1 struct DiskObject *diskobj)
{
	struct LibData *data;
	struct MyLibrary *libbase;
	BOOL result,magic=0;

	// Get library
	if (!(libbase=(struct MyLibrary *)FindName(&((struct ExecBase *)*((ULONG *)4))->LibList,"dopus5.library")))
		return 0;

	// Get data pointer
	data=(struct LibData *)libbase->ml_UserData;

	// See if icon type is set to inverse magic
	if (diskobj->do_Magic==(USHORT)~WB_DISKMAGIC)
	{
		// Fix it
		diskobj->do_Magic=WB_DISKMAGIC;

		// Set magic flag, which will stop us sending notification
		magic=1;
	}

	// Write icon
	result=L_WriteIcon(name,diskobj,libbase);

	// Succeeded?
	if (result && !magic) icon_notify(data,name,0,0);

	return result;
}


// Patched DeleteDiskObject()
BOOL __asm __saveds L_WB_DeleteDiskObject(register __a0 char *name)
{
	struct LibData *data;
	struct MyLibrary *libbase;
	BOOL result;
	char *full_name;

	// Get library
	if (!(libbase=(struct MyLibrary *)FindName(&((struct ExecBase *)*((ULONG *)4))->LibList,"dopus5.library")))
		return 0;

	// Get data pointer
	data=(struct LibData *)libbase->ml_UserData;

	// Get full name
	full_name=icon_fullname(data,name);

	// Write icon
	result=L_DeleteIcon(name,libbase);

#define DOSBase (data->dos_base)

	// Succeeded?
	if ((result || IoErr()==ERROR_OBJECT_NOT_FOUND) && full_name)
		icon_notify(data,full_name,INF_FULLNAME,1);

#undef DOSBase

	// Free full name buffer
	FreeVec(full_name);
	return result;
}


// Send icon notification
void icon_notify(struct LibData *data,char *name,ULONG flags,short delete)
{
	char *full_name;

	// Given full name?
	if (flags&INF_FULLNAME) full_name=name;

	// Get full name
	else
	if (!(full_name=icon_fullname(data,name)))
		return;

	// Send notify message
	L_SendNotifyMsg(DN_WRITE_ICON,0,delete,FALSE,full_name,0,data->dopus_base);

	// Free buffer
	if (!(flags&INF_FULLNAME)) FreeVec(full_name);
}


// Get icon full name
char *icon_fullname(struct LibData *data,char *name)
{
	char *full_name,*ptr;
	BPTR lock;
	short len;

	// Allocate buffer for full name
	if (!(full_name=AllocVec(300,0))) return 0;

	// Copy name
	strcpy(full_name,name);
	ptr=full_name+strlen(full_name);
	strcat(full_name,".info");

#define DOSBase (data->dos_base)

	// Lock icon
	if (!(lock=Lock(full_name,ACCESS_READ)))
	{
		// Strip .info
		*ptr=0;
		if (lock=Lock(full_name,ACCESS_READ))
		{
			// Add .info back
			strcpy(ptr,".info");
		}
	}

	// Got lock?
	if (lock)
	{
		// Expand path
		L_DevNameFromLock(lock,full_name,256,data->dopus_base);

		// Disk?
		if ((ptr=strchr(full_name,':')) && stricmp(ptr+1,"disk.info")==0)
		{
			// Get real full name
			NameFromLock(lock,full_name,256);

			// Strip after colon
			if (ptr=strchr(full_name,':')) *(ptr+1)=0;
		}

		// Unlock lock
		UnLock(lock);
	}

#undef DOSBase

	// Strip .info from name
	if ((len=strlen(full_name))>5 &&
		stricmp(full_name+len-5,".info")==0)
	{
		// Strip it
		*(full_name+len-5)=0;
	}

	return full_name;
}


// PutDiskObject without notification
BOOL __asm __saveds L_WriteIcon(
	register __a0 char *name,
	register __a1 struct DiskObject *icon,
	register __a6 struct MyLibrary *libbase)
{
	struct LibData *data;
	BOOL result;

	// Get data pointer
	data=(struct LibData *)libbase->ml_UserData;

	// No icon?
	if (!icon) return 0;

#define NewIconBase	(data->new_icon_base)

	// A NewIcon?
	if (L_GetIconType(icon)==ICON_NEWICON && NewIconBase)
	{
		struct NewIconDiskObject *nido;
		struct DiskObject *temp,write_obj;

		// Get NIDO pointer
		nido=(struct NewIconDiskObject *)icon;

		// Save structure DiskObject pointer
		temp=nido->nido_NewDiskObject->ndo_StdObject;

		// Copy DiskObject to write
		CopyMem((char *)icon,(char *)&write_obj,sizeof(struct DiskObject));

		// Get original gadget structure fields
		write_obj.do_Gadget.GadgetRender=temp->do_Gadget.GadgetRender;
		write_obj.do_Gadget.SelectRender=temp->do_Gadget.SelectRender;
		write_obj.do_Gadget.Flags=temp->do_Gadget.Flags;
		write_obj.do_Gadget.Width=temp->do_Gadget.Width;
		write_obj.do_Gadget.Height=temp->do_Gadget.Height;

		// Fix revision
		if ((((ULONG)write_obj.do_Gadget.UserData)&WB_DISKREVISIONMASK)==0)
			*((ULONG *)&write_obj.do_Gadget.UserData)|=WB_DISKREVISION;

		// Clear SpecialInfo field so we won't get stuck in a loop
		write_obj.do_Gadget.SpecialInfo=0;

		// Set new icon pointer
		nido->nido_NewDiskObject->ndo_StdObject=&write_obj;

		// This is heavy magic, which stops us from notifying things twice
		// (PutNewDiskObject calls PutDiskObject..)
		write_obj.do_Magic=(USHORT)~WB_DISKMAGIC;

		// Write the NewIcon
		result=PutNewDiskObject(name,nido->nido_NewDiskObject);

		// Restore the DiskObject pointer in the NewIcon
		nido->nido_NewDiskObject->ndo_StdObject=temp;
	}

	// Write normally
	else
	{
		// Fix revision
		if ((((ULONG)icon->do_Gadget.UserData)&WB_DISKREVISIONMASK)==0)
			*((ULONG *)&icon->do_Gadget.UserData)|=WB_DISKREVISION;


#define IconBase	(data->icon_base)
	
	if	(IconBase->lib_Version>=44)
		result=PutIconTags(name,icon,TAG_DONE);

	else
		// Write the icon
		result=((BOOL __asm (*)
					(register __a0 char *,register __a1 struct DiskObject *,register __a6 struct Library *))
						data->wb_data.old_function[WB_PATCH_PUTDISKOBJECT])
					(name,icon,data->icon_base);
	}

#undef IconBase
#undef NewIconBase

	return result;
}


// DeleteDiskObject without notification
BOOL __asm __saveds L_DeleteIcon(
	register __a0 char *name,
	register __a6 struct MyLibrary *libbase)
{
	struct LibData *data;

	// Get data pointer
	data=(struct LibData *)libbase->ml_UserData;

	// Delete it
	return 
		((BOOL __asm (*)
			(register __a0 char *,register __a6 struct Library *))
				data->wb_data.old_function[WB_PATCH_DELETEDISKOBJECT])
			(name,data->icon_base);
}


// Get icon flags
ULONG __asm __saveds L_GetIconFlags(
	register __a0 struct DiskObject *icon)
{
	ULONG flags;

	// Valid icon?
	if (!icon) return 0;

	// Stored in gadget userdata
	flags=(ULONG)icon->do_Gadget.UserData;

	// Mask off flags we're not interested in
	flags&=ICONF_POSITION_OK|ICONF_ICON_VIEW|ICONF_BORDER_OFF|ICONF_NO_LABEL|ICONF_BORDER_ON;
	return flags;
}

// Get Opus icon position
void __asm __saveds L_GetIconPosition(
	register __a0 struct DiskObject *icon,
	register __a1 short *x,
	register __a2 short *y)
{
	// Clear values
	if (x) *x=0;
	if (y) *y=0;

	// Valid icon?
	if (!icon) return;

	// Get positions
	if (x) *x=((ULONG)icon->do_Gadget.MutualExclude)>>16;
	if (y) *y=((ULONG)icon->do_Gadget.MutualExclude)&0xffff;
}

// Set icon flags
void __asm __saveds L_SetIconFlags(
	register __a0 struct DiskObject *icon,
	register __d0 ULONG flags)
{
	ULONG oldflags;

	// Valid icon?
	if (!icon) return;

	// Get old flags from gadget
	oldflags=(ULONG)icon->do_Gadget.UserData;

	// Clear flags we're interested in
	oldflags&=~(ICONF_POSITION_OK|ICONF_ICON_VIEW|ICONF_BORDER_OFF|ICONF_NO_LABEL|ICONF_BORDER_ON);

	// Or new flags in
	oldflags|=flags;

	// Store back in gadget
	icon->do_Gadget.UserData=(APTR)oldflags;
}

// Set Opus icon position
void __asm __saveds L_SetIconPosition(
	register __a0 struct DiskObject *icon,
	register __d0 short x,
	register __d1 short y)
{
	// Valid icon?
	if (!icon) return;

	// Set position
	*((ULONG *)&icon->do_Gadget.MutualExclude)=(x<<16)|y;
}


// Get the icon type
short __asm __saveds L_GetIconType(register __a0 struct DiskObject *icon)
{
	if (!icon) return 0;

	if (icon->do_Gadget.SpecialInfo==(APTR)icon)
		return ICON_CACHED;
	else
	if (icon->do_Gadget.SpecialInfo==(APTR)(icon+1))
		return ICON_NEWICON;

	return ICON_NORMAL;
}



