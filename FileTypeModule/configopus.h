#ifndef _CONFIGOPUS
#define _CONFIGOPUS

#include "dopuscfg:configopus_pragmas.h"

typedef struct
{
	Cfg_ButtonBank	*bank;
	IPCData		*ipc;
	long		flag;
	unsigned long	button;
} ButtonsStartup;

void Test(void);
int Config_Settings(Cfg_Environment *,struct Screen *,IPCData *,IPCData *,char *);
unsigned long Config_Environment(Cfg_Environment *,struct Screen *,USHORT *,IPCData *,IPCData *,USHORT);
int Config_Buttons(ButtonsStartup *,IPCData *,IPCData *,struct Screen *,ULONG);
short Config_Filetypes(struct Screen *,IPCData *,IPCData *,ULONG,char *);
ULONG Config_ListerButtons(char *,IPCData *,IPCData *,struct Screen *,Cfg_ButtonBank *,ULONG,LONG);
ULONG Config_Menu(char *,IPCData *,IPCData *,struct Screen *,Cfg_ButtonBank *,char *,ULONG,char *,short);
BOOL ConvertConfig(char *,struct Screen *,IPCData *);
ULONG Config_Menus(IPCData *,IPCData *,struct Screen *,Cfg_ButtonBank *,ULONG,ULONG,char *);

// Edit filetype
Cfg_Filetype *EditFiletype(Cfg_Filetype *,struct Window *,IPCData *,IPCData *,ULONG);

#define EFTF_EDIT_CLASS		(1<<0)


// Return values
#define CONFIG_NO_CHANGE		(0)	// No change
#define CONFIG_OK			(1<<0)	// Nothing important changed
#define CONFIG_CHANGE_DISPLAY		(1<<1)	// Need to reset the whole screen
#define CONFIG_CHANGE_LISTERS		(1<<2)	// Need to reset the listers
#define CONFIG_CHANGE_BUTTONS		(1<<3)	// Need to reset the buttons
#define CONFIG_CHANGE_SORT		(1<<4)	// Sort format changed
#define CONFIG_CHANGE_LIST_FORMAT	(1<<5)	// List format changed
#define CONFIG_CHANGE_MAX_BUFFERS	(1<<6)	// Maximum number of buffers changed
#define CONFIG_CHANGE_FILTERS		(1<<7)	// List filters changed
#define CONFIG_CHANGE_LIST_DISPLAY	(1<<8)	// Lister display changed
#define CONFIG_CHANGE_LIST_FONT		(1<<9)	// Lister font changed
#define CONFIG_CHANGE_LIST_TOOLBAR	(1<<10)	// Lister toolbar changed
#define CONFIG_CHANGE_LIST_MENU		(1<<11)	// Lister menu changed
#define CONFIG_CHANGE_USER_MENU		(1<<12)	// User menu changed
#define CONFIG_CHANGE_DISPLAY_OPTIONS	(1<<13)	// Display options changed
#define CONFIG_CHANGE_PALETTE		(1<<14)	// Palette changed
#define CONFIG_CHANGE_LOCALE		(1<<15)	// Changed locale settings
#define CONFIG_CHANGE_APP_ICONS		(1<<16)	// Changed AppIcon settings
#define CONFIG_CHANGE_TOOLS_MENU	(1<<17)	// Changed Tools menu setting
#define CONFIG_CHANGE_BAD_DISKS		(1<<18)	// Changed Bad Disks setting
#define CONFIG_CHANGE_FILETYPES		(1<<19)	// Changed filetypes
#define CONFIG_CHANGE_BACKDROP		(1<<20)	// Changed backdrop pattern
#define CONFIG_CHANGE_HOTKEYS		(1<<21)	// Changed hotkeys
#define CONFIG_CHANGE_SCRIPTS		(1<<22)	// Changed scripts
#define CONFIG_CHANGE_MOUSEHOOK		(1<<23)	// Changed mouse button hook
#define CONFIG_CHANGE_ICON_FONT		(1<<24)	// Changed icon font
#define CONFIG_CHANGE_HIDDEN_DRIVES	(1<<25)	// Changed hidden drives
#define CONFIG_CHANGE_ICON_DISTINCT	(1<<26)	// Changed distinct icon positions

#define CONFIG_SAVE			(1<<30)	// Save it

// Messages for the button editor
enum
{
	BUTTONEDIT_BASE=0x8000,

	BUTTONEDIT_HELLO,		// Editor says hello
	BUTTONEDIT_GOODBYE,		// Editor says goodbye
	BUTTONEDIT_QUIT,		// Editor is told to quit
	BUTTONEDIT_NEW_BANK,		// New bank pointer
	BUTTONEDIT_REFRESH,		// Refresh bank
	BUTTONEDIT_GET_SELECTION,	// Get current row/column selection
	BUTTONEDIT_SET_SELECTION,	// Set current row/column selection
	BUTTONEDIT_GET_BUTTON,		// Get a button
	BUTTONEDIT_FLASH,		// Control select flash
	BUTTONEDIT_REDRAW_BUTTON,	// Redraw a button
	BUTTONEDIT_PROCESS_KEY,		// Asked to process a key
	BUTTONEDIT_EDIT_BUTTON,		// Edit a button
	BUTTONEDIT_RETURN,		// Returning an edited button
	BUTTONEDIT_GET_SCREEN_DATA,	// Get screen data
	BUTTONEDIT_NEW_COLOURS,		// Colours changed
	BUTTONEDIT_EDIT_ME,		// Edit this bank now
	BUTTONEDIT_CLIP_BUTTON,		// Copy a button to the clipboard
	BUTTONEDIT_SELECT_BUTTON,	// Select a button
	BUTTONEDIT_GIMME_BANK,		// Get current bank pointer
	BUTTONEDIT_GET_BUTTON_POINT,	// Get a button from a point
	BUTTONEDIT_BUTTON_TO_BANK,	// Send a button to a bank
	BUTTONEDIT_PISS_OFF,		// Stop editing this bank
	BUTTONEDIT_REOPEN,		// Re-open window
	BUTTONEDIT_RE_EDIT_ME,		// Re-edit a bank
	BUTTONEDIT_CHANGE_BANK,		// Change bank pointer
	BUTTONEDIT_REDRAW,		// Redraw bank

	BUTTONEDIT_MENU_NEW,
	BUTTONEDIT_MENU_OPEN,
	BUTTONEDIT_MENU_SAVE,
	BUTTONEDIT_MENU_SAVEAS,
	BUTTONEDIT_MENU_CLOSE,
	BUTTONEDIT_MENU_DEFAULTS,
	BUTTONEDIT_MENU_LASTSAVED,
	BUTTONEDIT_MENU_RESTORE,

	FUNCTIONEDIT_BASE=0x9000,
	FUNCTIONEDIT_RETURN,		// Function editor returning a function
	FUNCTIONEDIT_COPY_LINE,		// Copy a function

	FILETYPEEDIT_BASE=0x9100,
	FILETYPEEDIT_RETURN,		// Returning an edited filetype
	CLASSEDIT_RETURN,		// Return from the class editor

	CFG_APPMESSAGE_PASS=0x9200,	// Passed app message
	CFG_NEW_ID,			// New identity
};

struct colrow_data
{
	short col,row;
	short flag;
};

#define WINDOW_BUTTON_CONFIG	1000
#define WINDOW_FUNCTION_EDITOR	1001

enum
{
	MENUTYPE_USER,
	MENUTYPE_LISTER,
};

#endif
