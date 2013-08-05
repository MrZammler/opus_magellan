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
#include <proto/SysInfo.h>
#include <libraries/SysInfo.h>

#define TITLE_ERROR		1
#define ERROR_TIMEOUT	4

void calc_daymonyr(long days,long *day,long *month,long *year);

extern USHORT __chip moon_big_data[8][2][13],moon_small_data[8][2][9];

#define MOON_BIG_SIZE	13
#define MOON_SMALL_SIZE	9

// Clock task
void __saveds clock_proc(void)
{
	IPCData *ipc;
	struct Window *window=0;
	struct AppWindow *appwindow=0;
	struct Layer *layer=0;
	TimerHandle *timer;
	char datebuf[12],titlebuf[28];
	struct DateTime date;
	BOOL hide_clock;
	short clock_x=0;
	struct RastPort clock_rp={0};
	struct Screen *pubscreen=0;
	long mem_msg=0,error_count=0,days;
	char *error_txt=0;
	short bar_x=0,last_x=0,clock_on=0,error_time=0;
	struct Library *SysInfoBase;
	struct SysInfo *si=0;

	// Do startup
	if (ipc=IPC_ProcStartup(0,0))
	{
		// Try to open SysInfo library
		if (SysInfoBase=OpenLibrary(SYSINFONAME,SYSINFOVERSION))
			si=InitSysInfo();
		
		// Open timer
		if (timer=AllocTimer(UNIT_VBLANK,0))
		{
			// Send first timer request
			StartTimer(timer,0,0);

			// Event loop
			FOREVER
			{
				IPCMessage *imsg;
				struct IntuiMessage *msg;
				BOOL update=0,quit=0;

				// IPC messages?
				while (imsg=(IPCMessage *)GetMsg(ipc->command_port))
				{
					// Look at message
					switch (imsg->command)
					{
						// Quit
						case IPC_QUIT:
							quit=1;
							break;


						// Show
						case IPC_SHOW:

							// Not already open?
							if (!window && !layer)
							{
								short len;
								struct Screen *screen;

								// Get screen pointer
								screen=(struct Screen *)imsg->data;
								hide_clock=imsg->flags;

								// No screen?
								if (!screen)
								{
									// Lock public screen
									if (!(pubscreen=LockPubScreen(0)))
									{
										quit=1;
										break;
									}
									screen=pubscreen;
								}

								// Otherwise, lock screen if using title bar
								else
								if (!hide_clock)
								{
									// Try to lock screen
									if (FindPubScreen(screen,TRUE))
										pubscreen=screen;

									// Failed
									else
									{
										quit=1;
										break;
									}
								}
								
								// Calculate length of window we need
								len=TextLength(
									&screen->RastPort,
									"88:88:88pm  88-WWW-88 ",
									21);

								// Displaying in title bar?
								if (!hide_clock)
								{
									struct DrawInfo *drawinfo;
									struct Gadget *gadget;
									short a;
									char *ptr;

									// Copy rastport
									layer=screen->BarLayer;
									clock_rp=*layer->rp;

									// Get draw info
									drawinfo=GetScreenDrawInfo(screen);

									// Set pens
									SetAPen(&clock_rp,
										drawinfo->dri_Pens[(drawinfo->dri_Version>=2)?BARDETAILPEN:DETAILPEN]);
									SetBPen(&clock_rp,
										drawinfo->dri_Pens[(drawinfo->dri_Version>=2)?BARBLOCKPEN:BLOCKPEN]);
									SetDrMd(&clock_rp,JAM2);

									// Get position to render clock
									bar_x=screen->Width-16;
									if (gadget=FindGadgetType(screen->FirstGadget,GTYP_SDEPTH))
										bar_x-=gadget->Width;
									clock_x=bar_x-len;

									// Get memory message
									mem_msg=MSG_MEMORY_COUNTER;

									// Get length of memory text
									a=TextLength(
										&screen->RastPort,
										(ptr=GetString(&locale,mem_msg)),
										strlen(ptr));
									a+=TextLength(
										&screen->RastPort,
										"9999999999",
										10);
									a+=TextLength(
										&screen->RastPort,
										dopus_name,
										strlen(dopus_name));

									// Is there room for the full message?
									if (5+a>=clock_x)
									{
										// Use short message
										mem_msg=MSG_MEMORY_COUNTER_CLOCK;
									}
								}

								// Otherwise, open window
								else
								if (!(window=OpenWindowTags(0,
									WA_Left,environment->env->clock_left,
									WA_Top,environment->env->clock_top,
									WA_Width,len+50,
									WA_Height,screen->WBorTop+screen->RastPort.Font->tf_YSize+1,
									WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_CHANGEWINDOW|IDCMP_MOUSEBUTTONS,
									WA_DragBar,TRUE,
									WA_DepthGadget,TRUE,
									WA_CloseGadget,TRUE,
									WA_SimpleRefresh,TRUE,
									WA_NoCareRefresh,TRUE,
									WA_NewLookMenus,TRUE,
									WA_RMBTrap,TRUE,
									WA_PubScreen,screen,
									WA_PubScreenFallBack,TRUE,
									TAG_END))) quit=1;

								// Successful?
								if (!quit)
								{
									// Set flag to update window
									update=1;
								}

								// Did we lock a screen?
								if (pubscreen && !layer)
								{
									UnlockPubScreen(0,pubscreen);
									pubscreen=0;
								}

								// Got a window?
								if (window)
								{
									// Make an AppWindow
									appwindow=AddAppWindowA(0x12345678,0,window,GUI->appmsg_port,0);
								}
							}
							break;


						// Hide
						case IPC_HIDE:

							// Close window if it's open
							if (window)
							{
								// Remove AppWindow
								if (appwindow)
								{
									RemoveAppWindow(appwindow);
									appwindow=0;
								}

								// Close window
								CloseWindow(window);
								window=0;
							}

							// Unlock public screen if it's locked
							if (pubscreen)
							{
								UnlockPubScreen(0,pubscreen);
								pubscreen=0;
							}
							layer=0;
							break;


						// Error message
						case TITLE_ERROR:

							// Existing error text?
							if (error_txt) FreeVec(error_txt);

							// Get error text and timeout, flag for update
							error_txt=imsg->data_free;
							error_time=imsg->flags;
							imsg->data_free=0;
							error_count=0;
							update=1;

							// Default time
							if (error_time==0) error_time=ERROR_TIMEOUT;
							break;
					}

					// Reply the message
					IPC_Reply(imsg);
				}


				// Intuition messages
				if (window)
				{
					while (msg=(struct IntuiMessage *)GetMsg(window->UserPort))
					{
						struct IntuiMessage copy_msg;

						// Copy message and reply
						copy_msg=*msg;
						ReplyMsg((struct Message *)msg);

						// Close window?
						if (copy_msg.Class==IDCMP_CLOSEWINDOW)
						{
							if (!(misc_startup("dopus_quit",MENU_QUIT,window,0,1)))
								quit=1;
						}

						// Change window event
						else
						if (copy_msg.Class==IDCMP_CHANGEWINDOW)
						{
							// Store new position
							environment->env->clock_left=window->LeftEdge;
							environment->env->clock_top=window->TopEdge;
						}

						// Right button press
						else
						if (copy_msg.Class==IDCMP_MOUSEBUTTONS &&
							copy_msg.Code==MENUDOWN)
						{
							// Signal main program
							IPC_Command(&main_ipc,IPC_SHOW,0,0,0,0);
						}
					}
				}

				// Check quit flag
				if (quit) break;

				// Timer event
				if (CheckTimer(timer))
				{
					// Send next timer request
					StartTimer(timer,0,800000);

					// Set flag to update window
					update=1;
				}

				// Update?
				if (update)
				{
					// If error text, update error count
					if (error_txt)
					{
						++error_count;

						// Clear text when count reaches limit
						if (error_count>error_time)
						{
							// Free text
							FreeVec(error_txt);
							error_txt=0;
						}
					}

					// Valid window?
					if (window || layer)
					{
						short hours,seconds;
						unsigned long minutes;

						// Get current datestamp
						DateStamp(&date.dat_Stamp);

						// Convert to string
						date.dat_Format=environment->env->settings.date_format;
						date.dat_Flags=0;
						date.dat_StrDay=0;
						date.dat_StrDate=datebuf;
						date.dat_StrTime=0;

						// Convert date to string
						DateToStr(&date);

						// Save days value
						days=date.dat_Stamp.ds_Days;

						// Get hours, minutes and seconds
						hours=DivideU(date.dat_Stamp.ds_Minute,60,&minutes,UtilityBase);
						seconds=UDivMod32(date.dat_Stamp.ds_Tick,TICKS_PER_SECOND);

						// Build time string. 12 hour clock?
						if (environment->env->settings.date_flags&DATE_12HOUR)
						{
							char ampm='a';

							// Convert to 12 hour clock
							if (hours>11) {
								ampm='p';
								hours-=12;
							}
							if (hours==0) hours=12;

							// Build time string
							lsprintf(titlebuf,
								"%s  %2ld:%02ld:%02ld%lcm  ",
								datebuf,
								hours,
								minutes,
								seconds,
								ampm);
						}

						// 24 hour clock
						else
						{
							lsprintf(titlebuf,
								"%s  %02ld:%02ld:%02ld  ",
								datebuf,
								hours,
								minutes,
								seconds);
						}

						// Update window?
						if (window)
						{
							// Show title
							SetWindowTitles(window,titlebuf,(char *)-1);
						}

						// Or render to screen bar?
						else
						if (layer)
						{
							ULONG lock;
							BOOL ok=0;
							struct Window *tit_window=0;

							// Lock Intuition
							lock=LockIBase(0);

							// Check that the active window is an Opus window
							if (GetWindowID(IntuitionBase->ActiveWindow)!=WINDOW_UNKNOWN)
							{
								// Set ok flag
								ok=1;

								// Rendering to screen title bar, in Workbench compatible way?
								if (GUI->flags2&GUIF2_WB_TITLE)
								{
									// Save window pointer
									tit_window=IntuitionBase->ActiveWindow;
								}
							}

							// Unlock Intuition
							UnlockIBase(lock);

							// Check that the right button isn't down
							if (ok && InputBase && PeekQualifier()&IEQUALIFIER_RBUTTON)
								ok=0;

							// Ok to render?
							if (ok)
							{
								static short done_tit=0;

								// Got window pointer?
								if (tit_window)
								{
									// Not done title?
									if (done_tit++>=10)
									{
										// Call SetWindowTitles which will be patched
										SetWindowTitles(IntuitionBase->ActiveWindow,(UBYTE *)-1,"Amiga Workbench");
										done_tit=0;
									}
								}

								// Rendering as usual
								else
								{
									// Reset title flag
									done_tit=0;

									// Is clock on?
									if (GUI->flags&GUIF_CLOCK)
									{
										Move(&clock_rp,clock_x,clock_rp.TxBaseline+1);
										Text(&clock_rp,titlebuf,strlen(titlebuf));
										clock_on=1;
									}

									// Was it on last time?
									else
									if (clock_on)
									{
										// Clear flag, erase to end of display
										clock_on=0;
										last_x=bar_x;
									}

									// Custom title?
									if (environment->env->scr_title_text[0] && !error_txt)
									{
										// Show custom title
										clock_show_custom_title(
											&clock_rp,
											(GUI->flags&GUIF_CLOCK)?clock_x:last_x,days,&date.dat_Stamp,si,SysInfoBase);
									}

									// Normal title with memory monitor
									else
									{
										// Show free memory count
										clock_show_memory(
											&clock_rp,
											mem_msg,
											(GUI->flags&GUIF_CLOCK)?clock_x:last_x,
											error_txt);
									}

									// Remember position
									last_x=clock_rp.cp_x;
								}
							}
						}
					}
				}

				// Wait for an event
				Wait(1<<ipc->command_port->mp_SigBit|
					1<<timer->port->mp_SigBit|
					((window)?(1<<window->UserPort->mp_SigBit):0));
			}

			// Close window if it's open
			if (window)
			{
				// Remove AppWindow
				if (appwindow)
					RemoveAppWindow(appwindow);

				// Close window
				CloseWindow(window);

				// Signal main process
				IPC_Command(&main_ipc,MAINCMD_OPEN_DISPLAY,0,0,0,0);
			}

			// Unlock screen if it's locked
			if (pubscreen)
			{
				UnlockPubScreen(0,pubscreen);
			}

			// Free timer
			FreeTimer(timer);
		}

		// Free SysInfo
		if (si) FreeSysInfo(si);
		if (SysInfoBase) CloseLibrary(SysInfoBase);
	}

	// Outstanding error text?
	if (error_txt) FreeVec(error_txt);

	// Send goodbye message
	IPC_Goodbye(ipc,&main_ipc,0);

	// Exit
	IPC_Free(ipc);
}


// Varargs OpenWindowTags
struct Window *__stdargs OpenWindowTags(struct NewWindow *newwin,Tag tag,...)
{
	return OpenWindowTagList(newwin,(struct TagItem *)&tag);
}


// Show free memory
void clock_show_memory(struct RastPort *rp,long msg,long clock_x,char *error)
{
	unsigned long chipmem;

	// Error text?
	if (error) strcpy(GUI->screen_title,error);

	// Update memory string
	else
	{
		// Thousands separator?
		if (environment->env->settings.date_flags&DATE_1000SEP && GUI->flags&GUIF_LOCALE_OK)
			++msg;

		// Get chip memory
		chipmem=AvailMem(MEMF_CHIP);

		// Build string
		lsprintf(GUI->screen_title,
			GetString(&locale,msg),
			dopus_name,
			chipmem,
			AvailMem(MEMF_ANY)-chipmem);
	}

	// Render text
	Move(rp,5,rp->Font->tf_Baseline+1);
	Text(rp,GUI->screen_title,strlen(GUI->screen_title));

	// Erase to start of clock text
	if (clock_x>rp->cp_x)
	{
		unsigned char fp;

		// Save front pen
		fp=rp->FgPen;
		SetAPen(rp,rp->BgPen);
		RectFill(rp,
			rp->cp_x,0,
			clock_x-1,rp->Font->tf_YSize+1);
		SetAPen(rp,fp);
	}
}


// Title-bar error message
void title_error(char *txt,short time)
{
	char *copy=0;
	IPCData *ipc;

	// Lock process list
	GetSemaphore(&GUI->process_list.lock,SEMF_SHARED,0);

	// Find clock process
	if (ipc=IPC_FindProc(&GUI->process_list,NAME_CLOCK,0,0))
	{
		// Allocate copy of text
		if (!txt || (copy=AllocVec(strlen(txt)+1,0)))
		{
			// Copy text
			if (copy) strcpy(copy,txt);

			// Send command
			IPC_Command(ipc,TITLE_ERROR,time,0,copy,0);
		}
	}

	// Unlock process list
	FreeSemaphore(&GUI->process_list.lock);
}

#define TITLE_SIZE	256

// Show custom title
void clock_show_custom_title(struct RastPort *rp,long clock_x,long days,struct DateStamp *date,struct SysInfo *si,struct Library *SysInfoBase)
{
	char *ptr,*format,*title_buffer;
	short pos=0,moon_day=-1,moon_pos=0;

	// Get numeric formatting string
	format=(environment->env->settings.date_flags&DATE_1000SEP && GUI->flags&GUIF_LOCALE_OK)?
		"%lU":"%ld";
	title_buffer=GUI->screen_title;

	// Go through title text
	for (ptr=environment->env->scr_title_text;*ptr && pos<TITLE_SIZE;ptr++)
	{
		short esc=0;
		unsigned long memval=(unsigned long)-1,memtotal=(unsigned long)-1;
		char buf[80];

		// Clear buffer
		buf[0]=0;

		// Escape command?
		if (*ptr=='%')
		{
			// Literal percent character?
			if (*(ptr+1)=='%')
			{
				title_buffer[pos++]='%';
				esc=1;
			}

			// Directory Opus version
			else
			if (*(ptr+1)=='d' && *(ptr+2)=='v')
			{
				extern short version_num;
				lsprintf(buf,"5.%ld",version_num);
				esc=2;
			}

			// Kickstart version
			else
			if (*(ptr+1)=='k' && *(ptr+2)=='v')
			{
				strcpy(buf,GUI->ver_kickstart);
				esc=2;
			}

			// Workbench version
			else
			if (*(ptr+1)=='w' && *(ptr+2)=='v')
			{
				strcpy(buf,GUI->ver_workbench);
				esc=2;
			}

			// CPU type
			else
			if (*(ptr+1)=='p' && *(ptr+2)=='r')
			{
				strcpy(buf,GUI->ver_cpu);
				esc=2;
			}

			// Coprocessor type
			else
			if (*(ptr+1)=='c' && *(ptr+2)=='p')
			{
				strcpy(buf,GUI->ver_fpu);
				esc=2;
			}

			// Chip set
			else
			if (*(ptr+1)=='c' && *(ptr+2)=='s')
			{
				strcpy(buf,GUI->ver_chips);
				esc=2;
			}

			// Total memory
			else
			if (*(ptr+1)=='t' && *(ptr+2)=='m')
			{
				memval=AvailMem(MEMF_TOTAL|MEMF_ANY);
				esc=2;
			}

			// Total chip memory
			else
			if (*(ptr+1)=='t' && *(ptr+2)=='c')
			{
				memval=AvailMem(MEMF_TOTAL|MEMF_CHIP);
				esc=2;
			}

			// Total fast memory
			else
			if (*(ptr+1)=='t' && *(ptr+2)=='f')
			{
				memval=AvailMem(MEMF_TOTAL|MEMF_FAST);
				esc=2;
			}

			// Free memory
			else
			if (*(ptr+1)=='f' && *(ptr+2)=='m')
			{
				memval=AvailMem(MEMF_ANY);
				if (*(ptr+3)=='%') memtotal=AvailMem(MEMF_ANY|MEMF_TOTAL);
				esc=2;
			}

			// Free chip memory
			else
			if (*(ptr+1)=='f' && *(ptr+2)=='c')
			{
				memval=AvailMem(MEMF_CHIP);
				if (*(ptr+3)=='%') memtotal=AvailMem(MEMF_CHIP|MEMF_TOTAL);
				esc=2;
			}

			// Free fast memory
			else
			if (*(ptr+1)=='f' && *(ptr+2)=='f')
			{
				memval=AvailMem(MEMF_FAST);
				if (*(ptr+3)=='%') memtotal=AvailMem(MEMF_FAST|MEMF_TOTAL);
				esc=2;
			}

			// Used memory
			else
			if (*(ptr+1)=='u' && *(ptr+2)=='m')
			{
				memval=(memtotal=AvailMem(MEMF_TOTAL|MEMF_ANY))-AvailMem(MEMF_ANY);
				esc=2;
			}

			// Used chip memory
			else
			if (*(ptr+1)=='u' && *(ptr+2)=='c')
			{
				memval=(memtotal-AvailMem(MEMF_TOTAL|MEMF_CHIP))-AvailMem(MEMF_CHIP);
				esc=2;
			}

			// Used fast memory
			else
			if (*(ptr+1)=='u' && *(ptr+2)=='f')
			{
				memval=(memtotal-AvailMem(MEMF_TOTAL|MEMF_FAST))-AvailMem(MEMF_FAST);
				esc=2;
			}

			// Task count
			else
			if (*(ptr+1)=='t' && *(ptr+2)=='a')
			{
				lsprintf(buf,"%ld",GetStatistics(STATID_TASKCOUNT));
				esc=2;
			}

			// Processor usage
			else
			if (*(ptr+1)=='p' && *(ptr+2)=='u')
			{
				if (si && si->cpu_usage_implemented&CPU_USAGEF_LASTSEC_IMPLEMENTED)
				{
					struct SI_CpuUsage cpu;
					GetCpuUsage(si,&cpu);
					lsprintf(buf,"%ld",UDivMod32(UMult32(100,cpu.used_cputime_lastsec),cpu.used_cputime_lastsec_hz));
				}
				else
					lsprintf(buf,"%ld",GetStatistics(STATID_CPU_USAGE)/10);
				esc=2;
			}

			// Screen count
			else
			if (*(ptr+1)=='s' && *(ptr+2)=='c')
			{
				ULONG lock;
				short count;
				struct Screen *screen;

				// Lock Intuition and count screens
				lock=LockIBase(0);
				for (count=0,screen=IntuitionBase->FirstScreen;screen;count++,screen=screen->NextScreen);
				UnlockIBase(lock);
				lsprintf(buf,"%ld",count);
				esc=2;
			}

			// Free pen count
			else
			if (*(ptr+1)=='f' && *(ptr+2)=='p')
			{
				struct ColorMap *cm;
				if (GUI->screen_pointer &&
					(cm=GUI->screen_pointer->ViewPort.ColorMap) &&
					cm->Type>=COLORMAP_TYPE_V39 &&
					cm->PalExtra)
					lsprintf(buf,"%ld",cm->PalExtra->pe_NFree);
				else
					strcpy(buf,"n/a");
				esc=2;
			}

			// Phase of the moon
			else
			if (*(ptr+1)=='p' && *(ptr+2)=='m')
			{
				long day,month,year,sub=4;

				// Calculate day, month, year from number of days
				calc_daymonyr(days,&day,&month,&year);
				if (year>100)
				{
					year-=100;
					sub=8;
				}

				// Calculate phase of the moon
				year-=(year/19)*19;					// Take modulo 19 of year
				if (year>9) year-=19;				// If year>9, subtract 19 to give -9>=y>=9
				year*=11;							// Multiply by 11
				if (year<0)
					year=-((-year)-(((-year)/30)*30));
				else								// Reduce modulo 30 to give -29>=y>=29
					year-=(year/30)*30;
				year+=(month<3)?month+2:month;		// Add month number (except jan/feb = 3 and 4)
				year+=day;							// Add day of month
				year-=sub;							// Subtract 4 (or 8 for 2000+)
				year-=(year/30)*30;					// Reduce modulo 30 to give 0>=y>=29

				// If font is big enough, we have a graphical moon
				if (rp->TxHeight>=8)
				{
					// Save moon day and position
					moon_day=year;
					moon_pos=pos;
					strcpy(buf,"  ");
				}
				else
					lsprintf(buf,"%ld",year);
				esc=2;
			}

			// Internet time
			else
			if (*(ptr+1)=='i' && *(ptr+2)=='t')
			{
				if (locale.li_Locale)
				{
					long minutes;
					minutes=date->ds_Minute;
					minutes+=locale.li_Locale->loc_GMTOffset;
					if (minutes<0) minutes+=1440;
					minutes+=60;
					minutes%=1440;
					minutes*=60;
					minutes+=date->ds_Tick/TICKS_PER_SECOND;
					minutes*=1000;
					minutes/=1440;
					minutes/=60;
					lsprintf(buf,"%03ld",minutes);
					esc=2;
				}
			}

			// Environment variable
			else
			if (*(ptr+1)=='e' && *(ptr+2)=='v')
			{
				char name[32],*p;

				// Get variable name
				stccpy(name,ptr+3,30);
				if (p=strchr(name,'%')) *p=0;

				// Get variable
				GetVar(name,buf,sizeof(buf)-1,GVF_GLOBAL_ONLY);
				if (p=strchr(buf,'\n')) *p=0;

				// Get length to skip
				if (p=strchr(ptr+2,'%'))
					esc=p-ptr;
			}

			// Memory value?
			if (memval!=(unsigned long)-1)
			{
				// As kilobytes/megabytes/smart/bytes
				if (*(ptr+3)=='K' || *(ptr+3)=='k')
				{
					DivideToString(buf,memval,1024,(*(ptr+3)=='K')?1:0,(environment->env->settings.date_flags&DATE_1000SEP)?GUI->decimal_sep:0);
					++esc;
				}
				else
				if (*(ptr+3)=='M' || *(ptr+3)=='m')
				{
					DivideToString(buf,memval,1024*1024,(*(ptr+3)=='M')?1:0,(environment->env->settings.date_flags&DATE_1000SEP)?GUI->decimal_sep:0);
					++esc;
				}
				else
				if (*(ptr+3)=='S' || *(ptr+3)=='s')
				{
					BytesToString(memval,buf,(*(ptr+3)=='S')?1:0,(environment->env->settings.date_flags&DATE_1000SEP)?GUI->decimal_sep:0);
					++esc;
				}
				else
				if (*(ptr+3)=='%' && memtotal!=(unsigned long)-1)
				{
					// Get percentage string
					if (memtotal<100)
						strcpy(buf,"100");
					else
						DivideToString(buf,memval,memtotal/100,0,0);
					++esc;
				}
				else
					lsprintf(buf,format,memval);
			}
				
			// Got a string to add?
			if (buf[0])
			{
				// Add on to title string
				stccpy(title_buffer+pos,buf,TITLE_SIZE-pos);
				pos=strlen(title_buffer);
			}
		}

		// Skip characters from escape sequence?
		if (esc) ptr+=esc;

		// Otherwise, add literal character to string
		else title_buffer[pos++]=*ptr;
	}

	// Null-terminate buffer
	title_buffer[pos]=0;

	// Render text
	Move(rp,5,rp->Font->tf_Baseline+1);
	Text(rp,title_buffer,(moon_day>-1)?moon_pos:strlen(title_buffer));

	// Moon to show?
	if (moon_day>-1)
	{
		short x,image,size;
		struct BitMap bm;

		// Find position for moon
		x=rp->cp_x;

		// Map moon day to image
		if (moon_day<2)
			image=0;
		else
		if (moon_day<6)
			image=1;
		else
		if (moon_day<10)
			image=2;
		else
		if (moon_day<13)
			image=3;
		else
		if (moon_day<16)
			image=4;		// Full
		else
		if (moon_day<19)
			image=5;
		else
		if (moon_day<23)
			image=6;
		else
		if (moon_day<28)
			image=7;
		else
			image=0;

		// Big moon?
		if (rp->TxHeight>=13)
		{
			InitBitMap(&bm,2,MOON_BIG_SIZE,MOON_BIG_SIZE);
			bm.Planes[0]=(PLANEPTR)moon_big_data[image][0];
			bm.Planes[1]=(PLANEPTR)moon_big_data[image][1];
			size=MOON_BIG_SIZE;
		}

		// Small moon
		else
		{
			InitBitMap(&bm,2,MOON_SMALL_SIZE,MOON_SMALL_SIZE);
			bm.Planes[0]=(PLANEPTR)moon_small_data[image][0];
			bm.Planes[1]=(PLANEPTR)moon_small_data[image][1];
			size=MOON_SMALL_SIZE;
		}

		// Draw moon
		BltBitMapRastPort(
			&bm,0,0,
			rp,x,(rp->TxHeight+2-size)>>1,
			size,size,
			0xc0);

		// Draw the rest of the text
		Move(rp,x+size,rp->Font->tf_Baseline+1);
		if ((x=strlen(title_buffer)-moon_pos-2)>0)
			Text(rp,title_buffer+moon_pos+2,x);
	}

	// Erase to start of clock text
	if (clock_x>rp->cp_x)
	{
		unsigned char fp;

		// Save front pen
		fp=rp->FgPen;
		SetAPen(rp,rp->BgPen);
		RectFill(rp,
			rp->cp_x,0,
			clock_x-1,rp->Font->tf_YSize+1);
		SetAPen(rp,fp);
	}
}

static char mondays[12]={31,28,31,30,31,30,31,31,30,31,30,31};

// Calculate day, month and year from number of days since 1978-01-11
void calc_daymonyr(long days,long *day,long *month,long *year)
{
	long i,mdays,ldays=1461,year2=78;

	year2+=(days/ldays)*4;
	days%=ldays;
	while (days)
	{
		mdays=365;
		if ((year2&3)==0) mdays++;
		if (days<mdays) break;
		days-=mdays;
		year2++;
	}
	for (i=0,days++;i<12;i++)
	{
		mdays=mondays[i];
		if (i==1 && (year2&3)==0) mdays++;
		if (days<=mdays) break;
		days-=mdays;
	}

	// Return information
	*day=days;
	*month=i+1;
	*year=year2;
}
