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

#ifndef _DOPUS_PROTOS
#define _DOPUS_PROTOS

void __stdargs loc_printf(char *,char *,long,...);

// main.c
void main(int,char **);
Cfg_Function *new_default_function(char *func,APTR);
void process_args(int argc,char **argv);
void env_update_settings(BOOL save);
IPCData *show_startup_picture(void);
BOOL main_bump_progress(APTR,short,BOOL);
void startup_misc_init(void);
void startup_check_assign(void);
void startup_open_dopuslib(void);
void startup_check_duplicate(void);
void startup_run_update(void);
void startup_open_libraries(void);
void startup_init_gui(void);
void startup_process_args(int,char **);
void startup_show_startup_picture(IPCData **);
void startup_init_desktop(void);
void startup_read_positions(void);
void startup_check_registration(void);
void startup_init_ports(void);
void startup_get_env(void);
void startup_init_arexx_cx(void);
void startup_init_environment(void);
void startup_init_commands(void);
void startup_init_filetypes(void);
void startup_init_notification(void);
void startup_init_icons(void);
void startup_prestartup_script(void);
void startup_init_display(IPCData *);
void startup_check_pirates(void);
void startup_misc_final(void);

// commands.c
#define SCAN_MODULES	(1<<0)
#define SCAN_REXX		(1<<2)
#define SCAN_BOTH		(SCAN_MODULES|SCAN_REXX)
#define SCAN_USER		(1<<3)

void startup_init_commands(void);
void update_commands(ULONG flag);
void init_commands_scan(short);
void command_expunge(char *name);
CommandList *add_command(char *,char *,char *,char *,ULONG,char *,Att_List *,char *,ULONG);
void command_new(BackdropInfo *,IPCData *,char *);
void command_remove(char *);

// event_loop.c
void event_loop(void);
BOOL menu_process_event(ULONG,struct MenuItem *,struct Window *);

// string_data.c
void init_locale_data(struct DOpusLocale *);
void free_locale_data(struct DOpusLocale *);

// Miscellaneous prototypes
void KPrintF __ARGS((char *,...));
void lsprintf __ARGS((char *,char *,...));
void __stdargs ArgArrayDone(void);
UBYTE **__stdargs ArgArrayInit(long,UBYTE **);
STRPTR __stdargs ArgString(UBYTE **,STRPTR,STRPTR);

// clock_task.c
void clock_proc(void);
void clock_show_memory(struct RastPort *,long,long,char *);
void title_error(char *txt,short);
void clock_show_custom_title(struct RastPort *,long,long,struct DateStamp *,struct SysInfo *,struct Library *);

// tile.c
void tile_windows(TileInfo *);

// serial.c
void check_serial(void);
BOOL check_rego(struct _rego_data *);
BOOL second_check(void);
void show_about(struct Screen *screen,IPCData *ipc);

// status_text.c
status_display_error(Lister *lister,int errcode);
void status_text(Lister *lister,char *text);
void status_okay(Lister *lister);
void status_abort(Lister *lister);

int function_copy_file(
	struct FileInfoBlock *s_info,
	struct FileInfoBlock *d_info,
	FunctionHandle *handle,
	char *,char *,long *,char *,ULONG);

// scripts.c
BOOL RunScript(short,char *);


// iconpos.c
void iconpos_configure(IPCData *,struct MsgPort *);
void iconpos_new_area(struct Rectangle *);
BOOL iconpos_idcmp(struct IntuiMessage *);


// main_reset.c
void main_handle_reset(ULONG *,APTR);


// text.c
#include "text.h"


// file_openwith.c
void file_open_with(struct Window *,char *,ULONG);
BOOL file_launch(char *,short,char *);
void startup_read_openwith(void);
void add_open_with(char *);


// function_runcommand.c
Cfg_Function *function_load_function(char *name);

#endif
