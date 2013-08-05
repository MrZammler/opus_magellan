#include "config_lib.h"
#include "config_environment.h"
#include "dopusmod:modules.h"

// Update output window settings
void _config_env_update_output(config_env_data *data)
{
	short num,a,num_flag=0;

	// Get dimensions
	for (a=0,num=0;data->config->output_window[a];a++)
	{
		// Number?
		if (data->config->output_window[a]>='0' &&
			data->config->output_window[a]<='9')
		{
			// Another number
			if (!num_flag)
			{
				// Get number
				data->output_dims[num++]=atoi(data->config->output_window+a);
				num_flag=1;
			}
		}

		// Otherwise, end of a number
		else
		{
			num_flag=0;
			if (num==4) break;
		}
	}

	// If this is a slash, skip over it
	if (data->config->output_window[a]=='/') ++a;

	// Valid name?
	if (data->config->output_window[a])
	{
		// Set name
		SetGadgetValue(
			data->objlist,
			GAD_ENVIRONMENT_OUTPUT_NAME,
			(ULONG)data->config->output_window+a);
	}

	// Update dimensions
	_config_env_update_output_dims(data);
}


// Update dimensions display
void _config_env_update_output_dims(config_env_data *data)
{
	char buf[30];

	// Build string
	lsprintf(buf,"%ld,%ld -> %ld,%ld",
		data->output_dims[0],
		data->output_dims[1],
		data->output_dims[0]+data->output_dims[2]-1,
		data->output_dims[1]+data->output_dims[3]-1);

	// Update display
	SetGadgetValue(
		data->objlist,
		GAD_ENVIRONMENT_OUTPUT_DIMENSIONS,
		(ULONG)buf);
}


// Set output dimensions
IPCMessage *_config_env_output_window_set(config_env_data *data,USHORT id)
{
	struct Window *window;
	IPCMessage *ret=0;
	struct IBox dims;

	// Get dimensions
	if (id==GAD_ENVIRONMENT_OUTPUT_SET) dims=*((struct IBox *)data->output_dims);
	else
	{
		dims.Width=data->config->lister_width;
		if (dims.Width<64) dims.Width=320;
		dims.Height=data->config->lister_height;
		if (dims.Height<32) dims.Height=200;
		dims.Left=data->window->WScreen->MouseX-dims.Width+8;
		dims.Top=data->window->WScreen->MouseY-dims.Height+4;
	}

	// Open window
	if (!(window=my_OpenWindowTags(
		WA_Left,dims.Left,
		WA_Top,dims.Top,
		WA_Width,dims.Width,
		WA_Height,dims.Height,
		WA_Title,
			(id==GAD_ENVIRONMENT_OUTPUT_SET)?
				(char *)GetGadgetValue(data->objlist,GAD_ENVIRONMENT_OUTPUT_NAME):
				(char *)GetString(locale,MSG_ENVIRONMENT_DEFLISTER_SIZE),
		WA_CloseGadget,TRUE,
		WA_DragBar,TRUE,
		WA_DepthGadget,TRUE,
		WA_SizeGadget,TRUE,
		WA_SizeBRight,TRUE,
		WA_SimpleRefresh,TRUE,
		WA_Activate,TRUE,
		WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_REFRESHWINDOW,
		WA_PubScreen,data->window->WScreen,
		WA_MinWidth,64,
		WA_MinHeight,data->window->BorderTop+data->window->BorderBottom+8+data->window->RPort->TxHeight*2,
		WA_MaxWidth,-1,
		WA_MaxHeight,-1,
		TAG_END)))
	{
		DisplayBeep(data->window->WScreen);
		return 0;
	}

	// Display instructions
	SetAPen(window->RPort,1);
	SetDrMd(window->RPort,JAM1);
	SetFont(window->RPort,data->window->RPort->Font);
	_config_env_size_instructions(window,id);

	// Event loop
	FOREVER
	{
		struct IntuiMessage *msg;
		IPCMessage *imsg;
		BOOL quit=0;

		// Any IPC messages?
		while (imsg=(IPCMessage *)GetMsg(data->ipc->command_port))
		{
			// Look at command
			switch (imsg->command)
			{
				// Quit immediately
				case IPC_QUIT:
				case IPC_HIDE:
					ret=imsg;
					quit=1;
					imsg=0;
					break;


				// Activate
				case IPC_ACTIVATE:
					WindowToFront(data->window);
					WindowToFront(window);
					ActivateWindow(window);
					break;
			}

			// Reply message
			IPC_Reply(imsg);
		}

		// Window message?
		while (msg=(struct IntuiMessage *)GetMsg(window->UserPort))
		{
			// Close window?
			if (msg->Class==IDCMP_CLOSEWINDOW)
				quit=1;

			// Refresh
			else if (msg->Class==IDCMP_REFRESHWINDOW)
			{
				// Refresh instructions
				BeginRefresh(window);
				EndRefresh(window,TRUE);
				_config_env_size_instructions(window,id);
			}

			ReplyMsg((struct Message *)msg);
		}

		// Quit?
		if (quit) break;

		// Wait for event
		Wait(1<<window->UserPort->mp_SigBit|1<<data->ipc->command_port->mp_SigBit);
	}


	// Not quitting prematurely
	if (!ret)
	{
		// Store new position
		if (id==GAD_ENVIRONMENT_OUTPUT_SET)
		{
			data->output_dims[0]=window->LeftEdge;
			data->output_dims[1]=window->TopEdge;
			data->output_dims[2]=window->Width;
			data->output_dims[3]=window->Height;

			// Update display
			_config_env_update_output_dims(data);
		}
		else
		{
			data->config->lister_width=window->Width;
			data->config->lister_height=window->Height;

			// Update display
			_config_env_update_listersize(data);
		}
	}

	// Close window and return
	CloseWindow(window);
	return ret;
}

// Show instructions
void _config_env_size_instructions(struct Window *window,USHORT id)
{
	short a,len,inst[2];
	struct TextExtent extent;
	char *ins;

	// Get instruction IDs
	inst[0]=(id==GAD_ENVIRONMENT_OUTPUT_SET)?MSG_ENVIRONMENT_SIZE_INST_1:MSG_ENVIRONMENT_SIZE_INST_3;
	inst[1]=MSG_ENVIRONMENT_SIZE_INST_2;

	// Two lines of text
	for (a=0;a<2;a++)
	{
		// Get instruction string
		ins=GetString(locale,inst[a]);
		len=strlen(ins);

		// See how much will fit
		len=TextFit(
			window->RPort,
			ins,len,
			&extent,
			0,1,
			window->GZZWidth-8,
			window->GZZHeight-4-(a*window->RPort->TxHeight));

		// Show text
		if (len>0)
		{
			Move(window->RPort,
				window->BorderLeft+8,
				window->BorderTop+4+window->RPort->TxBaseline+(a*window->RPort->TxHeight));
			Text(window->RPort,ins,len);
		}
	}
}



// Update lister size settings
void _config_env_update_listersize(config_env_data *data)
{
	char buf[20];
	short width,height;

	// Get width and height
	width=data->config->lister_width;
	height=data->config->lister_height;

	// Check for 0
	if (width<64) width=320;
	if (height<32) height=200;

	// Build string
	lsprintf(buf,"%ld x %ld",width,height);

	// Update display
	SetGadgetValue(data->objlist,GAD_ENVIRONMENT_DEFAULT_SIZE,(ULONG)buf);
}

