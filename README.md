# Timezoner
A command line tool for collaborating across timezones.

## Building
### Linux and MacOS X
1. Clone the repository.
2. `make OS=linux`
3. `make install INSTALL_PATH=/usr/local/bin`

### Microsoft Windows using MinGW
This is currently a work in progress.

1. Clone the repository.
2. You can build for x86 or x86_64.

    To build for Windows x86:

        make OS=windows-x86

    To build for Windows x86_64:

        make OS=windows-x86_64

## Configuration

Timezoner will create a default configuration under your user's home directory (~/.timezoner) if one
does not already exist.  The configuration file is structured with several fields that are
separated with whitespace. The fields are:

* The IANA timezone code.
* The email address of the contact wrapped in double-quotes.
* The full name of the contact wrapped in double-quotes.
* The office phone number of the contact wrapped in double-quotes.
* The mobile phone number of the contact wrapped in double-quotes.

As an example, a valid configuration file looks like this:

	# Timezone           Email                  Name                OfficePhone         MobilePhone
	America/New_York     "edward@example.com"   "Edward Teach "     "n/a"               "+1 731 555 1234"
	America/Denver       "henry@dexample.com"   "Henry Morgan"      "+1 646 555 5678"   "+1 954 555 5678"
	America/New_York     "john@example.com"     "John Auger"        "n/a"               "+1 902 555 1234"
	America/Los_Angeles  "sam@example.com"      "Samuel Bellamy"    "+1 347 535 1234"   "+1 994 555 5678"
	America/Los_Angeles  "william@example.com"  "William Kidd"      "+1 330 555 5678"   "+1 305 555 1234"
	America/Chicago      "israel@example.com"   "Israel Hands"      "+1 507 555 1234"   "+1 208 555 5678"

## Grouping Contacts By Time

With the '-T' option, contacts are grouped by their local time. For the above configuration, this looks
like this:

![Grouping Contacts By Time](/screenshots/timezoner-1.png?s=800&raw=true "Grouping Contacts By Time")

## Grouping Contacts By UTC Offset

If you prefer to group contacts by their UTC offset, you can use the '-U' option to do this.

![Grouping Contacts By UTC Offset](/screenshots/timezoner-2.png?s=800&raw=true "Grouping Contacts By Time")

## Using Custom Configuration

You can also create custom configuration files and use them to see grouped contacts.  For example, he's how you
can view the local time for a 'science team.'

![Using Custom Configuration](/screenshots/timezoner-3.png?s=800&raw=true "Using Custom Configuration")

## Modeling Timezone Differences Using a Specific Time

Sometimes you want to see what time it will be in other timezones at a specific local time.  You can do exactly this
with the '-t' option.

![Using a Specific Time](/screenshots/timezoner-4.png?s=800&raw=true "Using a Specific Time")

## License

	Copyright (C) 2019, Joe Marrero. http://www.joemarrero.com/
	Copyright (C) 2017, End Point Corporation. http://www.endpoint.com/

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in
	all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
	THE SOFTWARE.

