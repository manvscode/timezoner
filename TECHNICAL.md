



In a C structure, we can capture this information about our contacts with a declaration like this:

	typedef struct timezone_contact {
		double utc_offset;
		int dst; /* 1 if DST is observed */
		const char* email;
		const char* name;
		const char* office_phone;
		const char* mobile_phone;
	} timezone_contact_t;

Now, that we have our basic data structure defined. Let's define how our configuration file will be
structured. Each contact will be defined on a single line like this:

	# This is a comment.
	# Offset  DST  Email                Name            OfficePhone         MobilePhone
	+01.0     1    "goofy@disney.com"   "Goofy"         "n/a"               "+1 731 555 1234"
	+02.0     1    "minnie@disney.com"  "Minnie Mouse"  "+1 646 555 5678"   "+1 954 555 5678"
	+05.0     1    "pluto@disney.com"   "Pluto"         "n/a"               "+1 902 555 1234"
	+05.5     0    "mickey@disney.com"  "Mickey Mouse"  "+1 347 535 1234"   "+1 994 555 5678"
	-05.0     1    "donald@disney.com"  "Donald Duck"   "+1 330 555 5678"   "+1 305 555 1234"
	-07.0     1    "pete@disney.com"    "Pete"          "+1 507 555 1234"   "+1 208 555 5678"

Each field in the configuration file is separated by white space. String fields begin and end with a
double quote character (i.e. '"'), and numeric fields will not.

Next, let's start reading this data.

## Reading and Parsing Configuration

In the configuration file, each non-comment line is organized into six groups. The first group is the
utc offset.  Valid values include stuff like "1", "+01.0", "-5", "0", "-5.5", et cetera.  This can be
parsed with the following regular expression:

	([\+\-]?\d+\.?\d*)

Next, we have to parse the DST flag.  This should only be a zero or a one and can be parsed with the
following regular expression:

	(0|1)

The next fields are the email and name fields. These fields are string values and are enclosed in
double-quote characters.  We can parse these fields with this regular expression:

	"(.*)"

Lastly, we have the phone number fields. These fields should only allow valid phone numbers like "+1
954-555-1234", "(305) 555-1123", et cetera. We can parse these fields with this regular expression:

	"(\+?[\/\-\(\)\w\s]*)"

With the above pieces, we can combine them into a much larger regular expression to parse each line.
This would look like this:

	([\+\-]?\d+\.?\d*)\s+(0|1)\s+"(.*)"\s+"(.*)"\s+"(\+?[\/\-\(\)\w\s]*)"\s+"(\+?[\/\-\(\)\w\s]*)"

Notice that we added \s+ between each grouping since that's how each configuration line is composed.

In C, we can use POSIX regular expression library (in glibc) to parse the entire line. This would
look like this:

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


Now that we have our regular expression defined. We have to compile it so that we can repeatedly use
it when parsing each line.

	regex_t regex;
	int regex_comp_result = regcomp( &regex, CONFIG_LINE_REGEX, REG_EXTENDED | REG_ICASE);

	if( regex_comp_result )
	{
		// handle error condition...
	}

Note that we are using the REG_EXTENDED flag since we are using the extended POSIX regex syntax in
CONFIG_LINE_REGEX. Now we can execute our regular expression on each line and build a
timezone_contact_t object for the current line.  The code to do this looks like this:

	const int max_groups = 7;
	regmatch_t matches[ max_groups ];
	int regex_result = regexec( &regex, line, max_groups, matches, 0 );

	if( !regex_result )
	{
		// Do something with the result...
	}

Notice that max_groups is seven. This is because the whole string is matched at index 0. From index
1 to 6, we have each of the matched groups for the offset, the DST flag, the email address, the
name, the office number, and mobile number, respectivtly. For example, we can extract the group
match for the UTC offset, like this:

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

Once we have extracted all of the matched strings, we can build a timezone_contact_t object and add
it to collection like this:

	timezone_contact_t contact = (timezone_contact_t) {
		.utc_offset   = strtod(utc_offset, NULL),
		.dst          = atoi(dst),
		.email        = email,
		.name         = name,
		.office_phone = office_phone,
		.mobile_phone = mobile_phone,
	};

	// TODO: Clean up utc_offset and dst string allocations.

	// Add the contact to the collection of contacts
	lc_vector_push( *contacts, contact );

## Organizing the Data.

In this utility, we would like to output the information in a tabular manner. This can be done in
two ways--by rows and by columns.  Outputing row-based tabular data is the most natural way to
output data in command line programs. Given our timezone_contact_t structure, this might resemble
something like this:

	Donald Duck        06:48:03 PM     donald@disney.com        +1 330 555 5678      +1 305 555 1234
	Goofy              12:48:03 AM     goofy@disney.com         n/a                  +1 731 555 1234
	Minnie Mouse       01:48:03 AM     minnie@disney.com        +1 646 555 5678      +1 954 555 5678
	Pluto              04:48:03 AM     pluto@disney.com         n/a                  +1 902 555 1234
	Mickey Mouse       04:18:03 AM     mickey@disney.com        +1 347 535 1234      +1 994 555 5678

Each row is a complete object where each column is a field from the structure.  Output progresses by
iterating over each object in a collection and printing a line of output.

[[TODO: Picture of iterating over rows]]

The other technique is column-based tabular data.  This is where objects are outputted along columns
where each column has a subset of the collection.

	     UTC-05.0              UTC+01.0              UTC+02.0              UTC+05.0              UTC+05.5
	--------------------------------------------------------------------------------------------------------------
	  Donald Duck           Goofy                 Minnie Mouse          Pluto                 Mickey Mouse
	   06:58:34 PM           12:58:34 AM           01:58:34 AM           04:58:34 AM           04:28:34 AM
	   donald@disney.com     goofy@disney.com      minnie@disney.com     pluto@disney.com      mickey@disney.com
	   +1 330 555 5678       n/a                   +1 646 555 5678       n/a                   +1 347 535 1234
	   +1 305 555 1234       +1 731 555 1234       +1 954 555 5678       +1 902 555 1234       +1 994 555 5678

The achieve column-based output, we will need to take our collection of contacts and find groups of
contacts that belong in the same UTC timezone.  To do this, we need to utilize some kind of balanced
binary tree.  In this case, a tree-map is used because of the convenience of keeping the key and
value separated but any AVL or red-black tree would suffice.

[[TODO: Picture of iterating over tree]]

As we iterate over the collection of contacts, we will look for an existing node in the tree-map for
the contact's UTC offset.  If we do not find an existing node, then we add a new node for the UTC
offset and a new collection containing the contact.  Otherwise, if we find an existing node, then we
will need to add the contact to collection for this UTC offset.

Since we want to be able to display both column and row based tables, we will also utilize the
tree-map when outputting the row-based table.

## Displaying the Data

Knowing how to effectively use printf() is the key to beautiful output on the command line. For
outputing tabular output it is crucial to understand how to display fixed width strings, and
variable width strings. Finally, to make console output stand out, we should know how to display color
on VT100 compatible terminals.

### Using Fixed Width Output

In C, we can use printf() to output fixed string strings in two ways.  The first way is when you
would like the field to possibly exceed the width you intended.  You can do that like this:

	// Output the email and use a width of at least 40 characters
	printf( "%40s", contact->email );

Furthermore, the string's alignment can be controlled with a '-' character in the format specifier
like this:

	// Output the email left-aligned and use a width of at least 40 characters
	printf( "%-40s", contact->email );

This is useful when outputing command line arguments where if an argument exceeds it's width, then
you still want to output it without truncation.

	printf( "Command Line Options:\n" );
	printf( "    %-2s, %-12s   %-50s\n", "-r", "--rows", "Display using rows." );
	printf( "    %-2s, %-12s   %-50s\n", "-c", "--columns", "Display using columns" );


Sometimes, you do not want your output to exceed a certain width.  In such cases, we should truncate
the remaining string.  This can be easily done like this:

	// Output the email and truncate all characters after the 40th character.
	printf( "%.40s", 21, contact->email );

	// Like above but left-aligned
	printf( "%-.40s", 21, contact->email );

### Using Variable Width Output

In addition to hard coded widths, we could also programmatically specify the width. This can be done
when you need columns to scale based on the largest string to output. Suppose the largest string is
21 characters long, then we can control the width by using the '\*' in the format specifier, like
this:

	size_t largest_length = 21; // Figure out the largest string length

	// Truncated width
	printf( "%.*s", largest_length, contact->email );

	// Truncated width, left-aligned
	printf( "%-.*s", largest_length, contact->email );

	// fixed width
	printf( "%*s", largest_length, contact->email );

	// fixed width, left-aligned
	printf( "%-*s", largest_length, contact->email );

### Using Color

Using color is quite easy and most Linux terminals support displaying 16 colors out of the box.
The basic idea is to output terminal color codes like this:

	int fg_color = 34; // blue

	// Set the forground color
    fprintf( stream, "\033[%dm", fg_color );

	int bg_color = 43; // yellow

	// Set the background color
    fprintf( stream, "\033[%d;1m", bg_color );

Here are the possible colors that can be set:

			 foreground background
	black        30         40
	red          31         41
	green        32         42
	yellow       33         43
	blue         34         44
	magenta      35         45
	cyan         36         46
	white        37         47

After setting the color, you can now write the text that you would like to be colorized. Finally, we
must reset the terminal like this:

	// Reset the terminal
    printf( "\033[0m" );

## The Finale



## Source Code
