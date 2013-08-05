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

// Valid toolbar?
BOOL lister_valid_toolbar(Lister *lister)
{
	return (BOOL)
		(lister->toolbar &&
		!(IsListEmpty(&lister->toolbar->buttons->buttons)) &&
		lister->toolbar->button_height &&
		(!(lister->flags&LISTERF_VIEW_ICONS) || lister->flags&LISTERF_ICON_ACTION));
}


// Show a lister toolbar
void lister_show_toolbar(Lister *lister)
{
	Cfg_Button *button;
	struct Rectangle rect;
	short x,button_end,count,num,col,off,width,arrow=0;
	struct TagItem draw_tags[4];
	ToolBarInfo *toolbar;

	// Do we have a toolbar?
	if (!lister_valid_toolbar(lister)) return;

	// If lister is in icon action mode, clear toolbar area
	if (lister->flags&LISTERF_ICON_ACTION)
	{
		SetAPen(&lister->toolbar_area.rast,0);
		RectFill(
			&lister->toolbar_area.rast,
			lister->toolbar_area.rect.MinX,
			lister->toolbar_area.rect.MinY,
			lister->toolbar_area.rect.MaxX,
			lister->toolbar_area.rect.MaxY);
	}

	// Cache toolbar pointer
	toolbar=lister->toolbar;

	// Initialise position
	button_end=lister->toolbar_area.rect.MinX-1;
	lister->toolbar_arrow_left=-1;

	// Initialise draw tags
	draw_tags[0].ti_Tag=IM_Rectangle;
	draw_tags[0].ti_Data=(ULONG)&rect;
	draw_tags[1].ti_Tag=IM_ClipBoundary;
	draw_tags[1].ti_Data=(toolbar->buttons->window.flags&BTNWF_BORDERLESS)?0:2;
	draw_tags[2].ti_Tag=IM_NoIconRemap;
	draw_tags[2].ti_Data=(environment->env->desktop_flags&DESKTOPF_NO_REMAP)?TRUE:FALSE;
	draw_tags[3].ti_Tag=TAG_DONE;

	// Get offset of first button, and width we can show
	off=toolbar->button_array[lister->toolbar_offset].MinX;
	width=RECTWIDTH(&lister->toolbar_area.rect);

	// If offset is not 0, we need an arrow from the outset
	if (lister->toolbar_offset>0)
	{
		// Get width of toolbar arrow
		arrow=RECTWIDTH(&toolbar->button_array[toolbar->count]);

		// Subtract from available width and start again
		width-=arrow;
	}

	// Find the number of buttons we can show
	for (x=lister->toolbar_offset;x<toolbar->count;x++)
	{
		// Is this button too big for the display?
		if (toolbar->button_array[x].MaxX-off>=width)
		{
			// Already got arrow width?
			if (arrow) break;

			// Get width of toolbar arrow
			arrow=RECTWIDTH(&toolbar->button_array[toolbar->count]);

			// Subtract from available width and start again
			width-=arrow;
			x=lister->toolbar_offset-1;
		}
	}

	// Are we showing the last lot of buttons?
	if (x==toolbar->count)
	{
		// Offset from first?
		if (arrow)
		{
			// Get offset
			off=toolbar->button_array[toolbar->count-1].MaxX+1-width;
			
			// Start from end and go backwards
			for (x=toolbar->count-1;x>=0;x--)
			{
				// Too big for display?
				if (toolbar->button_array[x].MinX<off)
					break;
			}

			// Get new offset
			lister->toolbar_offset=x+1;
			x=toolbar->count;
		}
	}

	// Get number of buttons we can show
	num=x-lister->toolbar_offset;

	// Bounds-check offset
	if (lister->toolbar_offset+num>=toolbar->cols)
		lister->toolbar_offset=toolbar->cols-num;
	if (lister->toolbar_offset<0) lister->toolbar_offset=0;

	// Get first displayed button
	for (button=(Cfg_Button *)toolbar->buttons->buttons.lh_Head,col=0;
		button->node.ln_Succ && col<lister->toolbar_offset;
		button=(Cfg_Button *)button->node.ln_Succ,col++);

	// Go through buttons on toolbar
	for (count=0,lister->toolbar_show=0;;count++,button=(Cfg_Button *)button->node.ln_Succ)
	{
		// Toolbar arrow?
		if (arrow && count==num)
		{
			// Get position
			rect.MinX=button_end+1;
			rect.MinY=lister->toolbar_area.rect.MinY;
			rect.MaxX=rect.MinX+toolbar->button_array[toolbar->count].MaxX;
			rect.MaxY=rect.MinY+toolbar->button_array[toolbar->count].MaxY;
		}

		// Normal button
		else
		{
			// Get button rectangle
			rect=toolbar->button_array[col+count];

			// Adjust for toolbar offset
			rect.MinX-=toolbar->button_array[col].MinX;
			rect.MaxX-=toolbar->button_array[col].MinX;

			// Map to toolbar area
			rect.MinX+=lister->toolbar_area.rect.MinX;
			rect.MinY+=lister->toolbar_area.rect.MinY;
			rect.MaxX+=lister->toolbar_area.rect.MinX;
			rect.MaxY+=lister->toolbar_area.rect.MinY;
		}

		// Is button not going to fit, or end of buttons?
		if (!arrow &&
			(rect.MaxX>lister->toolbar_area.rect.MaxX || !button || !button->node.ln_Succ))
			break;

		// Remember end of button
		button_end=rect.MaxX;

		// Draw button if arrow
		if (arrow && count==num)
		{
			// Draw button border
			if (!(toolbar->buttons->window.flags&BTNWF_BORDERLESS))
				do3dbox(&lister->toolbar_area.rast,&rect,0);

			// Draw button image
			RenderImage(
				&lister->toolbar_area.rast,
				GUI->toolbar_arrow_image,
				0,0,
				draw_tags);

			// Save position
			lister->toolbar_arrow_left=rect.MinX;
			lister->toolbar_arrow_right=rect.MaxX;
			break;
		}

		// Otherwise, increment show count
		else ++lister->toolbar_show;
	}

	// If we have a button cache, draw buttons from that
	if (toolbar->cache)
	{
		// Get size to draw
		width=toolbar->button_array[lister->toolbar_offset+num-1].MaxX-
			  toolbar->button_array[lister->toolbar_offset].MinX+1;

		// Draw buttons
		ClipBlit(
			&toolbar->rp,
			toolbar->button_array[lister->toolbar_offset].MinX,0,
			&lister->toolbar_area.rast,
			lister->toolbar_area.rect.MinX,
			lister->toolbar_area.rect.MinY,
			width,toolbar->height,
			0xc0);
	}

	// Draw to end of button area
	if (button_end<lister->toolbar_area.rect.MaxX)
	{
		unsigned short lister_stipple[2];

		// Initialise pattern
		lister_stipple[0]=0xaaaa;
		lister_stipple[1]=0x5555;

		// Draw shine line
		SetAPen(&lister->toolbar_area.rast,GUI->draw_info->dri_Pens[SHINEPEN]);
		Move(&lister->toolbar_area.rast,button_end+1,lister->toolbar_area.rect.MaxY-1);
		Draw(&lister->toolbar_area.rast,lister->toolbar_area.rect.MaxX,lister->toolbar_area.rect.MaxY-1);

		// Draw shadow line below it
		SetAPen(&lister->toolbar_area.rast,GUI->draw_info->dri_Pens[SHADOWPEN]);
		Move(&lister->toolbar_area.rast,button_end+1,lister->toolbar_area.rect.MaxY);
		Draw(&lister->toolbar_area.rast,lister->toolbar_area.rect.MaxX,lister->toolbar_area.rect.MaxY);

		// Stipple-fill the background above
		SetAfPt(&lister->toolbar_area.rast,lister_stipple,1);
		RectFill(&lister->toolbar_area.rast,
			button_end+1,
			lister->toolbar_area.rect.MinY,
			lister->toolbar_area.rect.MaxX,
			lister->toolbar_area.rect.MaxY-2);
		SetAfPt(&lister->toolbar_area.rast,0,0);
	}
}


// Handle a click on the toolbar
void lister_toolbar_click(
	Lister *lister,
	short x,
	short y,
	unsigned short code,
	unsigned short qual)
{
	short but;
	struct Rectangle rect;
	Cfg_Button *button;
	BOOL arrow_flag=0;
	struct TagItem draw_tags[6];
	ToolBarInfo *toolbar;

	// Cache toolbar pointer
	if (!(toolbar=lister->toolbar)) return;

	// Get the button we're over
	if (lister->tool_sel!=-1)
	{
		// Get button values
		but=lister->tool_sel;
		button=lister->tool_sel_button;

		// Over arrow?
		if (but==-2) arrow_flag=1;
	}

	// Calculate button
	else
	if ((button=lister_get_toolbar_button(lister,x,y,&but))==(Cfg_Button *)-1)
	{
		// Got arrow button
		arrow_flag=1;
		button=0;
	}

	else
	if (!button) return;

	// Arrow?
	if (arrow_flag)
	{
		// Get absolute button rectangle
		rect=toolbar->button_array[toolbar->count];

		// Offset by position
		rect.MinX+=lister->toolbar_arrow_left;
		rect.MinY+=lister->toolbar_area.rect.MinY;
		rect.MaxX+=lister->toolbar_arrow_left;
		rect.MaxY+=lister->toolbar_area.rect.MinY;
	}

	// Normal button
	else
	{
		// Get absolute button rectangle
		rect=toolbar->button_array[but];

		// Get relative to start of displayed region
		rect.MinX-=toolbar->button_array[lister->toolbar_offset].MinX;
		rect.MaxX-=toolbar->button_array[lister->toolbar_offset].MinX;

		// Get relative to display area
		rect.MinX+=lister->toolbar_area.rect.MinX;
		rect.MinY+=lister->toolbar_area.rect.MinY;
		rect.MaxX+=lister->toolbar_area.rect.MinX;
		rect.MaxY+=lister->toolbar_area.rect.MinY;
	}

	// Is button outside of display?
	if (rect.MaxX>lister->toolbar_area.rect.MaxX) return;

	// Draw tags in case we need to redraw the button
	draw_tags[0].ti_Tag=IM_Rectangle;
	draw_tags[0].ti_Data=(ULONG)&rect;
	draw_tags[1].ti_Tag=IM_State;
	draw_tags[1].ti_Data=0;
	draw_tags[2].ti_Tag=IM_Erase;
	draw_tags[2].ti_Data=0;
	draw_tags[3].ti_Tag=IM_ClipBoundary;
	draw_tags[3].ti_Data=(toolbar->buttons->window.flags&BTNWF_BORDERLESS)?0:2;
	draw_tags[4].ti_Tag=IM_NoIconRemap;
	draw_tags[4].ti_Data=(environment->env->desktop_flags&DESKTOPF_NO_REMAP)?TRUE:FALSE;
	draw_tags[5].ti_Tag=TAG_DONE;

	// Button release?
	if (code==(lister->tool_button|IECODE_UP_PREFIX))
	{
		BOOL high=1;

		// Redraw image if necessary
		if (lister->tool_type!=FTYPE_LEFT_BUTTON ||
			(lister->toolbar_image[1] && lister->tool_selected))
		{
			Cfg_ButtonFunction *func;
			APTR image=0;

			// Arrow?
			if (arrow_flag) image=GUI->toolbar_arrow_image;

			// Get left button function
			else
			if (func=(Cfg_ButtonFunction *)
				FindFunctionType((struct List *)&button->function_list,FTYPE_LEFT_BUTTON))
				image=func->image;

			// Got image?
			if (image)
			{
				// Draw button image
				RenderImage(
					&lister->toolbar_area.rast,
					image,
					0,0,
					draw_tags);

				// Draw border
				if (!lister->tool_selected &&
					!(toolbar->buttons->window.flags&BTNWF_BORDERLESS))
					do3dbox(&lister->toolbar_area.rast,&rect,0);
				high=0;
			}
		}

		// Released over button?
		if (lister->tool_selected)
		{
			// Draw border
			if (!(toolbar->buttons->window.flags&BTNWF_BORDERLESS))
				do3dbox(&lister->toolbar_area.rast,&rect,0);
			else
			if (high)
			{
				ClipBlit(
					&lister->toolbar_area.rast,0,0,
					&lister->toolbar_area.rast,rect.MinX,rect.MinY,
					RECTWIDTH(&rect),RECTHEIGHT(&rect),
					0x50);
			}

			// Unless it was aborted
			if (qual!=0xffff)
			{
				// Was it the arrow button?
				if (arrow_flag)
				{
					// Are we at the end already?
					if (lister->toolbar_offset+lister->toolbar_show>=toolbar->cols)
					{
						// Wrap to 0
						lister->toolbar_offset=0;
					}

					// Otherwise
					else
					{
						// Bump offset
						lister->toolbar_offset+=lister->toolbar_show;
					}

					// Redraw the toolbar
					lister_show_toolbar(lister);
				}

				// Launch function
				else
				{
					Cfg_Function *function;

					// Get the function to run
					if (function=button_valid(lister->tool_sel_button,lister->tool_type))
					{
						// Launch function
						function_launch_quick(
							FUNCTION_RUN_FUNCTION,
							function,
							lister);
					}	
				}
			}
		}

		// Clear flags
		lister->tool_sel=-1;
		lister->tool_selected=0;
		lister->tool_button=0;

		// Clear mousemove flag
		lister->window->Flags&=~WFLG_REPORTMOUSE;
	}

	// Another button release?
	else
	if (code&IECODE_UP_PREFIX) return;

	// Otherwise, have we moved off the button?
	else
	if (x<rect.MinX ||
		 x>rect.MaxX ||
		 y<rect.MinY ||
		 y>rect.MaxY)
	{
		BOOL high=1;

		if (lister->tool_selected)
		{
			// Alternate image?
			if (lister->toolbar_image[1])
			{
				// Draw original image
				RenderImage(
					&lister->toolbar_area.rast,
					lister->toolbar_image[0],
					0,0,
					draw_tags);
				high=0;
			}

			// No longer over button
			lister->tool_selected=0;
			if (!(toolbar->buttons->window.flags&BTNWF_BORDERLESS))
				do3dbox(&lister->toolbar_area.rast,&rect,0);
			else
			if (high)
			{
				ClipBlit(
					&lister->toolbar_area.rast,0,0,
					&lister->toolbar_area.rast,rect.MinX,rect.MinY,
					RECTWIDTH(&rect),RECTHEIGHT(&rect),
					0x50);
			}
		}
	}

	// Or moved onto it?
	else
	if (!lister->tool_selected)
	{
		BOOL high=1;

		// Just pressed?
		if (lister->tool_sel==-1)
		{
			// Get function type
			if (!arrow_flag)
			{
				// Default is invalid
				lister->tool_type=-1;

				// Edit function if alt is down
				if (qual&IEQUAL_ANYALT && !(qual&IEQUALIFIER_LCOMMAND))
				{
					// Can only edit global toolbar
					if (lister->toolbar!=GUI->toolbar)
					{
						// Flash error
						DisplayBeep(lister->window->WScreen);
					}

					// Launch process to configure toolbar
					else lister_toolbar_edit(but);
				}

				// Otherwise
				else
				{
					if ((code==MIDDLEDOWN ||
						(code==SELECTDOWN && (qual&IEQUAL_ANYSHIFT))) &&
						button_valid(button,FTYPE_MID_BUTTON))
						lister->tool_type=FTYPE_MID_BUTTON;
					else
					if (code==SELECTDOWN && button_valid(button,FTYPE_LEFT_BUTTON))
						lister->tool_type=FTYPE_LEFT_BUTTON;
					else
					if (code==MENUDOWN && button_valid(button,FTYPE_RIGHT_BUTTON))
						lister->tool_type=FTYPE_RIGHT_BUTTON;
				}
			}
			else lister->tool_type=FTYPE_LEFT_BUTTON;

			// Valid button press?
			if (arrow_flag || lister->tool_type!=-1)
			{
				Cfg_ButtonFunction *func;
				APTR image=0;
	
				// Save button selection
				lister->tool_sel=but;
				lister->tool_row=0;
				lister->tool_sel_button=button;
				lister->tool_button=code;

				// Set mousemove flag
				lister->window->Flags|=WFLG_REPORTMOUSE;

				// Clear image pointers
				lister->toolbar_image[0]=0;
				lister->toolbar_image[1]=0;

				// Arrow?
				if (arrow_flag) image=GUI->toolbar_arrow_image;

				// Valid function?
				else
				if (func=(Cfg_ButtonFunction *)FindFunctionType((struct List *)&button->function_list,lister->tool_type))
					image=func->image;

				// Valid image?
				if (image)
				{
					struct TagItem tags[2];

					// See if image has an alternate image
					tags[0].ti_Tag=IM_State;
					tags[1].ti_Tag=TAG_DONE;
					GetImageAttrs(image,tags);

					// Left button press?
					if (lister->tool_type==FTYPE_LEFT_BUTTON)
					{
						// Only draw image if there's an alternate one
						if (tags[0].ti_Data)
						{
							lister->toolbar_image[0]=image;
							lister->toolbar_image[1]=image;
						}
					}

					// Otherwise
					else
					{
						// Draw initial image
						lister->toolbar_image[0]=image;

						// Valid selected image?
						if (tags[0].ti_Data) lister->toolbar_image[1]=image;
					}
				}

				// Image to draw?
				if (lister->toolbar_image[0])
				{
					// Selected image?
					if (lister->toolbar_image[1])
					{
						draw_tags[1].ti_Data=1;
						high=0;
					}

					// Draw button image
					RenderImage(
						&lister->toolbar_area.rast,
						lister->toolbar_image[0],
						0,0,
						draw_tags);
				}
			}
			else return;
		}

		// Already pressed
		else
		{
			// Alternate image?
			if (lister->toolbar_image[1])
			{
				// Draw it
				draw_tags[1].ti_Data=1;
				high=0;
				RenderImage(
					&lister->toolbar_area.rast,
					lister->toolbar_image[1],
					0,0,
					draw_tags);
			}
		}

		// Highlight button
		lister->tool_selected=1;
		if (!(toolbar->buttons->window.flags&BTNWF_BORDERLESS))
			do3dbox(&lister->toolbar_area.rast,&rect,1);
		else
		if (high)
		{
			ClipBlit(
				&lister->toolbar_area.rast,0,0,
				&lister->toolbar_area.rast,rect.MinX,rect.MinY,
				RECTWIDTH(&rect),RECTHEIGHT(&rect),
				0x50);
		}
	}
}


// Get toolbar button from coordinate
Cfg_Button *lister_get_toolbar_button(Lister *lister,short x,short y,short *but)
{
	Cfg_Button *button;
	short num;

	// Got toolbar arrow?
	if (lister->toolbar_arrow_left>-1)
	{
		// Check position
		if (x>=lister->toolbar_arrow_left &&
			x<=lister->toolbar_arrow_right)
		{
			// Set flag, return special value
			if (but) *but=-2;
			return (Cfg_Button *)-1;
		}
	}

	// Shift position to toolbar-relative
	x-=lister->toolbar_area.rect.MinX;
	x+=lister->toolbar->button_array[lister->toolbar_offset].MinX;

	// Find button
	for (button=(Cfg_Button *)lister->toolbar->buttons->buttons.lh_Head,num=0;
		button->node.ln_Succ;
		button=(Cfg_Button *)button->node.ln_Succ,num++)
	{
		// Match coordinates
		if (x>=lister->toolbar->button_array[num].MinX &&
			x<=lister->toolbar->button_array[num].MaxX)
		{
			// Store number
			if (but) *but=num;

			// Return button
			return button;
		}
	}

	// Not found
	return 0;
}


// Attach a new toolbar to a lister
void lister_new_toolbar(Lister *lister,char *name,ToolBarInfo *bank)
{
	BOOL old_toolbar;
	short old_height=0;

	// Store pathname
	if (name && name[0])
	{
		BPTR lock;

		// Does it exist?
		if (lock=Lock(name,ACCESS_READ))
		{
			// Yes, store name verbatim
			UnLock(lock);
			strcpy(lister->toolbar_path,name);
		}

		// No, assume it's in the buttons drawer
		else
		{
			lsprintf(lister->toolbar_path,"dopus5:buttons/%s",name);
		}
	}
	else
	if (!bank) lister->toolbar_path[0]=0;

	// Did we have an old toolbar?
	old_toolbar=lister_valid_toolbar(lister);
	if (lister->toolbar) old_height=lister->toolbar->height;

	// Free existing custom toolbar
	FreeToolBar(lister->toolbar_alloc);
	lister->toolbar_alloc=0;

	// Given a toolbar?
	if (bank)
		lister->toolbar=bank;

	// Want the global toolbar?
	else
	if (stricmp(lister->toolbar_path,environment->toolbar_path)==0 && GUI->toolbar)
		lister->toolbar=GUI->toolbar;

	// Need to load toolbar
	else
	{
		// Try to get new toolbar
		if (lister->toolbar_alloc=OpenToolBar(0,name))
			GetToolBarCache(lister->toolbar_alloc,TRUE);

		// Use as new toolbar
		lister->toolbar=lister->toolbar_alloc;
	}

	// Reset offset
	lister->toolbar_offset=0;

	// Is window open?
	if (lister_valid_window(lister))
	{
		// Has toolbar height changed? (so we need to completely redraw)
		if (old_toolbar!=lister_valid_toolbar(lister) ||
			(lister->toolbar && lister->toolbar->height!=old_height))
		{
			// Complete (well, almost) refresh
			lister_init_display(lister);
			lister_refresh(lister,LREFRESH_FULL|LREFRESH_FULL_ICON);
		}

		// No! We only need to redraw the toolbar
		else
		if (lister_valid_toolbar(lister))
		{
			// Clear area
			display_gui_clear(&lister->toolbar_area);

			// Show toolbar
			lister_show_toolbar(lister);

			// Update size limits
			lister_set_limits(lister);
		}
	}
}


// Build lister tool menu
void lister_build_menu(Lister *lister)
{
	// Get menu lock
	GetSemaphore(&GUI->lister_menu_lock,SEMF_SHARED,0);

	// Build list
	PopUpFreeHandle(lister->lister_tool_menu);
	lister->lister_tool_menu=popup_from_bank(GUI->lister_menu,0);

	// Release menu lock
	FreeSemaphore(&GUI->lister_menu_lock);
}


// Edit lister toolbar
void lister_toolbar_edit(short which)
{
	IPCData *ipc;
	Cfg_ButtonBank *bank=0;
	Buttons *buttons=0;
	BOOL ok=1;

	// Lock buttons list
	lock_listlock(&GUI->buttons_list,FALSE);

	// Go through button banks
	for (ipc=(IPCData *)GUI->buttons_list.list.lh_Head;
		ipc->node.mln_Succ;
		ipc=(IPCData *)ipc->node.mln_Succ)
	{
		// Get buttons pointer
		buttons=IPCDATA(ipc);

		// Toolbar buttons?
		if (buttons->flags&BUTTONF_TOOLBAR)
		{
			ok=0;
			break;
		}
	}

	// Unlock buttons list
	unlock_listlock(&GUI->buttons_list);

	// If a "toolbar" bank is already visible, don't open it again
	if (ok)
	{
		// Copy toolbar bank
		if (GUI->toolbar) bank=CopyButtonBank(GUI->toolbar->buttons);

		// Open toolbar as a button bank
		if (!(buttons=buttons_new(environment->toolbar_path,bank,0,0,BUTTONF_FAIL|BUTTONF_TOOLBAR)))
		{
			// Create new bank
			if (!(buttons=buttons_new(0,0,0,1,0)))
			{
				DisplayBeep(GUI->screen_pointer);
				return;
			}
		}

		// Set flag in bank
		buttons->bank->window.flags|=BTNWF_TOOLBAR;

		// Open bank
		if (IPC_Command(buttons->ipc,BUTTONS_OPEN,0,GUI->screen_pointer,0,REPLY_NO_PORT)==IPC_ABORT)
		{
			// Failed to open
			return;
		}
	}

	// Edit this bank
	buttons_edit_bank(buttons,which,0,0,0,TRUE);
}
