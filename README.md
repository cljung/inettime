# inettime
Gets/sets computers time from internet based time source

## Abstract
This is a program that dates back to the late 90's when I had the idea that syncing a computers clock against the internet would be a good idea.
The program is very simple. It makes a http request to a trusted source and reads the Date: attribute in the http response. 
I have successfully used the program on Windows, Linux, FreeBSD, OpenVMS and MacOS for almost 20 years.

## Revision history
In the beginning, this tool used the US Navy's websites output and screen scraped output in the html body, but when the program became popular world wide, 
the website started to use random output date formats. Then I changed to using the Date: attribute in the standard http response.
Some 17 years after I wrote the program, I switched to use Microsoft Azure as the source of time, since I thought that if Azure have a time skew problem, many enterprises
in the world have a time skew problem.

## Building the sample
### Linux/MacOS
The below works on Linux Ubuntu 14/16 and MacOS El Capitain/Sierra
<pre>
<code>
git clone https://github.com/cljung/inettime.git
cd inettime
make
</code>
</pre>

### Windows
Open inettime.sln in Visual Studio and build it. I've used VS2015 and Windows 10 for my latest build environment

### OpenVMS
Run the DCL script inettime_openvms.com to build the program on OpenVMS.
Back in the hay-days I built on OpenVMS 7.2 and it was used for a very long time at an not-so-small financial institute who asked me to update and maintain it for them.

## Running the program
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

To update the computers clock you need to run it with UAC priviledges (admin) on Windows. On MacOS/Linux, you need to run it with sudo and on OpenVMS you neet to type SET PROC/PRIV=ALL on the command prompt before running with -S switch.

### Task Scheduling
To use this program more seriously, you schedule it as a background job using Task Scheduler on Windows or cron on Linux/MacOS. 
It's as simple as selecting your frequency and run the program like below.
<pre>
<code>
"C:\path\inettime.exe -s -t 10"
</code>
</pre>
The -t switch sets the timeout in the http call. 

### Why use this and not NTP?
That's a very legitimate question. If you have a good and reliable NTP setup you should of cause use it. If you don't and want to be in sync with one of the biggest public cloud, you CAN use this tool.

## Logging
To report that it actually adjusts the systems time, the program logs an event in the Eventlog on Windows. That is why you have the Win32 Message Compiler msgs.mc file there.
On Linux-based systems, the program emits info to the syslog.

