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

struct RexxStem
{
	struct Node	rs_Node;
	char		rs_Value[2];
};

struct RexxMsg *__asm L_CreateRexxMsgEx(
	register __a0 struct MsgPort *,
	register __a1 UBYTE *,
	register __d0 UBYTE *);
long __asm L_SetRexxVarEx(
	register __a0 struct RexxMsg *,
	register __a1 char *,
	register __d0 char *,
	register __d1 long);
