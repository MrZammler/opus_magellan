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

static __chip unsigned short
	command_arrow[7]={
		0x3c00,0x3c00,0x3c00,0xff00,0x7e00,0x3c00,0x1800},
	parent_arrow[6]={
		0x0e00,0x3800,0xe000,0xe000,0x3800,0x0e00};

// Display disk name and size
// Called from the LISTER PROCESS
void lister_show_name(Lister *lister)
{
	DirBuffer *buffer;

#ifdef DEBUG
	check_call("lister_show_name",lister);
#endif

	// Invalid lister?
	if (!lister) return;

	// Get buffer
	buffer=(DirBuffer *)lister->cur_buffer;

	// Custom title?
	if (buffer->buf_CustomTitle[0])
		strcpy(lister->title,buffer->buf_CustomTitle);

	// If invalid path
	else
	if (!(buffer->flags&DWF_VALID))
	{
		// Save status?
		if (buffer->flags&DWF_SAVE_STATUS)
			strcpy(lister->title,buffer->last_status);

		// Device list?
		else
		if (buffer->more_flags&DWF_DEVICE_LIST)
			strcpy(lister->title,GetString(&locale,MSG_DEVICE_LIST));

		// Cache list?
		else
		if (buffer->more_flags&DWF_CACHE_LIST)
			strcpy(lister->title,GetString(&locale,MSG_BUFFER_LIST));

		// Invalid
		else lister->title[0]=0;
	}

	// Otherwise, build title string
	else
	{
		short count,pos;

		// Clear save status flag
		buffer->flags&=~DWF_SAVE_STATUS;

		// Copy disk name in
		strcpy(lister->title,buffer->buf_VolumeLabel);

		// Find out directory level (by number of slashes)
		for (pos=0,count=0;buffer->buf_ExpandedPath[pos] && count<2;pos++)
			if (buffer->buf_ExpandedPath[pos]=='/') ++count;

		// Sub-directory?
		if (buffer->buf_ExpandedPath[0] && buffer->buf_ExpandedPath[strlen(buffer->buf_ExpandedPath)-1]!=':')
		{
			strcat(lister->title,(count>1)?":..":":");
			strcat(lister->title,buffer->buf_ObjectName);
		}

		// Displaying free space?
		if (buffer->buf_TotalDiskSpace>0 && !(lister->more_flags&LISTERF_TITLEBARRED))
		{
			char space_buf[50],*ptr;

			// If read-only, display in parentheses
			if (buffer->flags&DWF_READONLY)
			{
				space_buf[0]='(';
				ptr=space_buf+1;
			}
			else ptr=space_buf;

			// Convert to kilobytes
			BytesToString(
				buffer->buf_FreeDiskSpace,
				ptr,
				1,
				(environment->env->settings.date_flags&DATE_1000SEP)?GUI->decimal_sep:0);

			// Add free string
			strcat(space_buf," ");
			strcat(space_buf,GetString(&locale,MSG_FREE));

			// Space used
			strcat(space_buf,", ");

			// Convert in-use to kilobytes
			BytesToString(
				buffer->buf_TotalDiskSpace-buffer->buf_FreeDiskSpace,
				space_buf+strlen(space_buf),
				1,
				(environment->env->settings.date_flags&DATE_1000SEP)?GUI->decimal_sep:0);

			// Add free string
			strcat(space_buf," ");
			strcat(space_buf,GetString(&locale,MSG_USED));

			// Add comma
			strcat(space_buf,", ");

			// Full?
			if (buffer->buf_FreeDiskSpace<10 || buffer->buf_TotalDiskSpace<1000)
				strcat(space_buf,"100");

			// Calculate percentage
			else
			DivideToString(
				space_buf+strlen(space_buf),
				(buffer->buf_TotalDiskSpace/10)-(buffer->buf_FreeDiskSpace/10),
				buffer->buf_TotalDiskSpace/1000,
				0,
				(environment->env->settings.date_flags&DATE_1000SEP)?GUI->decimal_sep:0);

			// Add percentage string
			strcat(space_buf,"% ");
			strcat(space_buf,GetString(&locale,MSG_FULL));

			// End ) if read-only
			if (buffer->flags&DWF_READONLY)
				strcat(space_buf,") ");

			// Tack on to end of title
			strcat(lister->title,",  ");
			strcat(lister->title,space_buf);

			// Got gauge gadget?
			if (lister->gauge_gadget)
			{
				// Update gauge gadget
				SetGadgetAttrs(
					lister->gauge_gadget,
					lister->window,
					0,
					DGG_Total,buffer->buf_TotalDiskSpace,
					DGG_Free,buffer->buf_TotalDiskSpace-buffer->buf_FreeDiskSpace,
					TAG_END);
			}
		}
	}

	// Set window title
	lister_set_title(lister);
}


// Show lister status
// Called from the LISTER PROCESS
void lister_show_status(Lister *lister)
{
	char text[12];
	struct TextExtent extent;
	int len,lock=0;
#ifdef DEBUG
	check_call("lister_show_status",lister);
#endif

	// Showing icons?
	if (lister->flags&LISTERF_VIEW_ICONS && !(lister->flags&LISTERF_ICON_ACTION))
		return;

	// Is this a locked source or destination?
	if (lister->flags&LISTERF_SOURCEDEST_LOCK &&
		(lister->flags&LISTERF_SOURCE || lister->flags&LISTERF_DEST)) lock=1;

	// Show command area
	lister_title_pens(&lister->command_area.rast,lister,1);
	lister_clear_title_area(&lister->command_area);
	SetAPen(&lister->command_area.rast,lister->command_area.rast.BgPen);
	SetDrMd(&lister->command_area.rast,JAM1);
	BltTemplate(
		(char *)command_arrow,0,2,
		&lister->command_area.rast,
		lister->command_area.rect.MinX+(lister->command_area.box.Width-8)/2,
		lister->command_area.rect.MinY+(lister->command_area.box.Height-7)/2,
		8,7);

	// Show parent area
	lister_title_pens(&lister->parent_area.rast,lister,1);
	lister_clear_title_area(&lister->parent_area);
	SetAPen(&lister->parent_area.rast,lister->parent_area.rast.BgPen);
	SetDrMd(&lister->parent_area.rast,JAM1);
	BltTemplate(
		(char *)parent_arrow,0,2,
		&lister->parent_area.rast,
		lister->parent_area.rect.MinX+(lister->parent_area.box.Width-7)/2,
		lister->parent_area.rect.MinY+(lister->parent_area.box.Height-6)/2,
		7,6);

	// Update status area
	if (lister->status_area.rect.MinX<lister->status_area.rect.MaxX)
	{
		lister_title_pens(&lister->status_area.rast,lister,!lock);
		lister_clear_title_area(&lister->status_area);
	}

	// Get text for status.. busy?
	if (lister->flags&LISTERF_BUSY) strcpy(text,GetString(&locale,MSG_LISTER_STATUS_BUSY));

	// Source
	else
	if (lister->flags&LISTERF_SOURCE) strcpy(text,GetString(&locale,MSG_LISTER_STATUS_SOURCE));

	// Destination
	else
	if (lister->flags&LISTERF_DEST) strcpy(text,GetString(&locale,MSG_LISTER_STATUS_DEST));

	// Other
	else strcpy(text,GetString(&locale,MSG_LISTER_STATUS_OFF));

	// Locked?
	if (lock) strcat(text,"!");

	// Set pens
	lister_title_pens(&lister->status_area.rast,lister,lock);

	// Calculate text extent
	len=TextFit(
		&lister->status_area.rast,
		text,strlen(text),
		&extent,
		0,1,
		lister->status_area.box.Width,
		lister->status_area.box.Height);

	// Display text
	if (len>0)
	{
		Move(&lister->status_area.rast,
			lister->status_area.box.Left+
				(lister->status_area.box.Width-extent.te_Width)/2,
			lister->status_area.box.Top+
				lister->status_area.rast.Font->tf_Baseline+
				(lister->status_area.box.Height-extent.te_Height)/2);
		Text(&lister->status_area.rast,text,len);
	}

	// Update selection info
	select_show_info(lister,1);
}


// Set lister title pens depending on status
void lister_title_pens(struct RastPort *rp,Lister *lister,int type)
{
	short fg,bg;

	// Lister busy?
	if (lister->flags&(LISTERF_BUSY|LISTERF_BUSY_VISUAL)==(LISTERF_BUSY|LISTERF_BUSY_VISUAL))
	{
		fg=GUI->draw_info->dri_Pens[TEXTPEN];
		bg=GUI->draw_info->dri_Pens[BACKGROUNDPEN];
	}

	// Source window
	else
	if (lister->flags&LISTERF_SOURCE ||
		(lister->flags&LISTERF_STORED_SOURCE &&
		lister->flags&LISTERF_BUSY &&
		!(lister->flags&LISTERF_BUSY_VISUAL)))
	{
		// Get pens from configuration
		fg=environment->env->source_col[0];
		bg=environment->env->source_col[1];

		// Fix pens
		if (fg>=4 && fg<252) fg=GUI->pens[fg-4];
		if (bg>=4 && bg<252) bg=GUI->pens[bg-4];
	}

	// Destination window
	else
	if (lister->flags&LISTERF_DEST ||
		(lister->flags&LISTERF_STORED_DEST &&
		lister->flags&LISTERF_BUSY &&
		!(lister->flags&LISTERF_BUSY_VISUAL)))
	{
		// Get pens from configuration
		fg=environment->env->dest_col[0];
		bg=environment->env->dest_col[1];

		// Fix pens
		if (fg>=4 && fg<252) fg=GUI->pens[fg-4];
		if (bg>=4 && bg<252) bg=GUI->pens[bg-4];
	}

	// Off
	else
	{
		fg=GUI->draw_info->dri_Pens[TEXTPEN];
		bg=GUI->draw_info->dri_Pens[BACKGROUNDPEN];
	}

	if (type)
	{
		SetAPen(rp,bg);
		SetBPen(rp,fg);
	}
	else
	{
		SetAPen(rp,fg);
		SetBPen(rp,bg);
	}
}


// Show lister status
void lister_status(Lister *lister,char *text)
{
	if (!text) select_show_info(lister,1);
	else
	{
		// Display title
		stccpy(lister->title,text,79);
		lister_set_title(lister);

		// Copy to buffer
		strcpy(lister->cur_buffer->last_status,lister->title);
		lister->cur_buffer->flags|=DWF_SAVE_STATUS;
	}
}


// Show selection information
// Called from LISTER PROCESS
void select_show_info(Lister *lister,BOOL dodef)
{
#define TITLE_SIZE	99
	char title_buffer[TITLE_SIZE+1];
	int title_len;
	struct TextExtent extent;
	DirBuffer *buffer;

#ifdef DEBUG
	check_call("select_show_info",lister);
#endif

	// Get buffer pointer
	if (!lister || !lister_valid_window(lister)) return;
	buffer=lister->cur_buffer;

	// Text/icon action mode?
	if (!(lister->flags&LISTERF_VIEW_ICONS) || lister->flags&LISTERF_ICON_ACTION)
	{
		// Device list?
		if (buffer->more_flags&DWF_DEVICE_LIST)
			strcpy(title_buffer,GetString(&locale,MSG_DEVICE_LIST));

		// Cache list
		else
		if (buffer->more_flags&DWF_CACHE_LIST)
			strcpy(title_buffer,GetString(&locale,MSG_BUFFER_LIST));

		// Custom header?
		else
		if (buffer->buf_CustomHeader[0])
		{
			// Special?
			if (buffer->buf_CustomHeader[0]=='-' &&
				buffer->buf_CustomHeader[1]==0) title_buffer[0]=0;

			// Get string
			else strcpy(title_buffer,buffer->buf_CustomHeader);
		}

		// Otherwise
		else
		{
			// Is list invalid?
			if (!(buffer->flags&DWF_VALID) || buffer->flags&DWF_READING)
				title_buffer[0]=0;

			// Valid
			else
			{
				short pos=0;
				char *ptr,*format;
				short which;

				// Icon action mode?
				if (lister->flags&LISTERF_ICON_ACTION) which=1;
				else which=0;

				// Get formatting string
				format=(environment->env->settings.date_flags&DATE_1000SEP && GUI->flags&GUIF_LOCALE_OK)?
					"%lU":"%ld";

				// Go through status text
				for (ptr=environment->env->status_text;*ptr && pos<TITLE_SIZE;ptr++)
				{
					short esc=0;
					char buf[32];

					// Clear buffer
					buf[0]=0;

					// Escape command?
					if (*ptr=='%')
					{
						// Literal percent?
						if (*(ptr+1)=='%')
						{
							title_buffer[pos++]='%';
							esc=1;
						}

						// Directories?
						else
						if (*(ptr+1)=='d')
						{
							// Selected?
							if (*(ptr+2)=='s')
							{
								lsprintf(buf,format,buffer->buf_SelectedDirs[which]);
								esc=2;
							}

							// Total?
							else
							if (*(ptr+2)=='t')
							{
								lsprintf(buf,format,buffer->buf_TotalDirs[which]);
								esc=2;
							}
						}

						// Files?
						else
						if (*(ptr+1)=='f')
						{
							// Selected?
							if (*(ptr+2)=='s')
							{
								lsprintf(buf,format,buffer->buf_SelectedFiles[which]);
								esc=2;
							}

							// Total?
							else
							if (*(ptr+2)=='t')
							{
								lsprintf(buf,format,buffer->buf_TotalFiles[which]);
								esc=2;
							}
						}

						// Bytes?
						else
						if (*(ptr+1)=='b')
						{
							// Selected?
							if (*(ptr+2)=='s')
							{
								lsprintf(buf,format,buffer->buf_SelectedBytes[which]);
								esc=2;
							}

							// Total?
							else
							if (*(ptr+2)=='t')
							{
								lsprintf(buf,format,buffer->buf_TotalBytes[which]);
								esc=2;
							}
						}

						// Hidden?
						else
						if (*(ptr+1)=='h')
						{
							// Get hidden string
							for (esc=0;;esc++)
							{
								// Copy to buffer
								if (esc==31 || (buf[esc]=*(ptr+2+esc))=='%' || !buf[esc])
								{
									buf[esc]=0;
									esc+=2;
									break;
								}
							}

							// Do we actually want the hidden string?
							if (IsListEmpty((struct List *)&buffer->reject_list))
								buf[0]=0;

							// Or is the string empty?
							else
							if (!buf[0])
							{
								// Get count of hidden entries
								lsprintf(buf,format,Att_NodeCount((Att_List *)&buffer->reject_list));
							}
						}

						// Got a string to add?
						if (buf[0])
						{
							short len;

							// Add on
							title_buffer[pos]=0;
							len=strlen(buf);
							if (pos+len>TITLE_SIZE)
							{
								len=TITLE_SIZE-pos;
								buf[len]=0;
							}
							strcat(title_buffer,buf);
							pos=strlen(title_buffer);
						}
					}

					// Skip characters?		
					if (esc) ptr+=esc;

					// Otherwise, add literal character
					else title_buffer[pos++]=*ptr;
				}

				// Null-terminate buffer
				title_buffer[pos]=0;
			}
		}

		// Get amount of text that will fit
		title_len=TextFit(
			&lister->name_area.rast,
			title_buffer,
			strlen(title_buffer),
			&extent,
			0,
			1,
			lister->name_area.box.Width-4,
			lister->name_area.rast.Font->tf_YSize);

		// Set pens
		lister_title_pens(&lister->name_area.rast,lister,0);

		// Display information
		Move(&lister->name_area.rast,
			lister->name_area.box.Left+2,
			lister->name_area.box.Top+lister->name_area.rast.Font->tf_Baseline);
		Text(&lister->name_area.rast,title_buffer,title_len);

		// Clear beginning of box
		SetAPen(&lister->name_area.rast,lister->name_area.rast.BgPen);
		RectFill(&lister->name_area.rast,
				lister->name_area.rect.MinX,
				lister->name_area.rect.MinY,
				lister->name_area.rect.MinX+1,
				lister->name_area.rect.MaxY);

		// Clear to end of box
		if (lister->name_area.box.Left+extent.te_Width+2<=lister->name_area.rect.MaxX)
		{
			RectFill(&lister->name_area.rast,
				lister->name_area.box.Left+extent.te_Width+2,
				lister->name_area.rect.MinY,
				lister->name_area.rect.MaxX,
				lister->name_area.rect.MaxY);
		}
	}

	// Update title bar?
	if (lister->flags&LISTERF_SHOW_TITLE && dodef)
	{
		// Refresh title bar
		lister_refresh_name(lister);
		lister->flags&=~LISTERF_SHOW_TITLE;
	}
}


// Clear an area
void lister_clear_title_area(GUI_Element *area)
{
	RectFill(
		&area->rast,
		area->rect.MinX,
		area->rect.MinY,
		area->rect.MaxX,
		area->rect.MaxY);
}


// Update title
void lister_set_title(Lister *lister)
{
	// Window open?
	if (!lister_valid_window(lister)) return;

	// Is window refreshing?
	if (lister->window->Flags&WFLG_WINDOWREFRESH)
	{
		// Defer title update
		lister->flags|=LISTERF_TITLE_DEFERRED;
		return;
	}

	// Refresh title
	SetWindowTitles(lister->window,lister->title,(char *)-1);
	lister->flags&=~LISTERF_TITLE_DEFERRED;
}


// Show title
void lister_show_title(Lister *lister,BOOL show)
{
	DirBuffer *buffer;
	short item,start,end,len,x,num;
	struct RastPort *rp;

	// Get buffer pointer
	buffer=lister->cur_buffer;

	// Special buffer has no title
	if (buffer==lister->special_buffer)
		return;

	// Cache rastport
	rp=&lister->title_area.rast;

	// Clear box pointers
	for (item=0;item<16;item++)
	{
		lister->title_boxes[item].tb_Left=-1;
		lister->title_boxes[item].tb_Right=-1;
		lister->title_boxes[item].tb_Item=-1;
		lister->title_boxes[item].tb_ItemNum=item;
	}

	// If buffer is empty, just clear area
	if (IsListEmpty((struct List *)&buffer->entry_list))
	{
		// Clear area
		if (show) lister_clear_title(lister);
		return;
	}

	// Start at left edge of display
	x=lister->title_area.rect.MinX;

	// Key selector?
	if (lister->flags&LISTERF_KEY_SELECTION)
	{
		struct Rectangle rect;

		// Offset rectangle
		rect.MinX=x;
		rect.MinY=lister->title_area.rect.MinY;
		rect.MaxX=x+KEY_SEL_OFFSET-1;
		rect.MaxY=lister->title_area.rect.MaxY;

		// Clip to left-edge
		if (rect.MaxX>lister->title_area.rect.MaxX)
			rect.MaxX=lister->title_area.rect.MaxX;

		// Actually doing drawing?
		if (show)
		{
			// Draw rectangle
			do3dbox(rp,&rect,FALSE);

			// Clear the inside
			SetAPen(rp,GUI->draw_info->dri_Pens[BACKGROUNDPEN]);
			if (rect.MaxX>rect.MinX+1 &&
				rect.MaxY>rect.MinY+1) RectFill(rp,rect.MinX+1,rect.MinY+1,rect.MaxX-1,rect.MaxY-1);
		}

		// Increment position
		x=rect.MaxX+1;
	}

	// Go through items
	for (item=0,num=0,start=0;item<DISPLAY_LAST;item++)
	{
		short length,next;
		BOOL visible=0;
		char *text=0;

		// Get length of item, skip if nothing there
		if ((length=buffer->buf_FieldWidth[buffer->buf_ListFormat.display_pos[item]])<1 &&
			!(buffer->more_flags&DWF_DEVICE_LIST))
			continue;

		// Find the next valid item
		for (next=item+1;next<DISPLAY_LAST;next++)
		{
			// Use if field has something in it
			if (buffer->buf_FieldWidth[buffer->buf_ListFormat.display_pos[next]]>0)
				break;
		}

		// Get length to use (if last item, will fill lister)
		len=(item==DISPLAY_LAST-1 || buffer->buf_ListFormat.display_pos[next]==DISPLAY_LAST || buffer->buf_ListFormat.display_pos[next]==-1)?-1:length;

		// Get end position of this item
		end=start+length-1;

		// Is this item visible?
		if (show &&
			((start>=buffer->buf_HorizOffset && start<buffer->buf_HorizOffset+lister->text_width) ||
			(end>=buffer->buf_HorizOffset && end<buffer->buf_HorizOffset+lister->text_width) ||
			(start<buffer->buf_HorizOffset && end>=buffer->buf_HorizOffset+lister->text_width)))
			visible=1;

		// Device list?
		if (buffer->more_flags&DWF_DEVICE_LIST)
		{
			// Save item, get string to display
			lister->title_boxes[num].tb_Item=-1;
			lister->title_boxes[num].tb_ItemNum=item;
			if (visible)
				text=GetString(&locale,MSG_LISTER_TITLE_VOLUME+item);
		}

		// Normal files
		else
		{
			// Save item
			lister->title_boxes[num].tb_Item=buffer->buf_ListFormat.display_pos[item];
			lister->title_boxes[num].tb_ItemNum=item;

			// Get string to display
			if (visible) text=lister_title_string(buffer,item);
		}

		// Set character values
		lister->title_boxes[num].tb_Position=start;
		lister->title_boxes[num].tb_Width=length;

		// Item to display?
		if (text)
		{
			struct Rectangle rect;
			short tlen,tx;
			struct TextExtent extent;

			// Last item?
			if (len==-1)
				rect.MaxX=lister->title_area.rect.MaxX;

			// Otherwise
			else
			{
				// Not completely visible to the left?
				if (start<buffer->buf_HorizOffset)
					len-=buffer->buf_HorizOffset-start;

				// Get length
				rect.MaxX=x+len-1;
			}

			// Build rectangle
			rect.MinX=x;
			rect.MinY=lister->title_area.rect.MinY;
			rect.MaxY=lister->title_area.rect.MaxY;

			// Clip to left-edge
			if (rect.MaxX>lister->title_area.rect.MaxX)
				rect.MaxX=lister->title_area.rect.MaxX;

			// Store position
			lister->title_boxes[num].tb_Left=rect.MinX;
			lister->title_boxes[num].tb_Right=rect.MaxX;

			// Valid drawing?
			if (rect.MinX<rect.MaxX)
			{
				// Draw rectangle
				do3dbox(rp,&rect,FALSE);
			}

			// Get length to display
			if (rect.MaxX>=rect.MinX+8)
				tlen=TextFit(rp,text,strlen(text),&extent,0,1,RECTWIDTH(&rect)-8,rp->TxHeight);
			else
			{
				tlen=0;
				extent.te_Width=0;
			}

			// Increment x position
			x=rect.MaxX+1;

			// Move the rectangle in
			rect.MinX++;
			rect.MinY++;
			rect.MaxX--;
			rect.MaxY--;

			// Get text coordinates
			tx=rect.MinX+2;

			// Bounds check rectangle
			if (rect.MinX<=rect.MaxX)
			{
				short max,x1;

				// Bounds-check max
				if ((max=rect.MaxX)>lister->title_area.rect.MaxX)
					max=lister->title_area.rect.MaxX;

				// Clear the inside
				SetAPen(rp,GUI->draw_info->dri_Pens[BACKGROUNDPEN]);
				RectFill(rp,rect.MinX,rect.MinY,(tx<max)?tx:max,rect.MaxY);
				if ((x1=rect.MinX+extent.te_Width+3)>tx && x1<=max)
					RectFill(rp,x1,rect.MinY,max,rect.MaxY);
			}

			// Show text if there's any to show
			if (tlen>0 && (++tx)<rect.MaxX)
			{
				SetAPen(rp,GUI->draw_info->dri_Pens[TEXTPEN]);
				Move(rp,tx,rect.MinY+rp->TxBaseline);
				Text(rp,text,tlen);
				tx=rp->cp_x;
			}

			// Is this the sort field?			
			if (buffer->buf_ListFormat.sort.sort==buffer->buf_ListFormat.display_pos[item])
			{
				// Show arrow for sort field
				if (tx<rect.MaxX-11)
				{
					// Reverse sort?
					if (buffer->buf_ListFormat.sort.sort_flags&SORT_REVERSE)
					{
						SetAPen(rp,GUI->draw_info->dri_Pens[SHADOWPEN]);
						Move(rp,rect.MaxX-8,rect.MinY+2);
						Draw(rp,rect.MaxX-5,rect.MaxY-2);
						Draw(rp,rect.MaxX-11,rect.MaxY-2);
						SetAPen(rp,GUI->draw_info->dri_Pens[SHINEPEN]);
						Draw(rp,rect.MaxX-8,rect.MinY+2);
					}

					// Forwards sort
					else
					{
						SetAPen(rp,GUI->draw_info->dri_Pens[SHINEPEN]);
						Move(rp,rect.MaxX-5,rect.MinY+2);
						Draw(rp,rect.MaxX-11,rect.MinY+2);
						Draw(rp,rect.MaxX-8,rect.MaxY-2);
						SetAPen(rp,GUI->draw_info->dri_Pens[SHADOWPEN]);
						Draw(rp,rect.MaxX-5,rect.MinY+2);
					}
				}
			}
		}

		// End of it?
		if (len==-1) break;

		// Increment entry number and position
		++num;
		start=end+1;
	}

	// Kludge for the end
	if (show && x<lister->title_area.rect.MaxX)
	{
		struct Rectangle rect;

		// Get final rectangle
		rect.MinX=x;
		rect.MinY=lister->title_area.rect.MinY;
		rect.MaxY=lister->title_area.rect.MaxY;
		rect.MaxX=lister->title_area.rect.MaxX;

		// Draw rectangle
		do3dbox(rp,&rect,FALSE);

		// Is it big enough?
		if (rect.MinX<rect.MaxX-1)
		{
			// Clear the inside
			SetAPen(rp,GUI->draw_info->dri_Pens[BACKGROUNDPEN]);
			RectFill(rp,
				rect.MinX+1,
				rect.MinY+1,
				rect.MaxX-1,
				rect.MaxY-1);
		}
	}
}


// Highlight a title box
void lister_title_highlight(Lister *lister,short item,short sel)
{
	struct Rectangle rect;

	// Build rectangle
	rect.MinX=lister->title_boxes[item].tb_Left;
	rect.MinY=lister->title_area.rect.MinY;
	rect.MaxX=lister->title_boxes[item].tb_Right;
	rect.MaxY=lister->title_area.rect.MaxY;

	// Draw rectangle
	do3dbox(&lister->title_area.rast,&rect,(sel==1)?1:0);

	// Highlight for sizing box?
	if (sel==2 || sel==-1)
		lister_title_highlight_sep(lister);
}


// Highlight separator
void lister_title_highlight_sep(Lister *lister)
{
	struct Rectangle rect;

	// Get complement box
	rect.MinX=lister->title_drag_x-1;
	rect.MaxX=rect.MinX+3;
	rect.MinY=lister->title_area.rect.MinY;
	rect.MaxY=lister->title_area.rect.MaxY;

	// Bounds-check
	if (rect.MaxX>lister->title_area.rect.MaxX)
		rect.MaxX=lister->title_area.rect.MaxX;

	// Complement the selection area
	SetDrMd(&lister->title_area.rast,COMPLEMENT);
	RectFill(&lister->title_area.rast,rect.MinX,rect.MinY,rect.MaxX,rect.MaxY);
	SetDrMd(&lister->title_area.rast,JAM2);
}


// Clear title area
void lister_clear_title(Lister *lister)
{
	EraseRect(&lister->title_area.rast,
		lister->title_area.rect.MinX,
		lister->title_area.rect.MinY,
		lister->title_area.rect.MaxX,
		lister->title_area.rect.MaxY);
}


// Handle drop of a field title
void lister_title_drop(Lister *lister,short item,short x,short y)
{
	short drop_over,after,d,d1;
	char display[DISPLAY_LAST];

	// Drop on title bar?
	if (y<lister->title_area.rect.MinY ||
		y>lister->title_area.rect.MaxY) return;

	// Go through items
	for (drop_over=0;drop_over<DISPLAY_LAST;drop_over++)
	{
		// Finished?
		if (lister->title_boxes[drop_over].tb_Item==-1)
			return;

		// Drop over this one?
		if (x>=lister->title_boxes[drop_over].tb_Left &&
			x<=lister->title_boxes[drop_over].tb_Right)
			break;
	}

	// Not found?
	if (drop_over==DISPLAY_LAST) return;

	// Drop over same box?
	if (drop_over==item) return;

	// We put the new one before the old, unless it's our neighbour on the right
	after=(drop_over==item+1)?1:0;

	// Get the real items we're moving
	item=lister->title_boxes[item].tb_Item;
	drop_over=lister->title_boxes[drop_over].tb_Item;

	// Go through display array
	for (d=0,d1=0;d<DISPLAY_LAST;d1++)
	{
		// Insert now?
		if (lister->cur_buffer->buf_ListFormat.display_pos[d1]==drop_over && !after)
		{
			// Insert before
			display[d++]=item;
		}

		// Copy to new array (unless its the one we're moving)
		if (lister->cur_buffer->buf_ListFormat.display_pos[d1]!=item)
			display[d++]=lister->cur_buffer->buf_ListFormat.display_pos[d1];

		// Insert now?
		if (lister->cur_buffer->buf_ListFormat.display_pos[d1]==drop_over && after)
		{
			// Insert after
			display[d++]=item;
		}
	}

	// Copy array back to lister and buffer
	for (d=0;d<DISPLAY_LAST;d++)
	{
		lister->cur_buffer->buf_ListFormat.display_pos[d]=display[d];
		lister->format.display_pos[d]=display[d];
		lister->user_format.display_pos[d]=display[d];
	}

	// Refresh lister and title
	lister_refresh(lister,0);
	lister_show_title(lister,1);
}


// Get field title string
char *lister_title_string(DirBuffer *buffer,short item)
{
	short txt;
	char *text;

	// Get string index
	txt=buffer->buf_ListFormat.display_pos[item];
	if (txt<DISPLAY_LAST && buffer->buf_TitleFields[txt])
		text=buffer->buf_TitleFields[txt];
	else text=GetString(&locale,MSG_LISTER_TITLE_NAME+txt);

	return text;
}
