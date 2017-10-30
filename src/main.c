/*
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
#include <string.h>
#include <wchar.h>
#include <locale.h>
#define __USE_XOPEN
#include <time.h>
#include <limits.h>
#include <pwd.h>
#include <unistd.h>
#include <regex.h>
#include <libutility/utility.h>
#include <libutility/console.h>
#define VECTOR_GROW_AMOUNT(array)      (1)
#include <libcollections/vector.h>
#include <libcollections/tree-map.h>

#define VERSION "1.0"

typedef struct timezone_contact {
	double utc_offset;
	const char* timezone; /* IANA Timezone Code; https://en.wikipedia.org/wiki/List_of_tz_database_time_zones */
	const char* email;
	const char* name;
	const char* office_phone;
	const char* mobile_phone;
} timezone_contact_t;


static void about ( int argc, char* argv[] );
static void organize_data ( const timezone_contact_t* contacts, lc_tree_map_t* map, time_t now, bool organize_by_time );
static bool read_configuration_from_home ( timezone_contact_t** contacts );
static bool configuration_read ( const char* configuration_filename, timezone_contact_t** contacts );
static bool configuration_read_line ( const char* line, int line_number, regex_t* regex, timezone_contact_t** contacts );
static bool configuration_write_default ( const char* configuration_filename );
static bool timezone_map_element_destroy ( void *p_key, void *p_value );
static int  timezone_map_compare ( const void *p_key_left, const void *p_key_right );
static int  contact_name_compare ( const void *l, const void *r );
static bool check_alloc( void* mem );
static void display_time_grouping ( lc_tree_map_t* map, time_t now );
static void display_utc_grouping ( lc_tree_map_t* map, time_t now );


int main( int argc, char* argv[] )
{
	bool organize_by_time = 1; // this is the default
	const char* config_filename = NULL;
	time_t now = time(NULL);

	setlocale( LC_ALL, "" );

	if( argc >= 2 )
	{
		// Since we have at least two command line arguments
		// let's parse them.
		for( int arg = 1; arg < argc; arg++ )
		{
			if( strcmp( "-T", argv[arg] ) == 0 || strcmp( "--group-time", argv[arg] ) == 0 )
			{
				organize_by_time = true;
			}
			else if( strcmp( "-U", argv[arg] ) == 0 || strcmp( "--group-utc-offset", argv[arg] ) == 0 )
			{
				organize_by_time = false;
			}
			else if( strcmp( "-f", argv[arg] ) == 0 || strcmp( "--file", argv[arg] ) == 0 )
			{
				if( (arg + 1) < argc )
				{
					config_filename = argv[ arg + 1];
				}
				else
				{
					console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
					fprintf( stderr, "ERROR: " );
					console_reset( stderr );
					fprintf( stderr, "Missing parameter for option '%s'\n", argv[arg] );
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
					localtime_r( &now, &tm );
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
						now = mktime( &tm );
					}
					else
					{
						console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
						fprintf( stderr, "ERROR: " );
						console_reset( stderr );
						fprintf( stderr, "Failed to match time for '%s'\n", argv[arg + 1] );
						return -2;
					}
				}
				else
				{
					console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
					fprintf( stderr, "ERROR: " );
					console_reset( stderr );
					fprintf( stderr, "Missing parameter for option '%s'\n", argv[arg] );
					return -2;
				}
				 arg += 1;
			}
			else if( strcmp( "-h", argv[arg] ) == 0 || strcmp( "--help", argv[arg] ) == 0 )
			{
				about( argc, argv );
				return 0;
			}
			else
			{
				console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
				fprintf( stderr, "ERROR: " );
				console_reset( stderr );
				fprintf( stderr, "Unrecognized command line option '%s'\n", argv[arg] );
				return -2;
			}
		} // for
	} // if

	timezone_contact_t* contacts = NULL;
	lc_vector_create( contacts, 1 );

	if( !check_alloc(contacts) )
	{
		goto done;
	}

	lc_tree_map_t map;
	lc_tree_map_create( &map, timezone_map_element_destroy, timezone_map_compare, malloc, free );

	if( config_filename )
	{
		if( !configuration_read( config_filename, &contacts ) )
		{
			goto done;
		}
	}
	else if( !read_configuration_from_home( &contacts ) )
	{
		goto done;
	}

	organize_data( contacts, &map, now, organize_by_time );

	if( organize_by_time )
	{
		display_time_grouping( &map, now );
	}
	else
	{
		display_utc_grouping( &map, now );
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

void about( int argc, char* argv[] )
{
	printf( "Timezoner v%s\n", VERSION );
	printf( "Copyright (c) 2017, End Point Corporation.\n");
	printf( "\n" );

	struct passwd *pw = getpwuid(getuid());
	const char *homedir = pw->pw_dir;

	char configuration_filename[ PATH_MAX ];
	snprintf( configuration_filename, sizeof(configuration_filename), "%s/.timezoner", homedir );
	configuration_filename[ sizeof(configuration_filename) - 1 ] = '\0';

	printf( "This program expects a configuration file under \"%s\"\n", configuration_filename );
	printf( "If the file doesn't exist then an example configuration is created.\n");
	printf( "\n" );

	printf( "Command Line Options:\n" );
	printf( "    %-2s, %-20s  %-50s\n", "-f", "--file", "Use a specific configuration file." );
	printf( "    %-2s, %-20s  %-50s\n", "-t", "--time", "Use a specific time." );
	printf( "    %-2s, %-20s  %-50s\n", "-T", "--group-time", "Group contacts by time." );
	printf( "    %-2s, %-20s  %-50s\n", "-U", "--group-utc-offset", "Group contacts by UTC offset." );
	printf( "\n" );
}

void organize_data( const timezone_contact_t* contacts, lc_tree_map_t* map, time_t now, bool organize_by_time )
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
			//strftime(group_key, sizeof(group_key), "%z" /* utc offset */, tz_time );
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

void display_time_grouping( lc_tree_map_t* map, time_t now )
{
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
			printf( "\u250c\u2500\u2500\u2524 " );
		}
		else
		{
			printf( "\u251c\u2500\u2500\u2524 " );
		}
		console_fg_color_256( stdout, CONSOLE_COLOR256_BRIGHT_YELLOW);


		struct tm* tz_time = time_local( now, list[0]->timezone );
		char time_str[ 32 ];
		strftime(time_str, sizeof(time_str), "%r" /* %T for 24-hour time */, tz_time );

		printf( "%s", time_str );
		console_reset( stdout );

		printf( " \u251c" );
		int count = 90;
		while( count-- > 0 )
		{
			printf( "\u2500" );
		}
		if( first )
		{
			printf( "\u2510\n" );
			first = false;
		}
		else
		{
			printf( "\u2524\n" );
		}

		for( int i = 0; i < lc_vector_size(list); i++ )
		{
			timezone_contact_t* contact = list[ i ];

			printf( "\u2502 " );

			console_fg_color_256( stdout, CONSOLE_COLOR256_BRIGHT_CYAN);
			if( strlen(contact->name) > 30)
			{
				// truncated
				printf( "%-.*s...  ", 27, contact->name );
			}
			else
			{
				// fixed width
				printf( "%-*s  ", 30, contact->name );
			}
			console_reset( stdout );

			console_fg_color_256( stdout, CONSOLE_COLOR256_GREY_15);
			if( strlen(contact->email) > 23)
			{
				// truncated
				printf( "%lc %-.*s...  ", (wchar_t) 0x2709, 23, contact->email );
			}
			else
			{
				// fixed width
				printf( "%lc %-*s  ", (wchar_t) 0x2709, 25, contact->email );
			}
			console_reset( stdout );

			console_fg_color_256( stdout, CONSOLE_COLOR256_GREY_15);
			if( strlen(contact->office_phone) > 17)
			{
				// truncated
				printf( "%lc  %-.*s... ", (wchar_t) 0x260e, 17, contact->office_phone );
			}
			else
			{
				// fixed width
				printf( "%lc  %-*s ", (wchar_t) 0x260e, 19, contact->office_phone );
			}
			console_reset( stdout );

			console_fg_color_256( stdout, CONSOLE_COLOR256_GREY_15);
			if( strlen(contact->mobile_phone) > 17)
			{
				// truncated
				printf( "%lc%-.*s... ", (wchar_t) 0x1f4f1, 17, contact->mobile_phone );
			}
			else
			{
				// fixed width
				printf( "%lc%-*s ", (wchar_t) 0x1f4f1, 19, contact->mobile_phone );
			}
			console_reset( stdout );

			printf( "\u2502\n" );
		} // for
	} // for



	printf( "\u2514" );
	int count = 107;
	while( count-- > 0 )
	{
		printf( "\u2500" );
	}
	printf( "\u2518\n" );

	//printf( "+-----------------------------------------------------------------------------------------------------------+\n" );
}

void display_utc_grouping( lc_tree_map_t* map, time_t now )
{
	if( lc_tree_map_size( map ) == 0 )
	{
		return;
	}

	lc_tree_map_iterator_t last_node = lc_tree_map_node_maximum( lc_tree_map_root(map) );

	// start of headers
	{
		printf("\u250c");
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			int column_width = 25;
			while( column_width-- > 0 )
			{
				printf( "\u2500" );
			}

			if( itr == last_node )
			{
				printf( "\u2510" );
			}
			else
			{
				printf( "\u252c" );
			}

		} // for
		printf("\n");

		printf("\u2502");
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;
			timezone_contact_t* contact = lc_vector_last(list);

			console_fg_color_256( stdout, CONSOLE_COLOR256_BRIGHT_MAGENTA );
			printf( "        UTC%s         ", (const char*) itr->key );
			console_reset( stdout );
			printf("\u2502");
		} // for
		printf("\n");

		printf("\u251c");
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			int column_width = 25;
			while( column_width-- > 0 )
			{
				printf( "\u2500" );
			}

			if( itr == last_node )
			{
				printf( "\u2524" );
			}
			else
			{
				printf( "\u253c" );
			}

		} // for
		printf("\n");


	} // end of headers


	int timezone_count = lc_tree_map_size( map );

	while( timezone_count > 0 )
	{
		printf("\u2502");
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;
			if( list && lc_vector_size(list) > 0)
			{
				timezone_contact_t* contact = lc_vector_last(list);

				console_fg_color_256( stdout, CONSOLE_COLOR256_BRIGHT_CYAN);
				if( strlen(contact->name) > 23)
				{
					// truncated
					printf( " %-.*s... ", 20, contact->name );
				}
				else
				{
					// fixed width
					printf( " %-*s ", 23, contact->name );
				}
				console_reset( stdout );
			}
			else
			{
				printf( " %-23s ", "" );
			}
			printf("\u2502");
		} // for
		printf("\n");

		printf("\u2502");
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

				console_fg_color_256( stdout, CONSOLE_COLOR256_BRIGHT_YELLOW);
				printf( "  \u23f0 %-*s ", 19, time_str );
				console_reset( stdout );
			}
			else
			{
				printf( "%-24s ", "" );
			}
			printf("\u2502");
		} // for
		printf("\n");

		printf("\u2502");
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;
			if( list && lc_vector_size(list) > 0)
			{
				timezone_contact_t* contact = lc_vector_last(list);

				console_fg_color_256( stdout, CONSOLE_COLOR256_GREY_15);

				if( strlen(contact->email) > 17)
				{
					// truncated
					printf( "  \u2709 %-.*s... ", 17, contact->email );
				}
				else
				{
					// fixed width
					printf( "  \u2709 %-*s ", 20, contact->email );
				}
				console_reset( stdout );
			}
			else
			{
				printf( "%-24s ", "" );
			}
			printf("\u2502");
		} // for
		printf("\n");

		printf("\u2502");
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;
			if( list && lc_vector_size(list) > 0)
			{
				timezone_contact_t* contact = lc_vector_last(list);

				console_fg_color_256( stdout, CONSOLE_COLOR256_GREY_15);
				if( strlen(contact->office_phone) > 17)
				{
					// truncated
					printf( "  \u260E  %-.*s... ", 17, contact->office_phone );
				}
				else
				{
					// fixed width
					printf( "  \u260E  %-*s ", 19, contact->office_phone );
				}
				console_reset( stdout );
			}
			else
			{
				printf( "%-24s ", "" );
			}

			printf("\u2502");
		} // for
		printf("\n");

		printf("\u2502");
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;
			if( list && lc_vector_size(list) > 0)
			{
				timezone_contact_t* contact = lc_vector_last(list);

				console_fg_color_256( stdout, CONSOLE_COLOR256_GREY_15);
				if( strlen(contact->mobile_phone) > 17)
				{
					// truncated
					printf( "   %lc%-.*s ", (wchar_t) 0x1f4f1, 17, contact->mobile_phone );
				}
				else
				{
					// fixed width
					printf( "   %lc%-*s ", (wchar_t) 0x1f4f1, 19, contact->mobile_phone );
				}
				console_reset( stdout );
				printf("\u2502");
			}
			else
			{
				printf( "%-24s ", "" );
				printf("\u2502");
			}
		} // for
		printf("\n");



		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;

			if( list )
			{
				if(lc_vector_size(list) > 0)
				{
					timezone_contact_t* contact = lc_vector_last(list);
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
		printf("\u2514");
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			int column_width = 25;
			while( column_width-- > 0 )
			{
				printf( "\u2500" );
			}

			if( itr == last_node )
			{
				printf( "\u2518" );
			}
			else
			{
				printf( "\u2534" );
			}

		} // for
		printf("\n");

		/*
		printf("+");
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		     itr != lc_tree_map_end( );
		     itr = lc_tree_map_next(itr) )
		{
			printf( "-------------------------+" );
		} // for
		printf("\n");
		*/
	} // end of footer

	lc_tree_map_clear( map );

}

bool read_configuration_from_home( timezone_contact_t** contacts )
{
	bool result = true;
	struct passwd *pw = getpwuid(getuid());
	const char *homedir = pw->pw_dir;

	char configuration_filename[ PATH_MAX ];
	snprintf( configuration_filename, sizeof(configuration_filename), "%s/.timezoner", homedir );
	configuration_filename[ sizeof(configuration_filename) - 1 ] = '\0';

	if( file_exists( configuration_filename ) )
	{
		if( !configuration_read( configuration_filename, contacts ) )
		{
			console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
			fprintf( stderr, "ERROR: " );
			console_reset( stderr );
			fprintf( stderr, "Unable to read configuration at '%s'\n", configuration_filename );
			result = false;
			goto done;
		}
	}
	else
	{
		if( !configuration_write_default( configuration_filename ) )
		{
			console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
			fprintf( stderr, "ERROR: " );
			console_reset( stderr );
			fprintf( stderr, "Failed to create '%s'.\n", configuration_filename );
			result = false;
			goto done;
		}

		result = read_configuration_from_home( contacts );
	}

done:
	return result;
}

bool configuration_read( const char* configuration_filename, timezone_contact_t** contacts )
{
	bool result = false;
	FILE* config = fopen( configuration_filename, "r" );

	if( config )
	{
		const char* CONFIG_LINE_REGEX = "([[:alpha:]]+?/?[[:alnum:]_]+)" /* group 1: timezone code */
		                                "[[:space:]]+"
		                                "\"(.*)\"" /* group 2: email */
		                                "[[:space:]]+"
		                                "\"(.*)\"" /* group 3: name */
		                                "[[:space:]]+"
		                                "\"(.*)\"" /* group 4: office number */
		                                "[[:space:]]+"
		                                "\"(.*)\""; /* group 5: mobile number */
		regex_t regex;
		int regex_comp_result = regcomp( &regex, CONFIG_LINE_REGEX, REG_EXTENDED | REG_ICASE);

		if( regex_comp_result )
		{
			console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
			fprintf( stderr, "ERROR: " );
			console_reset( stderr );
			fprintf( stderr, "Unable to compile regular expression.\n" );
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
						console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
						fprintf( stderr, "ERROR: " );
						console_reset( stderr );
						fprintf( stderr, "Line exceeds maximum possible length of %zu.\n", sizeof(line) );

						result = false;
						goto cleanup;
					}

					string_trim( line, " \t\r\n" );

					if( !configuration_read_line(line, line_number, &regex, contacts ) )
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

bool configuration_read_line( const char* line, int line_number, regex_t* regex, timezone_contact_t** contacts )
{
	char* tz_string = NULL;

	char* email = NULL;
	char* name = NULL;
	char* office_phone = NULL;
	char* mobile_phone = NULL;

	if( line[0] == '#' )
	{
		// skipping comments
		goto line_read_success;
	}
	else if( strcmp( line, "" ) == 0 )
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
			if( !check_alloc(tz_string) ) goto line_read_failed;
			memcpy( tz_string, line + matches[ 1 ].rm_so, tz_string_len );
			tz_string[ tz_string_len ] = '\0';

			size_t email_len = matches[ 2 ].rm_eo - matches[ 2 ].rm_so;
			email = malloc( email_len + 1 );
			if( !check_alloc(email) ) goto line_read_failed;
			memcpy( email, line + matches[ 2 ].rm_so, email_len );
			email[ email_len ] = '\0';

			size_t name_len = matches[ 3 ].rm_eo - matches[ 3 ].rm_so;
			name = malloc( name_len + 1 );
			if( !check_alloc(name) ) goto line_read_failed;
			memcpy( name, line + matches[ 3 ].rm_so, name_len );
			name[ name_len ] = '\0';

			size_t office_phone_len = matches[ 4 ].rm_eo - matches[ 4 ].rm_so;
			office_phone = malloc( office_phone_len + 1 );
			if( !check_alloc(office_phone) ) goto line_read_failed;
			memcpy( office_phone, line + matches[ 4 ].rm_so, office_phone_len );
			office_phone[ office_phone_len ] = '\0';

			size_t mobile_phone_len = matches[ 5 ].rm_eo - matches[ 5 ].rm_so;
			mobile_phone = malloc( mobile_phone_len + 1 );
			if( !check_alloc(mobile_phone) ) goto line_read_failed;
			memcpy( mobile_phone, line + matches[ 5 ].rm_so, mobile_phone_len );
			mobile_phone[ mobile_phone_len ] = '\0';

			double utc_offset = time_utc_offset( tz_string );

			timezone_contact_t contact = (timezone_contact_t) {
				.utc_offset   = utc_offset,
				.timezone     = tz_string,
				.email        = email,
				.name         = name,
				.office_phone = office_phone,
				.mobile_phone = mobile_phone,
			};
			lc_vector_push( *contacts, contact );
		}
		else if (regex_result == REG_NOMATCH)
		{
			// line did not match.
			console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
			fprintf( stderr, "ERROR: " );
			console_reset( stderr );
			fprintf( stderr, "Unable to match line with regular expression (see line %d).\n", line_number );
			goto line_read_failed;
		}
		else
		{
			console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
			fprintf( stderr, "ERROR: " );
			console_reset( stderr );
			fprintf( stderr, "Unable to execute regular expression.\n" );
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


bool configuration_write_default( const char* configuration_filename )
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
			timezone_contact_t* contact = lc_vector_last(list);
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
	return strcmp((*left)->name, (*right)->name );
}

bool check_alloc( void* mem )
{
	bool result = true;
	if( !mem )
	{
		console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
		fprintf( stderr, "ERROR: " );
		console_reset( stderr );
		fprintf( stderr, "Out of memory.\n" );
		result = false;
	}

	return result;
}
