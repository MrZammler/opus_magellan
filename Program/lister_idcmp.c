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

// Process lister IDCMP messages
void __asm __saveds lister_process_msg(
	register __d0 Lister *lister,
	register __a0 struct IntuiMessage *msg)
{
	struct Gadget *gadget;
	BOOL key_ok=0;

	// Get pointer to gadget (if applicable)
	gadget=(struct Gadget *)msg->IAddress;

	// Key presses ok?
	if (!(lister->flags&LISTERF_LOCK) &&
		(!(lister->flags&LISTERF_VIEW_ICONS) || lister->flags&LISTERF_ICON_ACTION)) key_ok=1;

	// Look at message class
	switch (msg->Class)
	{
		// Refresh window
		case IDCMP_REFRESHWINDOW:

			// Refresh display
			lister_refresh(
				lister,
				(lister->flags&LISTERF_ICON_ACTION)?LREFRESH_NOICON|LREFRESH_REFRESH:LREFRESH_REFRESH);
			break;


		// Size/position changed
		case IDCMP_CHANGEWINDOW:
			lister_refresh_callback(IDCMP_CHANGEWINDOW,0,lister);
			break;


		// Window closed
		case IDCMP_CLOSEWINDOW:

			// Send ourselves a quit message, will handle busy state correctly
			IPC_Command(lister->ipc,IPC_QUIT,0,0,0,0);
			break;


		// Menu pick
		case IDCMP_MENUPICK:
		case IDCMP_MENUHELP:
			{
				struct MenuItem *item;
				struct Menu *oldstrip=lister->window->MenuStrip;
				USHORT nextselect;

				// End a direct edit
				lister_end_edit(lister,0);

				// Get item
				nextselect=msg->Code;
				while (item=ItemAddress(lister->window->MenuStrip,nextselect))
				{
					int a;

					// Get next selection
					nextselect=item->NextSelect;

					// Help?
					if (msg->Class==IDCMP_MENUHELP)
					{
						help_menu_help((long)GTMENUITEM_USERDATA(item),0);
						break;
					}

					// See if we can handle it
					if ((a=lister_do_function(lister,(ULONG)GTMENUITEM_USERDATA(item)))==-1)
					{
						MenuEvent *event;
						APTR menuitem;
						BOOL ok=0;

						// Lock menu item list
						menuitem=LockAppList();

						// Search list
						while (menuitem=NextAppEntry(menuitem,APP_MENU))
						{
							// Match?
							if (menuitem==(APTR)(GTMENUITEM_USERDATA(item)))
							{
								DOpusAppMessage *msg;
								struct MsgPort *port;

								// Set flag
								ok=1;

								// Lock current buffer
								buffer_lock(lister->cur_buffer,FALSE);

								// Build AppMessage
								if (msg=alloc_appmsg_files(0,lister->cur_buffer,1))
								{
									// Set message type
									msg->da_Msg.am_Type=MTYPE_APPMENUITEM;

									// Get port and info
									port=WB_AppWindowData(
										(struct AppWindow *)menuitem,
										&msg->da_Msg.am_ID,
										&msg->da_Msg.am_UserData);

									// Send the message
									PutMsg(port,(struct Message *)msg);
								}

								// Unlock buffer
								buffer_unlock(lister->cur_buffer);
								break;
							}
						}

						// Unlock menu list
						UnlockAppList();

						// If not an AppMenuItem, send to main process
						if (!ok)
						{
							// Allocate event
							if (event=AllocVec(sizeof(MenuEvent),0))
							{
								// Fill out event	
								event->id=(ULONG)GTMENUITEM_USERDATA(item);
								event->menu=item;
								event->window=lister->window;

								// Send command to main process
								IPC_Command(&main_ipc,MAIN_MENU_EVENT,0,0,event,0);
							}
						}
					}

					// Is window shut now?
					else
					if (!lister->window) break;

					// Check for valid next
					if (!nextselect || lister->window->MenuStrip!=oldstrip) break;
				}
			}
			break;


		// Key press
		case IDCMP_RAWKEY:

			// Send to editor
			if (lister_edit_key(lister,msg))
				break;

			// Ignore keys if playing with a title button
			if (lister->title_click==-1)
			{
				DirBuffer *buffer;
				Cfg_Function *function;
				BOOL ok=0;
				char key=0;
				int scroll_line=1;

				// Can we match a function key?
				if (function=match_function_key(msg->Code,msg->Qualifier,0,lister,WINDOW_LISTER,0))
				{
					// Run function
					function_launch_quick(
						FUNCTION_RUN_FUNCTION,
						function,
						lister);
					break;
				}

				// Get current buffer
				buffer=lister->cur_buffer;

				// Convert from rawkey
				ConvertRawKey(msg->Code,msg->Qualifier,&key);

				// Amiga key down?
				if (msg->Qualifier&IEQUAL_ANYAMIGA)
				{
					// Amiga-space means scroll to first selected entry
					if (!(lister->flags&LISTERF_VIEW_ICONS) && msg->Code==0x40)
					{
						// Left amiga = first file
						if (msg->Qualifier&IEQUALIFIER_LCOMMAND)
							lister_show_selected(lister,ENTRY_FILE);

						// Right amiga = first directory
						else lister_show_selected(lister,ENTRY_DIRECTORY);
					}

					// Copy entry to clipboard?
					else
					if (msg->Qualifier&IEQUALIFIER_RCOMMAND && (key=='c' || key=='C'))
					{
						lister_clip_entries(lister,msg->Qualifier);
					}

					break;
				}

				// Do cursor up mapping
				if (msg->Code==CURSORUP)
				{
					// Map shift/control to page up/home
					if (msg->Qualifier&IEQUAL_ANYSHIFT) msg->Code=PAGEUP;
					else
					if (msg->Qualifier&IEQUALIFIER_CONTROL) msg->Code=HOME;
					else msg->Code=KEY_CURSORUP;
				}

				// Or cursor down mapping
				else
				if (msg->Code==CURSORDOWN)
				{
					// Map shift/control to page down/end
					if (msg->Qualifier&IEQUAL_ANYSHIFT) msg->Code=PAGEDOWN;
					else
					if (msg->Qualifier&IEQUALIFIER_CONTROL) msg->Code=END;
					else msg->Code=KEY_CURSORDOWN;
				}

				// Mouse wheel support
				else
				if (msg->Code==0x7a)
				{
					if (msg->Qualifier&IEQUAL_ANYSHIFT) msg->Code=PAGEUP;
					else
					if (msg->Qualifier&IEQUALIFIER_CONTROL) msg->Code=HOME;
					else
					{
						msg->Code=KEY_CURSORUP;
						scroll_line=GUI->wheel_lines;
					}
				}
				else
				if (msg->Code==0x7b)
				{
					if (msg->Qualifier&IEQUAL_ANYSHIFT) msg->Code=PAGEDOWN;
					else
					if (msg->Qualifier&IEQUALIFIER_CONTROL) msg->Code=END;
					else
					{
						msg->Code=KEY_CURSORDOWN;
						scroll_line=GUI->wheel_lines;
					}
				}

				// Look at key
				switch (msg->Code)
				{
					// Help?
					case 0x5f:
						if (!(msg->Qualifier&VALID_QUALIFIERS))
						{
							help_get_help(
								msg->MouseX+lister->window->LeftEdge,
								msg->MouseY+lister->window->TopEdge,
								msg->Qualifier);
						}
						else ok=1;
						break;


					// Cursor up
					case KEY_CURSORUP:

						// In key selection mode?
						if (lister->flags&LISTERF_KEY_SELECTION)
						{
							// Move selector up one
							if (set_list_selector(lister,lister->selector_pos-scroll_line))
								break;
						}

						// Scroll up a line
						lister_scroll(lister,0,-scroll_line);
						break;


					// Page up
					case PAGEUP:

						// In key selection mode?
						if (lister->flags&LISTERF_KEY_SELECTION)
						{
							// Move selector to top if necessary
							if (set_list_selector(lister,0))
								break;
						}

					// Home
					case HOME:

						// If already at the top of the list, break
						if (buffer->buf_VertOffset==0)
						{
							// Make sure selector is at the top
							if (lister->flags&LISTERF_KEY_SELECTION)
							{
								// Move to top of page
								set_list_selector(lister,0);
							}
							break;
						}

						// Home?
						if (msg->Code==HOME) buffer->buf_VertOffset=0;

						// Otherwise page up
						else
						{
							buffer->buf_VertOffset-=lister->text_height-1;
							if (buffer->buf_VertOffset<0) buffer->buf_VertOffset=0;
						}

						// In key selection mode?
						if (lister->flags&LISTERF_KEY_SELECTION)
						{
							// Erase old selector
							show_list_selector(lister,lister->selector_pos,1);
						}

						// Refresh scroller and window
						lister_update_slider(lister,SLIDER_VERT);
						lister_display_dir(lister);

						// Move selector to the top of the page
						if (lister->flags&LISTERF_KEY_SELECTION)
						{
							set_list_selector(lister,0);
							show_list_selector(lister,lister->selector_pos,0);
						}
						break;


					// Cursor down
					case KEY_CURSORDOWN:

						// In key selection mode?
						if (lister->flags&LISTERF_KEY_SELECTION)
						{
							// Move selector down one
							if (set_list_selector(lister,lister->selector_pos+scroll_line))
								break;
						}

						// Scroll down a line
						lister_scroll(lister,0,scroll_line);
						break;


					// Page down
					case PAGEDOWN:

						// In key selection mode?
						if (lister->flags&LISTERF_KEY_SELECTION)
						{
							// Move selector to bottom if necessary
							if (set_list_selector(lister,0xffff))
								break;
						}

					// End
					case END:

						// If already at the bottom of the list, break
						if (buffer->buf_VertOffset+lister->text_height>=buffer->buf_TotalEntries[0])
						{
							// Make sure selector is at the bottom
							if (lister->flags&LISTERF_KEY_SELECTION)
							{
								// Move to bottom of page
								set_list_selector(lister,0xffff);
							}
							break;
						}

						// End?
						if (msg->Code==END)
							buffer->buf_VertOffset=buffer->buf_TotalEntries[0]-lister->text_height;

						// Otherwise page down
						else
						{
							buffer->buf_VertOffset+=lister->text_height-1;
							if (buffer->buf_VertOffset+lister->text_height>buffer->buf_TotalEntries[0])
								buffer->buf_VertOffset=buffer->buf_TotalEntries[0]-lister->text_height;
							if (buffer->buf_VertOffset<0) buffer->buf_VertOffset=0;
						}

						// In key selection mode?
						if (lister->flags&LISTERF_KEY_SELECTION)
						{
							// Erase old selector
							show_list_selector(lister,lister->selector_pos,1);
						}

						// Refresh scroller and window
						lister_update_slider(lister,SLIDER_VERT);
						lister_display_dir(lister);

						// Move selector to the bottom of the page
						if (lister->flags&LISTERF_KEY_SELECTION)
						{
							set_list_selector(lister,0xffff);
							show_list_selector(lister,lister->selector_pos,0);
						}
						break;


					// Cursor left = scroll left
					case CURSORLEFT:
					case 0x2d:

						// If already at the left of the list, break
						if (buffer->buf_HorizOffset==0) break;

						// Control or shift?
						if (msg->Qualifier&(IEQUALIFIER_CONTROL|IEQUAL_ANYSHIFT))
						{
							// Control means to extreme left
							if (msg->Qualifier&IEQUALIFIER_CONTROL)
								buffer->buf_HorizOffset=0;

							// Otherwise shift means a page left
							else
							{
								buffer->buf_HorizOffset-=lister->text_width;
								if (buffer->buf_HorizOffset<0) buffer->buf_HorizOffset=0;
							}

							// Refresh window
							lister_refresh_display(lister,REFRESHF_SLIDERS);
							break;
						}

						// Scroll left a position
						lister_scroll(lister,-1,0);
						break;


					// Cursor right = scroll right
					case CURSORRIGHT:
					case 0x2f:

						// If already at the right of the list, break
						if (buffer->buf_HorizOffset>=buffer->buf_HorizLength-lister->text_width)
							break;

						// Control or shift?
						if (msg->Qualifier&(IEQUALIFIER_CONTROL|IEQUAL_ANYSHIFT))
						{
							// Control means full right
							if (msg->Qualifier&IEQUALIFIER_CONTROL)
								buffer->buf_HorizOffset=0xffff;

							// Otherwise shift means a page right
							else buffer->buf_HorizOffset+=lister->text_width;

							// Refresh window
							lister_refresh_display(lister,REFRESHF_SLIDERS);
							break;
						}

						// Scroll right a position
						lister_scroll(lister,1,0);
						break;


					// Space toggles keyboard selection
					case 0x40:
						if (!(lister->flags&LISTERF_LOCK))
						{
							// Toggle flag
							lister->flags^=LISTERF_KEY_SELECTION;

							// Initialise area with new size
							lister_init_filelist(lister);
							lister_init_lister_area(lister);

							// Refresh display
							lister->selector_pos=0;
							lister_refresh_display(lister,REFRESHF_SLIDERS);
						}
						break;


					// Tab activates the next window
					case 0x42:
						{
							IPCData *next;
							Lister *nextlister;

							// Get the next window
							lock_listlock(&GUI->lister_list,FALSE);
							next=lister->ipc;
							do
							{
								next=(IPCData *)next_node(
									&GUI->lister_list.list,
									(struct Node *)&next->node,
									(msg->Qualifier&IEQUAL_ANYSHIFT)?-1:1);
								nextlister=IPCDATA(next);
							} while ((nextlister!=lister && (nextlister->flags&LISTERF_SOURCEDEST_LOCK)) ||
									 !nextlister->window);
							unlock_listlock(&GUI->lister_list);

							// Make that window active
							if (nextlister && nextlister!=lister)
								IPC_Command(nextlister->ipc,LISTER_MAKE_SOURCE,0,0,0,0);
						}
						break;

					// Enter
					case 0x43:

						// Enter does double-click in key selection mode
						if (!(lister->flags&LISTERF_LOCK) &&
							lister->flags&LISTERF_KEY_SELECTION)
						{
							// Select this item
							select_key_select(lister,1);
						}
						break;

					// Return
					case 0x44:

						// In key selection mode?
						if (!(lister->flags&LISTERF_LOCK) &&
							lister->flags&LISTERF_KEY_SELECTION)
						{
							// Toggle this item
							if (select_key_select(lister,0))
							{
								// Move selector down one
								if (!(set_list_selector(lister,
									lister->selector_pos+((msg->Qualifier&IEQUAL_ANYSHIFT)?-1:1))))
								{
									// Move up a line?
									if (msg->Qualifier&IEQUAL_ANYSHIFT)
										lister_scroll(lister,0,-1);

									// Move down a line
									else
										lister_scroll(lister,0,1);
								}
							}
							break;
						}

						// Activate path field
						if (key_ok) ActivateStrGad(lister->path_field,lister->window);
						break;

					// Escape
					case 0x45:

						// With shift, closes window
						if (msg->Qualifier&IEQUAL_ANYSHIFT)
						{
							// Send ourselves a quit message, will handle busy state correctly
							IPC_Command(lister->ipc,IPC_QUIT,0,0,0,0);
							break;
						}

						// Busy/locked?
						if (lister->flags&(LISTERF_BUSY|LISTERF_LOCK))
						{
							// Aborts
							lister_send_abort(lister);
						}

						// Otherwise, in key selection mode?
						else
						if (lister->flags&LISTERF_KEY_SELECTION &&
							environment->env->lister_options&LISTEROPTF_EDIT_BOTH)
						{
							// Start edit
							lister_start_edit(lister,-1,lister->selector_pos,-2);
						}
						break;

					// Backspace does parent
					case 0x41:
						if (key_ok) lister_do_function(lister,MENU_LISTER_PARENT);
						break;

					// delete does edit
					case 0x46:
						if (key_ok) lister_configure(lister);
						break;


					// F10 means window to front
					case 0x59:
						WindowToFront(lister->window);
						break;


					// Default
					default:
						ok=1;
						break;
				}

				// Nothing so far?
				if (ok && key_ok)
				{
					// Look at vanilla key
					switch (key)
					{
						// / does parent
						case '/':
							lister_do_function(lister,MENU_LISTER_PARENT);
							break;

						// ? does parent in new window
						case '?':
							lister_do_function(lister,MENU_LISTER_PARENT_NEW);
							break;

						// : does root in new window
						case ':':
							lister_do_function(lister,MENU_LISTER_ROOT_NEW);
							break;

						// ; does root
						case ';':
							lister_do_function(lister,MENU_LISTER_ROOT);
							break;

						// * does select
						case '*':
							function_launch_quick(
								FUNCTION_RUN_FUNCTION,
								def_function_select,
								lister);
							break;

						// ( does all
						case '(':
							select_global_state(lister,1);
							lister_refresh_display(lister,(lister->flags&LISTERF_ICON_ACTION)?REFRESHF_ICONS:0);
							break;

						// ) does none
						case ')':
							select_global_state(lister,0);
							lister_refresh_display(lister,(lister->flags&LISTERF_ICON_ACTION)?REFRESHF_ICONS:0);
							break;

						// - does toggle
						case '-':
							select_global_toggle(lister);
							lister_refresh_display(lister,(lister->flags&LISTERF_ICON_ACTION)?REFRESHF_ICONS:0);
							break;

						// + does device list
						case '+':
							function_launch_quick(
								FUNCTION_RUN_FUNCTION,
								def_function_devicelist,
								lister);
							break;

						// . does buffer list
						case '.':
							function_launch_quick(
								FUNCTION_RUN_FUNCTION,
								def_function_cachelist,
								lister);
							break;

						// = returns from special buffer
						case '=':
							if (check_special_buffer(lister,1))
								lister_show_buffer(lister,lister->cur_buffer,1,1);
							break;

						// \ does reselect
						case '\\':
							// Do reselection
							buffer_lock(lister->cur_buffer,FALSE);
							DoReselect(&lister->reselect,lister,1);
							buffer_unlock(lister->cur_buffer);

							// Refresh
							lister_refresh_display(lister,(lister->flags&LISTERF_ICON_ACTION)?REFRESHF_ICONS:0);
							select_show_info(lister,1);
							break;

						// No match
						default:

							// Don't do anything for device lists, etc
							if (lister->cur_buffer==lister->special_buffer)
								break;

							// Valid key (and not control)?
							if (msg->Code<0x40 && !(msg->Qualifier&IEQUALIFIER_CONTROL))
							{
								// Is key printable?
								if (isprint(key))
								{
									// Extended key selection, and not alt?
									if (environment->env->settings.general_flags&GENERALF_EXTENDED_KEY &&
										!(msg->Qualifier&IEQUAL_ANYALT))
									{
										// Show hot name requester
										lister_add_hotname(lister,key);
									}
	
									// Do immediate scroll
									else
									{
										// Build hot buffer
										lister->hot_name[0]=key;
										lister->hot_name[1]=0;

										// Do scroll
										lister_handle_hotname(lister);
									}
								}
							}
							break;
					}
				}
			}
			break;


		// Window activated
		case IDCMP_ACTIVEWINDOW:

			// Set pointer to current lister
			GUI->current_lister=lister;

/*
			// Is lister busy, and do we have a known locker?
			if (lister->flags&LISTERF_BUSY && lister->locker_ipc)
			{
				// Send activate command
				IPC_Command(lister->locker_ipc,IPC_ACTIVATE,0,0,0,0);
				break;
			}
*/

			// Make sure progress bar is visible
			if (lister->progress_window)
				ShowProgressWindow(lister->progress_window,0,0);

			// See if buffer needs to be re-read
			if (!(lister->flags&LISTERF_LOCK))
			{
				lister_check_old_buffer(lister,0);
			}
			break;


		// Mouse move
		case IDCMP_MOUSEMOVE:

			// Playing with a toolbar button?
			if (lister->flags&LISTERF_TOOLBAR && lister->tool_sel!=-1)
			{
				lister_toolbar_click(lister,msg->MouseX,msg->MouseY,0,0);
				break;
			}

			// Playing with a title button?
			if (lister->title_click!=-1)
			{
				// Dragging separator?
				if (lister->title_sel==2)
				{
					// Check it's not been dragged too far left
					if (lister->window->MouseX>lister->title_boxes[lister->title_click].tb_Left+7)
					{
						// Remove old separator
						lister_title_highlight_sep(lister);

						// Show new separator
						lister->title_drag_x=lister->window->MouseX;
						lister_title_highlight_sep(lister);
					}
				}

				// Dragging already?
				else
				if (lister->drag_info)
				{
					// Show Image
					ShowDragImage(
						lister->drag_info,
						lister->window->MouseX-lister->title_click_x,
						lister->window->MouseY-lister->title_click_y);
				}

				// Moved far enough from click spot?
				else
				if (msg->MouseX<lister->title_click_x-1 ||
					msg->MouseX>lister->title_click_x+1 ||
					msg->MouseY<lister->title_click_y-1 ||
					msg->MouseY>lister->title_click_y+1)
				{
					short item;

					// Cache item
					item=lister->title_click;

					// Fix box highlighting
					lister_title_highlight(lister,item,0);

					// Allocate DragInfo
					if (lister->drag_info=
						GetDragInfo(
							lister->window,
							lister->window->RPort,
							lister->title_boxes[item].tb_Right-
								lister->title_boxes[item].tb_Left+1,
							lister->title_area.box.Height,
							DRAGF_NEED_GELS|DRAGF_CUSTOM))
					{
						struct RastPort *rp;
						struct Rectangle rect;
						struct TextExtent extent;
						char *text;
						short tlen;

						// Want opaque image, and no layer locking
						lister->drag_info->flags|=DRAGF_OPAQUE|DRAGF_NO_LOCK;

						// Cache rastport
						rp=&lister->drag_info->drag_rp;

						// Draw border
						rect.MinX=0;
						rect.MinY=0;
						rect.MaxX=lister->drag_info->width-1;
						rect.MaxY=lister->drag_info->height-1;
						do3dbox(rp,&rect,FALSE);

						// Set drag font
						SetFont(rp,lister->window->WScreen->RastPort.Font);

						// Get text and length to display
						text=lister_title_string(lister->cur_buffer,lister->title_boxes[item].tb_ItemNum);
						tlen=TextFit(rp,text,strlen(text),&extent,
										0,1,RECTWIDTH(&rect)-8,rp->TxHeight);

						// Show text
						SetAPen(rp,GUI->draw_info->dri_Pens[TEXTPEN]);
						Move(rp,4,rp->TxBaseline+1);
						Text(rp,text,tlen);

/*
						// Get drag image
						GetDragImage(
							lister->drag_info,
							lister->title_boxes[item].tb_Left,
							lister->title_area.box.Top);
*/

						// Get mask
						GetDragMask(lister->drag_info);

						// Get offset
						lister->title_click_x=
							msg->MouseX-lister->title_boxes[item].tb_Left;
						lister->title_click_y=
							msg->MouseY-lister->title_area.box.Top;

						// Show Image
						ShowDragImage(
							lister->drag_info,
							msg->MouseX-lister->title_click_x,
							msg->MouseY-lister->title_click_y);
					}

					// Failed, so stop clicking
					else lister->title_click=-1;
				}
			}

			// No gadget down?
			if (!lister->down_gadget) break;

		// Intuiticks
		case IDCMP_INTUITICKS:

			if (msg->Class==IDCMP_INTUITICKS)
			{
				// No gadget down?
				if (!lister->down_gadget ||
					lister->down_gadget->GadgetID==GAD_VERT_SCROLLER ||
					lister->down_gadget->GadgetID==GAD_HORIZ_SCROLLER) break;
			}

			// Fake gadget down
			msg->IAddress=lister->down_gadget;

		// Gadget down
		case IDCMP_GADGETDOWN:

			// End a direct edit
			lister_end_edit(lister,EDITF_KEEP);

			// Store gadget
			if (lister->down_gadget!=msg->IAddress)
			{
				lister->down_gadget=msg->IAddress;
				ModifyIDCMP(lister->window,lister->window->IDCMPFlags|IDCMP_INTUITICKS);
				lister->window->Flags|=WFLG_REPORTMOUSE;
			}

			// Is gadget selected?
			if (!(lister->down_gadget->Flags&GFLG_SELECTED))
				break;

			// Look at gadget ID
			switch (lister->down_gadget->GadgetID)
			{
				// Up arrow
				case GAD_VERT_ARROW_UP:
					lister_scroll(lister,0,-1);
					break;

				// Down arrow
				case GAD_VERT_ARROW_DOWN:
					lister_scroll(lister,0,1);
					break;

				// Left arrow
				case GAD_HORIZ_ARROW_LEFT:
					lister_scroll(lister,-1,0);
					break;

				// Right arrow
				case GAD_HORIZ_ARROW_RIGHT:
					lister_scroll(lister,1,0);
					break;

				// Vertical slider
				case GAD_VERT_SCROLLER:
					lister_pos_slider(lister,SLIDER_VERT);
					break;

				// Horizontal slider
				case GAD_HORIZ_SCROLLER:
					lister_pos_slider(lister,SLIDER_HORZ);
					break;
			}
			break;


		// Gadget released
		case IDCMP_GADGETUP:

			// End a direct edit
			lister_end_edit(lister,0);

			// Clear down gadget
			lister->down_gadget=0;
			ModifyIDCMP(lister->window,lister->window->IDCMPFlags&~IDCMP_INTUITICKS);
			lister->window->Flags&=~WFLG_REPORTMOUSE;

			// Look at GadgetID
			switch (gadget->GadgetID)
			{
				// Vertical slider
				case GAD_VERT_SCROLLER:
					lister_pos_slider(lister,SLIDER_VERT);
					break;

				// Horizontal slider
				case GAD_HORIZ_SCROLLER:
					lister_pos_slider(lister,SLIDER_HORZ);
					break;

				// Path
				case GAD_PATH:

					// Hot name requester shown?
					if (lister->hot_name_req)
					{
						// Remove it
						lister_rem_hotname(lister);
					}

					// Check not locked
					else
					if (!(lister->flags&LISTERF_LOCK))
					{
						// Custom handler?
						if (lister->cur_buffer->buf_CustomHandler[0])
						{
							// Send message
							rexx_handler_msg(
								0,
								lister->cur_buffer,
								RXMF_WARN,
								HA_String,0,"path",
								HA_Value,1,lister,
								HA_String,2,lister->path_buffer,
								TAG_END);
							break;
						}

						// Make this window active
						if (!(lister->flags&LISTERF_SOURCEDEST_LOCK))
						{
							// Make us the source
							lister_do_function(lister,MENU_LISTER_SOURCE);

							// If we're not the source now set stored source bit
							if (!(lister->flags&LISTERF_SOURCE))
							lister->flags|=LISTERF_STORED_SOURCE;
						}

						// Check path for FTP
						if (!(lister_check_ftp(lister,lister->path_buffer)))
						{
							// See if path has changed
							if (stricmp(lister->path_buffer,lister->cur_buffer->buf_ExpandedPath))
							{
								// Read directory
								read_directory(lister,lister->path_buffer,GETDIRF_CANMOVEEMPTY|GETDIRF_CANCHECKBUFS);
							}

							// Same path; force reread
							else
							{
								read_directory(lister,lister->path_buffer,0);
							}
						}
					}
					break;


				// Parent
				case GAD_PARENT:
					lister_do_function(
						lister,
						(msg->Qualifier&IEQUAL_ANYSHIFT)?MENU_LISTER_PARENT_NEW:MENU_LISTER_PARENT);
					break;

				// Iconify
				case GAD_ICONIFY:
					lister_do_function(lister,MENU_LISTER_ICONIFY);
					break;


				// Lock format
				case GAD_LOCK:
					if (gadget->Flags&GFLG_SELECTED) lister->more_flags|=LISTERF_LOCK_FORMAT;
					else lister->more_flags&=~LISTERF_LOCK_FORMAT;
					break;

				// Zoom
				case GAD_ZOOM:
					lister_zoom_window(lister);
					break;
			}
			break;


		// Window made inactive
		case IDCMP_INACTIVEWINDOW:

			// Clear pointer to current lister
			Forbid();
			if (GUI->current_lister==lister)
				GUI->current_lister=0;
			Permit();

			// Left button up
			msg->Class=IDCMP_MOUSEBUTTONS;
			msg->Code=SELECTUP;
			msg->Qualifier=0xffff;

			// End a direct edit
			lister_end_edit(lister,0);

		// Mouse button
		case IDCMP_MOUSEBUTTONS:

			// Pass to editor
			if (lister_edit_key(lister,msg))
				break;

			// Clear down gadget
			lister->down_gadget=0;
			ModifyIDCMP(lister->window,lister->window->IDCMPFlags&~IDCMP_INTUITICKS);

			// Hot name requester open?
			if (lister->hot_name_req)
			{
				// Remove it
				lister_rem_hotname(lister);
			}

			// Title button selected?
			if (lister->title_click!=-1)
			{
				// Button released?
				if (msg->Code==SELECTUP)
				{
					// Turn off mouse movements
					lister->window->Flags&=~WFLG_REPORTMOUSE;

					// Dragging separator?
					if (lister->title_sel==2)
					{
						short num,pos,left;

						// Cache item type, get new position
						num=lister->cur_buffer->buf_ListFormat.display_pos[lister->title_click];
						left=lister->title_boxes[lister->title_click].tb_Right-lister->cur_buffer->buf_FieldWidth[num];
						pos=lister->title_drag_x-left+1;

						// Changed significantly?
						if (pos<lister->cur_buffer->buf_FieldWidth[num]-1 || pos>lister->cur_buffer->buf_FieldWidth[num]+1)
						{
							// Non-proportional font?
							if (!(lister->more_flags&LISTERF_PROP_FONT))
							{
								// Make sure field width is a multiple of the font width
								pos=((pos+lister->text_area.rast.TxWidth-1)/lister->text_area.rast.TxWidth)*lister->text_area.rast.TxWidth;
							}

							// Check width has changed
							if (lister->cur_buffer->buf_FieldWidth[num]!=pos)
							{
								// Get new field width
								lister->cur_buffer->buf_FieldWidth[num]=pos;

								// Set custom width bit
								lister->cur_buffer->buf_CustomWidthFlags|=1<<num;

								// Refresh lister
								lister_refresh_display(lister,REFRESHF_SLIDERS);
							}

							// Not refreshing, so remove separator highlight
							else lister_title_highlight_sep(lister);
						}

						// Didn't move
						else
						{
							// Not refreshing, so remove separator highlight
							lister_title_highlight_sep(lister);

							// Check for doubleclick
							if (DoubleClick(
								lister->sep_old_seconds,lister->sep_old_micros,
								lister->seconds,lister->micros))
							{
								// Custom bit set?
								if (lister->cur_buffer->buf_CustomWidthFlags&(1<<num))
								{
									// Clear custom width bit
									lister->cur_buffer->buf_CustomWidthFlags&=~(1<<num);

									// Refresh lister
									lister_refresh_display(lister,REFRESHF_SLIDERS);
								}
							}

							// Not double-click
							else
							{
								lister->sep_old_seconds=lister->seconds;
								lister->sep_old_micros=lister->micros;
							}
						}
					}

					// Was dragging?
					else
					if (lister->drag_info)
					{
						// Free drag stuff
						FreeDragInfo(lister->drag_info);
						lister->drag_info=0;

						// Do drop?
						if (msg->Qualifier!=0xffff)
						{
							// Handle drop
							lister_title_drop(lister,lister->title_click,msg->MouseX,msg->MouseY);
						}
					}

					// Change sort
					else
					{
						// Highlight box back to normal
						lister_title_highlight(lister,lister->title_click,(lister->title_sel==2)?-1:0);

						// Do sort?
						if (msg->Qualifier!=0xffff)
						{
							// Change sort
							lister_set_sort(lister,lister->title_boxes[lister->title_click].tb_Item,msg->Qualifier);
						}
					}

					// Clear value
					lister->title_click=-1;
				}

				// Cancel title size
				else
				if (msg->Code==MENUDOWN && lister->title_sel==2)
				{
					// Remove separator
					lister_title_highlight_sep(lister);
					lister->title_click=-1;
				}

				// Cancel drag
				else
				if (msg->Code==MENUDOWN && lister->drag_info)
				{
					// Free drag stuff
					FreeDragInfo(lister->drag_info);
					lister->drag_info=0;

					// Clear value
					lister->title_click=-1;
				}
				break;
			}

			// Unless we're playing with a toolbar, turn off mouse movements
			if (!(lister->flags&LISTERF_TOOLBAR) || lister->tool_sel==-1)
				lister->window->Flags&=~WFLG_REPORTMOUSE;

			// Right button?
			if (msg->Code==MENUDOWN)
			{
				// Click in lister?
				if (point_in_element(
					&lister->list_area,
					msg->MouseX,
					msg->MouseY))
				{
					// Double-click test
					if (DoubleClick(
						lister->rmb_old_seconds,lister->rmb_old_micros,
						lister->seconds,lister->micros))
					{
						// Edit lister format
						if (!(lister->flags&LISTERF_LOCK) &&
							!(environment->env->lister_options&LISTEROPTF_2XCLICK))
							lister_configure(lister);

						// Progress window to front
						else
						if (lister->progress_window)
							ShowProgressWindow(lister->progress_window,0,0);
					}

					// Not double-click
					else
					{
						lister->rmb_old_seconds=lister->seconds;
						lister->rmb_old_micros=lister->micros;
						select_rmb_scroll(lister,lister->window->MouseX,lister->window->MouseY);
					}
					break;
				}
			}

			// Click on title?
			else
			if (lister->more_flags&LISTERF_TITLE && msg->Code==SELECTDOWN &&
				point_in_element(
					&lister->title_area,
					msg->MouseX,
					msg->MouseY))
			{
				short item,ok=0;

				// Find which item
				for (item=0;item<16;item++)
				{
					// Last item?
					if (lister->title_boxes[item].tb_Left==-1 ||
						lister->title_boxes[item].tb_Item==-1)
					{
						if (ok)
						{
							item=16;
							break;
						}
						continue;
					}

					// Skip an item?
					if (lister->title_boxes[item].tb_Left==-2)
						continue;

					// Set flag
					ok=1;

					// Within item?
					if (msg->MouseX>=lister->title_boxes[item].tb_Left &&
						msg->MouseX<=lister->title_boxes[item].tb_Right+2) break;
				}

				// Got one?
				if (item<16 && lister->title_boxes[item].tb_Item>-1)
				{
					// Remember it
					lister->title_click=item;
					lister->title_click_x=msg->MouseX;
					lister->title_click_y=msg->MouseY;

					// Click is on title separators?
					if (item<15 &&
						msg->MouseX>=lister->title_boxes[item].tb_Right-2 &&
						msg->MouseX<=lister->title_boxes[item].tb_Right+2)
					{
						// Flag indicates separator is selected
						lister->title_sel=2;
						lister->title_drag_x=lister->title_boxes[item].tb_Right;
					}

					// Normal header item selected
					else
						lister->title_sel=1;

					// Highlight box
					lister_title_highlight(lister,item,lister->title_sel);

					// Turn on mouse movements
					lister->window->Flags|=WFLG_REPORTMOUSE;
					break;
				}
			}

			// Not locked?
			if (!(lister->flags&LISTERF_LOCK))
			{
				// Right button over borders?
				if (msg->Code==MENUDOWN &&
					cx_mouse_outside(lister->window,msg->MouseX,msg->MouseY))
				{
					// Do popup menu
					backdrop_popup(
						lister->backdrop_info,
						msg->MouseX,
						msg->MouseY,
						msg->Qualifier,
						0,
						0,
						0);
				}

				// Click on toolbar?
				else
				if (lister->flags&LISTERF_TOOLBAR &&
					(lister->tool_sel!=-1 ||
						point_in_element(
							&lister->toolbar_area,
							msg->MouseX,
							msg->MouseY)))
				{
					// Handle the click
					lister_toolbar_click(lister,msg->MouseX,msg->MouseY,msg->Code,msg->Qualifier);
				}
					
				// Click on title bar?
				else
				if (msg->Code==SELECTDOWN &&
					point_in_element(
						&lister->name_area,
						msg->MouseX,
						msg->MouseY))
				{
					// Alt turns lister off
					if (msg->Qualifier&(IEQUALIFIER_LALT|IEQUALIFIER_RALT))
					{
						lister_do_function(lister,MENU_LISTER_OFF);
					}

					// Control does locking
					else
					if (msg->Qualifier&IEQUALIFIER_CONTROL)
					{
						// Is lister already locked?
						if (lister->flags&LISTERF_SOURCEDEST_LOCK)
							lister_do_function(lister,MENU_LISTER_UNLOCK);

						// Otherwise, lock lister
						else
						{
							// Lock as destination
							if (lister->flags&LISTERF_DEST ||
								(msg->Qualifier&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)))
								lister_do_function(lister,MENU_LISTER_LOCK_DEST);

							// Lock as source
							else lister_do_function(lister,MENU_LISTER_LOCK_SOURCE);
						}
					}

					// Otherwise, lister must be unlocked to do anything
					else
					if (!(lister->flags&LISTERF_SOURCEDEST_LOCK))
					{
						// Normal selection - destination if shift is down
						if (msg->Qualifier&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT))
							lister_do_function(lister,MENU_LISTER_DEST);

						// Otherwise, select as source
						else lister_do_function(lister,MENU_LISTER_SOURCE);
					}

					// Update window free space
					lister_refresh_name(lister);
				}

				// Or click on status display?
				else
				if (msg->Code==SELECTDOWN &&
					point_in_element(
						&lister->status_area,
						msg->MouseX,
						msg->MouseY))
				{
					USHORT res;

					// Do pop-up menu
					res=lister_listerpopup(lister,msg->Code);

					// Do function
					if (res!=(USHORT)-1)
					{
						// Help?
						if (res&POPUP_HELPFLAG) help_menu_help(res&~POPUP_HELPFLAG,0);

						// Otherwise call function
						else lister_do_function(lister,res);
					}
				}

				// Or on parent button?
				else
				if (msg->Code==SELECTDOWN &&
					point_in_element(
						&lister->parent_area,
						msg->MouseX,
						msg->MouseY))
				{
					// Show parent menu
					lister_parent_popup(lister,msg->Code);
				}

				// Command button? (Lister menu)
				else
				if (msg->Code==SELECTDOWN &&
					point_in_element(
						&lister->command_area,
						msg->MouseX,
						msg->MouseY) &&
					lister->lister_tool_menu)
				{
					USHORT res;
					PopUpItem *item;
					BOOL help=0;

					// Get menu lock
					GetSemaphore(&GUI->lister_menu_lock,SEMF_SHARED,0);

					// Do pop-up menu
					if ((res=DoPopUpMenu(lister->window,&lister->lister_tool_menu->ph_Menu,&item,msg->Code))!=(USHORT)-1)
					{
						// Help?
						if (res&POPUP_HELPFLAG) help=1;

						// Get item; check it's not a sub-item header
						if (item && !(item->flags&POPUPF_SUB))
						{
							Cfg_Function *function;

							// Get function
							function=(Cfg_Function *)item->data;

							// Help?
							if (help) help_button_help(0,function,msg->Qualifier,0,0);

							// Otherwise
							else
							{
								// Launch function
								function_launch_quick(
									FUNCTION_RUN_FUNCTION,
									function,
									lister);
							}
						}
					}

					// Release menu lock
					FreeSemaphore(&GUI->lister_menu_lock);
				}

				// Or click in lister?
				else
				if (point_in_element(
					&lister->list_area,
					msg->MouseX,
					msg->MouseY))
				{
					// Left button does select
					if (msg->Code==SELECTDOWN)
					{
						select_select_files(
							lister,
							msg->Qualifier,
							msg->MouseX,
							msg->MouseY);
					}

					// Middle button does edit, if allowed
					else
					if (msg->Code==MIDDLEDOWN && environment->env->lister_options&LISTEROPTF_EDIT_MID)
					{
						BPTR lock;
						struct InfoData __aligned info;

						// Clear flag
						info.id_DiskState=ID_VALIDATED;

						// Lock current path
						if (lock=Lock("",ACCESS_READ))
						{
							// Get information
							Info(lock,&info);
							UnLock(lock);
						}

						// Disk writeable?
						if (info.id_DiskState==ID_VALIDATED)
						{
							// Activate window if not active
							if (!(lister->window->Flags&WFLG_WINDOWACTIVE))
								ActivateWindow(lister->window);

							// Start edit
							lister_start_edit(lister,msg->MouseX,msg->MouseY,-1);
						}
					}
				}
			}
			break;
	}
}
