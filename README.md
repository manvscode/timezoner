# Timezoner

> A command line tool for collaborating across timezones.

## Configuration

Timezoner will create a default configuration under your user's home directory (~/.timzoner) if one
does not already exist.  The configuration file is structured with several fields that are
separated with whitespace. The fields are:

* The IANA timezone code.  The second field is
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

## Grouping Contacts by Time

With the '-T' option, contacts are grouped by their local time. For the above configuration, this looks
like this:

	$ timezoner -T
	+--[ 12:43:27 PM ]-------------------------------------------------------------------------------------------+
	| Samuel Bellamy                  ✉ sam@example.com            ☎  +1 347 535 1234     ☏  +1 994 555 5678     |
	| William Kidd                    ✉ william@example.com        ☎  +1 330 555 5678     ☏  +1 305 555 1234     |
	+--[ 01:43:27 PM ]-------------------------------------------------------------------------------------------+
	| Henry Morgan                    ✉ henry@dexample.com         ☎  +1 646 555 5678     ☏  +1 954 555 5678     |
	+--[ 02:43:27 PM ]-------------------------------------------------------------------------------------------+
	| Israel Hands                    ✉ israel@example.com         ☎  +1 507 555 1234     ☏  +1 208 555 5678     |
	+--[ 03:43:27 PM ]-------------------------------------------------------------------------------------------+
	| Edward Teach                    ✉ edward@example.com         ☎  n/a                 ☏  +1 731 555 1234     |
	| John Auger                      ✉ john@example.com           ☎  n/a                 ☏  +1 902 555 1234     |
	+------------------------------------------------------------------------------------------------------------+

## Grouping Contacts by UTC Offset

If you prefer to group contacts by their UTC offset, you can use the '-U' option to do this.

	$ timezoner -U                                                                                                                                                                                                                          *[master] 
	+-------------------------+-------------------------+-------------------------+-------------------------+
	|        UTC-05.0         |        UTC-06.0         |        UTC-07.0         |        UTC-08.0         |
	+-------------------------+-------------------------+-------------------------+-------------------------+
	| John Auger              | Israel Hands            | Henry Morgan            | William Kidd            |
	|  ⏰ 03:43:57 PM         |  ⏰ 02:43:57 PM         |  ⏰ 01:43:57 PM         |  ⏰ 12:43:57 PM         |
	|  ✉ john@example.com     |  ✉ israel@example.co... |  ✉ henry@dexample.co... |  ✉ william@example.c... |
	|  ☎  n/a                 |  ☎  +1 507 555 1234     |  ☎  +1 646 555 5678     |  ☎  +1 330 555 5678     |
	|  ☏  +1 902 555 1234     |  ☏  +1 208 555 5678     |  ☏  +1 954 555 5678     |  ☏  +1 305 555 1234     |
	| Edward Teach            |                         |                         | Samuel Bellamy          |
	|  ⏰ 03:43:57 PM         |                         |                         |  ⏰ 12:43:57 PM         |
	|  ✉ edward@example.co... |                         |                         |  ✉ sam@example.com      |
	|  ☎  n/a                 |                         |                         |  ☎  +1 347 535 1234     |
	|  ☏  +1 731 555 1234     |                         |                         |  ☏  +1 994 555 5678     |
	+-------------------------+-------------------------+-------------------------+-------------------------+

## Using Custom Configuration

You can also create custom configuration files and use them to see grouped contacts.  For example, he's how you
can view the local time for a 'science team.'

	$ timezoner -f ./science-team.cfg
	+--[ 01:03:26 PM ]-------------------------------------------------------------------------------------------+
	| Dr. Feynman                     ✉ feynman@science.com        ☎  +1 347 535 1234     ☏  +1 994 555 5678     |
	+--[ 02:03:26 PM ]-------------------------------------------------------------------------------------------+
	| Dr. Oppenheimer                 ✉ oppenheimer@science.com    ☎  n/a                 ☏  +1 731 555 1234     |
	+--[ 04:03:26 PM ]-------------------------------------------------------------------------------------------+
	| Dr. Einstein                    ✉ einstein@science.com       ☎  +1 646 555 5678     ☏  +1 954 555 5678     |
	+--[ 09:03:26 PM ]-------------------------------------------------------------------------------------------+
	| Dr. Heisenberg                  ✉ heisenberg@science.com     ☎  +1 330 555 5678     ☏  +1 305 555 1234     |
	| Dr. Planck                      ✉ planck@science.com         ☎  n/a                 ☏  +1 902 555 1234     |
	+------------------------------------------------------------------------------------------------------------+

## Modeling Timezone Differences Using Specific Time

Sometimes you want to see what time it will be in other timezones at a specific local time.  You can do exactly this
with the '-t' option.

	$ timezoner -t "3:00 pm"
	+--[ 12:00:00 PM ]-------------------------------------------------------------------------------------------+
	| Dr. Feynman                     ✉ feynman@science.com        ☎  +1 347 535 1234     ☏  +1 994 555 5678     |
	+--[ 01:00:00 PM ]-------------------------------------------------------------------------------------------+
	| Dr. Oppenheimer                 ✉ oppenheimer@science.com    ☎  n/a                 ☏  +1 731 555 1234     |
	+--[ 03:00:00 PM ]-------------------------------------------------------------------------------------------+
	| Dr. Einstein                    ✉ einstein@science.com       ☎  +1 646 555 5678     ☏  +1 954 555 5678     |
	+--[ 08:00:00 PM ]-------------------------------------------------------------------------------------------+
	| Dr. Heisenberg                  ✉ heisenberg@science.com     ☎  +1 330 555 5678     ☏  +1 305 555 1234     |
	| Dr. Planck                      ✉ planck@science.com         ☎  n/a                 ☏  +1 902 555 1234     |
	+------------------------------------------------------------------------------------------------------------+

## License

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

