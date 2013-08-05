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
#include "async.h"

typedef struct _IFFHandle
{
	APTR				iff_File;			// File handle
	short				iff_Mode;			// Reading or writing
	short				iff_Error;			// Any error
	APTR				iff_Memory;			// Any memory used for the handle

	unsigned long		iff_Form;			// IFF Form
	long				iff_Size;			// Total size

	unsigned long		iff_Chunk;			// Current chunk
	long				iff_ChunkSize;		// Current chunk size
	long				iff_ChunkPos;		// Position of chunk in file
	long				iff_ChunkOK;		// Current chunk is OK?

	struct MinList		iff_ChunkData;		// Current chunk data

	struct MsgPort		*iff_ClipPort;		// Clipboard port
	struct IOClipReq	*iff_ClipReq;		// Clipboard request

	BPTR				iff_SafeFile;		// Safe file
	char				iff_TempName[20];	// Temporary name
	long				iff_Success;		// Indicates success
	BOOL				iff_Async;			// Using AsyncIO

	char				iff_Name[1];		// Filename
} IFFHandle;

typedef struct
{
	struct MinNode	chk_Node;
	long			chk_Size;
	char			chk_Data[1];
} IFFChunkData;

/*
#define IFF_READ		MODE_OLDFILE
#define IFF_WRITE		MODE_NEWFILE
#define IFF_CLIP		0x8000
#define IFF_CLIP_READ	(IFF_CLIP|IFF_READ)
#define IFF_CLIP_WRITE	(IFF_CLIP|IFF_WRITE)
#define IFF_SAFE		0x4000
*/

static long iff_Write(IFFHandle *,APTR,long);
static long iff_Seek(IFFHandle *,long,long);
static long iff_Read(IFFHandle *handle,APTR data,long size);

// Open an IFF file
IFFHandle *__asm __saveds L_IFFOpen(
	register __a0 char *name,
	register __d0 unsigned short mode,
	register __d1 ULONG form)
{
	IFFHandle *handle;
	char *filename=0;
	BOOL clip=0,safe=0,async=0;

	// Safe?
	if (mode&IFF_SAFE)
	{
		safe=1;
		mode&=~IFF_SAFE;
	}

	// Async?
	if (mode&IFF_ASYNC)
	{
#ifdef USE_ASYNC
		async=1;
#endif
		mode&=~IFF_ASYNC;
	}

	// Clipboard?
	if (mode&IFF_CLIP)
	{
		clip=1;
		mode&=~IFF_CLIP;
	}

	// Valid filename?
	else
	if (name) filename=name;

	// Allocate handle
	if (!(handle=AllocVec(sizeof(IFFHandle)+(filename?strlen(filename):0),MEMF_CLEAR)))
		return 0;

	// Store mode, form and name
	handle->iff_Mode=mode;
	handle->iff_Form=form;
	if (filename) strcpy(handle->iff_Name,filename);
	handle->iff_Success=1;
	handle->iff_Async=async;

	// Allocate memory handle
	if (!(handle->iff_Memory=L_NewMemHandle(1024,256,MEMF_CLEAR)))
	{
		FreeVec(handle);
		return 0;
	}

	// Clipboard?
	if (clip)
	{
		// Open clipboard
		if (!(handle->iff_ClipPort=CreateMsgPort()) ||
			!(handle->iff_ClipReq=(struct IOClipReq *)
				CreateIORequest(handle->iff_ClipPort,sizeof(struct IOClipReq))) ||
			(OpenDevice("clipboard.device",(ULONG)name,(struct IORequest *)handle->iff_ClipReq,0)))
		{
			L_IFFClose(handle);
			return 0;
		}

		// File is open
		handle->iff_File=(APTR)1;
	}
		
	// File
	else
	{
		// Safe?
		if (safe)
		{
			// Lock existing file
			if (handle->iff_SafeFile=Lock(name,ACCESS_READ))
			{
				BPTR parent;
				short res=0;
				struct FileInfoBlock __aligned fib;

				// Get file information
				Examine(handle->iff_SafeFile,&fib);

				// See if file is read-only
				if (fib.fib_Protection&FIBF_DELETE)
				{
					// We fail since this can't be over-written
				}

				// Get parent directory
				else
				if (parent=ParentDir(handle->iff_SafeFile))
				{
					long key;
					char __aligned oldname[34];
					short a;

					// Rename file
					key=(long)FindTask(0)+(long)parent;

					// Get old filename
					strcpy(oldname+1,FilePart(name));
					oldname[0]=strlen(oldname+1);

					// Try a maximum of 3 times
					for (a=0;a<3;a++)
					{
						// Build name
						lsprintf(handle->iff_TempName+1,"safe%lx",key);
						handle->iff_TempName[0]=strlen(handle->iff_TempName+1);

						// Try to rename file
						if (res=DoPkt(
							((struct FileLock *)BADDR(parent))->fl_Task,
							ACTION_RENAME_OBJECT,
							parent,
							MKBADDR(oldname),
							parent,
							MKBADDR(handle->iff_TempName),0)) break;

						// If this failed other than because object exists, abort
						if (IoErr()!=ERROR_OBJECT_EXISTS) break;

						// Increment key and try again
						key+=3;
					}

					// Unlock parent
					UnLock(parent);
				}

				// Unsuccessful?
				if (!res)
				{
					// Unlock safe file
					UnLock(handle->iff_SafeFile);
					handle->iff_SafeFile=0;

					// Failed to open file
					L_IFFClose(handle);
					return 0;
				}
			}
		}

#ifdef USE_ASYNC
		// Async?
		if (handle->iff_Async)
		{
			// Open file
			if (!(handle->iff_File=OpenAsync(name,(mode==IFF_READ)?MODE_READ:MODE_WRITE,8192)))
				handle->iff_Async=0;
		}
#endif
		
		// Open file
		if (!handle->iff_File &&
			!(handle->iff_File=L_OpenBuf(name,mode,4096)))
		{
			// Failed to open file
			L_IFFClose(handle);
			return 0;
		}
	}

	// Writing file?
	if (mode==IFF_WRITE)
	{
		ULONG foo[3];

		// Initialise FORM header
		foo[0]=ID_FORM;
		foo[1]=0;
		foo[2]=form;

		// Write form header
		if ((iff_Write(handle,&foo,sizeof(ULONG)*3))!=sizeof(ULONG)*3)
		{
			long error;

			// Failed; Remember error
			error=IoErr();

			// Close up
			L_IFFClose(handle);

			// Delete partial file
			if (!clip) DeleteFile(name);

			// Restore error
			SetIoErr(error);
			return 0;
		}

		// Initialise size
		handle->iff_Size=sizeof(ULONG);
	}

	// Reading
	else
	{
		ULONG foo[3];

		// Read header
		if ((iff_Read(handle,&foo,sizeof(ULONG)*3))!=sizeof(ULONG)*3)
		{
			long error;

			// Failed; Remember error
			error=IoErr();

			// Close up
			L_IFFClose(handle);

			// Restore error
			SetIoErr(error);
			return 0;
		}

		// Check form for validity
		if (foo[0]!=ID_FORM || (form && foo[2]!=form))
		{
			// Close up
			L_IFFClose(handle);

			// Set error
			SetIoErr(ERROR_OBJECT_WRONG_TYPE);
			return 0;
		}

		// Initialise info
		handle->iff_Size=foo[1];
		handle->iff_Form=foo[2];
	}

	return handle;
}


// Close an IFF file
void __asm __saveds L_IFFClose(register __a0 IFFHandle *handle)
{
	// Valid handle?
	if (handle)
	{
		// Is file open?
		if (handle->iff_File)
		{
			// Were we writing?
			if (handle->iff_Mode==IFF_WRITE)
			{
				// Written any data?
				if (handle->iff_Size)
				{
					// Seek back to start of file
					iff_Seek(handle,sizeof(ULONG),OFFSET_BEGINNING);

					// Write out size
					iff_Write(handle,&handle->iff_Size,sizeof(ULONG));
				}
			}

			// Clipboard?
			if (handle->iff_ClipReq)
			{
				// Were we writing?
				if (handle->iff_Mode==IFF_WRITE)
				{
					// Update clipboard
					handle->iff_ClipReq->io_Command=CMD_UPDATE;
					DoIO((struct IORequest *)handle->iff_ClipReq);
				}

				// Reading
				else
				{
					char buffer[128];

					// Flush clipboard
					handle->iff_ClipReq->io_Command=CMD_READ;
					handle->iff_ClipReq->io_Data=buffer;
					handle->iff_ClipReq->io_Length=127;
					while (handle->iff_ClipReq->io_Actual)
						if (DoIO((struct IORequest *)handle->iff_ClipReq))
							break;
				}

				// Close clipboard
				CloseDevice((struct IORequest *)handle->iff_ClipReq);
			}

			// A file
			else
			{
				// Close file
#ifdef USE_ASYNC
				if (handle->iff_Async) CloseAsync(handle->iff_File);
				else L_CloseBuf(handle->iff_File);
#else
				L_CloseBuf(handle->iff_File);
#endif

				// Were we writing?
				if (handle->iff_Mode==IFF_WRITE)
				{
					// Set correct protection bits
					SetProtection(handle->iff_Name,FIBF_EXECUTE);
				}
			}

			// Safe file?
			if (handle->iff_SafeFile)
			{
				BPTR parent;

				// Get parent lock
				if (parent=ParentDir(handle->iff_SafeFile))
				{
					// Unlock safe file
					UnLock(handle->iff_SafeFile);

					// Successful?
					if (handle->iff_Success)
					{
						// Delete safe file
						DoPkt(
							((struct FileLock *)BADDR(parent))->fl_Task,
							ACTION_DELETE_OBJECT,
							parent,
							MKBADDR(handle->iff_TempName),
							0,0,0);
					}

					// Need to rename back
					else
					{
						char __aligned oldname[34];

						// Delete new file
						DeleteFile(handle->iff_Name);

						// Get old filename
						strcpy(oldname+1,FilePart(handle->iff_Name));
						oldname[0]=strlen(oldname+1);

						// Rename file
						DoPkt(
							((struct FileLock *)BADDR(parent))->fl_Task,
							ACTION_RENAME_OBJECT,
							parent,
							MKBADDR(handle->iff_TempName),
							parent,
							MKBADDR(oldname),0);
					}

					// Unlock parent
					UnLock(parent);
				}

				// Failed, unlock lock
				else UnLock(handle->iff_SafeFile);
			}
		}

		// Close clipboard stuff
		if (handle->iff_ClipReq)
			DeleteIORequest((struct IORequest *)handle->iff_ClipReq);
		if (handle->iff_ClipPort)
			DeleteMsgPort(handle->iff_ClipPort);

		// Free memory
		L_FreeMemHandle(handle->iff_Memory);

		// Free handle
		FreeVec(handle);
	}
}


// Start writing a chunk
long __asm __saveds L_IFFPushChunk(
	register __a0 IFFHandle *handle,
	register __d0 ULONG id)
{
	// Valid handle?
	if (!handle) return 0;

	// Initialise current chunk
	handle->iff_Chunk=id;
	handle->iff_ChunkSize=0;
	handle->iff_ChunkOK=1;
	NewList((struct List *)&handle->iff_ChunkData);

	// Store current position
	handle->iff_ChunkPos=iff_Seek(handle,0,OFFSET_CURRENT);

	return 1;
}


// Take some data for the chunk
long __asm __saveds L_IFFWriteChunkBytes(
	register __a0 IFFHandle *handle,
	register __a1 char *data,
	register __d0 long size)
{
	IFFChunkData *chunkdata;

	// Valid handle?
	if (!handle) return 0;

	// Can we buffer this?
	if (!handle->iff_ChunkOK ||
		!(chunkdata=L_AllocMemH(handle->iff_Memory,sizeof(IFFChunkData)+size)))
	{
		// Were we buffering before?
		if (handle->iff_ChunkOK)
		{
			// Set OK flag so we won't get null-padded
			handle->iff_ChunkOK=2;

			// Pop chunk and write what we can
			L_IFFPopChunk(handle);

			// Clear OK flag
			handle->iff_ChunkOK=0;
		}

		// Write straight to disk
		if (iff_Write(handle,data,size)!=size) return 0;

		// Increment total size
		handle->iff_Size+=size;
	}

	// We can
	else
	{
		// Store size
		chunkdata->chk_Size=size;

		// Copy data
		CopyMem(data,chunkdata->chk_Data,size);

		// Add to data list
		AddTail((struct List *)&handle->iff_ChunkData,(struct Node *)chunkdata);
	}

	// Increment chunk size
	handle->iff_ChunkSize+=size;
	return 1;
}


// Write a chunk to disk
long __asm __saveds L_IFFPopChunk(register __a0 IFFHandle *handle)
{
	char pad=0;
	long result=1;

	// Valid handle?
	if (!handle) return 0;

	// Was chunk unbuffered?
	if (!handle->iff_ChunkOK)
	{
		long cur_pos;

		// Seek back to start of chunk
		cur_pos=iff_Seek(handle,handle->iff_ChunkPos+sizeof(long),OFFSET_BEGINNING);

		// Write chunk size
		iff_Write(handle,&handle->iff_ChunkSize,sizeof(long));

		// Seek back to end of chunk
		iff_Seek(handle,cur_pos,OFFSET_BEGINNING);
	}

	// It was buffered
	else
	{
		IFFChunkData *data;

		// Write chunk header
		if (iff_Write(handle,&handle->iff_Chunk,sizeof(long)*2)!=sizeof(long)*2)
			result=0;

		else
		{
			// Increment total size
			handle->iff_Size+=sizeof(long)*2;

			// Go through data chunks
			for (data=(IFFChunkData *)handle->iff_ChunkData.mlh_Head;
				data->chk_Node.mln_Succ;
				data=(IFFChunkData *)data->chk_Node.mln_Succ)
			{
				// Write chunk data
				if (iff_Write(handle,data->chk_Data,data->chk_Size)!=data->chk_Size)
				{
					result=0;
					break;
				}

				// Increment total size
				handle->iff_Size+=data->chk_Size;
			}
		}
	}

	// Ok so far?
	if (result && handle->iff_ChunkOK!=2)
	{
		// Does chunk need to be null-padded?
		if (handle->iff_ChunkSize%2)
		{
			if (iff_Write(handle,&pad,1)!=1) result=0;
			else ++handle->iff_Size;
		}
	}

	// Clear chunk list and memory
	NewList((struct List *)&handle->iff_ChunkData);
	L_ClearMemHandle(handle->iff_Memory);

	return result;
}


// Write a chunk straight out
long __asm __saveds L_IFFWriteChunk(
	register __a0 IFFHandle *handle,
	register __a1 char *data,
	register __d0 ULONG chunk,
	register __d1 ULONG size)
{
	ULONG foo[2];
	char pad=0;

	// Valid handle?
	if (!handle) return 0;

	// Initialise chunk header
	foo[0]=chunk;
	foo[1]=size;

	// Write header
	if (iff_Write(handle,foo,sizeof(ULONG)*2)!=sizeof(ULONG)*2)
		return 0;

	// Increment total size
	handle->iff_Size+=sizeof(ULONG)*2;

	// Write data
	if (iff_Write(handle,data,size)!=size)
		return 0;

	// Increment total size
	handle->iff_Size+=size;

	// Null-pad if necessary
	if (size%2)
	{
		if (iff_Write(handle,&pad,1)!=1) return 0;
		else ++handle->iff_Size;
	}

	return 1;
}


// Skip to the next chunk
unsigned long __asm __saveds L_IFFNextChunk(
	register __a0 IFFHandle *handle,
	register __d0 unsigned long form)
{
	// Loop until satisfied
	FOREVER
	{
		// Got a valid chunk currently?
		if (handle->iff_ChunkOK)
		{
			// More to skip?
			if (handle->iff_ChunkPos<handle->iff_ChunkSize)
				iff_Seek(handle,handle->iff_ChunkSize-handle->iff_ChunkPos,OFFSET_CURRENT);

			// Skip null-pad?
			if (handle->iff_ChunkSize%2)
				iff_Seek(handle,1,OFFSET_CURRENT);
		}

		// Clear ID
		handle->iff_Chunk=0;
		handle->iff_ChunkOK=0;
		handle->iff_ChunkPos=0;

		// Read chunk header
		if ((iff_Read(handle,&handle->iff_Chunk,sizeof(ULONG)*2))!=sizeof(ULONG)*2)
			return 0;

		// Is this a FORM?
		if (handle->iff_Chunk==ID_FORM)
		{
			// Skip over FORM ID (won't handle nested FORMs as such)
			iff_Seek(handle,sizeof(ULONG),OFFSET_CURRENT);
			continue;
		}

		// Chunk is ok
		handle->iff_ChunkOK=1;

		// Don't care what chunk it is?
		if (!form) return handle->iff_Chunk;

		// Is it what we're looking for?
		if (form==handle->iff_Chunk) return handle->iff_Chunk;
	}
}


// Get current chunk ID
unsigned long __asm __saveds L_IFFChunkID(register __a0 IFFHandle *handle)
{
	if (handle->iff_ChunkOK) return handle->iff_Chunk;
	else return 0;
}


// Get size of the current chunk
long __asm __saveds L_IFFChunkSize(register __a0 IFFHandle *handle)
{
	if (handle->iff_ChunkOK) return handle->iff_ChunkSize;
	else return -1;
}


// Get the amount left of the current chunk
long __asm __saveds L_IFFChunkRemain(register __a0 IFFHandle *handle)
{
	if (handle->iff_ChunkOK) return handle->iff_ChunkSize-handle->iff_ChunkPos;
	else return 0;
}


// Get the current FORM
unsigned long __asm __saveds L_IFFGetFORM(register __a0 IFFHandle *handle)
{
	return handle->iff_Form;
}


// Read data from current chunk
long __asm __saveds L_IFFReadChunkBytes(
	register __a0 IFFHandle *handle,
	register __a1 APTR buffer,
	register __d0 long size)
{
	// Valid chunk?
	if (!handle->iff_ChunkOK) return -1;

	// Read full amount?
	if (size==-1)
		size=handle->iff_ChunkSize-handle->iff_ChunkPos;

	// Check we're not trying to read too much
	else
	if (size>handle->iff_ChunkSize-handle->iff_ChunkPos)
		size=handle->iff_ChunkSize-handle->iff_ChunkPos;

	// Anything to read?
	if (!size) return 0;

	// Read data
	if ((size=iff_Read(handle,buffer,size))>0)
	{
		// Advance buffer pointer
		handle->iff_ChunkPos+=size;
	}

	return size;
}


// Get file handle
APTR __asm __saveds L_IFFFileHandle(register __a0 IFFHandle *handle)
{
	return (handle)?handle->iff_File:0;
}


static long iff_Write(IFFHandle *handle,APTR data,long size)
{
	// Clipboard?
	if (handle->iff_ClipReq)
	{
		handle->iff_ClipReq->io_Command=CMD_WRITE;
		handle->iff_ClipReq->io_Data=data;
		handle->iff_ClipReq->io_Length=size;
		DoIO((struct IORequest *)handle->iff_ClipReq);
		return (long)handle->iff_ClipReq->io_Actual;
	}

#ifdef USE_ASYNC
	// Async
	else
	if (handle->iff_Async)
		return WriteAsync(handle->iff_File,data,size);

	// File
	else return L_WriteBuf(handle->iff_File,(char *)data,size);
#else
	return L_WriteBuf(handle->iff_File,(char *)data,size);
#endif
}

static long iff_Read(IFFHandle *handle,APTR data,long size)
{
	// Clipboard?
	if (handle->iff_ClipReq)
	{
		handle->iff_ClipReq->io_Command=CMD_READ;
		handle->iff_ClipReq->io_Data=data;
		handle->iff_ClipReq->io_Length=size;
		DoIO((struct IORequest *)handle->iff_ClipReq);
		return (long)handle->iff_ClipReq->io_Actual;
	}

#ifdef USE_ASYNC
	// Async
	else
	if (handle->iff_Async)
		return ReadAsync(handle->iff_File,data,size);

	// File
	else return L_ReadBuf(handle->iff_File,(char *)data,size);
#else
	return L_ReadBuf(handle->iff_File,(char *)data,size);
#endif
}

static long iff_Seek(IFFHandle *handle,long offset,long mode)
{
	// Clipboard?
	if (handle->iff_ClipReq)
	{
		long old=handle->iff_ClipReq->io_Offset;

		// Look at mode
		switch (mode)
		{
			case OFFSET_BEGINNING:
				handle->iff_ClipReq->io_Offset=offset;
				break;

			case OFFSET_CURRENT:
				handle->iff_ClipReq->io_Offset+=offset;
				break;

			default:
				old=-1;
				break;
		}

		return old;
	}

#ifdef USE_ASYNC
	// Async
	else
	if (handle->iff_Async)
		return SeekAsync(handle->iff_File,offset,mode);

	// File
	else return L_SeekBuf(handle->iff_File,offset,mode);
#else
	return L_SeekBuf(handle->iff_File,offset,mode);
#endif
}


// Indicate failure to have "Safe" file renamed
void __asm __saveds L_IFFFailure(register __a0 IFFHandle *iff)
{
	if (iff) iff->iff_Success=0;
}
