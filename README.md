# Timezoner

> A command line tool for collaborating across timezones.

At [End Point Corporation](https://endpoint.com/), our team is spread out across 10 time-zones. This can make
collaborating on opensource projects quite challenging. To make things easier and to set the stage
to develop this idea further, I have decided to write a small command line utility that will allow
one to quickly check what time it is for each their colleagues.

Every timezone is relative to [Coordinated Universal Time (UTC)](https://en.wikipedia.org/wiki/Coordinated_Universal_Time) and has an associated offset. These
offsets are usually in whole hour increments, but they may include partial hours. For example, [Eastern Standard Time (EST)](https://en.wikipedia.org/wiki/Eastern_Time_Zone)
is five hours behind UTC and [Indian Standard Time (IST)](https://en.wikipedia.org/wiki/Indian_Standard_Time) is five and half
hours ahead of UTC. Moreover, some regions may not observe [daylight savings time (DST)](https://en.wikipedia.org/wiki/Daylight_saving_time)
while other regions do. For Australia, this can be quite challenging since it's left to local governments to
decide whether DST is observed.

Now to make all of time coordination easier, you can use this utility to do all of the time
conversions for you.  First, you have to add each coworker's information to a configuration file
stored at ~/.timezoner. This configuration file describes every colleague's UTC offset for their
timezone, whether they observe daylight savings time, their full name, their email address, their
office phone number, and their mobile phone number. As an example, this is what the configuration
file looks like:

	# This is a comment.
	# Offset  DST  Email                  Name                OfficePhone         MobilePhone
	-05.0     1    "edward@example.com"   "Edward Teach "     "n/a"               "+1 731 555 1234"
	-06.0     1    "henry@dexample.com"   "Henry Morgan"      "+1 646 555 5678"   "+1 954 555 5678"
	-05.0     1    "john@example.com"     "John Auger"        "n/a"               "+1 902 555 1234"
	+05.5     0    "sam@example.com"      "Samuel Bellamy"    "+1 347 535 1234"   "+1 994 555 5678"
	-05.0     1    "william@example.com"  "William Kidd"      "+1 330 555 5678"   "+1 305 555 1234"
	-07.0     1    "israel@example.com"   "Israel Hands"      "+1 507 555 1234"   "+1 208 555 5678"

Now when I need to inquire what time is it for my team, I can run it and get some nice formatted
output:

	[~]$ timezoner -r
	+----------------------------------------------------[ UTC-07.0 ]---------------------------------------------------+
	| Israel Hands              11:12:14 PM   ✉ israel@example.com        ☎  +1 507 555 1234     ☏  +1 208 555 5678     |
	+----------------------------------------------------[ UTC-06.0 ]---------------------------------------------------+
	| Henry Morgan              12:12:14 AM   ✉ henry@dexample.com        ☎  +1 646 555 5678     ☏  +1 954 555 5678     |
	+----------------------------------------------------[ UTC-05.0 ]---------------------------------------------------+
	| Edward Teach              01:12:14 AM   ✉ edward@example.com        ☎  n/a                 ☏  +1 731 555 1234     |
	| John Auger                01:12:14 AM   ✉ john@example.com          ☎  n/a                 ☏  +1 902 555 1234     |
	| William Kidd              01:12:14 AM   ✉ william@example.com       ☎  +1 330 555 5678     ☏  +1 305 555 1234     |
	+----------------------------------------------------[ UTC+05.5 ]---------------------------------------------------+
	| Samuel Bellamy            10:42:14 AM   ✉ sam@example.com           ☎  +1 347 535 1234     ☏  +1 994 555 5678     |
	+-------------------------------------------------------------------------------------------------------------------+


	[~]$ timezoner -c
	+-------------------------+-------------------------+-------------------------+-------------------------+
	|        UTC-07.0         |        UTC-06.0         |        UTC-05.0         |        UTC+05.5         |
	+-------------------------+-------------------------+-------------------------+-------------------------+
	| Israel Hands            | Henry Morgan            | William Kidd            | Samuel Bellamy          |
	|    11:13:54 PM          |    12:13:54 AM          |    01:13:54 AM          |    10:43:54 AM          |
	|    israel@example.co... |    henry@dexample.co... |    william@example.c... |    sam@example.com      |
	|    +1 507 555 1234      |    +1 646 555 5678      |    +1 330 555 5678      |    +1 347 535 1234      |
	|    +1 208 555 5678      |    +1 954 555 5678      |    +1 305 555 1234      |    +1 994 555 5678      |
	|                         |                         | John Auger              |                         |
	|                         |                         |    01:13:54 AM          |                         |
	|                         |                         |    john@example.com     |                         |
	|                         |                         |    n/a                  |                         |
	|                         |                         |    +1 902 555 1234      |                         |
	|                         |                         | Edward Teach            |                         |
	|                         |                         |    01:13:54 AM          |                         |
	|                         |                         |    edward@example.co... |                         |
	|                         |                         |    n/a                  |                         |
	|                         |                         |    +1 731 555 1234      |                         |
	+-------------------------+-------------------------+-------------------------+-------------------------+

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

