#include "config_lib.h"
#include "old_config.h"

// Function types
enum {
	FT_INTERNAL,		// Internal
	FT_EXECUTABLE,		// AmigaDOS
	FT_WORKBENCH,		// Workbench
	FT_BATCH,			// Batch
	FT_AREXX			// ARexx
};

// Old type characters
#define FC_INTERNAL		'*'
#define FC_WORKBENCH	'%'
#define FC_BATCH		'$'
#define FC_AREXX		'&'

enum
{
	CONVERT_ENVIRONMENT,
	CONVERT_BUTTONS,
	CONVERT_MENUS,
	CONVERT_DRIVES,
	CONVERT_FILETYPES,
	CONVERT_HOTKEYS
};

char *copy_string(APTR,char *);
void convert_function(APTR,struct List *,old_NewFunction *,short type,BOOL);
BOOL convert_instructions(APTR,struct MinList *list,char *string);
void convert_function_data(APTR,struct MinList *,ULONG,char,char);
short convert_config(struct _ConfigStuff *,short,char *);
Cfg_ButtonBank *convert_button_bank(struct dopusgadgetbanks *,struct Config *,int);
void convert_old_commands(char *cmd);

BOOL __asm __saveds L_ConvertConfig(
	register __a0 char *name,
	register __a1 struct Screen *parent,
	register __a2 IPCData *owner_ipc)
{
	ConfigStuff cstuff;
	BOOL success=0;

	// Initialise ConfigStuff
	cstuff.config=AllocVec(sizeof(struct Config),MEMF_CLEAR);
	cstuff.firsttype=0;
	cstuff.firstbank=0;
	cstuff.firsthotkey=0;
	cstuff.memory=0;

	// Read old configuration
	if ((ReadOldConfig(name,&cstuff))==1)
	{
		struct Window *window;
		ObjectList *objlist;
		NewConfigWindow newwin;
		char basename[30]={0};
		short a,convert=0;
		short break_flag=0;

		// Fill out new window
		newwin.parent=parent;
		newwin.dims=&_config_convert_window;
		newwin.title=GetString(locale,MSG_CONVERT_REQ_TITLE);
		newwin.locale=locale;
		newwin.port=0;
		newwin.flags=WINDOW_SCREEN_PARENT|WINDOW_VISITOR|WINDOW_REQ_FILL|WINDOW_AUTO_KEYS;
		newwin.font=0;

		// Open requester
		if ((window=OpenConfigWindow(&newwin)) &&
			(objlist=AddObjectList(window,_config_convert_objects)))
		{
			// Initialise objects
			SetGadgetValue(objlist,GAD_CONVERT_ENVIRONMENT,1);
			SetGadgetValue(objlist,GAD_CONVERT_BUTTONS,1);
			SetGadgetValue(objlist,GAD_CONVERT_MENUS,1);
			SetGadgetValue(objlist,GAD_CONVERT_FILETYPES,1);
			SetGadgetValue(objlist,GAD_CONVERT_HOTKEYS,1);
			SetGadgetValue(objlist,GAD_CONVERT_BASENAME,(ULONG)GetString(locale,MSG_CONVERT_OLD_BASENAME));

			// Event loop
			FOREVER
			{
				struct IntuiMessage *msg;

				while (msg=GetWindowMsg(window->UserPort))
				{
					struct IntuiMessage msg_copy;
					msg_copy=*msg;
					ReplyWindowMsg(msg);

					// Close?
					if (msg_copy.Class==IDCMP_CLOSEWINDOW)
						break_flag=-1;
					else
					if (msg_copy.Class==IDCMP_GADGETUP)
					{
						if (((struct Gadget *)msg_copy.IAddress)->GadgetID==GAD_CONVERT_CANCEL)
							break_flag=-1;
						else if (((struct Gadget *)msg_copy.IAddress)->GadgetID==GAD_CONVERT_CONVERT)
							break_flag=1;
					}

					// Help?
					else
					if (msg_copy.Class==IDCMP_RAWKEY &&
						msg_copy.Code==0x5f &&
						!(msg_copy.Qualifier&VALID_QUALIFIERS))
					{
						// Set busy pointer
						SetWindowBusy(window);

						// Send help command
						IPC_Command(owner_ipc,IPC_HELP,(1<<31),"Config Convert",0,REPLY_NO_PORT);

						// Clear busy pointer
						ClearWindowBusy(window);
					}
				}

				if (break_flag) break;

				if ((Wait(1<<window->UserPort->mp_SigBit|SIGBREAKF_CTRL_F))&SIGBREAKF_CTRL_F)
				{
					break_flag=-1;
					break;
				}
			}

			// Get settings
			strcpy(basename,(char *)GetGadgetValue(objlist,GAD_CONVERT_BASENAME));
			for (a=GAD_CONVERT_ENVIRONMENT;a<=GAD_CONVERT_HOTKEYS;a++)
				if (GetGadgetValue(objlist,a)) convert|=1<<(a-GAD_CONVERT_ENVIRONMENT);

			// Close window
			CloseConfigWindow(window);
		}

		// Convert to new configuration
		if (break_flag==1)
		{
			struct Window *status;

			// Show status text
			status=OpenStatusWindow(
				0,
				GetString(locale,MSG_CONVERTING_CONFIGURATION),
				parent,
				0,
				WINDOW_NO_CLOSE);

			// Default basename?
			if (!basename[0]) strcpy(basename,"old");

			// Do conversion
			convert_config(&cstuff,convert,basename);

			// Send name back
			if (convert&(1<<CONVERT_ENVIRONMENT) && basename[0])
			{
				lsprintf(name,"dopus5:environment/%s",basename);
				success=1;
			}

			// Close status
			if (status) CloseConfigWindow(status);
		}
	}

	// Free old configuration
	FreeOldConfig(&cstuff);
	FreeVec(cstuff.config);
	return success;
}


short convert_config(
	ConfigStuff *cstuff,
	short convert,
	char *basename)
{
	int a;
	short error=0;
	char buffer[280];
	struct Config *oldconfig;

	// Get old configuration pointer
	oldconfig=cstuff->config;

	// Convert environment?
	if (convert&(1<<CONVERT_ENVIRONMENT))
	{
		CFG_ENVR *env;
		APTR iff;
		short fail=1,a;

		// Allocate environment structure
		if (env=AllocVec(sizeof(CFG_ENVR),MEMF_CLEAR))
		{
			env->hotkey_flags=oldconfig->hotkeyflags;
			env->hotkey_code=oldconfig->hotkeycode;
			env->hotkey_qual=oldconfig->hotkeyqual;

			env->list_format.files_unsel[0]=oldconfig->filesfg;
			env->list_format.files_unsel[1]=oldconfig->filesbg;
			env->list_format.files_sel[0]=oldconfig->filesselfg;
			env->list_format.files_sel[1]=oldconfig->filesselbg;
			env->list_format.dirs_unsel[0]=oldconfig->dirsfg;
			env->list_format.dirs_unsel[1]=oldconfig->dirsbg;
			env->list_format.dirs_sel[0]=oldconfig->dirsselfg;
			env->list_format.dirs_sel[1]=oldconfig->dirsselbg;
			env->list_format.sort.sort=oldconfig->sortmethod[0];
			env->list_format.sort.sort_flags=oldconfig->sortflags&1;
			env->list_format.sort.separation=oldconfig->separatemethod[0];
			memcpy(env->list_format.display_pos,oldconfig->displaypos[0],16);
			for (a=0;a<16;a++) env->list_format.display_len[a]=-1;
			env->list_format.show_free=oldconfig->showfree;
			strcpy(env->list_format.show_pattern,oldconfig->showpat);
			strcpy(env->list_format.show_pattern_p,oldconfig->showpatparsed);
			strcpy(env->list_format.hide_pattern,oldconfig->hidepat);
			strcpy(env->list_format.hide_pattern_p,oldconfig->hidepatparsed);

			env->source_col[0]=oldconfig->disknameselfg;
			env->source_col[1]=oldconfig->disknameselbg;
			env->dest_col[0]=oldconfig->disknamefg;
			env->dest_col[1]=oldconfig->disknamebg;
			env->devices_col[0]=oldconfig->filesfg;
			env->devices_col[1]=oldconfig->filesbg;
			env->volumes_col[0]=oldconfig->dirsfg;
			env->volumes_col[1]=oldconfig->dirsbg;

			if (oldconfig->screenmode==MODE_WORKBENCHUSE)
				env->screen_mode=MODE_WORKBENCHCLONE;
			else env->screen_mode=oldconfig->screenmode;
			env->screen_flags=oldconfig->screenflags;
			env->screen_depth=oldconfig->scrdepth;
			env->screen_width=oldconfig->scrw;
			env->screen_height=oldconfig->scrh;
			env->palette[0]=0x00100000;
			memcpy(env->palette+1,oldconfig->new_palette,sizeof(ULONG)*48);
			env->palette[49]=0x00000000;
			env->general_screen_flags=oldconfig->generalscreenflags;

			strcpy(env->font_name[0],oldconfig->fontbufs[11]);
			env->font_size[0]=oldconfig->fontsizes[0];
			strcpy(env->font_name[1],oldconfig->fontbufs[1]);
			env->font_size[1]=oldconfig->fontsizes[1];

			// Default status text
			strcpy(env->status_text,"D:[%ds/%dt] F:[%fs/%ft] B:[%bs/%bt] %h*%");

			// Convert output window to new format "x/y/w/h/name"
			{
				char *ptr;
				short a,val;

				env->output_window[0]=0;
				ptr=strchr(oldconfig->output,':');

				// Do four window dimensions
				for (a=0;a<4;a++)
				{
					// Valid string?
					if (ptr)
					{
						// Is it a number?
						if (isdigit(*(ptr+1)))
						{
							// Convert number
							val=atoi(ptr+1);

							// Find next slash
							ptr=strchr(ptr+1,'/');

							// Store number
							lsprintf(env->output_window+strlen(env->output_window),"%ld",val);
						}

						// Or a slash?
						else
						if (*(ptr+1)=='/') ++ptr;
					}

					// Add slash
					strcat(env->output_window,"/");
				}

				// Got slash for a name?
				if (ptr && *ptr=='/')
				{
					short len=strlen(env->output_window);
					strcat(env->output_window,ptr+1);
					if (ptr=strchr(env->output_window+len+1,'/'))
						*ptr=0;
				}
			}
				
			env->flags=ENVRF_AUTO_TILE;

			env->lister_popup_code=0xffff;
			env->lister_popup_qual=0;

			// Convert settings data
			if (oldconfig->dateformat&DATE_DOS) env->settings.date_format=FORMAT_DOS;
			else if (oldconfig->dateformat&DATE_INT) env->settings.date_format=FORMAT_INT;
			else if (oldconfig->dateformat&DATE_USA) env->settings.date_format=FORMAT_USA;
			else if (oldconfig->dateformat&DATE_AUS) env->settings.date_format=FORMAT_CDN;
			env->settings.date_flags=(oldconfig->dateformat>>4)&(DATE_SUBST|DATE_12HOUR);
			env->settings.copy_flags=oldconfig->copyflags;
			env->settings.delete_flags=oldconfig->deleteflags;
			env->settings.error_flags=oldconfig->errorflags;
			env->settings.general_flags=oldconfig->generalflags;
			env->settings.icon_flags=oldconfig->iconflags&~(ICONFLAG_DOUNTOICONS|ICONFLAG_AUTOSELECT);
			if (oldconfig->existflags&1) env->settings.replace_method=REPLACE_ALWAYS;
			else if (oldconfig->existflags&2) env->settings.replace_method=REPLACE_NEVER;
			else if (oldconfig->existflags&4) env->settings.replace_method=REPLACE_OLDER;
			else if (oldconfig->existflags&8) env->settings.replace_method=REPLACE_ASK;
			env->settings.update_flags=oldconfig->dynamicflags;
			env->settings.dir_flags=0;
			if (oldconfig->dirflags&DIRFLAGS_REREADOLD)
				env->settings.dir_flags|=DIRFLAGS_REREAD_CHANGED;
			env->settings.view_flags=oldconfig->viewbits;
			env->settings.max_buffer_count=oldconfig->bufcount*10;

			// Build filename
			lsprintf(buffer,"dopus5:environment/%s",basename);

			// Try to open file to write
			while (iff=IFFOpen(buffer,MODE_NEWFILE,ID_OPUS))
			{
				Cfg_Lister *lister;
				short len;

				// Write environment settings
				if (!(IFFWriteChunk(iff,env,ID_ENVR,sizeof(CFG_ENVR))))
					break;

				// Write hotkeys file name
				if (convert&(1<<CONVERT_HOTKEYS))
				{
					lsprintf(buffer,"dopus5:buttons/%s_hotkeys",basename);
					if (!(IFFWriteChunk(iff,buffer,ID_HKEY,strlen(buffer)+1)))
						break;
				}

				// Write menus file name
				if (convert&(1<<CONVERT_MENUS))
				{
					lsprintf(buffer,"dopus5:buttons/%s_menu",basename);
					if (!(IFFWriteChunk(iff,buffer,ID_UMEN,strlen(buffer)+1)))
						break;
				}

				// Add a couple of listers
				for (len=0;len<2;len++)
				{
					if (lister=NewLister(oldconfig->autodirs[len]))
					{
						lister->lister.format=env->list_format;
						SaveListerDef(iff,lister);
						FreeListerDef(lister);
					}
				}

				// Bring the default button bank up by default
				{
					struct IBox box;

					// Bank chunk
					if (!(IFFPushChunk(iff,ID_BTBK))) break;

					// Bank position
					box.Left=0;
					box.Top=32767;
					box.Width=2048;
					if (oldconfig->fontbufs[3][0])
						box.Height=oldconfig->fontsizes[3];
					else box.Height=((struct GfxBase *)GfxBase)->DefaultFont->tf_YSize;
					box.Height=(box.Height+4)*(((oldconfig)?oldconfig->gadgetrows+1:7))-1;

					// Write position
					if (!(IFFWriteChunkBytes(iff,&box,sizeof(struct IBox))))
						break;

					// Write bank path
					lsprintf(buffer,"dopus5:buttons/%s",basename);
					if (!(IFFWriteChunkBytes(iff,buffer,strlen(buffer)+1)) ||
						!(IFFPopChunk(iff)))
						break;
				}

				break;
			}
			IFFClose(iff);

			// Free environment data
			FreeVec(env);
		}

		if (fail) error|=1<<CONVERT_ENVIRONMENT;
	}

	// Convert buttons?
	if (convert&(1<<CONVERT_BUTTONS))
	{
		GadgetBank *bank;
		short count=0;

		// Go through banks
		for (bank=cstuff->firstbank;bank;bank=bank->next,count++)
		{
			Cfg_ButtonBank *new_bank;

			// Convert this bank
			if (new_bank=convert_button_bank(bank,oldconfig,0))
			{
				// Build name to save
				if (count==0) lsprintf(buffer,"dopus5:buttons/%s",basename);
				else lsprintf(buffer,"dopus5:buttons/%s.%ld",basename,count+1);

				// Save bank
				if (SaveButtonBank(new_bank,buffer))
					error|=1<<CONVERT_BUTTONS;

				// Free bank
				CloseButtonBank(new_bank);
			}
			else error|=1<<CONVERT_BUTTONS;
		}
	}


	// Convert drives
	if (convert&(1<<CONVERT_DRIVES))
	{
		Cfg_ButtonBank *bank;

		// Create a new button bank
		if (bank=NewButtonBank(0,0))
		{
			short a;

			// Initialise bank
			strcpy(bank->window.name,"Drive bank");

			// Get bank font
			if (oldconfig->fontbufs[3][0])
			{
				strcpy(bank->window.font_name,oldconfig->fontbufs[3]);
				bank->window.font_size=oldconfig->fontsizes[3];
			}
			else
			{
				strcpy(bank->window.font_name,((struct GfxBase *)GfxBase)->DefaultFont->tf_Message.mn_Node.ln_Name);
				bank->window.font_size=((struct GfxBase *)GfxBase)->DefaultFont->tf_YSize;
			}

			// Initialise bank size
			bank->window.pos.Top=32767;
			bank->window.pos.Width=80;
			bank->window.pos.Height=(bank->window.font_size+4)*((oldconfig->gadgetrows+1))-1;
			bank->window.columns=1;

			// Convert drives
			for (a=0;a<DRIVECOUNT;a++)
			{
				// Valid drive?
				if (oldconfig->drive[a].name[0] && oldconfig->drive[a].function)
				{
					Cfg_Button *button;
					Cfg_ButtonFunction *func;
					Cfg_Instruction *ins;

					// Create a new button
					if (button=NewButton(bank->memory))
					{
						// Link button in
						AddTail(&bank->buttons,&button->node);

						// Increment row count
						++bank->window.rows;

						// Initialise button colours
						button->button.fpen=oldconfig->drive[a].fpen;
						button->button.bpen=oldconfig->drive[a].bpen;

						// Create a new function
						if (func=NewButtonFunction(bank->memory,FTYPE_LEFT_BUTTON))
						{
							// Add to function list
							AddTail((struct List *)&button->function_list,&func->node);

							// Fill out name
							func->label=copy_string(bank->memory,oldconfig->drive[a].name);

							// Initialise function
							if (oldconfig->drive[a].key!=0)
								func->function.code=oldconfig->drive[a].key;
							else func->function.code=0xffff;
							func->function.qual=oldconfig->drive[a].qual;

							// Fill out instruction buffer
							lsprintf(buffer,"ScanDir %s",oldconfig->drive[a].function);

							// Create a new instruction
							if (ins=NewInstruction(bank->memory,FT_INTERNAL,buffer))
								AddTail((struct List *)&func->instructions,(struct Node *)ins);
						}
					}
				}
			}

			// Check row count isn't zero
			if (bank->window.rows<1) bank->window.rows=1;

			// Build name to save
			lsprintf(buffer,"dopus5:buttons/%s_drives",basename);

			// Save bank
			if (SaveButtonBank(bank,buffer))
				error|=1<<CONVERT_DRIVES;

			// Free bank
			CloseButtonBank(bank);
		}
		else error|=1<<CONVERT_DRIVES;
	}

	// Convert menus
	if (convert&(1<<CONVERT_MENUS))
	{
		Cfg_ButtonBank *bank=0;
		short menu,item,num,title=0;

		// Go through menus
		for (menu=0,num=0;menu<5;menu++)
		{
			// Go through items
			for (item=0;item<20;item++,num++)
			{
				// Valid item?
				if (oldconfig->menu[num].name &&
					oldconfig->menu[num].name[0])
				{
					// No bank yet?
					if (!bank)
					{
						// Create a new button bank
						if (bank=NewButtonBank(0,0))
						{
							// Initialise bank
							strcpy(bank->window.name,oldconfig->menutit[menu]);

							// Get bank font
							if (oldconfig->fontbufs[4][0])
							{
								strcpy(bank->window.font_name,oldconfig->fontbufs[4]);
								bank->window.font_size=oldconfig->fontsizes[4];
							}
							else
							{
								strcpy(bank->window.font_name,((struct GfxBase *)GfxBase)->DefaultFont->tf_Message.mn_Node.ln_Name);
								bank->window.font_size=((struct GfxBase *)GfxBase)->DefaultFont->tf_YSize;
							}

							// Initialise bank size
							bank->window.pos.Width=80;
							bank->window.pos.Height=bank->window.font_size+3;
							bank->window.columns=1;
							bank->window.rows=0;
						}
					}

					// Haven't done title yet?
					if (!title && bank)
					{
						Cfg_Button *button;

						// Create function
						if (button=NewButtonWithFunc(bank->memory,oldconfig->menutit[menu],FTYPE_LEFT_BUTTON))
						{
							// Set title flag
							button->button.flags|=BUTNF_TITLE;

							// Link button in
							AddTail(&bank->buttons,&button->node);

							// Increment row count
							++bank->window.rows;
						}

						// Set title flag
						title=1;
					}

					// Valid bank?
					if (bank)
					{
						Cfg_Button *button;
						char name[128];
						short len;
						BOOL sep=0;

						// Copy name
						stccpy(name,oldconfig->menu[num].name,126);

						// Separator bar?
						if (name[(len=(strlen(name)-1))]=='~')
						{
							name[len]=0;
							sep=1;
						}

						// Valid name still?
						if (name[0])
						{
							// Create a new button
							if (button=NewButton(bank->memory))
							{
								Cfg_ButtonFunction *func;

								// Link button in
								AddTail(&bank->buttons,&button->node);

								// Increment row count
								++bank->window.rows;

								// Initialise button colours
								button->button.fpen=oldconfig->menu[num].fpen;
								button->button.bpen=oldconfig->menu[num].bpen;

								// Convert function
								convert_function(
									bank->memory,
									(struct List *)&button->function_list,
									&oldconfig->menu[num],
									FTYPE_LEFT_BUTTON,0);

								// Get function
								if (func=(Cfg_ButtonFunction *)
									FindFunctionType((struct List *)&button->function_list,FTYPE_LEFT_BUTTON))
								{
									// Copy name
									func->label=copy_string(bank->memory,name);
								}
							}
						}

						// Separator bar?
						if (sep)
						{
							// Create a new button
							if (button=NewButton(bank->memory))
							{
								// Link button in
								AddTail(&bank->buttons,&button->node);

								// Increment row count and height
								++bank->window.rows;
								bank->window.pos.Height+=bank->window.font_size+4;
							}
						}
					}
				}
			}

			// Reset title flag
			title=0;
		}

		// Got a bank?
		if (bank)
		{
			// Check row count isn't zero
			if (bank->window.rows<1) bank->window.rows=1;

			// Build name to save
			lsprintf(buffer,"dopus5:buttons/%s_menu",basename);

			// Save bank
			if (SaveButtonBank(bank,buffer))
				error|=1<<CONVERT_MENUS;

			// Free bank
			CloseButtonBank(bank);
		}
	}

	// Convert filetypes
	if (convert&(1<<CONVERT_FILETYPES))
	{
		Filetype *oldtype;

		// Go through old filetypes
		for (oldtype=cstuff->firsttype;oldtype;oldtype=oldtype->next)
		{
			Cfg_Filetype *type;
			Cfg_FiletypeList list;
			BPTR lock;

			// Initialise dummy list
			NewList(&list.filetype_list);
			list.flags=0;

			// Build filetype path
			lsprintf(buffer,"dopus5:filetypes/%s",oldtype->type);

			// Check that it doesn't exist
			if (!(lock=Lock(buffer,ACCESS_READ)))
			{
				// Allocate new filetype
				if (type=NewFiletype(0))
				{
					// Add to dummy list
					AddTail(&list.filetype_list,&type->node);

					// Fill out filetype
					strcpy(type->type.name,oldtype->type);
					strcpy(type->type.id,oldtype->typeid);

					// Is this the "default" filetype?
					if (stricmp(type->type.name,"default")==0 ||
						stricmp(type->type.id,"default")==0)
						type->type.priority=-126;

					// Copy recognition string
					type->recognition=copy_string(0,oldtype->recognition);

					// Copy icon path
					type->icon_path=copy_string(0,oldtype->iconpath);

					// Copy actions
					for (a=0;a<FILETYPE_FUNCNUM;a++)
						if (oldtype->function[a] && oldtype->function[a][0])
							type->actions[a]=copy_string(0,oldtype->actionstring[a]);

					// Copy functions
					for (a=0;a<FILETYPE_FUNCNUM;a++)
					{
						if (oldtype->function[a] && oldtype->function[a][0])
						{
							Cfg_Function *func;

							// Allocate new function
							if (func=NewFunction(0,a))
							{
								BOOL int_only;

								// Add to function list
								AddTail(&type->function_list,&func->node);

								// Initialise function
								func->function.flags=oldtype->which[a];
								if (oldtype->delay[a]==-1)
									func->function.flags|=FUNCF_WAIT_CLOSE;

								// Convert instruction list
								int_only=convert_instructions(0,&func->instructions,oldtype->function[a]);

								// Convert stack, priority, etc
								convert_function_data(
									0,
									&func->instructions,
									oldtype->stack[a],
									oldtype->pri[a],
									(!(func->function.flags&FUNCF_OUTPUT_WINDOW) || int_only)?0:oldtype->delay[a]);
							}
						}
					}

					if (SaveFiletypeList(&list,buffer))
						error|=CONVERT_FILETYPES;

					// Free filetype
					FreeFiletype(type);
				}

				else error|=CONVERT_FILETYPES;
			}
			else UnLock(lock);
		}
	}

	// Convert hotkeys
	if (convert&(1<<CONVERT_HOTKEYS) && cstuff->firsthotkey)
	{
		Cfg_ButtonBank *bank;
		Hotkey *oldkey;

		// Create button bank
		if (bank=NewButtonBank(0,0))
		{
			// Initialise bank
			strcpy(bank->window.name,"Hotkeys");
			strcpy(bank->window.font_name,((struct GfxBase *)GfxBase)->DefaultFont->tf_Message.mn_Node.ln_Name);
			bank->window.font_size=((struct GfxBase *)GfxBase)->DefaultFont->tf_YSize;
			bank->window.pos.Top=100;
			bank->window.pos.Left=100;
			bank->window.pos.Width=100;
			bank->window.pos.Height=bank->window.font_size+3;
			bank->window.columns=1;
			bank->window.rows=0;

			// Go through hotkeys
			for (oldkey=cstuff->firsthotkey;oldkey;oldkey=oldkey->next)
			{
				Cfg_Button *button;

				// Create a new button
				if (button=NewButton(bank->memory))
				{
					old_NewFunction oldfunc;

					// Link button in
					AddTail(&bank->buttons,&button->node);

					// Increment row count and height
					++bank->window.rows;
					bank->window.pos.Height+=bank->window.font_size+4;

					// Initialise colours
					button->button.fpen=1;

					// Fill in old function
					oldfunc.name=oldkey->name;
					oldfunc.which=oldkey->func.which;
					oldfunc.stack=oldkey->func.stack;
					oldfunc.key=oldkey->code;
					oldfunc.qual=oldkey->qualifier;
					oldfunc.type=oldkey->func.type;
					oldfunc.pri=oldkey->func.pri;
					oldfunc.delay=oldkey->func.delay;
					oldfunc.function=oldkey->func.function;

					// Convert function
					convert_function(
						bank->memory,
						(struct List *)&button->function_list,
						&oldfunc,
						FTYPE_LEFT_BUTTON,TRUE);
				}
			}

			// Check row count isn't zero
			if (bank->window.rows<1) bank->window.rows=1;

			// Build name to save
			lsprintf(buffer,"dopus5:buttons/%s_hotkeys",basename);

			// Save bank
			if (SaveButtonBank(bank,buffer))
				error|=1<<CONVERT_HOTKEYS;

			// Free bank
			CloseButtonBank(bank);
		}
		else error|=1<<CONVERT_HOTKEYS;
	}

	return error;
}


// Convert button banks
Cfg_ButtonBank *convert_button_bank(
	GadgetBank *oldbank,
	struct Config *oldconfig,
	int do_all)
{
	Cfg_ButtonBank *bank;

	if (!oldbank) return 0;

	// Allocate new bank
	if (!(bank=NewButtonBank(0,0))) return 0;

	// Initialise bank
	strcpy(bank->window.name,"Converted button bank");

	// Get bank font
	if (oldconfig && oldconfig->fontbufs[3][0])
	{
		strcpy(bank->window.font_name,oldconfig->fontbufs[3]);
		bank->window.font_size=oldconfig->fontsizes[3];
	}
	else
	{
		strcpy(bank->window.font_name,((struct GfxBase *)GfxBase)->DefaultFont->tf_Message.mn_Node.ln_Name);
		bank->window.font_size=((struct GfxBase *)GfxBase)->DefaultFont->tf_YSize;
	}

	// Initialise bank size
	bank->window.pos.Left=0;
	bank->window.pos.Top=32767;
	bank->window.pos.Width=2048;
	bank->window.pos.Height=(bank->window.font_size+4)*(((oldconfig)?oldconfig->gadgetrows+1:7))-1;

	// Initialise rows/columns
	bank->window.columns=7;
	bank->window.rows=0;

	// Convert gadget banks
	do
	{
		int x,y,gad;
		Cfg_Button *button;

		// Increment row count
		bank->window.rows+=6;

		// Go through gadgets in this bank
		for (y=0,gad=0;y<6;y++)
		{
			for (x=0;x<7;x++,gad++)
			{
				// Create a new button
				if (button=NewButton(bank->memory))
				{
					// Link button in
					AddTail(&bank->buttons,&button->node);

					// Initialise button colours
					button->button.fpen=oldbank->gadgets[gad].fpen;
					button->button.bpen=oldbank->gadgets[gad].bpen;

					// Actually a valid bank?
					if (oldbank)
					{
						Cfg_ButtonFunction *func;

						// Function
						convert_function(
							bank->memory,
							(struct List *)&button->function_list,
							&oldbank->gadgets[gad],
							FTYPE_LEFT_BUTTON,0);

						// Find left button function
						if (func=(Cfg_ButtonFunction *)
							FindFunctionType((struct List *)&button->function_list,FTYPE_LEFT_BUTTON))
						{
							// Copy name
							func->label=copy_string(bank->memory,oldbank->gadgets[gad].name);
						}
							
						// Right button function?
						if (oldbank->gadgets[gad+42].name &&
							oldbank->gadgets[gad+42].function &&
							oldbank->gadgets[gad+42].function[0])
						{
							button->button.flags|=BUTNF_RIGHT_FUNC;

							// Function
							convert_function(
								bank->memory,
								(struct List *)&button->function_list,
								&oldbank->gadgets[gad+42],
								FTYPE_RIGHT_BUTTON,0);

							// Find right button function
							if (func=(Cfg_ButtonFunction *)
								FindFunctionType((struct List *)&button->function_list,FTYPE_RIGHT_BUTTON))
							{
								// Copy name
								func->label=copy_string(bank->memory,oldbank->gadgets[gad+42].name);
							}
						}
					}
				}
			}
		}

		// Doing all banks?
		if (!do_all) break;

		// Get next bank
		if (oldbank) oldbank=oldbank->next;
	} while (oldbank);

	return bank;
}

// Copy a string
char *copy_string(APTR memory,char *original)
{
	char *copy=0;

	if (original &&
		(copy=AllocMemH(memory,strlen(original)+1)))
		strcpy(copy,original);

	return copy;
}


// Convert a function
void convert_function(APTR memory,struct List *list,old_NewFunction *oldfunc,short type,BOOL label)
{
	Cfg_ButtonFunction *func;
	BOOL int_only;

	// Valid function?
	if (!oldfunc || !oldfunc->function || !oldfunc->function[0])
		return;

	// Allocate new function
	if (!(func=NewButtonFunction(memory,type)))
		return;

	// Add to function list
	AddTail(list,&func->node);

	// Label?
	if (label && oldfunc->name)
		func->label=copy_string(memory,oldfunc->name);

	// Initialise function
	func->function.flags=oldfunc->which;
	func->function.code=oldfunc->key;
	func->function.qual=oldfunc->qual;
	if (oldfunc->delay==-1)
		func->function.flags|=FUNCF_WAIT_CLOSE;
	func->function.flags&=FUNCF_ORIGINAL_FLAGS;

	// Convert instruction list
	int_only=convert_instructions(memory,&func->instructions,oldfunc->function);

	// Convert stack, priority, etc
	convert_function_data(
		0,
		&func->instructions,
		oldfunc->stack,
		oldfunc->pri,
		(!(func->function.flags&FUNCF_OUTPUT_WINDOW) || int_only)?0:oldfunc->delay);
}


// Convert a list of instructions for a function
BOOL convert_instructions(APTR memory,struct MinList *list,char *string)
{
	char func_buf[256];
	int buf_pos=0;
	int sep_flag=0;
	int last_space=-1;
	int pos;
	BOOL int_only=1;

	// Valid string?
	if (!string || !string[0]) return 1;

	// Go through function string
	for (pos=0;;pos++)
	{
		// End of a sub-string (or the main string)?
		if (string[pos]=='\0' ||
			(buf_pos>0 && string[pos]=='|' && sep_flag))
		{
			Cfg_Instruction *ins;

			// Null-terminate function buffer
			func_buf[buf_pos]=0;

			// Strip any trailing spaces
			if (last_space>-1) func_buf[last_space]=0;

			// Allocate new instruction
			if (ins=AllocMemH(memory,sizeof(Cfg_Instruction)))
			{
				char *ptr=func_buf;

				// Get type of instruction
				switch (func_buf[0])
				{
					// Internal
					case FC_INTERNAL:
						ins->type=FT_INTERNAL;
						++ptr;
						convert_old_commands(ptr);
						break;

					// Workbench function
					case FC_WORKBENCH:
						ins->type=FT_WORKBENCH;
						++ptr;
						break;

					// Batch file
					case FC_BATCH:
						ins->type=FT_BATCH;
						int_only=0;
						++ptr;
						break;

					// ARexx
					case FC_AREXX:
						ins->type=FT_AREXX;
						++ptr;
						break;

					// Executable
					default:
						ins->type=FT_EXECUTABLE;
						int_only=0;
						break;
				}

				// Copy string
				ins->string=copy_string(memory,ptr);

				// Link instruction in
				AddTail((struct List *)list,(struct Node *)ins);
			}

			// End of the function?
			if (string[pos]=='\0') break;

			// Reset buffer
			buf_pos=0;
			last_space=-1;
			sep_flag=0;
		}

		// Otherwise
		else
		{
			// If not at the start of the buffer, or this isn't a space
			if (buf_pos>0 || !isspace(string[pos]))
			{
				// Separator?
				if (string[pos]=='|' && string[pos+1]=='|')
					sep_flag=1;

				// If this is a space, store pointer to it
				else if (isspace(string[pos])) {
					if (last_space==-1)
						last_space=buf_pos;
				}

				// Reset last space position
				else last_space=-1;

				// Store character in function buffer
				if (!sep_flag) func_buf[buf_pos++]=string[pos];
			}
		}
	}

	return int_only;
}


// Convert old-style function data into extra instructions
void convert_function_data(APTR memory,struct MinList *list,ULONG stack,char pri,char delay)
{
	Cfg_Instruction *ins;
	char buf[80];

	// Need priority instruction?
	if (pri!=0)
	{
		lsprintf(buf,"changetaskpri %ld",pri);
		if (ins=NewInstruction(memory,FT_EXECUTABLE,buf))
			AddHead((struct List *)list,(struct Node *)ins);
	}

	// Need stack instruction?
	if (stack!=4000 && stack>2000)
	{
		lsprintf(buf,"stack %ld",stack);
		if (ins=NewInstruction(memory,FT_EXECUTABLE,buf))
			AddHead((struct List *)list,(struct Node *)ins);
	}

	// Need wait instruction?
	if (delay>0)
	{
		lsprintf(buf,"wait %ld",delay);
		if (ins=NewInstruction(memory,FT_EXECUTABLE,buf))
			AddTail((struct List *)list,(struct Node *)ins);
	}
}


char
	*old_cmd_table[]={
		"Hunt","FindFile",
		"LoopPlay","Play",
		"LPlay","Play",
		"FormatBG","Format",
		"DiskcopyBG","Diskcopy",
		"Auto2","User2",
		"Auto","User1",
		"Byte","GetSizes",
		"NNCopy","CopyAs",
		"NNMove","MoveAs",
		"UnByte","ClearSizes",
		"Execute","Run",
		"Clone","Duplicate",
		"ClearBuffers","FreeCaches",
		"BufferList","CacheList",
		"Verify","Confirm",
		0,0};

// Convert old function names to new ones
void convert_old_commands(char *cmd)
{
	short a;
	char buf[256];

	// Skip over any leading spaces
	while (*cmd && isspace(*cmd)) ++cmd;

	// Go through command table
	for (a=0;old_cmd_table[a];a+=2)
	{
		// Match command
		if (strnicmp(cmd,old_cmd_table[a],strlen(old_cmd_table[a]))==0)
		{
			// Copy new command
			strcpy(buf,old_cmd_table[a+1]);

			// Tack on remainder of line
			strcat(buf,cmd+strlen(old_cmd_table[a]));
			strcpy(cmd,buf);
			return;
		}
	}
}
