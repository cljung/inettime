# inettime
Gets/sets computers time from internet based time source

## Abstract
This is a program that dates back to the late 90's when I had the idea that syncing a computers clock against the internet would be a good idea.
The program is very simple. It makes a http request to a trusted source and reads the Date: attribute in the http response. 
I have successfully used the program on Windows, Linux, FreeBSD, OpenVMS and MacOS for almost 20 years.

## Revision history
In the beginning, this tool used the US Navy's websites output and screen scraped output in the html body, but when the program became popular world wide, 
the website started to use random output date format. Then I changed to using the Date: attribute in the standard http response.
Some 17 years after I wrote the program, I switched to use Microsoft Azure as the source of time, since if Azure have a time skew problem, many enterprises
in the world have a time skew problem.

## Building the sample on Linux
Git clone the code and either run make on MacOS/Linux/FreeBSD, the DCL script on OpenVMS or build the code using Visual Studio on Windows.
My most recent test builds have been on Windows 10/VS2015, MacOS El capitain, Linux Ubuntu 14.04. 
Back in the hay-days I built on OpenVMS 7.2 and it was used for a very long time at an not-so-small financial institute who asked me to update and maintain it for them.

Just running it from the command line, it will tell you the time difference between your machine and the source (Azure)
<pre>
<code>
Opening url: http://azure.com/
2017-03-31 20:39:09
Current System Time
2017-03-31 20:39:08 (UTC)
2017-03-31 22:39:08 (local)
</code>
</pre>

Running it with the -S switch will make the program try to update the computers clock.

On Windows, you need to run it with UAC priviledges (admin) and on MacOS/Linux, you need to run it with sudo if you want to update the clock.

## Logging
To report that it actually adjusts the systems time, the program logs an event in the Eventlog on Windows. That is why you have the Win32 Message Compiler msgs.mc file there.
On Linux-based systems, the program emits info to the syslog.

