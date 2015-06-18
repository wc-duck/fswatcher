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

#include "greatest.h"
#include <fswatcher/fswatcher.h>

#include <string.h>
#include <stdlib.h> // system

#include <stdio.h>

#if defined( _WIN32 )
#  include <windows.h>
#  define DIR_SEP "\\"
#else
#  define DIR_SEP "/"
#endif

static const char* get_test_dir()
{
	static char temp_path[4096] = {0};
	static bool fetched = false;
	if( !fetched )
	{
#if defined( _WIN32 )
		GetTempPathA( sizeof( temp_path ), temp_path );
		strcat( temp_path, "\\fswatcher_test\\" );
#else
		strcat( temp_path, P_tmpdir );
		strcat( temp_path, "/fswatcher_test/" );
#endif
		printf( "Running tests in \"%s\"\n", temp_path );
		fetched = true;
	}
	return temp_path;
}

static void run_system( const char* cmd, const char* path )
{
	char buffer[4096] = {0};
	strcat( buffer, cmd );
	strcat( buffer, path );
	// printf("system: %s\n", buffer);
	if( system( buffer ) < 0 )
		printf("faied to run system( %s )\n", buffer );
}

#if defined( _WIN32 )
static void create_dir ( const char* dirname  ) { run_system( "mkdir ", dirname ); }
static void create_file( const char* filename ) { run_system( "type nul>> ", filename ); }
static void remove_dir ( const char* dirname  ) { run_system( "rmdir /q /s ", dirname ); }
static void remove_file( const char* filename ) { run_system( "del ", filename ); }
static void move_file( const char* src, const char* dst )
{
	char buffer[4096] = {0};
	strcat( buffer, "move " );
	strcat( buffer, src );
	strcat( buffer, " " );
	strcat( buffer, dst );
	strcat( buffer, " >nul 2>&1" );
	if( system( buffer ) < 0 )
		printf("faied to run system( %s )\n", buffer );
}
#else
static void create_dir ( const char* dirname  ) { run_system( "mkdir -p ", dirname ); }
static void create_file( const char* filename ) { run_system( "touch ", filename ); }
static void remove_dir ( const char* dirname  ) { run_system( "rm -rf ", dirname ); }
static void remove_file( const char* filename ) { run_system( "rm ", filename ); }
static void move_file( const char* src, const char* dst )
{
	char buffer[4096] = {0};
	strcat( buffer, "mv " );
	strcat( buffer, src );
	strcat( buffer, " " );
	strcat( buffer, dst );
	if( system( buffer ) < 0 )
		printf("faied to run system( %s )\n", buffer );
}

static void create_symlink( const char* dst, const char* name, const char* src )
{
	char buffer[4096] = {0};
	strcat( buffer, "cd " );
	strcat( buffer, dst );
	strcat( buffer, ";ln -s " );
	strcat( buffer, src );
	strcat( buffer, " " );
	strcat( buffer, name );
	// printf( "%s\n", buffer );
	if( system( buffer ) < 0 )
		printf("faied to run system( %s )\n", buffer );
}
#endif

static const char* test_dir_path( const char* path, char* buffer )
{
	buffer[0] = '\0';
	strcat( buffer, get_test_dir() );
	strcat( buffer, path );
	return buffer;
}

static const char* test_dir_path( const char* path )
{
	static char buffer[4096];
	return test_dir_path( path, buffer );
}

static void setup_test_dir()
{
	// ... remove dir from prev test ...
	remove_dir( get_test_dir() );

	// ... and recreate it ...
	create_dir( get_test_dir() );
}

struct test_handler
{
	fswatcher_event_handler handler;
	fswatcher_event_type type;
	const char* src;
	const char* dst;
};

#define HANDLER_RESET( handler ) \
	free( (void*)handler.src ); handler.src = 0x0; \
	free( (void*)handler.dst ); handler.dst = 0x0; \
	handler.type = FSWATCHER_EVENT_ALL

static bool watch_event_handler( fswatcher_event_handler* handler, fswatcher_event_type evtype, const char* src, const char* dst )
{
	test_handler* h = (test_handler*)handler;
	h->type = evtype;
	h->src  = src ? strdup( src ) : 0;
	h->dst  = dst ? strdup( dst ) : 0;
	return true;
}

int check_event_handler( fswatcher_event_type evtype, const char* src, const char* dst, test_handler* handler )
{
	if( handler->type != evtype )
		printf("%d != %d, %s %s\n", handler->type, evtype, src, dst);
	ASSERT_EQ( handler->type, evtype );
	if( src )
		ASSERT_STR_EQ( src, handler->src );
	else
		ASSERT_EQ( src, handler->src );
	if( dst )
		ASSERT_STR_EQ( dst, handler->dst );
	else
		ASSERT_EQ( dst, handler->dst );
	return 0;
}

TEST create_remove_dir()
{
	setup_test_dir();

	fswatcher_t watcher = fswatcher_create( FSWATCHER_CREATE_DEFAULT, FSWATCHER_EVENT_ALL, get_test_dir(), 0x0 );
	ASSERT( 0x0 != watcher );
	test_handler handler = { { watch_event_handler }, FSWATCHER_EVENT_ALL, 0x0, 0x0 };

	const char* path = test_dir_path( "d1" );

	create_dir( path );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	if( int ret = check_event_handler( FSWATCHER_EVENT_CREATE, path, 0x0, &handler ) ) return ret;
	HANDLER_RESET( handler );

	remove_dir( path );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	if( int ret = check_event_handler( FSWATCHER_EVENT_REMOVE, path, 0x0, &handler ) ) return ret;
	HANDLER_RESET( handler );

	create_dir( path );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	if( int ret = check_event_handler( FSWATCHER_EVENT_CREATE, path, 0x0, &handler ) ) return ret;
	HANDLER_RESET( handler );

	fswatcher_destroy( watcher );
	return 0;
}

TEST create_remove_file()
{
	setup_test_dir();

	fswatcher_t watcher = fswatcher_create( FSWATCHER_CREATE_DEFAULT, FSWATCHER_EVENT_ALL, get_test_dir(), 0x0 );
	test_handler handler = { { watch_event_handler }, FSWATCHER_EVENT_ALL, 0x0, 0x0 };

	const char* path = test_dir_path( "f1" );
	create_file( path );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	ASSERT_EQ( 0, check_event_handler( FSWATCHER_EVENT_CREATE, path, 0x0, &handler ) );
	HANDLER_RESET( handler );

	remove_file( path );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	ASSERT_EQ( 0, check_event_handler( FSWATCHER_EVENT_REMOVE, path, 0x0, &handler ) );
	HANDLER_RESET( handler );

	fswatcher_destroy( watcher );
	return 0;
}

TEST create_remove_file_in_subdir()
{
#if !defined( _WIN32 )
	// TODO: make this work
	//       the events are posted but they require some polling and also on windows we get a modify
	//       event for the containing dir that we do not get on linux, how to handle? ( add event on linux,
	//       ignore modified for dirs on windows, just let the platforms differ? )
	setup_test_dir();
	create_dir( test_dir_path( "subdir" ) );

	fswatcher_t watcher = fswatcher_create( FSWATCHER_CREATE_DEFAULT, FSWATCHER_EVENT_ALL, get_test_dir(), 0x0 );
	test_handler handler = { { watch_event_handler }, FSWATCHER_EVENT_ALL, 0x0, 0x0 };

	const char* path = test_dir_path( "subdir" DIR_SEP "f1" );
	create_file( path );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	ASSERT_EQ( 0, check_event_handler( FSWATCHER_EVENT_CREATE, path, 0x0, &handler ) );
	HANDLER_RESET( handler );

	remove_file( path );

	fswatcher_poll( watcher, &handler.handler, 0x0 );
	ASSERT_EQ( 0, check_event_handler( FSWATCHER_EVENT_REMOVE, test_dir_path( "subdir" DIR_SEP "f1" ), 0x0, &handler ) );
	HANDLER_RESET( handler );

	fswatcher_destroy( watcher );
#endif
	return 0;
}

TEST test_move_file()
{
	setup_test_dir();
	char path1[2048];
	char path2[2048];
	test_dir_path( "f1", path1 );
	test_dir_path( "f2", path2 );

	create_file( path1 );

	fswatcher_t watcher = fswatcher_create( FSWATCHER_CREATE_DEFAULT, FSWATCHER_EVENT_ALL, get_test_dir(), 0x0 );
	test_handler handler = { { watch_event_handler }, FSWATCHER_EVENT_ALL, 0x0, 0x0 };

	move_file( path1, path2 );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	if( int ret = check_event_handler( FSWATCHER_EVENT_MOVE, path1, path2, &handler ) ) return ret;
	HANDLER_RESET( handler );

	fswatcher_destroy( watcher );
	return 0;
}

TEST watch_symlinked_dir()
{
#if !defined( _WIN32 )
	// TODO: make this work on windows
	setup_test_dir();
	char watch_me_dir_path[2048];
	char symlinked_dir_path[2048];
	char test_file1_path[2048];
	char test_file2_path[2048];
	test_dir_path( "watch_me", watch_me_dir_path );
	test_dir_path( "symlinked", symlinked_dir_path );
	test_dir_path( "watch_me/linked/test.file", test_file1_path );
	test_dir_path( "symlinked/test.file", test_file2_path );

	create_dir( watch_me_dir_path );
	create_dir( symlinked_dir_path );

	create_symlink( watch_me_dir_path, "linked", symlinked_dir_path );

	fswatcher_t watcher = fswatcher_create( FSWATCHER_CREATE_DEFAULT, FSWATCHER_EVENT_ALL, watch_me_dir_path, 0x0 );
	test_handler handler = { { watch_event_handler }, FSWATCHER_EVENT_ALL, 0x0, 0x0 };

	create_file( test_file2_path );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	if( int ret = check_event_handler( FSWATCHER_EVENT_CREATE, test_file1_path, 0x0, &handler ) ) return ret;
	HANDLER_RESET( handler );

	remove_file( test_file2_path );
	fswatcher_poll( watcher, &handler.handler, 0x0 );

	if( int ret = check_event_handler( FSWATCHER_EVENT_REMOVE, test_file1_path, 0x0, &handler ) ) return ret;
	HANDLER_RESET( handler );

	fswatcher_destroy( watcher );
#endif
	return 0;
}

GREATEST_SUITE( fswatcher )
{
	RUN_TEST( create_remove_dir );
	RUN_TEST( create_remove_file );
	RUN_TEST( create_remove_file_in_subdir );
	RUN_TEST( test_move_file );
	RUN_TEST( watch_symlinked_dir );
}

GREATEST_MAIN_DEFS();

int main( int argc, char **argv )
{
	get_test_dir();

    GREATEST_MAIN_BEGIN();
    RUN_SUITE( fswatcher );
    GREATEST_MAIN_END();
}
