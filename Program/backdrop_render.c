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

// Scroll the icons
void backdrop_scroll_objects(BackdropInfo *info,short off_x,short off_y)
{
	// Lock window
	GetSemaphore(&info->window_lock,SEMF_EXCLUSIVE,0);

	// Window open?
	if (info->window)
	{
		short damage=0,clear=0;

		// Bounds-check the deltas
		if (off_x<0 && off_x<-RECTWIDTH(&info->size))
			clear=1;
		else
		if (off_x>0 && off_x>RECTWIDTH(&info->size))
			clear=1;
		else
		if (off_y<0 && off_y<-RECTHEIGHT(&info->size))
			clear=1;
		else
		if (off_y>0 && off_y>RECTHEIGHT(&info->size))
			clear=1;

		// Clear instead of scrolling?
		if (clear)
		{
			// Clear the whole window
			EraseRect(
				info->window->RPort,
				info->size.MinX,
				info->size.MinY,
				info->size.MaxX,
				info->size.MaxY);
		}

		// Scroll
		else
		{
			// Check for 39
			if (GfxBase->LibNode.lib_Version>=39)
			{
				// Scroll backdrop window
				ScrollRasterBF(
					info->window->RPort,
					off_x,off_y,
					info->size.MinX,
					info->size.MinY,
					info->size.MaxX,
					info->size.MaxY);
			}

			// No backfills
			else
			{
				// Scroll backdrop window
				ScrollRaster(
					info->window->RPort,
					off_x,off_y,
					info->size.MinX,
					info->size.MinY,
					info->size.MaxX,
					info->size.MaxY);
			}

			// Damaged simple-refresh?
			if (info->window->Flags&WFLG_SIMPLE_REFRESH &&
				info->window->WLayer->Flags&LAYERREFRESH)
			{
				// Forbid
				Forbid();

				// Begin refreshing
				BeginRefresh(info->window);

				// Clear the new bits
				EraseRect(
					info->window->RPort,
					info->size.MinX,
					info->size.MinY,
					info->size.MaxX,
					info->size.MaxY);

				// End refreshing for the moment
				EndRefresh(info->window,FALSE);
				damage=1;
			}
		}

		// Got temporary region?
		if (info->temp_region)
		{
			struct Rectangle rect;

			// Get refresh region
			rect.MinX=(off_x==0)?info->size.MinX:((off_x>0)?info->size.MaxX-off_x:info->size.MinX);
			rect.MaxX=(off_x==0)?info->size.MaxX:((off_x>0)?info->size.MaxX:info->size.MinX-off_x);
			rect.MinY=(off_y==0)?info->size.MinY:((off_y>0)?info->size.MaxY-off_y:info->size.MinY);
			rect.MaxY=(off_y==0)?info->size.MaxY:((off_y>0)?info->size.MaxY:info->size.MinY-off_y);

			// Bounds check region
			if (rect.MinX<info->size.MinX) rect.MinX=info->size.MinX;
			if (rect.MinY<info->size.MinY) rect.MinY=info->size.MinY;
			if (rect.MaxX>info->size.MaxX) rect.MaxX=info->size.MaxX;
			if (rect.MaxY>info->size.MaxY) rect.MaxY=info->size.MaxY;

			// Add to damage list?
			if (damage)
			{
				// Or rectangle in
				OrRectRegion(info->window->WLayer->DamageList,&rect);
			}

			// Manually refresh
			else
			{
				// Set refresh region
				ClearRegion(info->temp_region);
				OrRectRegion(info->temp_region,&rect);

				// Install region
				InstallClipRegion(info->window->WLayer,info->temp_region);
			}
		}

		// Manual refresh?
		if (!damage)
		{
			// Refresh
			backdrop_show_objects(info,BDSF_NO_CLIP);

			// Remove clip region
			InstallClipRegion(info->window->WLayer,0);
		}

		// Automatic refresh
		else
		{
			// Lister?
			if (info->lister)
				lister_refresh_callback(IDCMP_REFRESHWINDOW,info->window,info->lister);

			// Other type
			else
			{
				struct IntuiMessage msg;

				// Fake IntuiMessage
				msg.Class=IDCMP_REFRESHWINDOW;

				// Handle refresh
				backdrop_idcmp(info,&msg,0);
			}

			// Enable multi-tasking
			Permit();
		}
	}

	// Unlock window
	FreeSemaphore(&info->window_lock);
}


// Show the backdrop objects
void backdrop_show_objects(BackdropInfo *info,USHORT flags)
{
	BackdropObject *object;

	// Lock backdrop list
	lock_listlock(&info->objects,0);

	// Lock window
	GetSemaphore(&info->window_lock,SEMF_EXCLUSIVE,0);

	// Window open?
	if (info->window)
	{
		// Are we in a refresh?
		if (flags&BDSF_IN_REFRESH)
		{
			// Lock layers
#ifdef LOCKLAYER_OK
			LockScreenLayer(info->window->WScreen);
#else
			Forbid();
#endif

			// End refresh temporarily
			EndRefresh(info->window,FALSE);

			// Install new clip region if we have it
			if (info->clip_region)
				InstallClipRegion(info->window->WLayer,info->clip_region);

			// Continue refresh
			BeginRefresh(info->window);
		}

		// Or, are we meant to be refreshing?
		else
		if (flags&BDSF_REFRESH)
		{
			// Start refresh here?
			if ((flags&BDSF_REFRESH_DONE)==BDSF_REFRESH_DONE)
			{
				// Lock layers
				Forbid();
			}

			// And our region with damagelist
			if (info->clip_region)
				AndRegionRegion(info->clip_region,info->window->WLayer->DamageList);

			// Begin the refresh
			BeginRefresh(info->window);
		}

		// Install clip region if we have it
		else
		if (!(flags&BDSF_NO_CLIP) && info->clip_region)
			InstallClipRegion(info->window->WLayer,info->clip_region);

		// Clear backdrop window
		if (flags&BDSF_CLEAR)
		{
			EraseRect(&info->rp,
				info->size.MinX,
				info->size.MinY,
				info->size.MaxX,
				info->size.MaxY);
		}

		// Not just clearing?
		if ((flags&BDSF_CLEAR_ONLY)!=BDSF_CLEAR_ONLY)
		{
			// Go through backdrop list (backwards)
			for (object=(BackdropObject *)info->objects.list.lh_TailPred;
				object->node.ln_Pred;
				object=(BackdropObject *)object->node.ln_Pred)
			{
				// Reset?
				if (flags&BDSF_RESET)
				{
					// Need to get masks?
					if (!backdrop_icon_border(object) &&
						!object->image_mask[0])
					{
						// Get masks for this icon
						backdrop_get_masks(object);
					}
				}

				// Valid position?
				if (!(object->flags&BDOF_NO_POSITION))
				{
					// Render this object
					backdrop_draw_object(
						info,
						object,
						BRENDERF_REAL,
						&info->rp,
						object->pos.Left,
						object->pos.Top);
				}
			}
		}

		// Refresh?
		if (flags&BDSF_REFRESH)
		{
			EndRefresh(info->window,((flags&BDSF_REFRESH_DONE)==BDSF_REFRESH_DONE)?TRUE:FALSE);

			// End refresh here?
			if ((flags&BDSF_REFRESH_DONE)==BDSF_REFRESH_DONE)
			{
				// Unlock layers
				Permit();
			}
		}

		// In refresh?
		else
		if (flags&BDSF_IN_REFRESH)
		{
			// End refresh temporarily
			EndRefresh(info->window,FALSE);

			// Remove clip region
			if (info->clip_region)
				InstallClipRegion(info->window->WLayer,0);

			// Continue refresh
			BeginRefresh(info->window);

			// Unlock layers
#ifdef LOCKLAYER_OK
			UnlockScreenLayer(info->window->WScreen);
#else
			Permit();
#endif
		}

		// Remove clip region
		else
		if (!(flags&BDSF_NO_CLIP) && info->clip_region)
			InstallClipRegion(info->window->WLayer,0);

		// Update virtual size
		if (flags&BDSF_RECALC) backdrop_calc_virtual(info);
	}

	// Unlock window
	FreeSemaphore(&info->window_lock);

	// Unlock backdrop list
	unlock_listlock(&info->objects);
}


// Render an object to the main window
void backdrop_render_object(
	BackdropInfo *info,
	BackdropObject *object,
	USHORT flags)
{
	// Lock window
	GetSemaphore(&info->window_lock,SEMF_EXCLUSIVE,0);

	// Window open?
	if (info->window)
	{
		// Install clip?
		if (flags&BRENDERF_CLIP && info->clip_region)
			InstallClipRegion(info->window->WLayer,info->clip_region);

		// Draw object
		backdrop_draw_object(
			info,
			object,
			flags|BRENDERF_REAL,
			&info->rp,
			object->pos.Left,
			object->pos.Top);

		// Remove clip region
		if (flags&BRENDERF_CLIP && info->clip_region)
			InstallClipRegion(info->window->WLayer,0);
	}

	// Unlock window
	FreeSemaphore(&info->window_lock);
}


// Draw an object
void backdrop_draw_object(
	BackdropInfo *info,
	BackdropObject *object,
	USHORT flags,
	struct RastPort *rp,
	short left,
	short top)
{
	UBYTE fpen=1,bpen=0,drawmode=JAM2,opus_drawmode=JAM2;
	USHORT *imagedata=0;
	struct Image *image=0;
	struct Rectangle rect;
	short len;
	BOOL comp=0,draw=1,state=0;
	short has_border;

	// No icon?
	if (!object->icon) return;

	// See if icon has no border
	has_border=backdrop_icon_border(object);

	// Not just clearing image?
	if (!(flags&BRENDERF_CLEAR))
	{
		// Get image to render
		if (image=(struct Image *)object->icon->do_Gadget.GadgetRender)
			imagedata=image->ImageData;

		// Is icon selected?
		if (object->state)
		{
			// Is there a select image?
			if (object->icon->do_Gadget.SelectRender &&
				(object->icon->do_Gadget.Flags&GFLG_GADGHIMAGE))
			{
				if (image=(struct Image *)object->icon->do_Gadget.SelectRender)
					imagedata=image->ImageData;
				state=1;
			}
			else comp=1;
		}
	}

	// Get object position
	if (flags&BRENDERF_REAL)
	{
		short border_x=0,border_y_top=0,border_y_bottom=0;

		// Border?
		if (has_border)
		{
			// Use default border
			border_x=ICON_BORDER_X;
			border_y_top=ICON_BORDER_Y_TOP;
			border_y_bottom=ICON_BORDER_Y_BOTTOM;
		}

		// Calculate full size
		object->full_size.MinX=left-border_x;
		object->full_size.MinY=top-border_y_top;
		object->full_size.MaxX=left+object->pos.Width+border_x-1;
		object->full_size.MaxY=top+object->pos.Height+border_y_bottom-1;

		// Get image rectangle
		rect.MinX=object->full_size.MinX+(info->size.MinX-info->offset_x);
		rect.MinY=object->full_size.MinY+(info->size.MinY-info->offset_y);
		rect.MaxX=object->full_size.MaxX+(info->size.MinX-info->offset_x);
		rect.MaxY=object->full_size.MaxY+(info->size.MinY-info->offset_y);

		// Store position
		object->show_rect=rect;
		object->image_rect=rect;

		// Is object offscreen?
		if (rect.MaxX<info->size.MinX ||
			rect.MaxY<info->size.MinY ||
			rect.MinX>info->size.MaxX ||
			rect.MinY>info->size.MaxY) draw=0;

		// Offset by coordinates
		left+=info->size.MinX-info->offset_x;
		top+=info->size.MinY-info->offset_y;
	}
	else
	{
		rect.MinX=left;
		rect.MinY=top;
		rect.MaxX=left+object->pos.Width-1;
		rect.MaxY=top+object->pos.Height-1;
	}

	// Clear area?
	if (flags&BRENDERF_CLEAR)
	{
		// Ok to draw?
		if (draw)
		{
			// Clear whole icon?
			if (!(flags&BRENDERF_LABEL))
			{
				EraseRect(rp,rect.MinX,rect.MinY,rect.MaxX,rect.MaxY);
			}
		}

		// Can't draw
		else flags&=~BRENDERF_CLEAR;
	}

	// Object state changed?
	else
	if (object->flags&BDOF_STATE_CHANGE)
	{
		// Need to clear if transparent
		if (!has_border)
			EraseRect(rp,rect.MinX,rect.MinY,rect.MaxX,rect.MaxY);
		object->flags&=~BDOF_STATE_CHANGE;
	}

	// Draw as normal
	if (!(flags&BRENDERF_CLEAR))
	{
		// Not drawing if no image
		if (!imagedata) draw=0;

		// Ok to draw?
		if (draw)
		{
			struct BitMap bitmap;
			BOOL use_mask=0;

			// Full icon?
			if (flags&BRENDERF_REAL)
			{
				// Want border?
				if (has_border)
				{
					// Draw border around icon
					DrawBox(rp,&rect,GUI->draw_info,object->state);

					// Get background colour
					SetAPen(
						rp,
						(object->flags&BDOF_BACKGROUND)?
							object->size:
							GUI->draw_info->dri_Pens[BACKGROUNDPEN]);

					// Clear boundary around image
					RectFill(rp,rect.MinX+1,rect.MinY+1,rect.MaxX-1,rect.MinY+2);
					RectFill(rp,rect.MinX+1,rect.MinY+1,rect.MinX+3,rect.MaxY-1);
					RectFill(rp,rect.MinX+4,rect.MaxY-3,rect.MaxX-1,rect.MaxY-1);
					RectFill(rp,rect.MaxX-3,rect.MinY+3,rect.MaxX-1,rect.MaxY-4);
				}
			}

			// Get image as a bitmap
			backdrop_image_bitmap(info,image,imagedata,&bitmap);

			// Using a mask (no border)?
			if (!has_border && object->image_mask[state])
			{
				// Draw using a mask
				BltMaskBitMapRastPort(
					&bitmap,0,0,
					rp,left,top,
					image->Width,image->Height,
					0xe0,
					(PLANEPTR)object->image_mask[state]);
				use_mask=1;
			}

			// Draw normally
			else
			{
				// Draw image
				BltBitMapRastPort(
					&bitmap,0,0,
					rp,left,top,
					image->Width,image->Height,
					0xc0);
			}

			// Complement?
			if (comp)
			{
				DragInfo *drag_info;
				UBYTE mask;

				// Only complement to image depth
				mask=rp->Mask;
				SetWrMsk(rp,image->PlanePick);

				// Need backfill?
				if ((object->icon->do_Gadget.Flags&GADGBACKFILL) &&
					(drag_info=GetDragInfo(info->window,0,-object->pos.Width,-object->pos.Height,0)))
				{
					// Draw icon into drag buffer
					BltBitMapRastPort(
						&bitmap,0,0,
						&drag_info->drag_rp,0,0,
						image->Width,image->Height,
						0xc0);

					// Build mask
					GetDragMask(drag_info);

					// Complement area with mask
					BltMaskBitMapRastPort(
						&drag_info->drag_bm,
						0,0,
						rp,
						left,top,
						image->Width,
						image->Height,
						0x20,
						(PLANEPTR)drag_info->bob.ImageShadow);

					// Free temporary drag info
					FreeDragInfo(drag_info);
				}

				// Just complement image
				else
				{
					ClipBlit(
						rp,	
						left,top,
						rp,
						left,top,
						image->Width,
						image->Height,
						0x50);
				}

				// Restore mask
				SetWrMsk(rp,mask);
			}

			// Left out (on desktop), or a link?
			if (!(environment->env->desktop_flags&DESKTOPF_NO_ARROW) &&
				((object->type==BDO_LEFT_OUT && !(object->flags&BDOF_DESKTOP_FOLDER) && info->flags&BDIF_MAIN_DESKTOP) ||
					object->flags&BDOF_LINK_ICON))
			{
				struct Image *image;

				// Get correct image for this resolution
				image=(GUI->screen_info&SCRI_LORES)?&arrow_image[1]:&arrow_image[0];

				// Is object big enough for the 'shortcut arrow'?
				if (object->pos.Width>(image->Width<<1) &&
					object->pos.Height>image->Height+4)
				{
					// Draw arrow in bottom-left corner
					DrawImage(rp,image,rect.MinX,rect.MaxY-image->Height+1);
				}
			}

			// Ghosted icon?
			if (object->flags&BDOF_GHOSTED)
			{
				// Draw ghosting over the icon
				backdrop_draw_icon_ghost(rp,&rect,(use_mask)?(PLANEPTR)object->image_mask[state]:0);
			}
		}

		// Get pens from configuration
		fpen=(info->flags&BDIF_MAIN_DESKTOP)?environment->env->icon_fpen:environment->env->iconw_fpen;
		bpen=(info->flags&BDIF_MAIN_DESKTOP)?environment->env->icon_bpen:environment->env->iconw_bpen;
		drawmode=(info->flags&BDIF_MAIN_DESKTOP)?environment->env->icon_style:environment->env->iconw_style;
		opus_drawmode=drawmode;

		// Shadow/Outline?
		if (drawmode==MODE_SHADOW || drawmode==MODE_OUTLINE)
		{
			UBYTE temp;

			// Set back to JAM1
			drawmode=JAM1;

			// Swap the pens
			temp=fpen;
			fpen=bpen;
			bpen=temp;
		}

		// Fix pens for user colours
		if (fpen>=4 && fpen<252) fpen=GUI->pens[fpen-4];
		if (bpen>=4 && bpen<252) bpen=GUI->pens[bpen-4];

		// Use fast call under 39
		if (GfxBase->LibNode.lib_Version>=39)
		{
			SetABPenDrMd(rp,fpen,bpen,drawmode);
		}
		else
		{
			SetAPen(rp,fpen);
			SetBPen(rp,bpen);
			SetDrMd(rp,drawmode);
		}
	}

	// Full icon?
	if (flags&BRENDERF_REAL && !(object->flags&BDOF_NO_LABEL))
	{
		char *name,namebuf[40];

		// Bad disk?
		if (object->type==BDO_BAD_DISK)
		{
			// Get bad disk name
			backdrop_bad_disk_name(object,namebuf);
			name=namebuf;
		}

		// Custom label?
		else
		if (object->flags&BDOF_CUSTOM_LABEL) name=object->device_name;

		// Get name pointer
		else name=object->name;

		// Get text length
		if (len=strlen(name))
		{
			struct TextExtent extent;
			short max_width=0,loop,textx,texty;

			// Splitting long labels?
			if (environment->env->desktop_flags&DESKTOPF_SPLIT_LABELS)
			{
				// Calculate maximum text width (minimum 6 chars)
				max_width=RECTWIDTH(&rect)+(RECTWIDTH(&rect)>>1);
				if (max_width<rp->Font->tf_XSize*6)
					max_width=rp->Font->tf_XSize*6;
			}

			// Position for text
			textx=rect.MinX+(RECTWIDTH(&rect)>>1);
			texty=rect.MaxY+ICON_LABEL_SPACE+rp->TxBaseline+((opus_drawmode==MODE_OUTLINE)?1:0);
			Move(rp,textx,texty);

			// Get text pixel size
			TextMultiLine(rp,name,len,max_width,TMLF_CENTER|TMLF_EXTENT,&extent);

			// Fix extent for shadow/outline
			if (opus_drawmode==MODE_SHADOW)
			{
				extent.te_Extent.MaxX++;
				extent.te_Width++;
				extent.te_Height++;
			}
			else
			if (opus_drawmode==MODE_OUTLINE)
			{
				extent.te_Extent.MinX--;
				extent.te_Extent.MaxX++;
				extent.te_Width+=2;
				extent.te_Height+=2;
			}

			// Stretch out rectangle sides if necessary
			if (extent.te_Extent.MinX<rect.MinX)
				object->full_size.MinX-=rect.MinX-extent.te_Extent.MinX;
			if (extent.te_Extent.MaxX>rect.MaxX)
				object->full_size.MaxX+=extent.te_Extent.MaxX-rect.MaxX;
			if (object->show_rect.MinX>extent.te_Extent.MinX)
				object->show_rect.MinX=extent.te_Extent.MinX;
			if (object->show_rect.MaxX<extent.te_Extent.MinX+extent.te_Width-1)
				object->show_rect.MaxX=extent.te_Extent.MinX+extent.te_Width-1;

			// Save bottom of text
			object->show_rect.MaxY+=ICON_LABEL_SPACE+extent.te_Height; // +((opus_drawmode==MODE_OUTLINE)?1:0);
			object->full_size.MaxY+=ICON_LABEL_SPACE+extent.te_Height; // +((opus_drawmode==MODE_OUTLINE)?1:0);

			// Clear?
			if (flags&BRENDERF_CLEAR)
			{
				EraseRect(rp,
					extent.te_Extent.MinX,rect.MaxY+ICON_LABEL_SPACE+((opus_drawmode==MODE_OUTLINE)?1:0),
					extent.te_Extent.MinX+extent.te_Width-1,rect.MaxY+ICON_LABEL_SPACE+extent.te_Height-1);
			}

			// Draw text
			else
			{
				// Is object selected?
				if (object->flags&BDOF_SELECTED)
				{
					if (drawmode==JAM2) SetDrMd(rp,INVERSVID|JAM2);
					else
					if (fpen!=GUI->draw_info->dri_Pens[HIGHLIGHTTEXTPEN])
						SetAPen(rp,GUI->draw_info->dri_Pens[HIGHLIGHTTEXTPEN]);
					else SetAPen(rp,GUI->draw_info->dri_Pens[BACKGROUNDPEN]);
				}

				// Shadow
				if (opus_drawmode==MODE_SHADOW)
				{
					// Draw shadow text first
					Move(rp,textx+1,texty+1);
					TextMultiLine(rp,name,len,max_width,TMLF_CENTER,0);

					// Draw text
					SetAPen(rp,bpen);
					Move(rp,textx,texty);
					TextMultiLine(rp,name,len,max_width,TMLF_CENTER,0);
				}

				// Normal text
				else
				if (opus_drawmode!=MODE_OUTLINE)
				{
					// Render text
					TextMultiLine(rp,name,len,max_width,TMLF_CENTER,0);
				}

				// Outline
				else
				{
					Move(rp,textx+1,texty+1);
					for (loop=0;loop<5;loop++)
					{
						// Render text
						TextMultiLine(rp,name,len,max_width,TMLF_CENTER,0);

						// Doing shadow?
						if (loop<4)
						{
							short s_x=0,s_y=0;

							// Switch to other pen?
							if (loop==3)
								SetAPen(rp,bpen);

							// Get the new offsets
							else
							if (loop==0)
							{
								s_x=-1;
								s_y=1;
							}
							else
							if (loop==1)
							{
								s_x=-1;
								s_y=-1;
							}
							else
							if (loop==2)
							{
								s_x=1;
								s_y=-1;
							}

							// Position for text
							Move(rp,textx+s_x,texty+s_y);
						}
					}
				}

				// Reset draw mode if necessary
				if (object->flags&BDOF_SELECTED)
					SetDrMd(rp,drawmode);
			}
		}
	}
}


// Convert an image to a bitmap
void backdrop_image_bitmap(
	BackdropInfo *info,
	struct Image *image,
	USHORT *imagedata,
	struct BitMap *bitmap)
{
	short depth,plane=1;
	long planesize;

	// Get depth
	depth=info->window->WScreen->RastPort.BitMap->Depth;

	// Initialise bitmap
	InitBitMap(bitmap,depth,image->Width,image->Height);

	// Get plane size
	planesize=((image->Width+15)>>4)*image->Height;

	// First plane pointer
	bitmap->Planes[0]=(PLANEPTR)imagedata;

	// More than 1 plane?
	if (image->Depth>1)
	{
		// Get second plane pointer
		bitmap->Planes[plane++]=(PLANEPTR)(imagedata+planesize);

		// If it's 3 planes, under 39, we can remap it
		if (image->Depth==3 && GfxBase->LibNode.lib_Version>=39 &&
			!(environment->env->desktop_flags&DESKTOPF_NO_REMAP))
		{
			for (plane=2;plane<depth;plane++)
				bitmap->Planes[plane]=(PLANEPTR)(imagedata+(planesize<<1));
		}

		// More than 2 planes?
		else
		if (image->Depth>2)
		{
			for (plane=2;plane<image->Depth;plane++)
				bitmap->Planes[plane]=(PLANEPTR)(imagedata+planesize*plane);
		}
	}

	// Clear extra planes
	for (;plane<depth;plane++) bitmap->Planes[plane]=0;
}


// Get image mask for an icon
void backdrop_get_masks(BackdropObject *object)
{
	short a;
	struct Image *image;

	// Free existing masks
	for (a=0;a<2;a++)
	{
		FreeVec(object->image_mask[a]);
		object->image_mask[a]=0;
	}

	// See if icon has border - no mask if so
	if (!backdrop_icon_border(object))
	{
		// Get first image data
		image=(struct Image *)object->icon->do_Gadget.GadgetRender;

		// Do two images
		for (a=0;a<2;a++)
		{
			// Allocate mask
			if (!(object->image_mask[a]=
				AllocVec(
					((image->Width+15)>>4)*image->Height*sizeof(USHORT),
					MEMF_CHIP|MEMF_CLEAR)))
				break;

			// Full transparency? (quicker)
			if (environment->env->desktop_flags&DESKTOPF_TRANSPARENCY)
			{
				short plane,row,col,word,width,off;

				// Get width in words
				width=(image->Width+15)>>4;

				// Build shadow mask
				for (plane=0,off=0;plane<image->Depth;plane++)
				{
					for (row=0,word=0;row<image->Height;row++)
					{
						for (col=0;col<width;col++,word++,off++)
						{
							// Build mask
							object->image_mask[a][word]|=image->ImageData[off];
						}
					}
				}
			}

			// Only transparent around the edge
			else
			{
				// Build mask
				BuildTransDragMask(
					object->image_mask[a],
					image->ImageData,
					image->Width,
					image->Height,
					image->Depth,0);
			}

			// Get second image
			if (!(image=(struct Image *)object->icon->do_Gadget.SelectRender)) break;
		}
	}
}


// Install clip region
void backdrop_install_clip(BackdropInfo *info)
{
	// Or clip rectangle in to region
	if (info->clip_region)
	{
		ClearRegion(info->clip_region);
		OrRectRegion(info->clip_region,&info->size);
	}
}


// Clear clip region
void backdrop_clear_region(BackdropInfo *info)
{
	// Clear clip region
	if (info->clip_region) ClearRegion(info->clip_region);
}


// Add rectangle to region
void backdrop_add_region(BackdropInfo *info,struct Rectangle *rect)
{
	// Or into region
	if (info->clip_region) OrRectRegion(info->clip_region,rect);
}


// Check region is within bounds
void backdrop_region_bounds(BackdropInfo *info)
{
	// And with full rectangle
	if (info->clip_region) AndRectRegion(info->clip_region,&info->size);
}


// Erase an icon
void backdrop_erase_icon(BackdropInfo *info,BackdropObject *object,USHORT flags)
{
	BackdropObject *icon;

	// Lock icon list
	lock_listlock(&info->objects,FALSE);

	// Lock window
	GetSemaphore(&info->window_lock,SEMF_EXCLUSIVE,0);

	// Window open?
	if (info->window)
	{
		// Install clip?
		if (info->clip_region)
			InstallClipRegion(info->window->WLayer,info->clip_region);

		// Erase icon area
		EraseRect(
			&info->rp,
			object->show_rect.MinX,
			object->show_rect.MinY,
			object->show_rect.MaxX,
			object->show_rect.MaxY);

		// Got through icons
		for (icon=(BackdropObject *)info->objects.list.lh_Head;
			icon->node.ln_Succ;
			icon=(BackdropObject *)icon->node.ln_Succ)
		{
			// Valid position?
			if (icon->flags&BDOF_NO_POSITION) continue;

			// Not the one we just removed?
			if (icon==object) continue;

			// Did its box intersect the one we removed?
			if (geo_box_intersect(&object->show_rect,&icon->show_rect))
			{
				// ReDraw object
				backdrop_draw_object(
					info,
					icon,
					BRENDERF_REAL,
					&info->rp,
					icon->pos.Left,
					icon->pos.Top);
			}
		}

		// Remove clip region
		InstallClipRegion(info->window->WLayer,0);

		// Update virtual size
		if (flags&BDSF_RECALC) backdrop_calc_virtual(info);
	}

	// Unlock window
	FreeSemaphore(&info->window_lock);

	// Unlock icon list
	unlock_listlock(&info->objects);
}


// Draw ghosted image over icon
void backdrop_draw_icon_ghost(struct RastPort *rp,struct Rectangle *rect,PLANEPTR mask)
{
	struct BitMap *ghost_bm;
	struct RastPort ghost_rp;
	USHORT *new_mask;
	unsigned short width,height,words;

	// No mask?
	if (!mask)
	{
		static USHORT stipple[2]={0xaaaa,0x5555};

		// Set stipple fill
		SetAfPt(rp,stipple,1);

		// Fill in black
		SetAPen(rp,1);
		SetDrMd(rp,JAM1);

		// Draw over the image
		RectFill(rp,rect->MinX,rect->MinY,rect->MaxX,rect->MaxY);

		// Clear stipple fill
		SetAfPt(rp,0,0);
		return;
	}

	// Get image size
	width=RECTWIDTH(rect);
	height=RECTHEIGHT(rect);

	// Create ghost bitmap
	if (!(ghost_bm=
			NewBitMap(
				width,
				height,
				1,
				0,
				rp->BitMap))) return;

	// Get image size in words
	words=((width+15)>>4)*height;

	// Allocate a copy of the mask
	if (new_mask=AllocVec(words*sizeof(USHORT),MEMF_CHIP))
	{
		short word,line,count;
		unsigned short mask_word=0xaaaa;

		// Copy mask
		CopyMem((char *)mask,(char *)new_mask,words*sizeof(USHORT));

		// Words per line
		line=(width+15)>>4;

		// Stipple mask
		for (word=0,count=0;word<words;word++)
		{
			// Mask off
			new_mask[word]&=~mask_word;

			// New line?
			if (++count==line)
			{
				// Switch mask value to create alternating dotted lines
				mask_word=(mask_word==0xaaaa)?0x5555:0xaaaa;
				count=0;
			}
		}

		// Use new mask as mask
		mask=(PLANEPTR)new_mask;
	}

	// Initialise ghost rastport
	InitRastPort(&ghost_rp);
	ghost_rp.BitMap=ghost_bm;

	// Set rastport to black
	SetRast(&ghost_rp,1);

	// Blit over icon (mask will create the stipple effect)
	BltMaskBitMapRastPort(
		ghost_bm,0,0,
		rp,rect->MinX,rect->MinY,
		width,height,
		0xe0,
		mask);

	// Wait for blitter
	WaitBlit();

	// Free ghost bitmap and mask
	DisposeBitMap(ghost_bm);
	FreeVec(new_mask);
}


// See if an icon has a border
BOOL backdrop_icon_border(BackdropObject *icon)
{
	ULONG iflags;

	// See if icon has no border
	if (((iflags=GetIconFlags(icon->icon))&ICONF_BORDER_OFF) ||
		(environment->env->desktop_flags&DESKTOPF_NO_BORDERS && !(iflags&ICONF_BORDER_ON))) return 0;
	return 1;
}


// Get the name of a bad disk
void backdrop_bad_disk_name(BackdropObject *object,char *namebuf)
{
	ULONG pad[2];
	char *ptr;
	short num;

	// Build name
	strcpy(namebuf,object->name);

	// Get dos type; quick null-padding using ULONGs
	pad[0]=object->misc_data;
	pad[1]=0;
	ptr=(char *)pad;

	// Check for printable characters
	for (num=0;num<4;num++,ptr++)
	{
		// Lowercase?	
		if (*ptr>='a' && *ptr<='Z')
			*ptr-=('a'-'A');

		// Number?
		else
		if (*ptr<10)
			*ptr+='0';

		// Non-letter?
		else
		if (*ptr<'A' || *ptr>'Z')
		{
			// Non-printable?
			if (*ptr<'0' || *ptr>'9') *ptr='?';
		}
	}

	// Kill a trailing zero
	ptr=(char *)pad;
	if (ptr[3]=='0') ptr[3]=0;

	// Get name pointer
	strcat(namebuf,(char *)pad);
}
