#include <fswatcher/fswatcher.h>

#include <sys/inotify.h>
#include <stdlib.h> // malloc
#include <unistd.h> // read
#include <stdio.h>  // printf
#include <dirent.h>
#include <string.h>

// write something about how we suppose that the kernels will keep on working as they do now:
// In the current kernel inotify implementation move events are always emitted as contiguous pairs with IN_MOVED_FROM immediately followed by IN_MOVED_TO

struct fswatcher_item
{
	int wd;
	const char* path;
};

struct fswatcher
{
	fswatcher_allocator* allocator;
	int notifierfd;

	int watching_dir_events;
	int watching_file_events;
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

static char* fswatcher_strdup( fswatcher_allocator* allocator, const char* str )
{
	size_t len = strlen( str ) + 1;
	char* res = (char*)fswatcher_realloc( allocator, 0x0, 0, len );
	strcpy( res, str );
	return res;
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

	int wd = inotify_add_watch( w->notifierfd, path, w->watch_flags );
	if( wd < 0 )
	{
		printf("buhuhuhu2\n");
		return;
	}
	w->watches[ w->num_watches ].wd = wd;
	w->watches[ w->num_watches ].path = fswatcher_strdup( w->allocator, path );
	++w->num_watches;
}

static void fswatcher_remove( fswatcher_t w, int wd )
{
	for( int i = 0; i < w->num_watches; ++ i )
	{
		if( wd != w->watches[i].wd )
			continue;

		fswatcher_free( w->allocator, (void*)w->watches[i].path );
		w->watches[i].wd = 0;
		w->watches[i].path = 0x0;

		int swap_index = w->num_watches - 1;
		if( i != swap_index )
			memcpy( w->watches + i, w->watches + swap_index, sizeof( fswatcher_item ) );
		--w->num_watches;
		return;
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

	fswatcher* w = (fswatcher*)fswatcher_realloc( allocator, 0x0, 0, sizeof( fswatcher ) );
	memset( w, 0x0, sizeof( fswatcher ) );
	w->allocator = allocator;
	w->watch_flags = 0;

	if( types & ( FSWATCHER_EVENT_FILE_CREATE | FSWATCHER_EVENT_DIR_CREATE ) ) w->watch_flags |= IN_CREATE;
	if( types & ( FSWATCHER_EVENT_FILE_REMOVE | FSWATCHER_EVENT_DIR_REMOVE ) ) w->watch_flags |= IN_DELETE;
	if( types & ( FSWATCHER_EVENT_FILE_MOVED  | FSWATCHER_EVENT_DIR_MOVED  ) ) w->watch_flags |= IN_MOVE;
	if( types &   FSWATCHER_EVENT_FILE_MODIFY ) w->watch_flags |= IN_MODIFY;
	if( types & ( FSWATCHER_EVENT_FILE_CREATE |
				  FSWATCHER_EVENT_FILE_REMOVE |
				  FSWATCHER_EVENT_FILE_MOVED ) )
		w->watching_file_events = 1;
	if( types & ( FSWATCHER_EVENT_DIR_CREATE |
				  FSWATCHER_EVENT_DIR_REMOVE |
				  FSWATCHER_EVENT_DIR_MOVED ) )
		w->watching_dir_events = 1;

	w->watch_flags |= IN_DELETE_SELF;

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
	for( int i = 0; i < watcher->num_watches; ++i )
		fswatcher_free( watcher->allocator, (void*)watcher->watches[i].path );
	fswatcher_free( watcher->allocator, watcher );
}

static char* fswatcher_build_full_path( fswatcher_t watcher, fswatcher_allocator* allocator, int wd, const char* name, uint32_t name_len )
{
	const char* dirpath = fswatcher_find_wd_path( watcher, wd );
	size_t dirlen = strlen( dirpath );
	size_t length = dirlen + 1 + name_len;
	char* res = (char*)fswatcher_realloc( allocator, 0x0, 0, length );
	if( res )
	{
		memcpy( res, dirpath, dirlen );
		res[dirlen-1] = '/';
		memcpy( res + dirlen, name, name_len );
		res[length-1] = 0;
	}
	return res;
}

#define FS_MAKE_CALLBACK( type, src, dst ) handler->callback( handler, (type), (src), (dst) );

static void fswatcher_make_callback_with_src_path( fswatcher_t watcher, fswatcher_event_handler* handler, fswatcher_event_type type, inotify_event* ev )
{
	char* src = fswatcher_build_full_path( watcher, watcher->allocator, ev->wd, ev->name, ev->len );
	FS_MAKE_CALLBACK( type, src, 0x0 );
	fswatcher_free( watcher->allocator, src );
}

static void fswatcher_make_callback_with_dst_path( fswatcher_t watcher, fswatcher_event_handler* handler, fswatcher_event_type type, inotify_event* ev )
{
	char* dst = fswatcher_build_full_path( watcher, watcher->allocator, ev->wd, ev->name, ev->len );
	FS_MAKE_CALLBACK( type, 0x0, dst );
	fswatcher_free( watcher->allocator, dst );
}

void fswatcher_poll( fswatcher_t watcher, fswatcher_event_handler* handler, fswatcher_allocator* allocator )
{
	if( allocator == 0x0 )
		allocator = &g_fswatcher_default_alloc;

	char*    move_src = 0x0;
	uint32_t move_cookie = 0;

	while( true )
	{
		char read_buffer[4096];
		ssize_t read_bytes = read( watcher->notifierfd, read_buffer, sizeof( read_buffer ) );
		if( read_bytes <= 0 )
			break;

		for( char* bufp = read_buffer; bufp < read_buffer + read_bytes; )
		{
			inotify_event* ev = (inotify_event*)bufp;
			bool is_dir       = ( ev->mask & IN_ISDIR );
			bool is_create    = ( ev->mask & IN_CREATE );
			bool is_remove    = ( ev->mask & IN_DELETE );
			bool is_modify    = ( ev->mask & IN_MODIFY );
			bool is_move_from = ( ev->mask & IN_MOVED_FROM );
			bool is_move_to   = ( ev->mask & IN_MOVED_TO );
			bool is_del_self  = ( ev->mask & IN_DELETE_SELF );

			if( is_dir )
			{
				if( watcher->watching_dir_events )
				{
					if( is_create )
					{
						char* src = fswatcher_build_full_path( watcher, allocator, ev->wd, ev->name, ev->len );
						fswatcher_add( watcher, src );
						FS_MAKE_CALLBACK( FSWATCHER_EVENT_DIR_CREATE, src, 0x0 );
						fswatcher_free( allocator, src );
					}
					else if( is_remove )
						fswatcher_make_callback_with_src_path( watcher, handler, FSWATCHER_EVENT_DIR_REMOVE, ev );
					else if( is_del_self )
						fswatcher_remove( watcher, ev->wd );
				}
			}
			else if( watcher->watching_file_events )
			{
				if( is_create )
					fswatcher_make_callback_with_src_path( watcher, handler, FSWATCHER_EVENT_FILE_CREATE, ev );
				else if( is_remove )
					fswatcher_make_callback_with_src_path( watcher, handler, FSWATCHER_EVENT_FILE_REMOVE, ev );
				else if( is_modify )
					fswatcher_make_callback_with_src_path( watcher, handler, FSWATCHER_EVENT_FILE_MODIFY, ev );
				else if( is_move_from )
				{
					if( move_src != 0x0 )
					{
						// ... this is a new pair of a move, so the last one was move "outside" the current watch ...
						FS_MAKE_CALLBACK( FSWATCHER_EVENT_FILE_MOVED, move_src, 0x0 );
						fswatcher_free( allocator, move_src );
					}

					// ... this is the first potential pair of a move ...
					move_src = fswatcher_build_full_path( watcher, allocator, ev->wd, ev->name, ev->len );
					move_cookie = ev->cookie;
				}
				else if( is_move_to )
				{
					if( move_src && move_cookie == ev->cookie )
					{
						// ... this is the dst for a move ...
						char* dst = fswatcher_build_full_path( watcher, allocator, ev->wd, ev->name, ev->len );
						FS_MAKE_CALLBACK( FSWATCHER_EVENT_FILE_MOVED, move_src, dst );
						fswatcher_free( watcher->allocator, dst );
						fswatcher_free( watcher->allocator, move_src );
						move_src = 0x0;
						move_cookie = 0;
					}
					else if( move_src != 0x0 )
					{
						// ... this is a "move to outside of watch" ...
						FS_MAKE_CALLBACK( FSWATCHER_EVENT_FILE_MOVED, move_src, 0x0 );
						fswatcher_free( watcher->allocator, move_src );
						move_src = 0x0;
						move_cookie = 0;

						// ...followed by a "move from outside to watch ...
						fswatcher_make_callback_with_dst_path( watcher, handler, FSWATCHER_EVENT_FILE_MOVED, ev );
					}
					else
					{
						// ... this is a "move from outside to watch" ...
						fswatcher_make_callback_with_dst_path( watcher, handler, FSWATCHER_EVENT_FILE_MOVED, ev );
					}
				}
			}
			else if( ev->mask & IN_Q_OVERFLOW )
			{
				FS_MAKE_CALLBACK( FSWATCHER_EVENT_BUFFER_OVERFLOW, 0x0, 0x0 );
			}

			bufp += sizeof(inotify_event) + ev->len;
		}
	}

	if( move_src )
	{
		// ... we have a "move to outside of watch" that was never closed ...
		FS_MAKE_CALLBACK( FSWATCHER_EVENT_FILE_MOVED, move_src, 0x0 );
		fswatcher_free( allocator, move_src );
	}
}

#undef FS_MAKE_CALLBACK

