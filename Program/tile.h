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

#ifndef _DOPUS_TILE
#define _DOPUS_TILE

typedef struct
{
	struct MinNode		node;
	struct IBox			dims;			// Box dimensions
	APTR				ptr;			// User-pointer
} TileNode;

typedef struct
{
	struct IBox			base;			// Area to tile within
	struct MinList		box_list;		// List of boxes to tile
	short				count;			// Number of boxes
	short				mode;			// Tile mode
	short				x_offset;		// Cascade offsets
	short				y_offset;
} TileInfo;

enum
{
	TILE_TILE_H,
	TILE_TILE_V,
	TILE_CASCADE
};

void tile_windows(TileInfo *);

#endif
