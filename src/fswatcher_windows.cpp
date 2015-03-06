/*
   A small drop-in library for watching the filesystem for changes.

   version 0.1, february, 2015

   Copyright (C) 2015- Fredrik Kihlander

   This software is provided 'as-is', without any express or implied
   warranty.  In no event will the authors be held liable for any damages
   arising from the use of this software.

   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
      claim that you wrote the original software. If you use this software
      in a product, an acknowledgment in the product documentation would be
      appreciated but is not required.
   2. Altered source versions must be plainly marked as such, and must not be
      misrepresented as being the original software.
   3. This notice may not be removed or altered from any source distribution.

   Fredrik Kihlander
*/
#include <fswatcher/fswatcher.h>

#include <stdlib.h> // malloc
#include <windows.h>

#include <stdio.h> // remove

struct fswatcher
{
    fswatcher_allocator* allocator;
    bool recursive;
    bool blocking;

    const char* watch_dir;
    size_t watch_dir_len;

    HANDLE     directory;
    OVERLAPPED overlapped;

    DWORD read_buffer[2048]; // hmmmm.
};

static void* fswatcher_default_realloc( fswatcher_allocator*, void* ptr, size_t, size_t new_size )
{
	return realloc( ptr, new_size );
}

static void fswatcher_default_free( fswatcher_allocator*, void* ptr )
{
	free( ptr );
}

static fswatcher_allocator g_fswatcher_default_alloc = { fswatcher_default_realloc, fswatcher_default_free };

static void* fswatcher_realloc( fswatcher_allocator* allocator, void* ptr, size_t old_size, size_t new_size )
{
	return allocator->realloc( allocator, ptr, old_size, new_size );
}

static void fswatcher_free( fswatcher_allocator* allocator, void* ptr )
{
	if( allocator->free )
		allocator->free( allocator, ptr );
}

#define FS_MAKE_CALLBACK( type, src, dst ) handler->callback( handler, (type), (src), (dst) );

static char* fswatcher_build_full_path( fswatcher_t watcher, fswatcher_allocator* allocator, FILE_NOTIFY_INFORMATION* ev )
{
	size_t path_len = (size_t)( ev->FileNameLength / 2 );
	char* res = (char*)fswatcher_realloc( allocator, 0x0, 0, path_len + watcher->watch_dir_len + 1 );
	strcpy( res, watcher->watch_dir );

	// HACKY! convert to utf8 or keep as WCHAR depending on compile-define.
	for( size_t i = 0; i < path_len; ++i )
	{
		if( ev->FileName[i] > 255 )
			printf("whooot? %u\n", ev->FileName[i]);
		res[ watcher->watch_dir_len + i ] = ev->FileName[i] > 255 ? '_' : (char)ev->FileName[i];
	}
	res[watcher->watch_dir_len + path_len ] = '\0';
	return res;
}

static void fswatcher_begin_read( fswatcher_t watcher )
{
    ::ZeroMemory( &watcher->overlapped, sizeof( watcher->overlapped ) );

    BOOL success = ::ReadDirectoryChangesW( watcher->directory,
                                            watcher->read_buffer,
                                            sizeof( watcher->read_buffer ),
                                            watcher->recursive ? TRUE : FALSE,
                                            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
                                            0x0,
                                            &watcher->overlapped,
                                            0x0 );
    if( !success )
        printf("NOOOOOOOOOOOOOO!");
}

fswatcher_t fswatcher_create( fswatcher_create_flags flags, fswatcher_event_type types, const char* watch_dir, fswatcher_allocator* allocator )
{
    if( allocator == 0x0 )
		allocator = &g_fswatcher_default_alloc;

	fswatcher_t w = (fswatcher_t)fswatcher_realloc( allocator, 0x0, 0, sizeof( fswatcher ) );
    w->allocator = allocator;
    w->watch_dir_len = strlen( watch_dir );
    w->watch_dir = (char*)fswatcher_realloc( w->allocator, 0x0, 0, w->watch_dir_len );
    strcpy( (char*)w->watch_dir, watch_dir );
    w->recursive = ( flags & FSWATCHER_CREATE_RECURSIVE ) > 0;
    w->blocking  = ( flags & FSWATCHER_CREATE_BLOCKING ) > 0;
    w->directory = ::CreateFile( watch_dir,
                                 FILE_LIST_DIRECTORY,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 NULL, // security descriptor
                                 OPEN_EXISTING, // how to create
                                 FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, // file attributes
                                 NULL ); // file with attributes to copy
    // handle error ...

    fswatcher_begin_read( w );
	return w;
}

void fswatcher_destroy( fswatcher_t watcher )
{
    ::CloseHandle( watcher->directory );
	fswatcher_free( watcher->allocator, watcher );
}

void fswatcher_poll( fswatcher_t watcher, fswatcher_event_handler* handler, fswatcher_allocator* allocator )
{
    DWORD bytes;
    BOOL res = ::GetOverlappedResult( watcher->directory,
                                      &watcher->overlapped,
                                      &bytes,
                                      watcher->blocking );
    if( res != TRUE )
    	return;

    char* move_src = 0x0;

	FILE_NOTIFY_INFORMATION* ev = (FILE_NOTIFY_INFORMATION*)watcher->read_buffer;
	do
	{
		switch(ev->Action)
		{
			case FILE_ACTION_ADDED:
			{
				char* src = fswatcher_build_full_path( watcher, watcher->allocator, ev );
				FS_MAKE_CALLBACK( FSWATCHER_EVENT_CREATE, src, 0x0 );
				fswatcher_free( watcher->allocator, src );
			}
			break;
			case FILE_ACTION_REMOVED:
			{
				char* src = fswatcher_build_full_path( watcher, watcher->allocator, ev );
				FS_MAKE_CALLBACK( FSWATCHER_EVENT_REMOVE, src, 0x0 );
				fswatcher_free( watcher->allocator, src );
			}
			break;
			case FILE_ACTION_MODIFIED:
			{
				char* src = fswatcher_build_full_path( watcher, watcher->allocator, ev );
				FS_MAKE_CALLBACK( FSWATCHER_EVENT_MODIFY, src, 0x0 );
				fswatcher_free( watcher->allocator, src );
			}
			break;
			case FILE_ACTION_RENAMED_OLD_NAME:
			{
				move_src = fswatcher_build_full_path( watcher, watcher->allocator, ev );
			}
			break;
			case FILE_ACTION_RENAMED_NEW_NAME:
			{
				char* dst = fswatcher_build_full_path( watcher, watcher->allocator, ev );
				FS_MAKE_CALLBACK( FSWATCHER_EVENT_MOVE, move_src, dst );
				fswatcher_free( watcher->allocator, move_src );
				fswatcher_free( watcher->allocator, dst );
				move_src = 0x0;
			}
			break;
			default:
				printf("unhandled action %d\n", ev->Action);
		}

		if( ev->NextEntryOffset == 0 )
			break;
		ev = (FILE_NOTIFY_INFORMATION*)((char*)ev + ev->NextEntryOffset);
	}
	while( true );

	if( move_src != 0x0 )
	{
		// how to handle this?
		fswatcher_free( watcher->allocator, move_src );
	}

	fswatcher_begin_read( watcher );
}
