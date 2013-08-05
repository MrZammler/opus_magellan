#include "config_lib.h"
#include "config_filetypes.h"
#include "config_buttons.h"

#define DOpusBase		(data->func_startup.dopus_base)
#define Locale			(data->func_startup.locale)

/*
#define DOSBase			(data->func_startup.dos_base)
#define IntuitionBase	(data->func_startup.int_base)
#define GfxBase			(data->func_startup.gfx_base)
#define AslBase			(data->func_startup.asl_base)
#define WorkbenchBase	(data->func_startup.wb_base)
#define UtilityBase		(data->func_startup.util_base)
#define LayersBase		(data->func_startup.layers_base)
*/

static BOOL __stdargs L_AslRequestTags(filetype_ed_data *data,APTR requester,Tag tag1,...);

void FiletypeEditor(void)
{
	filetype_ed_data *data=0;
	IPCData *ipc;
	short success=0,pending_quit=0;
	BOOL change_flag=0;

	// Do startup
	if (!(ipc=Local_IPC_ProcStartup((ULONG *)&data,_filetypeed_init)))
		return;

	// Fix A4
	putreg(REG_A4,data->func_startup.a4);

	// Create App stuff
	if (data->app_port=CreateMsgPort())
	{
		data->app_window=AddAppWindowA(0,0,data->window,data->app_port,0);
	}

	// Get icon image
	data->icon_image=OpenImage(data->type->icon_path,0);

	// Show icon image
	filetypeed_show_icon(data);

	// Launch class editor immediately?
	if (data->edit_flag) filetypeed_edit_definition(data);

	// Message loop
	FOREVER
	{
		IPCMessage *msg;
		struct IntuiMessage *imsg;
		short break_flag=0;

		// Check drag
		if (config_drag_check(&data->drag))
		{
			// End drag
			filetypeed_end_drag(data,0);
		}

		// Task message?
		while (msg=(IPCMessage *)GetMsg(ipc->command_port))
		{
			switch (msg->command)
			{
				// Close message?
				case IPC_QUIT:
					if (!pending_quit)
					{
						success=msg->flags;
						break_flag=1;
					}
					break;


				// Activate
				case IPC_ACTIVATE:
					if (data->window)
					{
						WindowToFront(data->window);
						ActivateWindow(data->window);
					}
					break;


				// Editor saying goodbye
				case IPC_GOODBYE:
					{
						ULONG which;

						// What's just gone?
						which=IPC_GetGoodbye(msg);

						// Class editor?
						if (which==(ULONG)-1) data->class_editor=0;

						// Icon menu editor?
						else
						if (which>15)
						{
							Att_Node *node;

							// Go through icon list
							for (node=(Att_Node *)data->icon_list->list.lh_Head;
								node->node.ln_Succ;
								node=(Att_Node *)node->node.ln_Succ)
							{
								// Match function
								if (((func_node *)node->data)->func==(Cfg_Function *)which)
								{
									// Clear editor pointer
									((func_node *)node->data)->editor=0;

									// Check for invalid function
									if (filetypeed_check_iconmenu(data,node,FALSE))
										change_flag=1;
									break;
								}
							}
						}

						// Normal filetype editor
						else data->editor[which]=0;
					}
					break;


				// Editor returning a function
				case FUNCTIONEDIT_RETURN:
					{
						short ret;

						if (ret=filetypeed_receive_edit(
							data,
							(FunctionReturn *)msg->data))
						{
							change_flag=1;
							filetypeed_update_actions(data);
							if (ret==2) filetypeed_update_iconmenu(data);
						}
					}
					break;


				// Class editor returning
				case CLASSEDIT_RETURN:
					filetypeed_receive_class(data,(Cfg_Filetype *)msg->data);
					change_flag=1;
					break;


				// Get a copy of a button
				case BUTTONEDIT_CLIP_BUTTON:

					// Handle this button
					if (filetypeed_get_button(data,(Cfg_Button *)msg->data,(Point *)msg->data_free))
						change_flag=1;
					break;
			}

			// Reply the message
			IPC_Reply(msg);
		}

		// Intuimessage
		if (data->window)
		{
			while (imsg=GetWindowMsg(data->window->UserPort))
			{
				struct IntuiMessage msg_copy;
				struct Gadget *gadget;
				struct TagItem *tags;

				// Copy message
				msg_copy=*imsg;

				// Don't reply to IDCMPUPDATE messages just yet
				if (imsg->Class!=IDCMP_IDCMPUPDATE)
				{
					ReplyWindowMsg(imsg);
					imsg=0;
				}

				// Get gadget and tag pointers
				gadget=(struct Gadget *)msg_copy.IAddress;
				tags=(struct TagItem *)gadget;

				// Look at message
				switch (msg_copy.Class)
				{
					// Close window
					case IDCMP_CLOSEWINDOW:
						if (!pending_quit) break_flag=1;
						break;


					// Gadget
					case IDCMP_GADGETUP:
						switch (gadget->GadgetID)
						{
							// Use
							case GAD_FILETYPEED_USE:
								success=1;

							// Cancel
							case GAD_FILETYPEED_CANCEL:
								if (!pending_quit) break_flag=1;
								break;


							// Select a function
							case GAD_FILETYPEED_ACTION_LIST:
								{
									Att_Node *node;

									// Get selected node
									if (!(node=Att_FindNode(data->action_list,msg_copy.Code)))
										break;

									// Enable edit action button
									DisableObject(data->objlist,GAD_FILETYPES_EDIT_ACTION,FALSE);

									// Double-click?
									if (!(DoubleClick(data->last_sec,data->last_mic,msg_copy.Seconds,msg_copy.Micros)) ||
										node!=data->last_sel)
									{
										data->last_sec=msg_copy.Seconds;
										data->last_mic=msg_copy.Micros;
										data->last_sel=node;
										data->last_icon=0;
										break;
									}
								}

								// Fall through

							case GAD_FILETYPES_EDIT_ACTION:

								// No current selection?
								if (!data->last_sel) break;

								// Is editor already up for this action?	
								if (data->editor[data->last_sel->data])
									IPC_Command(data->editor[data->last_sel->data],IPC_ACTIVATE,0,0,0,0);

								// Need to launch editor
								else filetypeed_edit_action(data,data->last_sel->data,data->last_sel->node.ln_Name);
								break;


							// Delete action
							case GAD_FILETYPES_DEL_ACTION:

								// No current selection?
								if (!data->last_sel) break;

								// Is editor up for this action?	
								if (data->editor[data->last_sel->data])
									IPC_Command(data->editor[data->last_sel->data],IPC_QUIT,0,0,0,0);

								// Delete it
								if (filetypeed_del_action(data,data->last_sel->data))
									change_flag=1;
								break;


							// Edit filetype definition
							case GAD_FILETYPEED_EDIT_CLASS:

								// Is class editor already up for this action?	
								if (data->class_editor)
									IPC_Command(data->class_editor,IPC_ACTIVATE,0,0,0,0);

								// Need to launch editor
								else filetypeed_edit_definition(data);
								break;


							// Select icon
							case GAD_FILETYPEED_SELECT_ICON:
								if (filetypeed_pick_icon(data))
									change_flag=1;
								break;


							// Add to icon menu
							case GAD_FILETYPES_ADD_ICON_MENU:
								filetypeed_add_iconmenu(data);
								break;


							// Select an icon menu
							case GAD_FILETYPES_ICON_MENU:
								{
									Att_Node *last=data->last_icon;

									// Handle selection
									if (!(filetypeed_sel_icon(data,msg_copy.Code))) break;

									// Double-click?
									if (data->last_icon!=last ||
										!(DoubleClick(
											data->last_sec,
											data->last_mic,
											msg_copy.Seconds,
											msg_copy.Micros)))
									{
										data->last_sec=msg_copy.Seconds;
										data->last_mic=msg_copy.Micros;
										data->last_sel=0;
										break;
									}
								}

								// Fall through

							case GAD_FILETYPES_EDIT_ICON_MENU:

								// No current selection?
								if (!data->last_icon) break;

								// Edit it
								filetypeed_edit_iconmenu(data,data->last_icon);
								break;


							// Delete from icon menu
							case GAD_FILETYPES_DEL_ICON_MENU:

								// No current selection?
								if (!data->last_icon) break;

								// Delete function
								if (filetypeed_check_iconmenu(data,data->last_icon,TRUE))
									change_flag=1;
								break;
						}
						break;


					// BOOPSI message
					case IDCMP_IDCMPUPDATE:
						{
							short item;

							// Icon list?
							if (GetTagData(GA_ID,0,tags)!=GAD_FILETYPES_ICON_MENU) break;

							// Get item
							if ((item=GetTagData(DLV_DragNotify,-1,tags))!=-1)
							{
								// Handle selection
								filetypeed_sel_icon(data,item);

								// Start the drag
								config_drag_start(&data->drag,data->icon_list,item,tags,TRUE);
							}
						}
						break;


					// Ticks
					case IDCMP_INTUITICKS:
						++data->drag.tick_count;
						break;


					// Mouse move
					case IDCMP_MOUSEMOVE:

						// Handle drag move
						config_drag_move(&data->drag);
						break;


					// Mouse buttons
					case IDCMP_MOUSEBUTTONS:

						// Valid drag info?
						if (data->drag.drag)
						{
							short ok=-1;

							// Dropped ok?
							if (msg_copy.Code==SELECTUP)
							{
								// Remember last position
								data->drag.drag_x=data->window->WScreen->MouseX;
								data->drag.drag_y=data->window->WScreen->MouseY;
								ok=1;
							}

							// Aborted
							else
							if (msg_copy.Code==MENUDOWN) ok=0;

							// End drag?
							if (ok!=-1 && filetypeed_end_drag(data,ok)) change_flag=1;
						}
						break;


					// Key press
					case IDCMP_RAWKEY:

						// Help?
						if (msg_copy.Code==0x5f &&
							!(msg_copy.Qualifier&VALID_QUALIFIERS))
						{
							// Set busy pointer
							SetWindowBusy(data->window);

							// Send help command
							IPC_Command(data->func_startup.main_owner,IPC_HELP,(1<<31),"File Type Editor",0,REPLY_NO_PORT);

							// Clear busy pointer
							ClearWindowBusy(data->window);
						}
						break;
				}

				// Reply to outstanding messages
				if (imsg) ReplyWindowMsg(imsg);
			}

			// Check break flag
			if (break_flag || pending_quit)
			{
				// See if all the editors are gone
				if (IsListEmpty(&data->proc_list.list))
					break;

				// Send quit?
				if (break_flag)
				{
					IPC_ListQuit(&data->proc_list,0,success,FALSE);
					SetWindowBusy(data->window);
				}
				pending_quit=1;
			}
		}

		// AppMessage
		if (data->app_window)
		{
			struct AppMessage *msg;

			while (msg=(struct AppMessage *)GetMsg(data->app_port))
			{
				// Got an argument?
				if (msg->am_NumArgs>0)
				{
					char name[256];
					short len;
					APTR image;

					// Get full name
					NameFromLock(msg->am_ArgList[0].wa_Lock,name,256);
					if (msg->am_ArgList[0].wa_Name &&
						*msg->am_ArgList[0].wa_Name)
						AddPart(name,msg->am_ArgList[0].wa_Name,256);

					// Add .info
					if ((len=strlen(name))<6 ||
						stricmp(name+len-5,".info")!=0) strcat(name,".info");

					// Try to get image
					if (image=OpenImage(name,0))
					{
						// Store path
						FreeMemH(data->type->icon_path);
						if (data->type->icon_path=AllocMemH(0,strlen(name)+1))
							strcpy(data->type->icon_path,name);

						// Free existing image
						CloseImage(data->icon_image);
						data->icon_image=image;

						// Show new image
						filetypeed_show_icon(data);
						change_flag=1;
					}
				}

				// Reply message
				ReplyMsg((struct Message *)msg);
			}
		}

		Wait(1<<ipc->command_port->mp_SigBit|
			1<<data->drag.timer->port->mp_SigBit|
			((data->window)?(1<<data->window->UserPort->mp_SigBit):0)|
			((data->app_window)?(1<<data->app_port->mp_SigBit):0));
	}

	// End any drag in progress
	filetypeed_end_drag(data,0);

	// Need to send button back?
	if (success==1 && change_flag)
	{
		if (IPC_Command(
			data->owner_ipc,
			FILETYPEEDIT_RETURN,
			(ULONG)data->type,
			data->node,
			0,
			REPLY_NO_PORT))
		{
			data->node=0;
		}
	}

	// Free edit filetype
	FreeFiletype(data->type);

	// Remove AppWindow
	RemoveAppWindow(data->app_window);

	// Close window
	CloseConfigWindow(data->window);

	// Close app port
	if (data->app_port)
	{
		struct Message *msg;
		while (msg=GetMsg(data->app_port))
			ReplyMsg(msg);
		DeleteMsgPort(data->app_port);
	}

	// Say goodbye
	IPC_Goodbye(ipc,data->owner_ipc,(success==-1)?0:(ULONG)data->node);

	// Free icon image
	CloseImage(data->icon_image);

	// Close timer
	FreeTimer(data->drag.timer);

	// Free data
	IPC_Free(ipc);
	Att_RemList(data->action_list,0);
	Att_RemList(data->icon_list,REMLIST_FREEDATA);
	FreeVec(data);
}


// Editor init
ULONG __asm _filetypeed_init(
	register __a0 IPCData *ipc,
	register __a1 filetype_ed_data *data)
{
	short a;

	// Fix A4
	putreg(REG_A4,data->func_startup.a4);

	// Store IPC pointer
	data->ipc=ipc;

	// Fill in new window
	data->new_win.title=data->type->type.name;
	data->new_win.flags=WINDOW_VISITOR|WINDOW_REQ_FILL|WINDOW_AUTO_KEYS;

	// Create timer
	if (!(data->drag.timer=AllocTimer(UNIT_VBLANK,0)))
		return 0;

	// Open window, create action list
	if (!(data->window=OpenConfigWindow(&data->new_win)) ||
		!(data->objlist=AddObjectList(data->window,data->obj_def)) ||
		!(data->action_list=Att_NewList(0)))
	{
		CloseConfigWindow(data->window);
		FreeTimer(data->drag.timer);
		return 0;
	}

	// Store window for drag
	data->drag.window=data->window;

	// Set window ID
	SetWindowID(data->window,0,WINDOW_BUTTON_CONFIG,(struct MsgPort *)data->ipc);

	// Build action list
	for (a=0;data->action_lookup[a];a+=2)
	{
		Att_NewNode(
			data->action_list,
			GetString(Locale,data->action_lookup[a]),
			data->action_lookup[a+1],
			0);
	}

	// Disable edit and delete buttons
	DisableObject(data->objlist,GAD_FILETYPES_EDIT_ACTION,TRUE);

	// Update action and icon list
	filetypeed_update_actions(data);
	filetypeed_update_iconmenu(data);

	// Initialise process list
	InitListLock(&data->proc_list,0);
	return 1;
}


// Update checkmarks in action list
void filetypeed_update_actions(filetype_ed_data *data)
{
	Att_Node *node;

	// Detach list from listview
	SetGadgetChoices(data->objlist,GAD_FILETYPEED_ACTION_LIST,(APTR)~0);

	// Go through action list
	for (node=(Att_Node *)data->action_list->list.lh_Head;
		node->node.ln_Succ;
		node=(Att_Node *)node->node.ln_Succ)
	{
		Cfg_Function *func;

		// Initially not selected
		node->node.lve_Flags&=~LVEF_SELECTED;

		// Go through function list
		for (func=(Cfg_Function *)data->type->function_list.lh_Head;
			func->node.ln_Succ;
			func=(Cfg_Function *)func->node.ln_Succ)
		{
			// Right type?
			if (func->function.func_type==node->data &&
				!(func->function.flags2&FUNCF2_LABEL_FUNC))
			{
				node->node.lve_Flags|=LVEF_SELECTED;
				break;
			}
		}
	}

	// Reattach list
	SetGadgetChoices(data->objlist,GAD_FILETYPEED_ACTION_LIST,data->action_list);
}


// Edit a filetype action
void filetypeed_edit_action(
	filetype_ed_data *data,
	short action,
	char *name)
{
	Cfg_Function *function;
	FunctionStartup *startup;
	BOOL success=0;

	// Allocate startup data
	if (!(startup=AllocVec(sizeof(FunctionStartup),MEMF_CLEAR)))
		return;

	// Find the function we want to edit
	if (!(function=FindFunctionType(&data->type->function_list,action)))
	{
		// Allocate a new function
		function=NewFunction(0,action);
	}

	// Otherwise, copy the function
	else function=CopyFunction(function,0,0);

	// Valid function?
	if (function)
	{
		// Copy startup data
		*startup=data->func_startup;
		startup->window=data->window;
		startup->function=function;
		startup->owner_ipc=data->ipc;
		startup->object=data->type;
		startup->object_flags=action;

		// Build title
		lsprintf(startup->title,"%s : %s",data->type->type.name,name);

		// Launch editor
		if ((IPC_Launch(
			&data->proc_list,
			&data->editor[action],
			"dopus_function_editor",
			(ULONG)FunctionEditor,
			STACK_DEFAULT,
			(ULONG)startup,
			(struct Library *)DOSBase)) && data->editor[action]) success=1;
	}

	// Free data if not successful
	if (!success)
	{
		FreeFunction(function);
		FreeVec(startup);
	}
}


// Receive edit from function editor
filetypeed_receive_edit(
	filetype_ed_data *data,
	FunctionReturn *ret)
{
	Cfg_Function *function=0;
	Cfg_Function *func_copy=0;
	BOOL no_func=0,icon=0;
	func_node *fndata=0;
	short success=0;

	// Is the new function empty?
	if (IsListEmpty((struct List *)&ret->function->instructions))
		no_func=1;

	// Try to copy the new function
	if (no_func ||
		(func_copy=CopyFunction(ret->function,0,0)))
	{
		// Icon menu? (this is a bit kludgy)
		if (ret->object_flags>15)
		{
			Att_Node *node;

			// Go through icon list
			for (node=(Att_Node *)data->icon_list->list.lh_Head;
				node->node.ln_Succ;
				node=(Att_Node *)node->node.ln_Succ)
			{
				// Match function
				if (((func_node *)node->data)->func==(Cfg_Function *)ret->object_flags)
				{
					// Get function pointer
					function=(Cfg_Function *)ret->object_flags;
					break;
				}
			}

			icon=1;
		}

		// Otherwise, find the function we want to replace
		else function=FindFunctionType(&data->type->function_list,ret->object_flags);

		// Add new function to the list
		if (func_copy)
		{
			// Fix type
			if (!icon) func_copy->function.func_type=ret->object_flags;

			// Add to list, in same position if applicable
			if (function) Insert(&data->type->function_list,&func_copy->node,&function->node);
			else AddTail(&data->type->function_list,&func_copy->node);
		}

		// Free old function
		if (function)
		{
			// Remove function and free it
			Remove(&function->node);
			FreeFunction(function);
		}

		// Store for icon menu
		if (fndata) fndata->func=func_copy;

		success=(icon)?2:1;
	}

	return success;
}


// Edit a filetype definition
void filetypeed_edit_definition(filetype_ed_data *data)
{
	fileclass_ed_data *startup;
	BOOL success=0;

	// Allocate startup data
	if (!(startup=AllocVec(sizeof(fileclass_ed_data),MEMF_CLEAR)) ||
		!(startup->type=CopyFiletype(data->type,0)))
	{
		FreeVec(startup);
		return;
	}

	// Supply IPC ports
	startup->owner_ipc=data->ipc;
	startup->main_owner=data->func_startup.main_owner;

	// Supply some library pointers
	startup->dopus_base=DOpusBase;
	startup->dos_base=(struct Library *)DOSBase;
	startup->int_base=(struct Library *)IntuitionBase;
	startup->gfx_base=(struct Library *)GfxBase;
	startup->asl_base=AslBase;

	// Fill out new window
	startup->new_win.parent=data->window;
	startup->new_win.dims=data->class_win;
	startup->new_win.title=data->type->type.name;
	startup->new_win.locale=Locale;
	startup->new_win.flags=WINDOW_VISITOR|WINDOW_REQ_FILL|WINDOW_AUTO_KEYS;

	// Supply object pointers
	startup->obj_def=data->class_obj;
	startup->lookup=data->class_lookup;
	startup->strings=data->class_strings;

	// Initialise reader list
	NewList((struct List *)&startup->readers);

	// Launch editor
	if ((IPC_Launch(
		&data->proc_list,
		&data->class_editor,
		"dopus_class_editor",
		(ULONG)FileclassEditor,
		STACK_DEFAULT,
		(ULONG)startup,
		(struct Library *)DOSBase)) && data->class_editor) success=1;

	// Free data if not successful
	if (!success)
	{
		FreeFiletype(startup->type);
		FreeVec(startup);
	}
}


// Receive an edited class definition
void filetypeed_receive_class(
	filetype_ed_data *data,
	Cfg_Filetype *type)
{
	// Store name, id and priority
	strcpy(data->type->type.name,type->type.name);
	strcpy(data->type->type.id,type->type.id);
	data->type->type.priority=type->type.priority;

	// Free recognition string
	FreeMemH(data->type->recognition);
	data->type->recognition=0;

	// Allocate new recognition
	if (type->recognition &&
		(data->type->recognition=AllocMemH(0,strlen(type->recognition)+1)))
		strcpy(data->type->recognition,type->recognition);

	// Set new window title
	if (data->window)
	{
		SetWindowTitles(data->window,data->type->type.name,(char *)-1);
	}
}


// Show icon image
void filetypeed_show_icon(filetype_ed_data *data)
{
	struct Rectangle bounds;
	struct TagItem tags[2];
	ImageRemap remap;

	// Clear icon area
	SetGadgetValue(data->objlist,GAD_FILETYPEED_ICON_AREA,0);

	// Valid icon?
	if (!data->icon_image) return;

	// Get icon area
	if (!(GetObjectRect(data->objlist,GAD_FILETYPEED_ICON_AREA,&bounds)))
		return;

	// Move bounds inwards
	bounds.MinX+=2;
	bounds.MinY+=1;
	bounds.MaxX-=2;
	bounds.MaxY-=1;

	// Fill out render tags
	tags[0].ti_Tag=IM_Rectangle;
	tags[0].ti_Data=(ULONG)&bounds;
	tags[1].ti_Tag=TAG_DONE;

	// Remap the icon
	remap.ir_PenArray=0;
	RemapImage(data->icon_image,data->window->WScreen,&remap);

	// Display icon
	RenderImage(data->window->RPort,data->icon_image,0,0,tags);

	// Free icon remap
	FreeRemapImage(data->icon_image,&remap);
	FreeImageRemap(&remap);
}


// Pick icon image
BOOL filetypeed_pick_icon(filetype_ed_data *data)
{
	char path[256],file[32],*ptr,pattern[10];
	BOOL ret=0;

	// Make window busy
	SetWindowBusy(data->window);

	// Get current path
	path[0]=0;
	file[0]=0;
	if (data->type->icon_path)
	{
		strcpy(path,data->type->icon_path);
		if (ptr=FilePart(path))
		{
			strcpy(file,ptr);
			*ptr=0;
		}
	}

	// Build pattern
	ParsePatternNoCase("#?.info",pattern,10);

	// Display requester
	if (L_AslRequestTags(data,DATA(data->window)->request,
		ASLFR_Flags1,FRF_PRIVATEIDCMP,
		ASLFR_Window,data->window,
		ASLFR_TitleText,GetString(Locale,MSG_FILETYPE_PICK_ICON),
		ASLFR_InitialDrawer,path,
		ASLFR_InitialFile,file,
		ASLFR_InitialPattern,"#?.info",
		ASLFR_AcceptPattern,pattern,
		TAG_END))
	{
		// Free existing path
		FreeMemH(data->type->icon_path);
		data->type->icon_path=0;

		// Build new path
		strcpy(path,DATA(data->window)->request->fr_Drawer);
		AddPart(path,DATA(data->window)->request->fr_File,256);

		// Throw it away if it's not an icon
		if (!(ptr=strstr(path,".info"))) path[0]=0;

		// Store new icon path
		if (path[0] &&
			(data->type->icon_path=AllocMemH(0,strlen(path)+1)))
			strcpy(data->type->icon_path,path);

		// Free existing image
		CloseImage(data->icon_image);

		// Get new image
		if (path[0])
			data->icon_image=OpenImage(path,0);
		else data->icon_image=0;

		// Show new image
		filetypeed_show_icon(data);
		ret=1;
	}

	// Make window unbusy
	ClearWindowBusy(data->window);

	return ret;
}


// varargs AslRequestTags
static BOOL __stdargs L_AslRequestTags(
	filetype_ed_data *data,
	APTR requester,
	Tag tag1,...)
{
	return AslRequest(requester,(struct TagItem *)&tag1);
}


// Update icon menu list
void filetypeed_update_iconmenu(filetype_ed_data *data)
{
	Cfg_Function *func;

	// Detach list from listview
	SetGadgetChoices(data->objlist,GAD_FILETYPES_ICON_MENU,(APTR)~0);

	// Delete icon list
	Att_RemList(data->icon_list,REMLIST_FREEDATA);

	// Create new list
	if (data->icon_list=Att_NewList(0))
	{
		// Go through filetype functions
		for (func=(Cfg_Function *)data->type->function_list.lh_Head;
			func->node.ln_Succ;
			func=(Cfg_Function *)func->node.ln_Succ)
		{
			// Label function?
			if (func->function.flags2&FUNCF2_LABEL_FUNC)
			{
				Cfg_Instruction *ins;

				// Go through instruction list
				for (ins=(Cfg_Instruction *)func->instructions.mlh_Head;
					ins->node.mln_Succ;
					ins=(Cfg_Instruction *)ins->node.mln_Succ)
				{
					// Label?
					if (ins->type==INST_LABEL)
					{
						func_node *node;

						// Allocate node data
						if (node=AllocVec(sizeof(func_node),MEMF_CLEAR))
						{
							// Fill out data
							node->func=func;

							// Create node
							if (!(Att_NewNode(
								data->icon_list,
								ins->string,
								(ULONG)node,
								0))) FreeVec(node);
						}
						break;
					}
				}
			}
		}
	}

	// Attach list
	SetGadgetChoices(data->objlist,GAD_FILETYPES_ICON_MENU,data->icon_list);

	// No selection
	filetypeed_no_iconsel(data);
}


// Add to icon menu
void filetypeed_add_iconmenu(filetype_ed_data *data)
{
	Cfg_Function *func;
	Att_Node *node;
	func_node *fndata;

	// Allocate a new function and data
	if (!(func=NewFunction(0,FTYPE_LIST)) ||
		!(fndata=AllocVec(sizeof(func_node),MEMF_CLEAR)))
	{
		FreeFunction(func);
		return;
	}

	// Detach list from listview
	SetGadgetChoices(data->objlist,GAD_FILETYPES_ICON_MENU,(APTR)~0);

	// Fill out data
	fndata->func=func;
	func->function.flags2|=FUNCF2_LABEL_FUNC;

	// Add to lister
	if (!(node=Att_NewNode(data->icon_list,0,(ULONG)fndata,0)))
	{
		// Failed
		FreeVec(fndata);
		FreeFunction(func);
	}

	// Add function to list
	else AddTail(&data->type->function_list,&func->node);

	// Attach list
	SetGadgetChoices(data->objlist,GAD_FILETYPES_ICON_MENU,data->icon_list);

	// Edit node
	if (node) filetypeed_edit_iconmenu(data,node);
}


// Edit icon menu node
void filetypeed_edit_iconmenu(filetype_ed_data *data,Att_Node *node)
{
	Cfg_Function *function;
	FunctionStartup *startup;
	func_node *fndata;
	BOOL success=0;

	// Get the function to edit
	if (!node || !(fndata=(func_node *)node->data)) return;

	// Editor already open?
	if (fndata->editor)
	{
		// Bring editor to the front
		IPC_Command(fndata->editor,IPC_ACTIVATE,0,0,0,0);
		return;
	}

	// Allocate startup data
	if (!(startup=AllocVec(sizeof(FunctionStartup),MEMF_CLEAR)))
		return;

	// Copy the function
	if (function=CopyFunction(fndata->func,0,0))
	{
		// Copy startup data
		*startup=data->func_startup;
		startup->window=data->window;
		startup->function=function;
		startup->owner_ipc=data->ipc;
		startup->object=data->type;
		startup->object_flags=(ULONG)fndata->func;
		startup->flags|=FUNCEDF_LABEL;

		// Build title
		lsprintf(startup->title,"%s : %s",data->type->type.name,GetString(Locale,MSG_ICON_MENU));

		// Launch editor
		if ((IPC_Launch(
			&data->proc_list,
			&fndata->editor,
			"dopus_function_editor",
			(ULONG)FunctionEditor,
			STACK_DEFAULT,
			(ULONG)startup,
			(struct Library *)DOSBase)) && fndata->editor) success=1;
	}

	// Free data if not successful
	if (!success)
	{
		FreeFunction(function);
		FreeVec(startup);
	}
}


// Check for empty function
BOOL filetypeed_check_iconmenu(filetype_ed_data *data,Att_Node *node,BOOL del)
{
	func_node *fndata;

	// Get data pointer
	fndata=(func_node *)node->data;

	// Invalid function?
	if (del || !fndata->func || IsListEmpty((struct List *)&fndata->func->instructions))
	{
		// Detach list from listview
		SetGadgetChoices(data->objlist,GAD_FILETYPES_ICON_MENU,(APTR)~0);

		// Free function
		Remove(&fndata->func->node);
		FreeFunction(fndata->func);

		// Remove node
		Att_RemNode(node);

		// Is there an editor for this?
		if (fndata->editor)
		{
			// Tell editor to shutdown
			IPC_Command(fndata->editor,IPC_QUIT,0,0,0,0);
		}

		// Free data
		FreeVec(fndata);

		// Attach list
		SetGadgetChoices(data->objlist,GAD_FILETYPES_ICON_MENU,data->icon_list);

		// No selection
		filetypeed_no_iconsel(data);
		return 1;
	}

	return 0;
}


// End drag
BOOL filetypeed_end_drag(filetype_ed_data *data,BOOL ok)
{
	struct Window *window;
	ULONG id=0;
	IPCData *ipc=0;
	BOOL ret=0;
	short drag_item;

	// Valid drag?
	if (!data->drag.drag) return 0;

	// End drag
	if (!(window=config_drag_end(&data->drag,ok))) return 0;

	// Not our own window?
	if (window!=data->window)
	{
		// Get window ID
		id=GetWindowID(window);

		// Forbid to get IPC
		Forbid();
		if (!(ipc=(IPCData *)GetWindowAppPort(window)))
			Permit();
	}

	// Unlock layer
	UnlockLayerInfo(&data->window->WScreen->LayerInfo);

	// Get drag item
	drag_item=Att_FindNodeNumber(data->icon_list,data->drag.drag_node);

	// Drop on our window?
	if (window==data->window)
	{
		long top,item;
		GL_Object *lister;

		// Convert to window coordinates
		data->drag.drag_x-=data->window->LeftEdge;
		data->drag.drag_y-=data->window->TopEdge;

		// Dropped over list?
		lister=GetObject(data->objlist,GAD_FILETYPES_ICON_MENU);
		if (!(CheckObjectArea(lister,data->drag.drag_x,data->drag.drag_y)))
			return 0;

		// Get top item
		GetAttr(DLV_Top,(Object *)GADGET(lister),(ULONG *)&top);

		// Get item we dropped over
		item=UDivMod32((data->drag.drag_y-lister->dims.Top),data->window->RPort->TxHeight)+top;

		// Valid item?
		if (item>=0 && item<Att_NodeCount(data->icon_list) && item!=drag_item)
		{
			Att_Node *node1,*node2;

			// Get the two nodes
			if ((node1=Att_FindNode(data->icon_list,item)) &&
				(node2=Att_FindNode(data->icon_list,drag_item)))
			{
				// Detach list from listview
				SetGadgetChoices(data->objlist,GAD_FILETYPES_ICON_MENU,(APTR)~0);

				// Swap the nodes
				SwapListNodes(
					(struct List *)data->icon_list,
					(struct Node *)node1,
					(struct Node *)node2);

				// Swap functions
				SwapListNodes(
					(struct List *)&data->type->function_list,
					(struct Node *)((func_node *)node1->data)->func,
					(struct Node *)((func_node *)node2->data)->func);

				// Attach list
				SetGadgetChoices(data->objlist,GAD_FILETYPES_ICON_MENU,data->icon_list);

				// Set no selection
				filetypeed_no_iconsel(data);
				ret=1;
			}
		}

		return ret;
	}

	// Another window
	else
	if (ipc)
	{
		// Valid function?
		if (drag_item!=-1)
		{
			Att_Node *node;

			// Get node
			if (node=Att_FindNode(data->icon_list,drag_item))
			{
				func_node *fndata;
				Cfg_Function *func=0;

				// Get function pointer
				if (fndata=(func_node *)node->data)
					func=fndata->func;

				// Drop on function editor?
				if (id==WINDOW_FUNCTION_EDITOR)
				{
					Point *pos;
					Cfg_Instruction *ins;
					FunctionEntry *entry,*first=0,*last=0;

					// Store position (screen relative)
					if (pos=AllocVec(sizeof(Point),0))
					{
						pos->x=data->drag.drag_x;
						pos->y=data->drag.drag_y;
					}

					// Go through functions
					for (ins=(Cfg_Instruction *)func->instructions.mlh_Head;
						ins->node.mln_Succ;
						ins=(Cfg_Instruction *)ins->node.mln_Succ)
					{
						// Skip label
						if (ins->type!=INST_LABEL)
						{
							// Create a function entry
							if (entry=AllocVec(sizeof(FunctionEntry),MEMF_CLEAR))
							{
								// Copy function data
								entry->type=ins->type;
								strcpy(entry->buffer,ins->string);

								// First?
								if (!first) first=entry;
								else last->node=(Att_Node *)entry;
								last=entry;
							}
						}
					}

					// Send function
					IPC_Command(ipc,FUNCTIONEDIT_COPY_LINE,0,first,pos,0);
					ok=1;
				}

				// Or on a button editor
				else
				if (id==WINDOW_BUTTON_CONFIG || id==WINDOW_BUTTONS || id==WINDOW_FUNCTION_EDITOR)
				{
					Point *pos;
					Cfg_Button *button;

					// Store position (screen relative)
					if (pos=AllocVec(sizeof(Point),0))
					{
						pos->x=data->drag.drag_x;
						pos->y=data->drag.drag_y;
					}

					// Create a new button
					if (button=NewButton(0))
					{
						Cfg_ButtonFunction *function;

						// Create function
						if (function=NewButtonFunction(0,FTYPE_LEFT_BUTTON))
						{
							Cfg_Instruction *ins;

							// Copy function data
							CopyFunction(func,0,(Cfg_Function *)function);

							// Fix type
							function->function.func_type=FTYPE_LEFT_BUTTON;
							function->function.flags2&=~FUNCF2_LABEL_FUNC;

							// Go through instructions in function copy
							for (ins=(Cfg_Instruction *)function->instructions.mlh_Head;
								ins->node.mln_Succ;
								ins=(Cfg_Instruction *)ins->node.mln_Succ)
							{
								// Label?
								if (ins->type==INST_LABEL)
								{
									// Remove from function
									Remove((struct Node *)ins);

									// Store label
									if (function->node.ln_Name=AllocMemH(0,strlen(ins->string)+1))
										strcpy(function->node.ln_Name,ins->string);
									if (function->label=AllocMemH(0,strlen(ins->string)+1))
										strcpy(function->label,ins->string);

									// Free instruction
									FreeInstruction(ins);
									break;
								}
							}

							// Add function to button
							AddTail((struct List *)&button->function_list,(struct Node *)function);
						}

						// Send button
						IPC_Command(ipc,BUTTONEDIT_CLIP_BUTTON,0,button,pos,0);
						ok=1;
					}

					// Failed
					else FreeVec(pos);
				}
			}
		}

		// Permit now the message has been sent
		Permit();
		if (ok) return ret;
	}

	// Illegal drop
	DisplayBeep(data->window->WScreen);
	return ret;
}


// Select nothing
void filetypeed_no_iconsel(filetype_ed_data *data)
{
	// No selection
	SetGadgetValue(data->objlist,GAD_FILETYPES_ICON_MENU,(ULONG)-1);
	data->last_icon=0;

	// Disable edit/delete buttons
	DisableObject(data->objlist,GAD_FILETYPES_EDIT_ICON_MENU,TRUE);
	DisableObject(data->objlist,GAD_FILETYPES_DEL_ICON_MENU,TRUE);
}


// Given a button
BOOL filetypeed_get_button(filetype_ed_data *data,Cfg_Button *button,Point *pos)
{
	short x=0,y=0;
	BOOL ret=0;

	// Get window-relative position
	if (pos)
	{
		x=pos->x-data->window->LeftEdge;
		y=pos->y-data->window->TopEdge;
	}

	// Dropped over the icon list?
	if (CheckObjectArea(GetObject(data->objlist,GAD_FILETYPES_ICON_MENU),x,y))
	{
		Cfg_ButtonFunction *func;

		// Find left-button function
		if (func=(Cfg_ButtonFunction *)FindFunctionType((struct List *)&button->function_list,FTYPE_LEFT_BUTTON))
		{
			Cfg_Function *newfunc;
			func_node *fndata;

			// Copy function, allocate node data
			if ((newfunc=CopyFunction((Cfg_Function *)func,0,0)) &&
				(fndata=AllocVec(sizeof(func_node),MEMF_CLEAR)))
			{
				char *name;
				Att_Node *node;

				// Get name pointer
				if ((name=func->label) || (name=func->node.ln_Name))
				{
					Cfg_Instruction *ins;

					// Create label instruction, add to function
					if (ins=NewInstruction(0,INST_LABEL,name))
						AddHead((struct List *)&newfunc->instructions,(struct Node *)ins);
				}

				// Set "label" flag
				newfunc->function.flags2|=FUNCF2_LABEL_FUNC;

				// Detach list from listview
				SetGadgetChoices(data->objlist,GAD_FILETYPES_ICON_MENU,(APTR)~0);

				// Fill out node entry
				fndata->func=newfunc;

				// Add to node
				if (!(node=Att_NewNode(data->icon_list,name,(ULONG)fndata,0)))
				{
					// Failed
					FreeVec(fndata);
					FreeFunction(newfunc);
				}

				// Add function to list
				else
				{
					AddTail(&data->type->function_list,&newfunc->node);
					ret=1;
				}

				// Attach list
				SetGadgetChoices(data->objlist,GAD_FILETYPES_ICON_MENU,data->icon_list);

				// New node?
				if (node)
				{
					// Select this node
					SetGadgetValue(
						data->objlist,
						GAD_FILETYPES_ICON_MENU,
						Att_FindNodeNumber(data->icon_list,node));

					// Enable edit/delete buttons
					DisableObject(data->objlist,GAD_FILETYPES_EDIT_ICON_MENU,FALSE);
					DisableObject(data->objlist,GAD_FILETYPES_DEL_ICON_MENU,FALSE);
				}
			}

			// Failed
			else
			if (newfunc) FreeFunction(newfunc);
		}
	}

	// Illegal drop
	else
	DisplayBeep(data->window->WScreen);

	// Free new button
	FreeButton(button);
	return ret;
}


// Delete action
BOOL filetypeed_del_action(filetype_ed_data *data,short action)
{
	Cfg_Function *func,*next;
	short count=0;

	// Go through list
	for (func=(Cfg_Function *)data->type->function_list.lh_Head;func->node.ln_Succ;func=next)
	{
		// Cache next
		next=(Cfg_Function *)func->node.ln_Succ;

		// Right type?
		if (func->function.func_type==action &&
			!(func->function.flags2&FUNCF2_LABEL_FUNC))
		{
			// Remove function and free it
			Remove((struct Node *)func);
			FreeFunction(func);
			++count;
		}
	}

	// No change?
	if (!count) return 0;

	// Update actions list
	filetypeed_update_actions(data);
	return 1;
}


// Select an icon item
BOOL filetypeed_sel_icon(filetype_ed_data *data,short item)
{
	Att_Node *node;

	// Get selected node
	if (!(node=Att_FindNode(data->icon_list,item))) return 0;

	// Enable edit/delete buttons
	DisableObject(data->objlist,GAD_FILETYPES_EDIT_ICON_MENU,FALSE);
	DisableObject(data->objlist,GAD_FILETYPES_DEL_ICON_MENU,FALSE);

	// Store pointer
	data->last_icon=node;

	return 1;
}
