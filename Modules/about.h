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

#include "modules_lib.h"

#define logo_width	63
#define logo_height	59
#define logo_mod	8
#define logo_frames	10

typedef struct
{
	short	width;
	short	height;
	short	depth;
	short	planesize;
	USHORT	*imagedata;
	ULONG	*palette;
} image_data;

#define image_count	17
#define image_special	14

extern ConfigWindow	about_window;
extern ObjectDef	about_objects[];
extern USHORT 		logo_image[logo_frames][472];

extern struct Library *DataTypesBase;
extern APTR logo_obj[logo_frames];
extern image_data images[image_count];
extern ImageRemap remap;

extern __far USHORT jon_data[],greg_data[],leo_data[],andrew_data[],ben_data[],juggler_data[],tick_data[],coopers_data[],bill_data[],thomas_data[],frederic_data[],john_data[],trevor_data[],ash_data[],harv_data[],martin_data[],dave_data[];
extern ULONG jon_palette[],greg_palette[],leo_palette[],andrew_palette[],ben_palette[],juggler_palette[],tick_palette[],coopers_palette[],bill_palette[],thomas_palette[],frederic_palette[],john_palette[],trevor_palette[],ash_palette[],harv_palette[],martin_palette[],dave_palette[];

enum
{
	GAD_ABOUT_LOGO,
	GAD_ABOUT_COPYRIGHT,
	GAD_ABOUT_REGISTRATION,
	GAD_ABOUT_OK,
	GAD_COPYRIGHT_1,
	GAD_COPYRIGHT_2,
	GAD_COPYRIGHT_3,
	GAD_REGISTRATION_1,
	GAD_REGISTRATION_2,
	GAD_REGISTRATION_3,
	GAD_REGISTRATION_4,
	GAD_REGISTRATION_5,
	GAD_REGISTRATION_6,
	GAD_REGISTRATION_7,
	GAD_REGISTRATION_8,
	GAD_REGISTRATION_9,
};

typedef struct
{
	struct Window	*window;

	GL_Object	*text_area;
	unsigned short	text_off;
	short		text_pos;
	short		text_disp;
	struct IBox	text_dims;
	struct RastPort	text_rp;
	struct Region	*text_reg;

	char		*text_line;
	short		text_len;
	short		text_size;

	char		*trans_info;
	char		*message;
} about_data;

extern unsigned char about_message[];
