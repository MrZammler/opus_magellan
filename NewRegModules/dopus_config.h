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

#ifndef _DOPUS_CONFIGURATION
#define _DOPUS_CONFIGURATION

#define CONFIG_VERSION_3 		2
#define CONFIG_VERSION_2		3
#define CONFIG_VERSION			4

// Sort format
typedef struct {
	BYTE	sort;				// Sort method
	BYTE	sort_flags;			// Sort flags
	BYTE	separation;			// File separation
} SortFormat;

#define SORT_REVERSE	1

// List format
typedef struct {
	UBYTE		files_unsel[2];		// Unselected files
	UBYTE		files_sel[2];		// Selected files
	UBYTE		dirs_unsel[2];		// Unselected directories
	UBYTE		dirs_sel[2];		// Selected directories

	SortFormat	sort;			// Sort method
	BYTE		display_pos[16];	// Item display position
	BYTE		display_len[15];	// Display length

	UBYTE		flags;

	BYTE		show_free;		// Show free space type
	char		show_pattern[40];	// Show pattern
	char		hide_pattern[40];	// Hide pattern

	char		show_pattern_p[40];	// Show pattern parsed
	char		hide_pattern_p[40];	// Hide pattern parsed
} ListFormat;

#define LFORMATF_REJECT_ICONS		(1<<0)	// Reject icons
#define LFORMATF_HIDDEN_BIT		(1<<1)	// Respect the H bit
#define LFORMATF_ICON_VIEW		(1<<2)	// Default to icon view
#define LFORMATF_SHOW_ALL		(1<<3)	// Show all

enum
{
	DISPLAY_NAME,
	DISPLAY_SIZE,
	DISPLAY_PROTECT,
	DISPLAY_DATE,
	DISPLAY_COMMENT,
	DISPLAY_FILETYPE,
	DISPLAY_OWNER,
	DISPLAY_GROUP,
	DISPLAY_NETPROT,
	DISPLAY_VERSION,

	DISPLAY_LAST
};

enum
{
	SEPARATE_MIX,
	SEPARATE_DIRSFIRST,
	SEPARATE_FILESFIRST,
};

#define	SHOWFREE_BYTES			(1<<0)
#define	SHOWFREE_KILO			(1<<1)
#define	SHOWFREE_BLOCKS			(1<<2)
#define	SHOWFREE_PERCENT		(1<<3)


// List format storage structure
typedef struct {
	UBYTE		files_unsel[2];		// Unselected files
	UBYTE		files_sel[2];		// Selected files
	UBYTE		dirs_unsel[2];		// Unselected directories
	UBYTE		dirs_sel[2];		// Selected directories

	SortFormat	sort;			// Sort method
	BYTE		display_pos[16];	// Item display position
	BYTE		display_len[15];	// Display length

	UBYTE		flags;

	BYTE		show_free;		// Show free space type
	char		show_pattern[40];	// Show pattern
	char		hide_pattern[40];	// Hide pattern
} ListFormatStorage;


enum
{
	ITEM_FILENAME,
	ITEM_FILESIZE,
	ITEM_PROTECTION,
	ITEM_DATE,
	ITEM_COMMENT,
	ITEM_FILETYPE,
	ITEM_OWNER,
	ITEM_GROUP,
	ITEM_NETPROT,
	ITEM_VERSION,
};


/****************************************************************************
                          IFF Configuration Storage
 ****************************************************************************/

#define ID_OPUS		MAKE_ID('O','P','U','S')	// Opus FORM
#define ID_BTBK		MAKE_ID('B','T','B','K')	// Button bank to open
#define ID_BANK		MAKE_ID('B','A','N','K')	// Button bank to open
#define ID_BTNW		MAKE_ID('B','T','N','W')	// Button window
#define ID_FUNC		MAKE_ID('F','U','N','C')	// Function definition
#define ID_BUTN		MAKE_ID('B','U','T','N')	// Button definition
#define ID_TYPE		MAKE_ID('T','Y','P','E')	// Filetype definition
#define ID_HKEY		MAKE_ID('H','K','E','Y')	// Hotkey definition
#define ID_SETS		MAKE_ID('S','E','T','S')	// Settings
#define ID_LSTR		MAKE_ID('L','S','T','R')	// Lister definition
#define ID_DLST		MAKE_ID('D','L','S','T')	// Lister definition
#define ID_LBUT		MAKE_ID('L','B','U','T')	// Lister buttons
#define ID_ENVR		MAKE_ID('E','N','V','R')	// Environment definition
#define ID_TBAR		MAKE_ID('T','B','A','R')	// Toolbar pathname
#define ID_LMEN		MAKE_ID('L','M','E','N')	// Lister menu pathname
#define ID_UMEN		MAKE_ID('U','M','E','N')	// User menu pathname
#define ID_SCRP		MAKE_ID('S','C','R','P')	// Scripts pathname
#define ID_DESK		MAKE_ID('D','E','S','K')	// Desktop


// FUNC - Function definition
typedef struct {
	ULONG				flags;			// Function flags
	ULONG				flags2;			// More function flags
	ULONG				pad_1;			// Some padding
	UWORD				code;			// Function key code
	UWORD				qual;			// Function qualifier
	UWORD				func_type;		// Function type
	UWORD				qual_mask;		// Qualifier mask
	UWORD				qual_same;		// Qualifier same

//	Followed by function string
} CFG_FUNC;

#define FUNCF_OUTPUT_WINDOW	(1<<0)	// Output window
#define FUNCF_OUTPUT_FILE	(1<<1)	// Output to file
#define FUNCF_WORKBENCH_OUTPUT	(1<<2)	// Open window on Workbench
#define FUNCF_RUN_ASYNC		(1<<4)	// Run asynchronously
#define FUNCF_CD_SOURCE		(1<<5)	// CD source
#define FUNCF_CD_DESTINATION	(1<<6)	// CD destination
#define FUNCF_DO_ALL_FILES	(1<<7)	// Repeat for all files
#define FUNCF_RECURSE_DIRS	(1<<8)	// Recursive directories
#define FUNCF_RELOAD_FILES	(1<<9)	// Reload each file when done
#define FUNCF_NO_QUOTES		(1<<11)	// Don't quote filenames
#define FUNCF_RESCAN_SOURCE	(1<<12)	// Rescan source directory
#define FUNCF_RESCAN_DEST	(1<<13)	// Rescan destination directory
#define FUNCF_WAIT_CLOSE	(1<<16)	// Wait for click before closing window

#define FUNCF_ORIGINAL_FLAGS	(FUNCF_OUTPUT_WINDOW|\
				FUNCF_OUTPUT_FILE|\
				FUNCF_WORKBENCH_OUTPUT|\
				FUNCF_RUN_ASYNC|\
				FUNCF_CD_SOURCE|\
				FUNCF_CD_DESTINATION|\
				FUNCF_DO_ALL_FILES|\
				FUNCF_RECURSE_DIRS|\
				FUNCF_RELOAD_FILES|\
				FUNCF_NO_QUOTES|\
				FUNCF_RESCAN_SOURCE|\
				FUNCF_RESCAN_DEST|\
				FUNCF_WAIT_CLOSE)

#define FUNCF2_HOTKEY_FUNC	(1<<0)	// Function is a hotkey function
#define FUNCF2_FILETYPE_FUNC	(1<<1)	// Function is a filetype function
#define FUNCF2_VALID_IX		(1<<2)	// Valid IX information (qual mask/same)
#define FUNCF2_LABEL_FUNC	(1<<3)	// Function has a label associated with it


// These flags are used internally by Opus and not stored
#define FUNCF2_ORIGINAL		(1<<30)	// Call original function
#define FUNCF2_FREE_FUNCTION	(1<<31)	// Free after use

#define FTYPE_NONE			0
#define FTYPE_LEFT_BUTTON		0
#define FTYPE_RIGHT_BUTTON		1
#define FTYPE_MID_BUTTON		2
#define FTYPE_LIST			100
#define FTYPE_SPECIAL_FLAG		(1<<14)

// BTNW - Button window
typedef struct {
	char				name[32];	// Button window name
	struct IBox			pos;		// Window position
	char				font_name[31];	// Font to use
	UBYTE				font_size;	// Font size
	UWORD				columns;	// Number of columns
	UWORD				rows;		// Number of rows
	ULONG				flags;		// Flags
} CFG_BTNW;

#define BTNWF_ICONIFIED		(1<<0)	// Window iconified initially
#define BTNWF_GFX		(1<<1)	// Buttons are graphical in nature
#define BTNWF_LISTER_BUTTONS	(1<<2)	// Lister buttons
#define BTNWF_NO_BORDER		(1<<3)	// No border
#define BTNWF_BORDERLESS	(1<<4)	// Borderless buttons
#define BTNWF_SMART_REFRESH	(1<<5)	// Smart refresh window
#define BTNWF_HORIZ		(1<<6)	// Horizontal
#define BTNWF_VERT		(1<<7)	// Vertical
#define BTNWF_TOOLBAR		(1<<8)	// Toolbar bank
#define BTNWF_NO_DOGEARS	(1<<9)	// No dog-ears
#define BTNWF_CHANGED		(1<<10)	// Bank has changed

// BUTN - Button definition
typedef struct {
	UBYTE				fpen;			// Text pen
	UBYTE				bpen;			// Background pen
	ULONG				flags;			// Flags
	short				count;			// Number of functions
	short				pad_1;
	ULONG				pad_2;

// Followed by name/image filename(s)
} CFG_BUTN;

#define BUTNF_RIGHT_FUNC	(1<<0)		// Button has a right button function
#define BUTNF_MID_FUNC		(1<<1)		// Button has a middle button function
#define BUTNF_GRAPHIC		(1<<2)		// Button is a graphic button
#define BUTNF_NO_BORDER		(1<<3)		// No border for button
#define BUTNF_NEW_FORMAT	(1<<4)		// New format button
#define BUTNF_GLOBAL		(1<<5)		// System-global
#define BUTNF_TITLE		(1<<6)		// Menu title
#define BUTNF_ITEM		(1<<7)		// Menu item
#define BUTNF_SUB		(1<<8)		// Sub item

#define BUTNF_TEMP		(1<<31)		// Temporary flag


// TYPE - Filetype definition
typedef struct {
	char				name[32];		// Filetype name
	char				id[8];			// Filetype ID
	UWORD				flags;			// Flags
	char				priority;		// Priority
	char				pad;			// Padding
	UWORD				count;			// Number of functions defined

// Followed by recognition string
// Followed by icon path
// Followed by action strings
} CFG_TYPE;

#define FILETYPEF_CHECKMOD	(1<<0)		// Check for ST module


// HKEY - Hotkey definition
typedef struct {
	ULONG				flags;			// Flags

// Followed by name
} CFG_HKEY;


// LBUT - Lister button definition
typedef struct {
	ULONG			flags;			// Flags
// Followed by buttons
} CFG_LBUT;


// SETS - Settings
typedef struct {
	unsigned long		copy_flags;		// Copy flags
	unsigned long		delete_flags;		// Delete flags
	unsigned long		error_flags;		// Error flags
	unsigned long		general_flags;		// General flags
	unsigned long		icon_flags;		// Icon flags
	unsigned short		replace_method;		// Replace method
	unsigned short		replace_flags;		// Replace flags
	unsigned long		update_flags;		// Update flags
	unsigned long		dir_flags;		// Directory flags
	unsigned long		view_flags;		// View flags
	unsigned char		hide_method;		// Hide method
	unsigned char		pad1;
	unsigned short		pad2;
	unsigned short		max_buffer_count;	// Maximum buffer count
	unsigned short		date_format;		// Date format
	unsigned short		date_flags;		// Date flags
	char			pri_main[2];		// Main priority
	char			pri_lister[2];		// Lister priority
	unsigned long		flags;
	USHORT			pop_code;
	USHORT			pop_qual;
	USHORT			pop_qual_mask;
	USHORT			pop_qual_same;
	unsigned long		pad[18];
} CFG_SETS;

#define DIRFLAGS_DISABLE_CACHING	(1<<0)		// Disable caching
#define DIRFLAGS_REREAD_CHANGED		(1<<1)		// Re-read changed buffers
#define DIRFLAGS_EXPANDPATHS		(1<<7)		// Expand pathnames



// ENVR - defines the screen and other display info
typedef struct
{
	ULONG		screen_mode;		// Screen mode
	UWORD		screen_flags;		// Screen flags
	UWORD		screen_depth;		// Screen depth
	UWORD		screen_width;		// Screen width
	UWORD		screen_height;		// Screen height
	ULONG		palette[50];		// Screen palette
	char		pubscreen_name[80];	// Public screen name to open on
	struct IBox	window_pos;		// Window dimensions
	ULONG		general_screen_flags;	// General screen flags

	UBYTE		source_col[2];		// Source window colours
	UBYTE		dest_col[2];		// Destination window colours

	UWORD		palette_count;		// Number of user colours
	UWORD		pad1;

	char		font_name[3][40];	// Fonts to use

	ULONG		pad3[110];

	char		status_text[80];	// Status bar text

	UBYTE		font_size[3];		// Font sizes

	char		pad4[13];

	UWORD		display_options;	// Display options
	UWORD		main_window_type;	// Main window type

	ULONG		hotkey_flags;		// Hotkey flags
	UWORD		hotkey_code;		// Hotkey code
	UWORD		hotkey_qual;		// Hotkey qualifier

	ListFormat	list_format;		// Default list format

	char		backdrop_prefs[80];	// Backdrop pattern prefs file

	char		startup_script[80];	// Startup script
	char		uniconify_script[80];	// Uniconify script

	ULONG		pad5[20];

	char		output_window[80];	// Output window
	char		output_device[80];	// Output device

	ULONG		pad6[48];

	UWORD		lister_options;		// Lister options

	ULONG		flags;			// Misc. flags

	UWORD		lister_popup_code;	// Lister popup code
	UWORD		lister_popup_qual;	// Lister popup qualifier

	ULONG		env_flags;		// Some other flags

	unsigned short	clock_left;		// Clock left position
	unsigned short	clock_top;		// Clock top position

	UBYTE		devices_col[2];		// Source window colours
	UBYTE		volumes_col[2];		// Destination window colours

	UWORD		lister_width;		// Default lister sizes
	UWORD		lister_height;

	short		version;
	short		pad2;

	unsigned char	icon_fpen;
	unsigned char	icon_bpen;
	unsigned char	icon_style;
	unsigned char	pad7;

	unsigned long	desktop_flags;

	ULONG		pad[18];		// Save recompilation
} CFG_ENVR;


#define	SCRFLAGS_DEFWIDTH	(1<<0)		// Default width
#define	SCRFLAGS_DEFHEIGHT	(1<<1)		// Default height

#define	MODE_WORKBENCHUSE	1
#define	MODE_WORKBENCHCLONE	2
#define	MODE_PUBLICSCREEN	3

#define ENVRF_AUTO_TILE		(1<<0)		// Do auto-tile on startup

#define FONT_SCREEN		0
#define FONT_DIRS		1
#define FONT_ICONS		2

// env_flags
#define ENVF_CLOCK		(1<<0)		// Clock on
#define ENVF_BACKDROP		(1<<1)		// Backdrop window
#define ENVF_CHANGED		(1<<2)		// Environment has changed

// display_options
#define DISPOPTF_NO_BACKDROP	(1<<0)		// No display backdrop
#define DISPOPTF_NO_ALLOCPEN	(1<<1)		// No alloc pens
#define DISPOPTF_SHOW_APPICONS	(1<<2)		// Show AppIcons
#define DISPOPTF_SHOW_TOOLS	(1<<3)		// Show Tools menu
#define DISPOPTF_HIDE_BAD	(1<<4)		// Hide bad disks
#define DISPOPTF_DEFPUBSCR	(1<<5)		// Default public screen

// lister options
#define LISTEROPTF_DEVICES	(1<<0)		// Device list in new lister
#define LISTEROPTF_2XCLICK	(1<<1)		// Double-click editing
#define LISTEROPTF_SIMPLE	(1<<2)		// Simple-refresh windows
#define LISTEROPTF_TITLES	(1<<3)		// Want field titles
#define LISTEROPTF_SNAPSHOT	(1<<4)		// Always use snapshot position
#define LISTEROPTF_POPUP	(1<<5)		// PopUp in name mode


// desktop flags
#define DESKTOPF_DISTINCT	(1<<0)		// Distinct icon positions
#define DESKTOPF_NO_CACHE	(1<<1)		// Don't cache icons
#define DESKTOPF_NO_REMAP	(1<<2)		// Don't remap icons


// main_window_type
enum
{
	MAINWINDOW_ICONS,
	MAINWINDOW_DEVICES,
	MAINWINDOW_EMPTY
};


// LSTR - defines a lister
typedef struct {
	struct IBox		pos[2];		// Window position
	ULONG			icon_x;		// Icon position
	ULONG			icon_y;
	ListFormat		format;		// Lister format
	ULONG			flags;		// Flags
	ULONG			pad[4];

// Followed by path to load
} CFG_LSTR;

typedef struct {
	struct IBox		pos;		// Window position
	ListFormat		format;		// Lister format
	ULONG			flags;		// Flags

// Followed by path to load
} CFG_DLST;

#define DLSTF_ICONIFIED		(1<<0)		// Lister opens iconified
#define DLSTF_DEVICE_LIST	(1<<1)		// Lister shows device list
#define DLSTF_NO_POSITION	(1<<2)		// No position set
#define DLSTF_LOCK_POS		(1<<3)		// Position is locked
#define DLSTF_SOURCE		(1<<4)		// Lister starts as a source
#define DLSTF_DEST		(1<<5)		// Starts as destination
#define DLSTF_LOCKED		(1<<6)		// Source/dest locked
#define DLSTF_ICON		(1<<7)		// View as icons
#define DLSTF_SHOW_ALL		(1<<8)		// Show all
#define DLSTF_CACHE_LIST	(1<<9)		// Cache list
#define DLSTF_ICON_ACTION	(1<<10)		// Icon action

// Error codes
#define ERROR_OLD_CONFIG	(-1)


// Desktop entry
typedef struct {
	USHORT		dt_Type;
	USHORT		dt_Size;
	ULONG		dt_Flags;
	ULONG		dt_Data;
} CFG_DESK;

enum
{
	DESKTOP_ICON,
	DESKTOP_HIDE,
	DESKTOP_LEAVEOUT
};


/****************************************************************************
                     Application Configuration Structures
 ****************************************************************************/

// Function instruction
typedef struct {
	struct MinNode		node;
	short			type;
	char			*string;
} Cfg_Instruction;

enum
{
	INST_COMMAND,		// Internal command
	INST_AMIGADOS,		// AmigaDOS command
	INST_WORKBENCH,		// Workbench command
	INST_SCRIPT,		// Batch file
	INST_AREXX,		// ARexx script
	INST_LABEL,		// Label

	INST_DIR=100,
	INST_PROJECT,
};


// Function definition
typedef struct {
	struct Node		node;
	CFG_FUNC		function;
	struct MinList		instructions;
} Cfg_Function;


// Button function
typedef struct {
	struct Node		node;
	CFG_FUNC		function;
	struct MinList		instructions;
	char			*label;
	APTR			image;
} Cfg_ButtonFunction;

// Button definition
typedef struct {
	struct Node		node;
	CFG_BUTN		button;		// Button definition
	struct MinList		function_list;	// Button functions
	short			current;	// Current selection
	short			pad;
} Cfg_Button;


// Button bank definition
typedef struct {
	struct Node		node;
	CFG_BTNW		window;		// Window definition
	struct List		buttons;	// List of buttons
	char			path[256];	// Full path of button bank
	APTR			memory;		// Memory pool for this bank
	struct SignalSemaphore	lock;		// Semaphore lock
} Cfg_ButtonBank;


// Lister definition
typedef struct {
	struct Node			node;
	CFG_LSTR			lister;		// Lister definition
	char				*path;		// Path to load
} Cfg_Lister;


// List of filetypes (for storage)
typedef struct {
	struct Node		node;
	char			path[256];	// File path
	struct DateStamp	date;		// File datestamp
	struct List		filetype_list;	// List of types
	ULONG			flags;
} Cfg_FiletypeList;

#define FTLISTF_INTERNAL		(1<<0)	// Internal list, not to be modified
#define FTLISTF_CHANGED			(1<<1)	// List has been modified
#define FTLISTF_STORE			(1<<2)	// List is to be stored
#define FTLISTF_NOT_FOUND		(1<<31)	// Special flag; internal use only


// Filetype definition
typedef struct {
	struct Node		node;
	CFG_TYPE		type;		// Filetype information
	unsigned char		*recognition;	// Recognition string
	char			*icon_path;	// Icon path
	char			**actions;	// Action strings
	struct List		function_list;	// Function list
	Cfg_FiletypeList	*list;		// List we are in
} Cfg_Filetype;


// Hotkey definition
typedef struct {
	struct Node		node;
	CFG_HKEY		hotkey;		// Hotkey definition
	char			*name;		// Hotkey name
	struct List		function_list;	// Hotkey function
} Cfg_Hotkey;


// Lister buttons
typedef struct {
	CFG_LBUT		header;		// Lister buttons header
	struct List		button_list;	// List of buttons
} Cfg_ListerButtons;


// Desktop entry
typedef struct {
	struct MinNode		node;
	CFG_DESK		data;
} Cfg_Desktop;


// Environment configuration
typedef struct {
	APTR			memory;			// Memory handle
	struct SignalSemaphore	lock;			// Lock

	CFG_ENVR		*env;			// Environment data
	char			path[256];		// Current environment path

	char			settings_path[256];	// Current settings path
	CFG_SETS		*settings;		// Settings data

	struct List		buttons;		// Button banks to open
	struct List		listers;		// Listers to open

	struct MinList		path_formats;		// Specific path formats

	APTR			volatile_memory;	// Volatile memory handle
	char			toolbar_path[256];	// Lister toolbar path
	APTR			settings_memory;	// Settings memory handle
	char			menu_path[256];		// Lister menu path
	char			user_menu_path[256];	// User menu path
	char			hotkeys_path[256];	// Hotkeys path
	char			scripts_path[256];	// Scripts path

	struct MinList		desktop;		// Desktop information
	APTR			desktop_memory;
	struct SignalSemaphore	desktop_lock;
} Cfg_Environment;


/****************************************************************************
                           Other useful structures
 ****************************************************************************/

typedef struct
{
	struct Screen		*screen;
	struct DrawInfo		*draw_info;
	unsigned short		depth;
	unsigned short		pen_alloc;
	unsigned short		pen_array[16];
	short			pen_count;
} DOpusScreenData;	


/****************************************************************************
                             Configuration flags
 ****************************************************************************/

// CFG_SETS		copy_flags
#define			COPY_DATE		(1<<0)
#define			COPY_PROT		(1<<1)
#define			COPY_NOTE		(1<<2)
#define			COPY_ARC		(1<<3)
#define			COPY_CHECK		(1<<4)
#define			COPY_UPDATE		(1<<5)

// CFG_SETS		date_flags
#define			DATE_SUBST		(1<<0)
#define			DATE_12HOUR		(1<<1)
#define			DATE_1000SEP		(1<<2)

// CFG_SETS		delete_flags
#define			DELETE_ASK		(1<<0)
#define			DELETE_FILES		(1<<1)
#define			DELETE_DIRS		(1<<2)
#define			DELETE_SET		(1<<3)

// CFG_SETS		icon_flags
#define			ICONFLAG_DOUNTOICONS	(1<<1)
#define			ICONFLAG_AUTOSELECT	(1<<2)
#define			ICONFLAG_TRAP_MORE	(1<<3)

// CFG_SETS		replace_method
enum {
			REPLACE_ALWAYS,
			REPLACE_NEVER,
			REPLACE_OLDER,
			REPLACE_ASK,
};

// CFG_SETS		replace_flags
#define			REPLACEF_CHECK_VERSION		(1<<0)
#define			REPLACEF_VERBOSE_REPLACE	(1<<1)

// CFG_SETS		update_flags
#define			UPDATEF_SCROLL		(1<<1)
#define			UPDATEF_NOTIFY		(1<<3)

// CFG_SETS		hide_method
enum
{
			HIDE_CLOCK,
			HIDE_NONE,
			HIDE_ICON,
			HIDE_MENU
};

// CFG_SETS		general_flags
#define			GENERALF_MOUSE_HOOK		(1<<20)
#define			GENERALF_VERBOSE_REPLACE	(1<<21)
#define			GENERALF_QUICK_QUIT		(1<<22)
#define			GENERALF_EXTENDED_KEY		(1<<23)
#define			GENERALF_FILETYPE_SNIFFER	(1<<24)
#define			GENERALF_VALID_POPKEY		(1<<25)

#endif
