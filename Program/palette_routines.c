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

#define PEN_MAX		8

// Load screen palette
void load_screen_palette(void)
{
	short num,pen;

	// Get number of colours in screen
	num=1<<GUI->screen->BitMap.Depth;

	// Load base 4 colours
	for (pen=0;pen<4;pen++)
	{
		if (GfxBase->LibNode.lib_Version>=39)
		{
			SetRGB32(
				&GUI->screen->ViewPort,
				pen,
				environment->env->palette[pen*3+1],
				environment->env->palette[pen*3+2],
				environment->env->palette[pen*3+3]);
		}
		else
		{
			SetRGB4(
				&GUI->screen->ViewPort,
				pen,
				(environment->env->palette[pen*3+1]>>28)&0xf,
				(environment->env->palette[pen*3+2]>>28)&0xf,
				(environment->env->palette[pen*3+3]>>28)&0xf);
		}
	}

	// Load top 4 colours (under 39)
	if (GfxBase->LibNode.lib_Version>=39 && num>4)
	{
		for (pen=0;pen<4;pen++)
		{
			SetRGB32(
				&GUI->screen->ViewPort,
				num-pen-1,
				environment->env->palette[(pen+4)*3+1],
				environment->env->palette[(pen+4)*3+2],
				environment->env->palette[(pen+4)*3+3]);
		}
	}

	// Under 37, load custom colours
	if (GfxBase->LibNode.lib_Version<39 && num>4)
	{
		// Only maximum of configured colours
		num-=4;
		if (num>environment->env->palette_count) num=environment->env->palette_count;
		if (num>PEN_MAX) num=PEN_MAX;

		// Load colours
		for (pen=0;pen<num;pen++)
		{
			SetRGB4(
				&GUI->screen->ViewPort,
				pen+4,
				(environment->env->palette[(pen+8)*3+1]>>28)&0xf,
				(environment->env->palette[(pen+8)*3+2]>>28)&0xf,
				(environment->env->palette[(pen+8)*3+3]>>28)&0xf);
		}
	}
}


// Allocate palette
void get_colour_table()
{
	short pen;

	// Already got palette?
	if (GUI->flags&GUIF_GOT_PALETTE)
		return;
	GUI->flags|=GUIF_GOT_PALETTE;

	// Initialise pens
	for (pen=0;pen<16;pen++) GUI->pens[pen]=pen;
	GUI->pen_alloc=0;

	// Using 37?
	if (GfxBase->LibNode.lib_Version<39)
	{
		// On our own screen, pens go from 4 onwards
		if (GUI->screen)
		{
			// Pens go from 4 onwards
			for (pen=0;pen<16;pen++) GUI->pens[pen]=pen+4;
			GUI->pen_alloc=0xff;
		}
		return;
	}

	// On our own screen?
	if (GUI->screen)
	{
		short top;

		// Get number of colours in display
		top=(1<<GUI->screen->BitMap.Depth)-1;

		// More than 4 colours?
		if (top>4)
		{
			// Allocate OS colours
			for (pen=0;pen<4;pen++)
			{
				// Get top four palette colours so they won't be used below
				if ((GUI->pens[12+pen]=
					ObtainPen(
						GUI->screen_pointer->ViewPort.ColorMap,
						top-pen,
						environment->env->palette[(pen+4)*3+1],
						environment->env->palette[(pen+4)*3+2],
						environment->env->palette[(pen+4)*3+3],
						0))!=-1)
				{
					GUI->pen_alloc|=1<<(12+pen);
				}
			}
		}
	}

	// Go through user pens
	for (pen=0;pen<environment->env->palette_count;pen++)
	{
		// Not already allocated?
		if (!(GUI->pen_alloc&(1<<pen)))
		{
			// Try to allocate a pen
			if ((GUI->pens[pen]=
				ObtainBestPen(
					GUI->screen_pointer->ViewPort.ColorMap,
					environment->env->palette[(pen+8)*3+1],
					environment->env->palette[(pen+8)*3+2],
					environment->env->palette[(pen+8)*3+3],
					OBP_Precision,PRECISION_EXACT,
					OBP_FailIfBad,TRUE,
					TAG_END))==-1)
			{
				// Failed to allocate, find closest match
				GUI->pens[pen]=
					FindColor(GUI->screen_pointer->ViewPort.ColorMap,
						environment->env->palette[(pen+8)*3+1],
						environment->env->palette[(pen+8)*3+2],
						environment->env->palette[(pen+8)*3+3],
						-1);
			}

			// Otherwise, we have an exclusive pen
			else
			{
				// Set flag to say we allocated it
				GUI->pen_alloc|=1<<pen;
			}
		}
	}
}


// Free allocated colours
void free_colour_table()
{
	// Haven't got palette?
	if (!(GUI->flags&GUIF_GOT_PALETTE))
		return;
	GUI->flags&=~GUIF_GOT_PALETTE;

	// Only 39+
	if (GfxBase->LibNode.lib_Version>=39)
	{
		short pen;

		// Go through pens
		for (pen=0;pen<16;pen++)
		{
			// Was this pen allocated?
			if (GUI->pen_alloc&(1<<pen))
			{
				// Free it
				ReleasePen(
					GUI->screen_pointer->ViewPort.ColorMap,
					GUI->pens[pen]);
			}
		}
	}

	// Clear allocation flag
	GUI->pen_alloc=0;
}


// Retrieve custom pen
short GetCustomPen(short pen,short num,ColourSpec32 *spec)
{
	short flag,bestpen;

	// Doesn't work under 37
	if (GfxBase->LibNode.lib_Version<39)
		return -1;

	// Get the flag
	flag=pen+(num*CUST_PENS);

	// See if pen is defined, unless forcing the issue
	if (!spec && !(environment->env->env_ColourFlag&(1<<flag)))
		return -1;

	// Lock the custom pens
	GetSemaphore(&GUI->custom_pen_lock,SEMF_EXCLUSIVE,0);

	// See if pen is already allocated
	if (GUI->custom_pen_alloc&(1<<flag))
	{
		// Increment count
		++GUI->custom_pen_count[pen][num];

		// Get pen
		bestpen=GUI->custom_pens[pen][num];
	}

	// Not allocated yet
	else
	{
		// Try to allocate pen, in exclusive mode
		if ((bestpen=
			ObtainPen(
				GUI->screen_pointer->ViewPort.ColorMap,
				-1,
				(spec)?spec->cs_Red:environment->env->env_Colours[pen][num][0],
				(spec)?spec->cs_Green:environment->env->env_Colours[pen][num][1],
				(spec)?spec->cs_Blue:environment->env->env_Colours[pen][num][2],
				PEN_EXCLUSIVE))==-1)
		{
			// If we wanted an editable pen, we have to fail, otherwise just find closest colour
			if (!spec)
			{
				// Look for best pen
				bestpen=
					ObtainBestPen(
						GUI->screen_pointer->ViewPort.ColorMap,
						(spec)?spec->cs_Red:environment->env->env_Colours[pen][num][0],
						(spec)?spec->cs_Green:environment->env->env_Colours[pen][num][1],
						(spec)?spec->cs_Blue:environment->env->env_Colours[pen][num][2],
						OBP_Precision,PRECISION_EXACT,
						TAG_END);
			}
		}

		// Got pen
		else
		if (bestpen>-1)
		{
			// Mark pen as allocated
			GUI->custom_pen_alloc|=(1<<flag);

			// Set count to one
			GUI->custom_pen_count[pen][num]=1;

			// Store pen
			GUI->custom_pens[pen][num]=bestpen;
		}
	}

	// Unlock the custom pens
	FreeSemaphore(&GUI->custom_pen_lock);

	// Return pen
	return bestpen;
}


// Free a custom pen
void FreeCustomPen(short pen,short num)
{
	short flag;

	// Doesn't work under 37
	if (GfxBase->LibNode.lib_Version<39)
		return;

	// Get the flag
	flag=pen+(num*CUST_PENS);

	// Lock the custom pens
	GetSemaphore(&GUI->custom_pen_lock,SEMF_EXCLUSIVE,0);

	// See if pen is allocated
	if (GUI->custom_pen_alloc&(1<<flag))
	{
		// Decrement count
		if ((--GUI->custom_pen_count[pen][num])==0)
		{
			// Count has reached zero, we can free the pen
			if (GUI->screen_pointer)
			{
				// Free the pen
				ReleasePen(GUI->screen_pointer->ViewPort.ColorMap,GUI->custom_pens[pen][num]);
			}

			// Clear allocation bit
			GUI->custom_pen_alloc&=~(1<<flag);
		}
	}

	// Unlock the custom pens
	FreeSemaphore(&GUI->custom_pen_lock);
}


// Initialise custom pens
void InitCustomPens(void)
{
	short pen,num;
	short flag;

	// Doesn't work under 37
	if (GfxBase->LibNode.lib_Version<39)
		return;

	// Lock the custom pens
	GetSemaphore(&GUI->custom_pen_lock,SEMF_EXCLUSIVE,0);

	// Go through pens, 2 for each
	for (pen=0;pen<CUST_PENS;pen++)
		for (num=0;num<2;num++)
		{
			// Get the flag
			flag=pen+(num*CUST_PENS);

			// Is this pen allocated?
			if (GUI->custom_pen_alloc&(1<<flag))
			{
				// Set count to zero
				GUI->custom_pen_count[pen][num]=0;

				// Is screen still open?
				if (GUI->screen_pointer)
				{
					// Free the pen
					ReleasePen(GUI->screen_pointer->ViewPort.ColorMap,GUI->custom_pens[pen][num]);
				}
			}
		}

	// Clear allocation flags
	GUI->custom_pen_alloc=0;

	// Unlock the custom pens
	FreeSemaphore(&GUI->custom_pen_lock);
}
