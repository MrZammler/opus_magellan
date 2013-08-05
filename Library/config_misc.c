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

#include "dopuslib.h"
#include "config.h"
#include "dopusprog:dopus_config.h"

Cfg_Function *__asm __saveds L_FindFunctionType(
	register __a0 struct List *list,
	register __d0 UWORD type)
{
	Cfg_Function *func;

	// Go through list
	for (func=(Cfg_Function *)list->lh_Head;
		func->node.ln_Succ;
		func=(Cfg_Function *)func->node.ln_Succ)
	{
		// Skip over 'label' functions
		if (func->function.flags2&FUNCF2_LABEL_FUNC)
			continue;

		// Correct type?
		if (func->function.func_type==type)
			return func;
	}

	return 0;
}
