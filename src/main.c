/*
 * Copyright (C) 2019, Joe Marrero. http://www.joemarrero.com/
 * Copyright (C) 2017, End Point Corporation. http://www.endpoint.com/
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#define __USE_XOPEN
#include <time.h>
#include <limits.h>
#include <regex.h>
#include <xtd/console.h>
#include <xtd/filesystem.h>
#include <xtd/string.h>
#include <xtd/time.h>
#define VECTOR_GROW_AMOUNT(array)      (1)
#include <collections/vector.h>
#include <collections/tree-map.h>
#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
#else
# include <unistd.h>
# include <pwd.h>
#endif

#define VERSION                 "1.2.2"
#define CONFIGURATION_FILENAME  ".timezoner"

typedef struct timezone_contact {
	double utc_offset;
	const char* timezone; /* IANA Timezone Code; https://en.wikipedia.org/wiki/List_of_tz_database_time_zones */
	const wchar_t* email;
	const wchar_t* name;
	const wchar_t* office_phone;
	const wchar_t* mobile_phone;
} timezone_contact_t;

typedef struct tz_app { /* App state */
	bool minimal;
	bool organize_by_time;
	int column_widths[ 2 ];
	time_t now;
} tz_app_t;

static void tz_about ( int argc, char* argv[] );
static void tz_print_error ( const tz_app_t* app,  const char* format, ... );
static void tz_organize_data ( const timezone_contact_t* contacts, lc_tree_map_t* map, time_t now, bool organize_by_time );
static bool tz_read_configuration_from_home ( const tz_app_t* app, timezone_contact_t** contacts );
static bool tz_configuration_read ( const tz_app_t* app, const char* configuration_name, timezone_contact_t** contacts );
static bool tz_configuration_read_line ( const tz_app_t* app, char* line, int line_number, regex_t* regex, timezone_contact_t** contacts );
static bool tz_configuration_write_default ( const char* configuration_filename );
static void tz_display_time_grouping ( lc_tree_map_t* map, time_t now, int name_width, int email_width );
static void tz_display_time_grouping_minimal ( lc_tree_map_t* map, time_t now, int name_width, int email_width );
static void tz_display_utc_grouping ( lc_tree_map_t* map, time_t now );
static void tz_display_utc_grouping_minimal ( lc_tree_map_t* map, time_t now );
static bool timezone_map_element_destroy ( void *p_key, void *p_value );
static int  timezone_map_compare ( const void *p_key_left, const void *p_key_right );
static int  contact_name_compare ( const void *l, const void *r );
static bool tz_check_alloc( const tz_app_t* app, void* mem );


int main( int argc, char* argv[] )
{
	tz_app_t app = (tz_app_t) {
		.minimal = false,
		.organize_by_time = true, // this is the default
		.column_widths = { 30, 25 },
		.now = time(NULL)
	};
	const char* configuration_name = NULL;

	setlocale( LC_ALL, "" );

	if( argc >= 2 )
	{
		// Since we have at least two command line arguments
		// let's parse them.
		for( int arg = 1; arg < argc; arg++ )
		{
			if( strcmp( "-T", argv[arg] ) == 0 || strcmp( "--group-time", argv[arg] ) == 0 )
			{
				app.organize_by_time = true;

				if( (arg + 1) < argc )
				{
					if( *argv[ arg + 1 ] != '-' )
					{
						app.column_widths[ 0 ] = atoi( argv[ arg + 1 ] );
						arg += 1;
					}

				}

				if( (arg + 1) < argc )
				{
					if( *argv[ arg + 1 ] != '-' )
					{
						app.column_widths[ 1 ] = atoi( argv[ arg + 1 ] );
						arg += 1;
					}
				}
			}
			else if( strcmp( "-U", argv[arg] ) == 0 || strcmp( "--group-utc-offset", argv[arg] ) == 0 )
			{
				app.organize_by_time = false;
			}
			else if( strcmp( "-m", argv[arg] ) == 0 || strcmp( "--minimal", argv[arg] ) == 0 )
			{
				app.minimal = true;
			}
			else if( strcmp( "-f", argv[arg] ) == 0 || strcmp( "--file", argv[arg] ) == 0 )
			{
				if( (arg + 1) < argc )
				{
					configuration_name = argv[ arg + 1];
				}
				else
				{
					tz_print_error( &app, "Missing parameter for option '%s'\n", argv[arg] );
					return -2;
				}
				 arg += 1;
			}
			else if( strcmp( "-t", argv[arg] ) == 0 || strcmp( "--time", argv[arg] ) == 0 )
			{
				if( (arg + 1) < argc )
				{
					string_trim( argv[ arg + 1 ], " \t\n" );

					const char* formats[] = {
						"%I:%M:%S %p", /* 01:35:10 PM */
						"%I:%M %p",    /* 01:35 PM */
						"%H:%M:%S",    /* 13:35:10  */
						"%H:%M"        /* 13:35 */
					};
					size_t formats_len = sizeof(formats) / sizeof(formats[0]);
					struct tm tm;
					bool time_parsed = false;

					/* Initialize with today's date and clear hours, minutes, seconds. */
					localtime_r( &app.now, &tm );
					tm.tm_sec  = 0;
					tm.tm_min  = 0;
					tm.tm_hour = 0;

					for( int i = 0; !time_parsed && i < formats_len; i++ )
					{
						char* result = strptime( argv[ arg + 1], formats[ i ], &tm );
						if( result && *result == '\0' )
						{
							time_parsed = true;
						}
					}

					if( time_parsed )
					{
						app.now = mktime( &tm );
					}
					else
					{
						tz_print_error( &app, "Failed to match time for '%s'\n", argv[arg + 1] );
						return -2;
					}
				}
				else
				{
					tz_print_error( &app, "Missing parameter for option '%s'\n", argv[arg] );
					return -2;
				}
				 arg += 1;
			}
			else if( strcmp( "-h", argv[arg] ) == 0 || strcmp( "--help", argv[arg] ) == 0 )
			{
				tz_about( argc, argv );
				return 0;
			}
			else
			{
				tz_print_error( &app, "Unrecognized command line option '%s'\n", argv[arg] );
				return -2;
			}
		} // for
	} // if

	timezone_contact_t* contacts = NULL;
	lc_vector_create( contacts, 1 );

	if( !tz_check_alloc(&app, contacts) )
	{
		goto done;
	}

	lc_tree_map_t map;
	lc_tree_map_create( &map, timezone_map_element_destroy, timezone_map_compare, malloc, free );

	if( configuration_name )
	{
		if( !tz_configuration_read( &app, configuration_name, &contacts ) )
		{
			goto done;
		}
	}
	else if( !tz_read_configuration_from_home( &app, &contacts ) )
	{
		goto done;
	}

	tz_organize_data( contacts, &map, app.now, app.organize_by_time );

	if( app.organize_by_time )
	{
		if (app.minimal)
		{
			tz_display_time_grouping_minimal( &map, app.now, app.column_widths[0], app.column_widths[1] );
		}
		else
		{
			tz_display_time_grouping( &map, app.now, app.column_widths[0], app.column_widths[1] );
		}
	}
	else
	{
		if (app.minimal)
		{
			tz_display_utc_grouping_minimal( &map, app.now );
		}
		else
		{
			tz_display_utc_grouping( &map, app.now );
		}
	}

done:
	lc_tree_map_destroy( &map );

	// free memory allocated for every contact
	while( lc_vector_size(contacts) > 0 )
	{
		timezone_contact_t* contact = &lc_vector_last(contacts);
		free( (void*) contact->timezone );
		free( (void*) contact->email );
		free( (void*) contact->name );
		free( (void*) contact->office_phone );
		free( (void*) contact->mobile_phone );
		lc_vector_pop(contacts);
	}

	lc_vector_destroy( contacts );
	return 0;
}

void tz_about( int argc, char* argv[] )
{
	printf( "Timezoner v%s\n", VERSION );
	printf( "Copyright (c) 2019, Joe Marrero.\n");
	printf( "Copyright (c) 2017, End Point Corporation.\n");
	printf( "\n" );

#if defined(_WIN32) || defined(_WIN64)
	//const char* programData[256];
	//char configuration_filename[ PATH_MAX ];

	//HRESULT folderPathResult = SHGetKnownFolderPath(
		//FOLDERID_ProgramData,
		//KF_FLAG_DEFAULT,
		//NULL,
		//programData
	//);

#else
	struct passwd *pw = getpwuid(getuid());
	const char* homedir = pw->pw_dir;

	char configuration_filename[ PATH_MAX ];
	snprintf( configuration_filename, sizeof(configuration_filename), "%s/%s", homedir, CONFIGURATION_FILENAME );
	configuration_filename[ sizeof(configuration_filename) - 1 ] = '\0';

	printf( "This program expects a configuration file under \"%s\"\n", configuration_filename );
	printf( "If the file doesn't exist then an example configuration is created.\n");
	printf( "\n" );
#endif


	printf( "Command Line Options:\n" );
	printf( "    %-2s, %-20s  %-50s\n", "-f", "--file", "Use a specific configuration file." );
	printf( "    %-2s, %-20s  %-50s\n", "-t", "--time", "Use a specific time." );
	printf( "    %-2s, %-20s  %-50s\n", "-T", "--group-time", "Group contacts by time. Two optional arguments for the column widths is possible." );
	printf( "    %-2s, %-20s  %-50s\n", "-U", "--group-utc-offset", "Group contacts by UTC offset." );
	printf( "    %-2s, %-20s  %-50s\n", "-m", "--minimal", "Use minimal formatting." );
	printf( "\n" );
}

void tz_print_error(const tz_app_t* app,  const char* format, ... )
{
	va_list args;
	va_start(args, format);

	if (app->minimal)
	{
		fprintf( stderr, "ERROR: " );
	}
	else
	{
		console_fg_color_8( stderr, CONSOLE_COLOR8_RED );
		fprintf( stderr, "ERROR: " );
		console_reset( stderr );
	}

	vfprintf( stderr, format, args );
	va_end(args);
}

void tz_organize_data( const timezone_contact_t* contacts, lc_tree_map_t* map, time_t now, bool organize_by_time )
{
	// Group all contacts by timezone...
	for( int i = 0; i < lc_vector_size(contacts); i++ )
	{
		const timezone_contact_t* contact = &contacts[ i ];

		struct tm* tz_time = time_local( now, contact->timezone );
		char group_key[ 32 ];

		if( organize_by_time )
		{
			strftime(group_key, sizeof(group_key), "%T" /* %T for 24-hour time */, tz_time );
		}
		else
		{
			snprintf(group_key, sizeof(group_key), "%+05.1f", contact->utc_offset );
		}

		lc_tree_map_iterator_t itr = lc_tree_map_find( map, group_key );

		if( itr != lc_tree_map_end() )
		{
			timezone_contact_t const ** list = itr->value;
			lc_vector_push( list, contact );

			/* list is relocatable in memory as vector grows. */
			itr->value = list;
		}
		else
		{
			timezone_contact_t const ** list = NULL;
			lc_vector_create(list, 1);
			lc_vector_push(list, contact);
			lc_tree_map_insert( map, string_dup(group_key), list );
		}
	}

	// sort each list by contact's name.
	for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
	     itr != lc_tree_map_end( );
	     itr = lc_tree_map_next(itr) )
	{
		timezone_contact_t** list = itr->value;
		size_t count = lc_vector_size(list);
		qsort( list, count, sizeof(timezone_contact_t*), contact_name_compare );
	}
}

void tz_display_time_grouping ( lc_tree_map_t* map, time_t now, int name_width, int email_width )
{
	if( name_width < 10 )
	{
		name_width = 10;
	}

	if( email_width < 10 )
	{
		email_width = 10;
	}

	if( lc_tree_map_size( map ) == 0 )
	{
		return;
	}

	bool first = true;

	for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
	     itr != lc_tree_map_end( );
	     itr = lc_tree_map_next(itr) )
	{
		timezone_contact_t** list = itr->value;

		if( first )
		{
			wprintf( L"\u250c\u2500\u2500\u2524 " );
		}
		else
		{
			wprintf( L"\u251c\u2500\u2500\u2524 " );
		}
		wconsole_fg_color_8( stdout, CONSOLE_COLOR8_BRIGHT_YELLOW);


		struct tm* tz_time = time_local( now, list[0]->timezone );
		char time_str[ 32 ];
		strftime(time_str, sizeof(time_str), "%r" /* %T for 24-hour time */, tz_time );

		wprintf( L"%s", time_str );
		wconsole_reset( stdout );

		wprintf( L" \u251c" );
		int count = 35 + name_width + email_width;
		while( count-- > 0 )
		{
			wprintf( L"\u2500" );
		}
		if( first )
		{
			wprintf( L"\u2510\n" );
			first = false;
		}
		else
		{
			wprintf( L"\u2524\n" );
		}

		for( int i = 0; i < lc_vector_size(list); i++ )
		{
			timezone_contact_t* contact = list[ i ];

			wprintf( L"\u2502 " );

			wconsole_fg_color_8( stdout, CONSOLE_COLOR8_BRIGHT_CYAN);
			if( wcslen(contact->name) > name_width)
			{
				// truncated
				wprintf( L"%-.*ls...  ", name_width - 3, contact->name );
			}
			else
			{
				// fixed width
				wprintf( L"%-*ls  ", name_width, contact->name );
			}
			wconsole_reset( stdout );

			wconsole_fg_color_8( stdout, CONSOLE_COLOR8_GREY_15);
			if( wcslen(contact->email) > email_width)
			{
				// truncated
				wprintf( L"%lc %-.*ls...  ", (wchar_t) 0x2709, email_width - 3, contact->email );
			}
			else
			{
				// fixed width
				wprintf( L"%lc %-*ls  ", (wchar_t) 0x2709, email_width, contact->email );
			}
			wconsole_reset( stdout );

			wconsole_fg_color_8( stdout, CONSOLE_COLOR8_GREY_15);
			if( wcslen(contact->office_phone) > 19)
			{
				// truncated
				wprintf( L"%lc  %-.*ls... ", (wchar_t) 0x260e, 16, contact->office_phone );
			}
			else
			{
				// fixed width
				wprintf( L"%lc  %-*ls ", (wchar_t) 0x260e, 19, contact->office_phone );
			}
			wconsole_reset( stdout );

			wconsole_fg_color_8( stdout, CONSOLE_COLOR8_GREY_15);
			if( wcslen(contact->mobile_phone) > 19)
			{
				// truncated
				wprintf( L"%lc%-.*ls... ", (wchar_t) 0x1f4f1, 16, contact->mobile_phone );
			}
			else
			{
				// fixed width
				wprintf( L"%lc%-*ls ", (wchar_t) 0x1f4f1, 19, contact->mobile_phone );
			}
			wconsole_reset( stdout );

			wprintf( L"\u2502\n" );
		} // for
	} // for


	wprintf( L"\u2514" );
	int count = 52 + name_width + email_width;
	while( count-- > 0 )
	{
		wprintf( L"\u2500" );
	}
	wprintf( L"\u2518\n" );
}


void tz_display_time_grouping_minimal( lc_tree_map_t* map, time_t now, int name_width, int email_width )
{
	if( name_width < 10 )
	{
		name_width = 10;
	}

	if( email_width < 10 )
	{
		email_width = 10;
	}

	if( lc_tree_map_size( map ) == 0 )
	{
		return;
	}

	for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
	     itr != lc_tree_map_end( );
	     itr = lc_tree_map_next(itr) )
	{
		timezone_contact_t** list = itr->value;


		struct tm* tz_time = time_local( now, list[0]->timezone );
		char time_str[ 32 ];
		strftime(time_str, sizeof(time_str), "%r" /* %T for 24-hour time */, tz_time );

		wprintf( L"%s\n", time_str );

		for( int i = 0; i < lc_vector_size(list); i++ ) // for each contact...
		{
			timezone_contact_t* contact = list[ i ];

			if( wcslen(contact->name) > name_width)
			{
				// truncated
				wprintf( L"%-.*ls...  ", name_width - 3, contact->name );
			}
			else
			{
				// fixed width
				wprintf( L"%-*ls  ", name_width, contact->name );
			}

			if( wcslen(contact->email) > email_width)
			{
				// truncated
				wprintf( L" %-.*ls...  ", email_width - 3, contact->email );
			}
			else
			{
				// fixed width
				wprintf( L" %-*ls  ", email_width, contact->email );
			}

			if( wcslen(contact->office_phone) > 19)
			{
				// truncated
				wprintf( L" %-.*ls... ", 16, contact->office_phone );
			}
			else
			{
				// fixed width
				wprintf( L"  %-*ls ", 19, contact->office_phone );
			}

			if( wcslen(contact->mobile_phone) > 19)
			{
				// truncated
				wprintf( L"%-.*ls... ", 16, contact->mobile_phone );
			}
			else
			{
				// fixed width
				wprintf( L"%-*ls ", 19, contact->mobile_phone );
			}

			wprintf(L"\n");
		} // for

		wprintf(L"\n");
	} // for
}


void tz_display_utc_grouping( lc_tree_map_t* map, time_t now )
{
	if( lc_tree_map_size( map ) == 0 )
	{
		return;
	}

	lc_tree_map_iterator_t last_node = lc_tree_map_node_maximum( lc_tree_map_root(map) );

	// start of headers
	{
		wprintf( L"\u250c" );
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			int column_width = 25;
			while( column_width-- > 0 )
			{
				wprintf( L"\u2500" );
			}

			if( itr == last_node )
			{
				wprintf( L"\u2510" );
			}
			else
			{
				wprintf( L"\u252c" );
			}

		} // for
		wprintf( L"\n" );

		wprintf( L"\u2502" );
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			wconsole_fg_color_8( stdout, CONSOLE_COLOR8_BRIGHT_MAGENTA );
			wprintf( L"        UTC%s         ", (const char*) itr->key );
			wconsole_reset( stdout );
			wprintf( L"\u2502" );
		} // for
		wprintf( L"\n" );

		wprintf( L"\u251c" );
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			int column_width = 25;
			while( column_width-- > 0 )
			{
				wprintf( L"\u2500" );
			}

			if( itr == last_node )
			{
				wprintf( L"\u2524" );
			}
			else
			{
				wprintf( L"\u253c" );
			}

		} // for
		wprintf( L"\n" );


	} // end of headers


	int timezone_count = lc_tree_map_size( map );

	while( timezone_count > 0 )
	{
		wprintf( L"\u2502" );
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;
			if( list && lc_vector_size(list) > 0)
			{
				timezone_contact_t* contact = lc_vector_last(list);

				wconsole_fg_color_8( stdout, CONSOLE_COLOR8_BRIGHT_CYAN);
				if( wcslen(contact->name) > 23)
				{
					// truncated
					wprintf( L" %-.*ls... ", 20, contact->name );
				}
				else
				{
					// fixed width
					wprintf( L" %-*ls ", 23, contact->name );
				}
				wconsole_reset( stdout );
			}
			else
			{
				wprintf( L" %-23s ", "" );
			}
			wprintf( L"\u2502" );
		} // for
		wprintf( L"\n" );

		wprintf( L"\u2502" );
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;

			if( list && lc_vector_size(list) > 0)
			{
				timezone_contact_t* contact = lc_vector_last(list);

				struct tm* tz_time = time_local( now, contact->timezone );

				char time_str[12];
				strftime(time_str, sizeof(time_str), "%I:%M:%S %p", tz_time);
				time_str[ sizeof(time_str) - 1 ] = '\0';

				wconsole_fg_color_8( stdout, CONSOLE_COLOR8_BRIGHT_YELLOW);
				wprintf( L"  \u23f0 %-*s ", 19, time_str );
				wconsole_reset( stdout );
			}
			else
			{
				wprintf( L"%-24s ", "" );
			}
			wprintf( L"\u2502" );
		} // for
		wprintf( L"\n" );

		wprintf( L"\u2502" );
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;
			if( list && lc_vector_size(list) > 0)
			{
				timezone_contact_t* contact = lc_vector_last(list);

				wconsole_fg_color_8( stdout, CONSOLE_COLOR8_GREY_15);

				if( wcslen(contact->email) > 20)
				{
					// truncated
					wprintf( L"  %lc %-.*ls... ", (wchar_t) 0x2709, 17, contact->email );
				}
				else
				{
					// fixed width
					wprintf( L"  %lc %-*ls ", (wchar_t) 0x2709, 20, contact->email );
				}
				wconsole_reset( stdout );
			}
			else
			{
				wprintf( L"%-24s ", "" );
			}
			wprintf( L"\u2502" );
		} // for
		wprintf( L"\n" );

		wprintf( L"\u2502" );
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;
			if( list && lc_vector_size(list) > 0)
			{
				timezone_contact_t* contact = lc_vector_last(list);

				wconsole_fg_color_8( stdout, CONSOLE_COLOR8_GREY_15);
				if( wcslen(contact->office_phone) > 17)
				{
					// truncated
					wprintf( L"  \u260E  %-.*ls... ", 17, contact->office_phone );
				}
				else
				{
					// fixed width
					wprintf( L"  \u260E  %-*ls ", 19, contact->office_phone );
				}
				wconsole_reset( stdout );
			}
			else
			{
				wprintf( L"%-24s ", "" );
			}

			wprintf( L"\u2502" );
		} // for
		wprintf( L"\n" );

		wprintf( L"\u2502" );
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;
			if( list && lc_vector_size(list) > 0)
			{
				timezone_contact_t* contact = lc_vector_last(list);

				wconsole_fg_color_8( stdout, CONSOLE_COLOR8_GREY_15);
				if( wcslen(contact->mobile_phone) > 17)
				{
					// truncated
					wprintf( L"   %lc%-.*ls ", (wchar_t) 0x1f4f1, 17, contact->mobile_phone );
				}
				else
				{
					// fixed width
					wprintf( L"   %lc%-*ls ", (wchar_t) 0x1f4f1, 19, contact->mobile_phone );
				}
				wconsole_reset( stdout );
				wprintf( L"\u2502" );
			}
			else
			{
				wprintf( L"%-24s ", "" );
				wprintf( L"\u2502" );
			}
		} // for
		wprintf( L"\n" );



		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;

			if( list )
			{
				if(lc_vector_size(list) > 0)
				{
					lc_vector_pop(list);
				}

				if( lc_vector_size(list) == 0 )
				{
					lc_vector_destroy(list);
					itr->value = NULL;
					timezone_count--;
				}
			}
		} // for
	} // while


	// start of footer
	{
		wprintf( L"\u2514" );
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			int column_width = 25;
			while( column_width-- > 0 )
			{
				wprintf( L"\u2500" );
			}

			if( itr == last_node )
			{
				wprintf( L"\u2518" );
			}
			else
			{
				wprintf( L"\u2534" );
			}

		} // for
		wprintf( L"\n" );
	} // end of footer

	lc_tree_map_clear( map );
}


void tz_display_utc_grouping_minimal( lc_tree_map_t* map, time_t now )
{
	if( lc_tree_map_size( map ) == 0 )
	{
		return;
	}

	// start of headers
	{
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			wprintf( L"UTC%-5s                ", (const char*) itr->key );
		} // for
		wprintf( L"\n\n" );

	} // end of headers


	int timezone_count = lc_tree_map_size( map );

	while( timezone_count > 0 )
	{
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;
			if( list && lc_vector_size(list) > 0)
			{
				timezone_contact_t* contact = lc_vector_last(list);

				if( wcslen(contact->name) > 20)
				{
					// truncated
					wprintf( L"%-.*ls... ", 20, contact->name );
				}
				else
				{
					// fixed width
					wprintf( L"%-*ls", 24, contact->name );
				}
			}
			else
			{
				wprintf( L"%-24s", "" );
			}
		} // for
		wprintf( L"\n" );

		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;

			if( list && lc_vector_size(list) > 0)
			{
				timezone_contact_t* contact = lc_vector_last(list);

				struct tm* tz_time = time_local( now, contact->timezone );

				char time_str[12];
				strftime(time_str, sizeof(time_str), "%I:%M:%S %p", tz_time);
				time_str[ sizeof(time_str) - 1 ] = '\0';

				wprintf( L"  %-*s", 22, time_str );
			}
			else
			{
				wprintf( L"%-24s", "" );
			}
		} // for
		wprintf( L"\n" );

		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;
			if( list && lc_vector_size(list) > 0)
			{
				timezone_contact_t* contact = lc_vector_last(list);

				if( wcslen(contact->email) > 19)
				{
					// truncated
					wprintf( L"  %-.*ls...", 19, contact->email );
				}
				else
				{
					// fixed width
					wprintf( L"  %-*ls", 22, contact->email );
				}
			}
			else
			{
				wprintf( L"%-24s", "" );
			}
		} // for
		wprintf( L"\n" );

		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;
			if( list && lc_vector_size(list) > 0)
			{
				timezone_contact_t* contact = lc_vector_last(list);

				if( wcslen(contact->office_phone) > 19)
				{
					// truncated
					wprintf( L"  %-.*ls...", 19, contact->office_phone );
				}
				else
				{
					// fixed width
					wprintf( L"  %-*ls", 22, contact->office_phone );
				}
			}
			else
			{
				wprintf( L"%-24s", "" );
			}

		} // for
		wprintf( L"\n" );

		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;
			if( list && lc_vector_size(list) > 0)
			{
				timezone_contact_t* contact = lc_vector_last(list);

				if( wcslen(contact->mobile_phone) > 19)
				{
					// truncated
					wprintf( L"  %-.*ls", 19, contact->mobile_phone );
				}
				else
				{
					// fixed width
					wprintf( L"  %-*ls", 22, contact->mobile_phone );
				}
			}
			else
			{
				wprintf( L"%-24s", "" );
			}
		} // for
		wprintf( L"\n\n" );



		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;

			if( list )
			{
				if(lc_vector_size(list) > 0)
				{
					lc_vector_pop(list);
				}

				if( lc_vector_size(list) == 0 )
				{
					lc_vector_destroy(list);
					itr->value = NULL;
					timezone_count--;
				}
			}
		} // for
	} // while


	lc_tree_map_clear( map );
}




bool tz_read_configuration_from_home( const tz_app_t* app, timezone_contact_t** contacts )
{
	bool result = true;
	struct passwd *pw = getpwuid(getuid());
	const char *homedir = pw->pw_dir;

	char configuration_filename[ PATH_MAX ];
	snprintf( configuration_filename, sizeof(configuration_filename), "%s/%s", homedir, CONFIGURATION_FILENAME );
	configuration_filename[ sizeof(configuration_filename) - 1 ] = '\0';

	if( file_exists( configuration_filename ) )
	{
		if( !tz_configuration_read( app, configuration_filename, contacts ) )
		{
			tz_print_error( app, "Unable to read configuration at '%s'\n", configuration_filename );
			result = false;
			goto done;
		}
	}
	else
	{
		if( !tz_configuration_write_default( configuration_filename ) )
		{
			tz_print_error( app, "Failed to create '%s'.\n", configuration_filename );
			result = false;
			goto done;
		}

		result = tz_read_configuration_from_home( app, contacts );
	}

done:
	return result;
}

bool tz_configuration_read( const tz_app_t* app, const char* configuration_name, timezone_contact_t** contacts )
{
	bool result = false;
	FILE* config = fopen( configuration_name, "r" );

	if( config )
	{
#ifdef __APPLE__
		const char* CONFIG_LINE_REGEX = "([[:alpha:]]+/?[[:alnum:]_]+)" /* group 1: timezone code */
		                                "[[:space:]]+"
                                        "\"(.*)\"" /* group 2: email */
		                                "[[:space:]]+"
		                                "\"(.*)\"" /* group 3: name */
		                                "[[:space:]]+"
		                                "\"(.*)\"" /* group 4: office number */
		                                "[[:space:]]+"
		                                "\"(.*)\""; /* group 5: mobile number */
#else // Linux and MinGW
		const char* CONFIG_LINE_REGEX = "([[:alpha:]]+?/?[[:alnum:]_]+)" /* group 1: timezone code */
		                                "[[:space:]]+"
		                                "\"(.*)\"" /* group 2: email */
		                                "[[:space:]]+"
		                                "\"(.*)\"" /* group 3: name */
		                                "[[:space:]]+"
		                                "\"(.*)\"" /* group 4: office number */
		                                "[[:space:]]+"
		                                "\"(.*)\""; /* group 5: mobile number */
#endif
		regex_t regex;
		int regex_comp_result = regcomp( &regex, CONFIG_LINE_REGEX, REG_EXTENDED | REG_ICASE);

		if( regex_comp_result )
		{
			char error[256];
			regerror(regex_comp_result, &regex, error, sizeof(error));
			tz_print_error( app, "Unable to compile regular expression.\nProblem: %s\n", error );
			result = false;
		}
		else
		{
			char line[ 256 ];
			int line_number = 1;

			while( !feof(config) )
			{
				if( fgets( line, sizeof(line), config ) )
				{
					if( strchr(line, '\n') == NULL )
					{
						// Check for lines longer than we can support.
						tz_print_error( app, "Line exceeds maximum possible length of %zu.\n", sizeof(line) );

						result = false;
						goto cleanup;
					}

					string_trim( line, " \t\r\n" );

					if( !tz_configuration_read_line(app, line, line_number, &regex, contacts ) )
					{
						result = false;
						goto cleanup;
					}

					line_number += 1;
				} // if line
			} // while !eof

			result = true;
		} // if regex_comp_result

		cleanup: {
			regfree( &regex );
			fclose( config );
		} // cleanup
	}// if config

	return result;
}

bool tz_configuration_read_line( const tz_app_t* app, char* line, int line_number, regex_t* regex, timezone_contact_t** contacts )
{
	char* tz_string = NULL;

	wchar_t* email = NULL;
	wchar_t* name = NULL;
	wchar_t* office_phone = NULL;
	wchar_t* mobile_phone = NULL;

	if( *line == '#' )
	{
		// skipping comments
		goto line_read_success;
	}
	else if( *line == '\0' )
	{
		// skipping empty lines
		goto line_read_success;
	}
	else
	{
		const int max_groups = 7;
		regmatch_t matches[ max_groups ];
		int regex_result = regexec( regex, line, max_groups, matches, 0 );

		if( !regex_result )
		{
			size_t tz_string_len = matches[ 1 ].rm_eo - matches[ 1 ].rm_so;
			tz_string = malloc( tz_string_len + 1 );
			if( !tz_check_alloc(app, tz_string) ) goto line_read_failed;
			memcpy( tz_string, line + matches[ 1 ].rm_so, tz_string_len );
			tz_string[ tz_string_len ] = '\0';

			line[ matches[ 2 ].rm_eo ] = '\0';
			size_t email_len = mb_strlen( line + matches[ 2 ].rm_so );
			email = malloc( sizeof(wchar_t) * (email_len + 1) );
			if( !tz_check_alloc(app, email) ) goto line_read_failed;
			mbstowcs( email, line + matches[ 2 ].rm_so, email_len );
			email[ email_len ] = '\0';

			line[ matches[ 3 ].rm_eo ] = '\0';
			size_t name_len = mb_strlen( line + matches[ 3 ].rm_so );
			name = malloc( sizeof(wchar_t) * (name_len + 1) );
			if( !tz_check_alloc(app, name) ) goto line_read_failed;
			mbstowcs( name, line + matches[ 3 ].rm_so, name_len );
			name[ name_len ] = '\0';

			line[ matches[ 4 ].rm_eo ] = '\0';
			size_t office_phone_len = mb_strlen( line + matches[ 4 ].rm_so );
			office_phone = malloc( sizeof(wchar_t) * (office_phone_len + 1) );
			if( !tz_check_alloc(app, office_phone) ) goto line_read_failed;
			mbstowcs( office_phone, line + matches[ 4 ].rm_so, office_phone_len );
			office_phone[ office_phone_len ] = '\0';


			line[ matches[ 5 ].rm_eo ] = '\0';
			size_t mobile_phone_len = mb_strlen( line + matches[ 5 ].rm_so );
			mobile_phone = malloc( sizeof(wchar_t) * (mobile_phone_len + 1) );
			if( !tz_check_alloc(app, mobile_phone) ) goto line_read_failed;
			mbstowcs( mobile_phone, line + matches[ 5 ].rm_so, mobile_phone_len );
			mobile_phone[ mobile_phone_len ] = '\0';

			double utc_offset = time_utc_offset( tz_string );

			timezone_contact_t contact = (timezone_contact_t) {
				.utc_offset   = utc_offset,
				.timezone     = tz_string,
				.email        = email,
				.name         = name,
				.office_phone = office_phone,
				.mobile_phone = mobile_phone
			};
			lc_vector_push( *contacts, contact );
		}
		else if (regex_result == REG_NOMATCH)
		{
			// line did not match.
			tz_print_error( app, "Unable to match line with regular expression (see line %d).\n", line_number );
			goto line_read_failed;
		}
		else
		{
			tz_print_error( app, "Unable to execute regular expression.\n" );
			goto line_read_failed;
		}
	}

line_read_success:
	return true;

line_read_failed:
	free( tz_string );
	free( email );
	free( name );
	free( office_phone );
	free( mobile_phone );
	return false;
}

bool tz_configuration_write_default( const char* configuration_filename )
{
	bool result = false;
	FILE* config = fopen( configuration_filename, "w" );

	if( config )
	{
		fprintf( config, "# -------------------------------\n" );
		fprintf( config, "# This is the configuration for the timezoner program. You can\n" );
		fprintf( config, "# add new contacts here. A list of IANA timezones is available\n" );
		fprintf( config, "# from here: https://en.wikipedia.org/wiki/List_of_tz_database_time_zones\n" );
		fprintf( config, "#\n" );
		fprintf( config, "# The format is:\n" );
		fprintf( config, "#\n" );
		fprintf( config, "# Timezone \t\tEmail \tName \tOfficePhone \tMobilePhone\n" );
		fprintf( config, "America/New_York \t\"john.doe@example.com\" \"John Doe\" \"+1 305 555 1234\" \"+1 954 555 5678\"\n" );

		fclose( config );
		result = true;
	}

	return result;
}

bool timezone_map_element_destroy( void *p_key, void *p_value )
{
	timezone_contact_t** list = p_value;

	free( p_key );

	if( list )
	{
		while(lc_vector_size(list) > 0)
		{
			lc_vector_pop(list);
		}

		lc_vector_destroy(list);
	}

	return true;
}

int timezone_map_compare( const void *p_key_left, const void *p_key_right )
{
	const char* l = p_key_left;
	const char* r = p_key_right;
	return strcmp( l, r );
}

int contact_name_compare( const void *l, const void *r )
{
	const timezone_contact_t** left = (const timezone_contact_t**) l;
	const timezone_contact_t** right = (const timezone_contact_t**) r;
	return wcscmp((*left)->name, (*right)->name );
}

bool tz_check_alloc( const tz_app_t* app, void* mem )
{
	bool result = true;
	if( !mem )
	{
		tz_print_error( app, "Out of memory.\n" );
		result = false;
	}

	return result;
}
