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


typedef struct ToolBarInfo
{
	Cfg_ButtonBank		*buttons;

	short			count;
	short			button_height;
	struct Rectangle	*button_array;

	BOOL			cache;
	struct BitMap		*bitmap;
	struct RastPort		rp;
	short			width;
	short			height;

	short			rows;
	short			cols;

	ImageRemap		remap;
	BOOL			done_remap;

	short			max_width;
} ToolBarInfo;

ToolBarInfo *OpenToolBar(Cfg_ButtonBank *,char *);
void FreeToolBar(ToolBarInfo *);
BOOL GetToolBarCache(ToolBarInfo *,BOOL);
void FreeToolBarCache(ToolBarInfo *);
void RemapToolBar(ToolBarInfo *);
void FreeToolBarRemap(ToolBarInfo *);
