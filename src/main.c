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
	int dst; /* 1 if DST is observed */
	const char* email;
	const char* name;
	const char* office_phone;
	const char* mobile_phone;
} timezone_contact_t;


static void about ( int argc, char* argv[] );
static void organize_data ( const timezone_contact_t* contacts, lc_tree_map_t* map );
static bool read_configuration ( timezone_contact_t** contacts );
static bool configuration_read ( const char* configuration_filename, timezone_contact_t** contacts );
static bool configuration_write_default ( const char* configuration_filename );
static bool timezone_map_element_destroy ( void *p_key, void *p_value );
static int  timezone_map_compare ( const void *p_key_left, const void *p_key_right );
static int  contact_name_compare ( const void *l, const void *r );
static void render_row_chart ( lc_tree_map_t* map, time_t now );
static void render_column_chart ( lc_tree_map_t* map, time_t now );


int main( int argc, char* argv[] )
{
	bool render_as_rows = 1; // this is the default

    if( argc >= 2 )
    {
		// Since we have at least two command line arguments
		// let's parse them.
        for( int arg = 1; arg < argc; arg++ )
        {
            if( strcmp( "-r", argv[arg] ) == 0 || strcmp( "--rows", argv[arg] ) == 0 )
            {
				render_as_rows = true;
            }
            else if( strcmp( "-c", argv[arg] ) == 0 || strcmp( "--columns", argv[arg] ) == 0 )
            {
				render_as_rows = false;
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

	lc_tree_map_t map;
	lc_tree_map_create( &map, timezone_map_element_destroy, timezone_map_compare, malloc, free );

	if( !contacts )
	{
		console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
		fprintf( stderr, "ERROR: " );
		console_reset( stderr );
		fprintf( stderr, "Out of memory.\n" );
		goto done;
	}

	if( !read_configuration( &contacts ) )
	{
		goto done;
	}

	organize_data( contacts, &map );

	time_t now = time(NULL);

	if( render_as_rows )
	{
		render_row_chart( &map, now );
	}
	else
	{
		render_column_chart( &map, now );
	}

done:
	lc_tree_map_destroy( &map );

	// free memory allocated for every contact
	while( lc_vector_size(contacts) > 0 )
	{
		timezone_contact_t* contact = &lc_vector_last(contacts);
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
    printf( "    %-2s, %-12s   %-50s\n", "-r", "--rows", "Display using rows." );
    printf( "    %-2s, %-12s   %-50s\n", "-c", "--columns", "Display using columns" );
    printf( "\n" );
}

void organize_data( const timezone_contact_t* contacts, lc_tree_map_t* map )
{
	// Group all contacts by timezone...
	for( int i = 0; i < lc_vector_size(contacts); i++ )
	{
		const timezone_contact_t* contact = &contacts[ i ];
		lc_tree_map_iterator_t itr = lc_tree_map_find( map, &contact->utc_offset );

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
			lc_tree_map_insert( map, &contact->utc_offset, list );
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

void render_row_chart( lc_tree_map_t* map, time_t now )
{
	if( lc_tree_map_size( map ) == 0 )
	{
		return;
	}

	for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
		 itr != lc_tree_map_end( );
		 itr = lc_tree_map_next(itr) )
	{
		timezone_contact_t** list = itr->value;

		printf( "+----------------------------------------------------[ " );
		console_fg_color_256( stdout, CONSOLE_COLOR256_BRIGHT_MAGENTA);
		printf( "UTC%+05.1f", *((double*)itr->key) );
		console_reset( stdout );
		printf( " ]---------------------------------------------------+\n" );

		for( int i = 0; i < lc_vector_size(list); i++ )
		{
			timezone_contact_t* contact = list[ i ];

			printf( "| " );

			console_fg_color_256( stdout, CONSOLE_COLOR256_BRIGHT_CYAN);
			if( strlen(contact->name) > 24)
			{
				// truncated
				printf( "%-.*s...  ", 21, contact->name );
			}
			else
			{
				// fixed width
				printf( "%-*s  ", 24, contact->name );
			}
			console_reset( stdout );


			struct tm* utc_time = gmtime(&now);
			//time_t offset_time = mktime(utc_time) + ((contact->utc_offset) * 3600.0);
			time_t offset_time = now + ((contact->utc_offset + contact->dst) * 3600.0);

			struct tm* other_time = gmtime(&offset_time);

			char time_str[12];
			strftime(time_str, sizeof(time_str), "%I:%M:%S %p", other_time);
			time_str[ sizeof(time_str) - 1 ] = '\0';

			console_fg_color_256( stdout, CONSOLE_COLOR256_BRIGHT_YELLOW);
			printf( "%-12s  ", time_str );
			console_reset( stdout );

			console_fg_color_256( stdout, CONSOLE_COLOR256_GREY_15);
			if( strlen(contact->email) > 21)
			{
				// truncated
				printf( "\u2709 %-.*s...  ", 21, contact->email );
			}
			else
			{
				// fixed width
				printf( "\u2709 %-*s  ", 24, contact->email );
			}
			console_reset( stdout );

			console_fg_color_256( stdout, CONSOLE_COLOR256_GREY_15);
			if( strlen(contact->office_phone) > 17)
			{
				// truncated
				printf( "\u260E  %-.*s... ", 17, contact->office_phone );
			}
			else
			{
				// fixed width
				printf( "\u260E  %-*s ", 19, contact->office_phone );
			}
			console_reset( stdout );

			console_fg_color_256( stdout, CONSOLE_COLOR256_GREY_15);
			if( strlen(contact->mobile_phone) > 17)
			{
				// truncated
				printf( "\u260F  %-.*s... ", 17, contact->mobile_phone );
			}
			else
			{
				// fixed width
				printf( "\u260F  %-*s ", 19, contact->mobile_phone );
			}
			console_reset( stdout );

			printf( "|\n" );
		} // for
	} // for

	printf( "+-------------------------------------------------------------------------------------------------------------------+\n" );
}

void render_column_chart( lc_tree_map_t* map, time_t now )
{
	if( lc_tree_map_size( map ) == 0 )
	{
		return;
	}

	// start of headers
	{
		printf("+");
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
			 itr != lc_tree_map_end( );
			 itr = lc_tree_map_next(itr) )
		{
			printf( "-------------------------+" );
		} // for
		printf("\n");

		printf("|");
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
			 itr != lc_tree_map_end( );
			 itr = lc_tree_map_next(itr) )
		{
			timezone_contact_t** list = itr->value;
			timezone_contact_t* contact = lc_vector_last(list);

			console_fg_color_256( stdout, CONSOLE_COLOR256_BRIGHT_MAGENTA);
			printf( "        UTC%+05.1f         ", *((double*)itr->key) );
			console_reset( stdout );
			printf("|");
		} // for
		printf("\n");

		printf("+");
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
			 itr != lc_tree_map_end( );
			 itr = lc_tree_map_next(itr) )
		{
			printf( "-------------------------+" );
		} // for
		printf("\n");
	} // end of headers


	int timezone_count = lc_tree_map_size( map );

	while( timezone_count > 0 )
	{
			printf("|");
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
				printf("|");
			} // for
			printf("\n");

			printf("|");
			for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
				 itr != lc_tree_map_end( );
				 itr = lc_tree_map_next(itr) )
			{
				timezone_contact_t** list = itr->value;
				if( list && lc_vector_size(list) > 0)
				{
					timezone_contact_t* contact = lc_vector_last(list);

					struct tm* utc_time = gmtime(&now);
					//time_t offset_time = mktime(utc_time) + ((contact->utc_offset) * 3600.0);
					time_t offset_time = now + ((contact->utc_offset + contact->dst) * 3600.0);
					struct tm* other_time = gmtime(&offset_time);

					char time_str[12];
					strftime(time_str, sizeof(time_str), "%I:%M:%S %p", other_time);
					time_str[ sizeof(time_str) - 1 ] = '\0';

					console_fg_color_256( stdout, CONSOLE_COLOR256_BRIGHT_YELLOW);
					printf( "  \u23f0 %-*s ", 19, time_str );
					console_reset( stdout );
				}
				else
				{
					printf( "%-24s ", "" );
				}
				printf("|");
			} // for
			printf("\n");

			printf("|");
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
				printf("|");
			} // for
			printf("\n");

			printf("|");
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

				printf("|");
			} // for
			printf("\n");

			printf("|");
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
						printf( "  \u260F  %-.*s... ", 17, contact->mobile_phone );
					}
					else
					{
						// fixed width
						printf( "  \u260F  %-*s ", 19, contact->mobile_phone );
					}
					console_reset( stdout );
					printf("|");
				}
				else
				{
					printf( "%-24s |", "" );
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
		printf("+");
		for( lc_tree_map_iterator_t itr = lc_tree_map_begin( map );
			 itr != lc_tree_map_end( );
			 itr = lc_tree_map_next(itr) )
		{
			printf( "-------------------------+" );
		} // for
		printf("\n");
	} // end of footer

	lc_tree_map_clear( map );

}

bool read_configuration( timezone_contact_t** contacts )
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

        result = read_configuration( contacts );
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
		/* RegEx:
		 *
		 *    ([\+\-]?\d+\.?\d*)\s+(0|1)\s+"(.*)"\s+"(.*)"\s+"(\+?[\/\-\(\)\w\s]*)"\s+"(\+?[\/\-\(\)\w\s]*)"
		 */
		const char* CONFIG_LINE_REGEX = "([\\+\\-]?[[:digit:]+][\\.[:digit:]]*)" /* group 1: utc offset */
										"[[:space:]]+"
										"(0|1)" /* group 2: dst */
										"[[:space:]]+"
										"\"(.*)\"" /* group 3: email */
										"[[:space:]]+"
										"\"(.*)\"" /* group 4: name */
										"[[:space:]]+"
										"\"(\\+?[-\\(\\)/[:alnum:][:space:]]*)\"" /* group 5: office number */
										"[[:space:]]+"
										"\"(\\+?[-\\(\\)/[:alnum:][:space:]]*)\""; /* group 6: mobile number */
		regex_t regex;
		int regex_comp_result = regcomp( &regex, CONFIG_LINE_REGEX, REG_EXTENDED | REG_ICASE);

		if( regex_comp_result )
		{
			console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
			fprintf( stderr, "ERROR: " );
			console_reset( stderr );
			fprintf( stderr, "Unable to compile regular expression.\n" );
			result = false;
			goto done;
		}

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
					regfree( &regex );
					result = false;
					goto done;
				}
				else
				{
					string_trim( line, " \t\r\n" );
				}

				if( line[0] == '#' )
				{
					// skipping comments
					continue;
				}
				else if( strcmp( line, "" ) == 0 )
				{
					// skipping empty lines
					continue;
				}
				else
				{
					const int max_groups = 7;
					regmatch_t matches[ max_groups ];
					int regex_result = regexec( &regex, line, max_groups, matches, 0 );

					if( !regex_result )
					{
						size_t utc_offset_len = matches[1].rm_eo - matches[1].rm_so;
						char *utc_offset = malloc(utc_offset_len + 1);
						if( !utc_offset )
						{
							console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
							fprintf( stderr, "ERROR: " );
							console_reset( stderr );
							fprintf( stderr, "Out of memory.\n" );
							regfree( &regex );
							result = false;
							goto done;
						}
						memcpy(utc_offset, line + matches[1].rm_so, utc_offset_len);
						utc_offset[ utc_offset_len ] = '\0';

						size_t dst_len = matches[2].rm_eo - matches[2].rm_so;
						char *dst = malloc(dst_len + 1);
						if( !dst )
						{
							console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
							fprintf( stderr, "ERROR: " );
							console_reset( stderr );
							fprintf( stderr, "Out of memory.\n" );
							regfree( &regex );
							result = false;
							goto done;
						}
						memcpy(dst, line + matches[2].rm_so, dst_len);
						dst[ dst_len ] = '\0';

						size_t email_len = matches[3].rm_eo - matches[3].rm_so;
						char *email = malloc(email_len + 1);
						if( !email )
						{
							console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
							fprintf( stderr, "ERROR: " );
							console_reset( stderr );
							fprintf( stderr, "Out of memory.\n" );
							regfree( &regex );
							result = false;
							goto done;
						}
						memcpy(email, line + matches[3].rm_so, email_len);
						email[ email_len ] = '\0';

						size_t name_len = matches[4].rm_eo - matches[4].rm_so;
						char *name = malloc(name_len + 1);
						if( !name )
						{
							console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
							fprintf( stderr, "ERROR: " );
							console_reset( stderr );
							fprintf( stderr, "Out of memory.\n" );
							regfree( &regex );
							result = false;
							goto done;
						}
						memcpy(name, line + matches[4].rm_so, name_len);
						name[ name_len ] = '\0';

						size_t office_phone_len = matches[5].rm_eo - matches[5].rm_so;
						char *office_phone = malloc(office_phone_len + 1);
						if( !office_phone )
						{
							console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
							fprintf( stderr, "ERROR: " );
							console_reset( stderr );
							fprintf( stderr, "Out of memory.\n" );
							regfree( &regex );
							result = false;
							goto done;
						}
						memcpy(office_phone, line + matches[5].rm_so, office_phone_len);
						office_phone[ office_phone_len ] = '\0';

						size_t mobile_phone_len = matches[6].rm_eo - matches[6].rm_so;
						char *mobile_phone = malloc(mobile_phone_len + 1);
						if( !mobile_phone )
						{
							console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
							fprintf( stderr, "ERROR: " );
							console_reset( stderr );
							fprintf( stderr, "Out of memory.\n" );
							regfree( &regex );
							result = false;
							goto done;
						}
						memcpy(mobile_phone, line + matches[6].rm_so, mobile_phone_len);
						mobile_phone[ mobile_phone_len ] = '\0';

						timezone_contact_t contact = (timezone_contact_t) {
							.utc_offset   = strtod(utc_offset, NULL), // WARNING: The regex above should ensure this does not fail.
							.dst          = atoi(dst), // WARNING: The regex above should ensure this does not fail.
							.email        = email,
							.name         = name,
							.office_phone = office_phone,
							.mobile_phone = mobile_phone,
						};
						free( utc_offset );
						free( dst );
						lc_vector_push( *contacts, contact );
					}
					else if (regex_result == REG_NOMATCH)
					{
						// line did not match.
						console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
						fprintf( stderr, "ERROR: " );
						console_reset( stderr );
						fprintf( stderr, "Unable to match line with regular expression (see line %d).\n", line_number );
						//fprintf( stderr, "Line: %s\n", line );
						regfree( &regex );
						result = false;
						goto done;
					}
					else
					{
						console_fg_color_256( stderr, CONSOLE_COLOR256_RED );
						fprintf( stderr, "ERROR: " );
						console_reset( stderr );
						fprintf( stderr, "Unable to execute regular expression.\n" );
						regfree( &regex );
						result = false;
						goto done;
					}
				}

				line_number += 1;
			} // if
		} // while

		regfree( &regex );
		fclose( config );
		result = true;
	} // if

done:
	return result;
}

bool configuration_write_default( const char* configuration_filename )
{
	bool result = false;
	FILE* config = fopen( configuration_filename, "w" );

	if( config )
	{
		fprintf( config, "# -------------------------------\n" );
		fprintf( config, "# This is the configuration for the timezoner program. You can\n" );
		fprintf( config, "# add new contacts here.\n" );
		fprintf( config, "#\n" );
		fprintf( config, "# The format is:\n" );
		fprintf( config, "#\n" );
		fprintf( config, "# Offset \tDST \tEmail \tName \tOfficePhone \tMobilePhone\n" );
		fprintf( config, "+1.0 \t1 \t\"john.doe@example.com\" \"John Doe\" \"+1 305 555 1234\" \"+1 954 555 5678\"\n" );

		fclose( config );
		result = true;
	}

	return result;
}

bool timezone_map_element_destroy( void *p_key, void *p_value )
{
	timezone_contact_t** list = p_value;

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
	const double* l = p_key_left;
	const double* r = p_key_right;

	if( *l < *r )
	{
		return -1;
	}
	else if( *l > *r )
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static int contact_name_compare( const void *l, const void *r )
{
	const timezone_contact_t** left = (const timezone_contact_t**) l;
	const timezone_contact_t** right = (const timezone_contact_t**) r;
	return strcmp((*left)->name, (*right)->name );
}

