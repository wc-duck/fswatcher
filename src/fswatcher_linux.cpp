#include <fswatcher/fswatcher.h>

#include <sys/inotify.h>
#include <stdlib.h> // malloc
#include <unistd.h> // read
#include <stdio.h>  // printf
#include <dirent.h>
#include <string.h>

struct fswatcher_item
{
	int wd;
	const char* path;
};

struct fswatcher
{
	fswatcher_allocator* allocator;
	int notifierfd;

	uint32_t watch_flags;

	int num_watches;
	fswatcher_item watches[256]; // TODO: configurable size!
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

static const char* fswatcher_find_wd_path( fswatcher_t w, int wd )
{
	for( int i = 0; i < w->num_watches; ++ i )
		if( wd == w->watches[i].wd )
			return w->watches[i].path;
	return 0x0;
}

static void fswatcher_add( fswatcher_t w, char* path )
{
	if( w->num_watches >= (int)( sizeof( w->watches ) / sizeof( w->watches[0] ) ) )
	{
		printf("to many dirs!\n");
		return;
	}

	w->watches[ w->num_watches ].wd = inotify_add_watch( w->notifierfd, path, IN_ALL_EVENTS );
	if( w->watches[ w->num_watches ].wd < 0 )
	{
		printf("buhuhuhu2\n");
		return;
	}
	w->watches[ w->num_watches ].path = strdup( path ); // TODO: how do we want to store these?
	++w->num_watches;
}

static void fswatcher_remove( fswatcher_t w, int wd )
{
	for( int i = 0; i < w->num_watches; ++ i )
	{
		if( wd == w->watches[i].wd )
		{
			fswatcher_free( w->allocator, (void*)w->watches[i].path );
			int swap_index = w->num_watches - 1;
			if( i != swap_index )
				memcpy( w->watches + i, w->watches + swap_index, sizeof( fswatcher_item ) );
			--w->num_watches;
			return;
		}
	}
}

static void fswatcher_recursive_add( fswatcher_t w, char* path_buffer, size_t path_len, size_t path_max )
{
	fswatcher_add( w, path_buffer );
	DIR* dirp = opendir( path_buffer );
	dirent* ent;

	path_buffer[path_len] = '/';
	while( ( ent = readdir( dirp ) ) != 0x0 )
	{
		if( ent->d_type != DT_DIR || strcmp( ent->d_name, "." ) == 0 || strcmp( ent->d_name, ".." ) == 0 )
			continue;

		size_t d_name_size = strlen( ent->d_name );
		if( path_len + d_name_size >= path_max )
			return; // TODO: handle!
		strcpy( path_buffer + path_len + 1, ent->d_name );
		fswatcher_recursive_add( w, path_buffer, path_len + 1 + d_name_size, path_max );
	}
	path_buffer[path_len] = '\0';

	closedir( dirp );
}

fswatcher_t fswatcher_create( fswatcher_create_flags flags, fswatcher_event_type types, const char* watch_dir, fswatcher_allocator* allocator )
{
	if( allocator == 0x0 )
		allocator = &g_fswatcher_default_alloc;
	(void)types;

	// TODO: memory alloc!
	fswatcher* w = (fswatcher*)fswatcher_realloc( allocator, 0x0, 0, sizeof( fswatcher ) );
	memset( w, 0x0, sizeof( fswatcher ) );

	int inotify_flags = 0;
	if( ( flags & FSWATCHER_CREATE_BLOCKING ) == 0 )
		inotify_flags |= IN_NONBLOCK;

	w->notifierfd = inotify_init1( inotify_flags );
	if( w->notifierfd < -1 )
	{
		fswatcher_free( allocator, w );
		return 0x0;
	}

	char path_buffer[4096];
	strncpy( path_buffer, watch_dir, sizeof( path_buffer ) );
	// TODO: make sure path fit ...

	fswatcher_recursive_add( w, path_buffer, strlen( path_buffer ), sizeof( path_buffer ) );
	return w;
}

void fswatcher_destroy( fswatcher_t watcher )
{
	close( watcher->notifierfd );
	fswatcher_free( watcher->allocator, watcher );
}

//static void mask_to_string( uint32_t mask )
//{
//    printf("mask = ");
//    if (mask & IN_ACCESS)        printf("IN_ACCESS ");
//    if (mask & IN_ATTRIB)        printf("IN_ATTRIB ");
//    if (mask & IN_CLOSE_NOWRITE) printf("IN_CLOSE_NOWRITE ");
//    if (mask & IN_CLOSE_WRITE)   printf("IN_CLOSE_WRITE ");
//    if (mask & IN_CREATE)        printf("IN_CREATE ");
//    if (mask & IN_DELETE)        printf("IN_DELETE ");
//    if (mask & IN_DELETE_SELF)   printf("IN_DELETE_SELF ");
//    if (mask & IN_IGNORED)       printf("IN_IGNORED ");
//    if (mask & IN_ISDIR)         printf("IN_ISDIR ");
//    if (mask & IN_MODIFY)        printf("IN_MODIFY ");
//    if (mask & IN_MOVE_SELF)     printf("IN_MOVE_SELF ");
//    if (mask & IN_MOVED_FROM)    printf("IN_MOVED_FROM ");
//    if (mask & IN_MOVED_TO)      printf("IN_MOVED_TO ");
//    if (mask & IN_OPEN)          printf("IN_OPEN ");
//    if (mask & IN_Q_OVERFLOW)    printf("IN_Q_OVERFLOW ");
//    if (mask & IN_UNMOUNT)       printf("IN_UNMOUNT ");
//    printf("\n");
//}

static char* fswatcher_build_full_path( fswatcher_t watcher, fswatcher_allocator* allocator, int wd, const char* name )
{
	const char* dirpath = fswatcher_find_wd_path( watcher, wd );
	size_t length = strlen( dirpath ) + 1 + strlen( name ) + 1;
	char* res = (char*)fswatcher_realloc( allocator, 0x0, 0, length );
	if( res )
	{
		// TODO: whuha =/ unsafe and slow!
		strcpy( res, dirpath );
		strcat( res, "/" );
		strcat( res, name );
	}
	return res;
}

#define FS_MAKE_CALLBACK( type, src, dst ) handler->callback( handler, (type), (src), (dst) );

void fswatcher_poll( fswatcher_t watcher, fswatcher_event_handler* handler, fswatcher_allocator* allocator )
{
	if( allocator == 0x0 )
		allocator = &g_fswatcher_default_alloc;

	while( true )
	{
		// TODO: switch to select!
		char read_buffer[4096];
		ssize_t read_bytes = read( watcher->notifierfd, read_buffer, sizeof( read_buffer ) );
		if( read_bytes <= 0 )
			return;

		for( char* bufp = read_buffer; bufp < read_buffer + read_bytes; )
		{
			inotify_event* ev = (inotify_event*)bufp;

			if( ev->mask & IN_ISDIR )
			{
				if( ev->mask & IN_CREATE )
				{
					char* src = fswatcher_build_full_path( watcher, allocator, ev->wd, ev->name );
					fswatcher_add( watcher, src );

					// ... notify user ...
					FS_MAKE_CALLBACK( FSWATCHER_EVENT_DIR_CREATE, src, 0x0 );
					fswatcher_free( allocator, src );
				}
				else if( ev->mask & IN_DELETE )
				{
					char* src = fswatcher_build_full_path( watcher, allocator, ev->wd, ev->name );
					fswatcher_remove( watcher, ev->wd );

					FS_MAKE_CALLBACK( FSWATCHER_EVENT_DIR_REMOVE, src, 0x0 );
					fswatcher_free( allocator, src );
				}
			}
			else
			{
				if( ev->mask & IN_CREATE )
				{
					char* src = fswatcher_build_full_path( watcher, allocator, ev->wd, ev->name );
					FS_MAKE_CALLBACK( FSWATCHER_EVENT_FILE_CREATE, src, 0x0 );
					fswatcher_free( allocator, src );
				}
				else if( ev->mask & IN_DELETE )
				{
					char* src = fswatcher_build_full_path( watcher, allocator, ev->wd, ev->name );
					FS_MAKE_CALLBACK( FSWATCHER_EVENT_FILE_REMOVE, src, 0x0 );
					fswatcher_free( allocator, src );
				}
				else if( ev->mask & IN_MODIFY )
				{
					char* src = fswatcher_build_full_path( watcher, allocator, ev->wd, ev->name );
					FS_MAKE_CALLBACK( FSWATCHER_EVENT_FILE_MODIFY, src, 0x0 );
					fswatcher_free( allocator, src );
				}

				// TODO: handle move!

				else if( ev->mask & IN_Q_OVERFLOW )
				{
					FS_MAKE_CALLBACK( FSWATCHER_EVENT_BUFFER_OVERFLOW, 0x0, 0x0 );
				}
//				else
//					mask_to_string( ev->mask );

				// handle overflow here!
			}

			bufp += sizeof(inotify_event) + ev->len;
		}
	}
}

#undef FS_MAKE_CALLBACK

