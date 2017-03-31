// inettime.cpp 
//
// This program uses TCP/IP and HTTP to retrieve the correct UTC time from
// a Perl script that US Navy has available on the internet. It the either
// tells you the difference or sets the computers clock.
//
// It compiles and runs under Windows NT/2000/2003 x86 and Win2003 AMD64, 
// Linux RedHat 6.2/8.0, Ubuntu 14.04, MacOS 10, OpenVMS 7.2 and FreeBSD 4.8.
// 
//
// Copyright (c), 1999-2017, Christer Ljung, RedBaronOfAzure
//
//         available from:   http://github.com/cljung/inettime
//
// You may use this program freely if you keep the comments above
// 
//
// July  6, 2003
//   Program updated since timesource change to format "July 05, 23:02:10 UTC"
//
// Sep  4, 2003
//   Program updated since timesource changed again.
//   We now use the HTTP header value "Date" in the response 
//   for time source attribute since that follows the RFC spec of the HTTP
//   protocol.
//
// Oct  6, 2003
//   changed program ret.code. 0=ok, 1=tcp err, 2=parse err
//
// Dec 22, 2003
//   Minor changes to get it to compile cleanly with MS CL for AMD64
//
// Mar 26, 2004
//   Minor changes in socket_transact() so that it's OK if last recv()
//   fails if previous succeeded. We might have enough data anyway!
//
// Mar 12, 2015
//   Didn't think I was ever going to change this program again, but
//   US Navy seems to have pulled the service off the grid. So now
//   let's use azure.com instead and use HTTP HEAD instead of GET to
//   minimize http response payload. Azure is in serious trouble with
//   it's cloud service if time is not correct
//	(Nowadays you can do this in a few lines of powershell on Windows)
//
#ifdef WIN32

#if (defined _WIN64) && (defined AMD64)
#pragma message("inettime - Windows AMD64 compilation...")
#else
#pragma message("inettime - Windows/Intel compilation...")
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>

#define WS_VERSION_REQD    0x0202
#include <winsock2.h>
#pragma comment(lib, "wsock32.lib")

/*/////////////////////////////////////////////////////////////////////// */
/* OpenVMS  */
#elif (defined __vms) /* OpenVMS */

#pragma message("inettime - OpenVMS/Alpha compilation...")

#include <stdlib.h>
#include <types.h>
#include <time.h>
#include <stdio.h>
#include <descrip.h>        /* VMS descriptor stuff */
#include <in.h>             /* internet system Constants and structures. */
#include <inet.h>           /* Network address info. */
#include <ioctl.h>          /* I/O Ctrl DEFS */
#include <iodef.h>          /* I/O FUNCTION CODE DEFS */
#include <lib$routines.h>   /* LIB$ RTL-routine signatures. */
#include <netdb.h>          /* Network database library info. */
#include <signal.h>         /* UNIX style Signal Value Definitions */
#include <socket.h>         /* TCP/IP socket definitions. */
#include <ssdef.h>          /* SS$_<xyz> sys ser return stati <8-) */
#include <starlet.h>        /* Sys ser calls */
#include <string.h>         /* String handling function definitions */
#ifdef __VMS_TCPIP_INETDEF
#include <tcpip$inetdef.h>    /* TCPIP network definitions */
#endif
#include <unixio.h>         /* Prototypes for UNIX emulation functions */
#include <stropts.h>         /* ioctl() function */
#include <errno.h>
#include <ctype.h>

#define closesocket(h)		close(h)
#define ioctlsocket(h,p,l)		ioctl(h,p,l)

/*/////////////////////////////////////////////////////////////////////// */
/* Linux assumed   */
#else /* Linux/UNIX includes */

#pragma message("inettime - Linux/FreeBSD compilation...")

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <pwd.h>
#include <signal.h>
#include <memory.h>
#include <ctype.h>
#include <netdb.h>
#include <errno.h>
#include <syslog.h>

/* defs so we can code in winsock style under Linux/UNIX */
#define send(h,p,l,f)		write(h,p,l)
#define recv(h,p,l,f)		read(h,p,l)
#define closesocket(h)		close(h)
#define ioctlsocket(h,x,y)	ioctl(h,x,y)

#endif // Linux

/*/////////////////////////////////////////////////////////////////////// */
/* common non-Windows stuff that we use anyway */
#ifndef WIN32

#ifndef INVALID_SOCKET
#define INVALID_SOCKET		-1
#endif

#ifndef SOCKET_ERROR
#define SOCKET_ERROR		-1
#endif

#define SOCKET				int

typedef struct sockaddr SOCKADDR;
typedef struct sockaddr *PSOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr_in *PSOCKADDR_IN;

typedef unsigned short WORD;
#define BOOL			bool
#define TRUE			1
#define FALSE			0

typedef struct _SYSTEMTIME {
   WORD wYear;
   WORD wMonth;
   WORD wDayOfWeek;
   WORD wDay;
   WORD wHour;
   WORD wMinute;
   WORD wSecond;
   WORD wMilliseconds;
} SYSTEMTIME;

// oddly enought this C-rtl is missing under OpenVMS and Linux
int strnicmp( const char *string1, const char *string2, size_t count );

#endif
/*/////////////////////////////////////////////////////////////////////// */

#ifdef WIN32
#include "msgs.h"
#endif

#define DIM(x)				(int)(sizeof(x)/sizeof(x[0]))
#define STR_EVENTSOURCE		"INetTime"
#define MAX_BUFSIZE			4096

#define SNTP_PORT			37
#define STR_DEF_SNTP		"tock.usno.navy.mil"
#define STR_HTTP			"http://"
//#define STR_WEBSRV			"tycho.usno.navy.mil"
//#define STR_URI				"/cgi-bin/timer.pl"
#define STR_WEBSRV			"azure.com"
#define STR_URI				"/"
#define STR_URL				STR_WEBSRV STR_URI

static bool			fSetSystemTime = false;
static char			*pszUrl = STR_HTTP STR_URL;
static bool			gfUseTcp = true;
static bool			gfVerbose = false;
static bool			gfUseProxy = false;
static char			gszProxyAddress[64];
static int			gnProxyPort = 8080;
static int			gnTimeout = 5000;

static char			*gMonths[] = {  "jan", "feb", "mar", "apr", "may", "jun"
							, "jul", "aug", "sep", "oct", "nov", "dec"
						};
static char			*gMonths2[] = {  "Jan", "Feb", "Mar", "Apr", "May", "Jun"
							, "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
						};

#ifdef WIN32
static HANDLE		hEventLog = 0;
#else
#endif

u_long GetHostAddress (char *pszHost);

#ifdef WIN32
void RegEventSource( void );
#endif

/////////////////////////////////////////////////////////////////////////////
// 
int ConvMonthToInt( char *pszMonth )
{
	if ( isdigit( *pszMonth ) )
		return atoi( pszMonth );

	int		n;

	for( n = 0; n < DIM(gMonths); n++ )
	{
		if ( !strnicmp( pszMonth, gMonths[n], strlen(gMonths[n]) ) )
			return n+1;
	}
	return 0;
}
// assumes month in range 1..12
int ConvMonthToStr( int month, char *pszMonth, int upcase )
{
	if ( month >= 1 && month <= 12 )
	{
		strcpy( pszMonth, gMonths[month-1] );
		if ( upcase )
		{
			char	*pch = pszMonth;
			while( *pch )
			{
				*pch = toupper( *pch );
				pch++;
			}
		}
		return 1;
	}
	else
	{
		*pszMonth = 0;
		return 0;
	}
}
/////////////////////////////////////////////////////////////////////////////
// 
#ifndef WIN32
/////////////////////////////////////////////////////////////////////////////
// 
int GetLastError()
{
	return errno;
}
void GetComputerTime( SYSTEMTIME *pSystemTime, bool fUTC )
{
	time_t		t;
	struct tm	*ptm;

	memset( pSystemTime, 0, sizeof(SYSTEMTIME) );

	time( &t );
	if ( fUTC )
		 ptm = gmtime( &t );
	else ptm = localtime( &t );

	pSystemTime->wSecond = (WORD)ptm->tm_sec;
	pSystemTime->wMinute = (WORD)ptm->tm_min;
	pSystemTime->wHour = (WORD)ptm->tm_hour;
	pSystemTime->wDay = (WORD)ptm->tm_mday;
	pSystemTime->wMonth = (WORD)ptm->tm_mon+1;
	pSystemTime->wYear = (WORD)ptm->tm_year + 1900;
	pSystemTime->wDayOfWeek = (WORD)ptm->tm_wday;
}
void GetSystemTime( SYSTEMTIME *pSystemTime )
{
	GetComputerTime( pSystemTime, true ); 
}
void GetLocalTime( SYSTEMTIME *pSystemTime )
{
	GetComputerTime( pSystemTime, false ); 
}

#ifdef __vms /* OpenVMS */
// OpenVMS implementation of Linux stime() C-rtl function so non Win32 code
// can call the same function. Since there exists no C-rtl api like stime()
// we use the command processors SET TIME command. This requires the appropriate
// OS priviledge, so type command SET PROC/PRIV=ALL before running under OpenVMS
// if you want to update the clock
int stime( const time_t *newtime )
{
	char	szCmd[ 80 ];
	char	szMonth[8];
	struct tm	*ptm;

	ptm = localtime( newtime );
	ConvMonthToStr( ptm->tm_mon+1, szMonth, 1 );
	sprintf( szCmd, "SET TIME=%02d-%s-%04d:%02d:%02d:%02d"
			, ptm->tm_mday, szMonth, ptm->tm_year+1900
			, ptm->tm_hour, ptm->tm_min, ptm->tm_sec
			);
	//printf( "%s\n", szCmd );
	//return 1;
	return system( szCmd );
}
#endif

#if (defined __FREEBSD__) || (defined __APPLE__)
// FreeBSD implementation of Linux stime() C-rtl function
int stime( const time_t *newtime )
{
	struct timeval	tv;
	tv.tv_sec = (long)*newtime;
	tv.tv_usec = 0;
	return settimeofday( &tv, 0 );
}
#endif

BOOL SetSystemTime( const SYSTEMTIME *pSystemTime )
{
	time_t		t;
	struct tm	stm;
	struct tm	*ptm;

	memset( &stm, 0, sizeof(stm) );
	
	// dig up if daylight savings time is active
	// doesn't work if we change date/time accross DST border...
	time( &t );
	ptm = localtime( &t );
	stm.tm_isdst = ptm->tm_isdst;

	// Convert SYSTEMTIME to time_t

	stm.tm_sec  = (int)pSystemTime->wSecond;
	stm.tm_min = (int)pSystemTime->wMinute;
	stm.tm_hour = (int)pSystemTime->wHour;
	stm.tm_mday = (int)pSystemTime->wDay;
	stm.tm_mon = (int)(pSystemTime->wMonth - 1);
	stm.tm_year = (int)(pSystemTime->wYear - 1900);
	stm.tm_wday = (int)pSystemTime->wDayOfWeek;

	t = mktime( &stm ); // mktime treats the struct tm according to TZ/ISDST

	// Looks like Linux stime() function assumes the time_t value is
	// expressed in localtime rather than UTC time. We therefor adjust
	// the hour to UTC time.
#if (defined WIN32)
	t = t + (-1 * _timezone);
#elif (defined __APPLE__) // OpenVMS
	t = t + (-1 * timezone);
#elif (defined __LINUX__)// Linux
	t = t + (-1 * __timezone);
#elif (defined __vms) // OpenVMS
	t = t + (-1 * timezone);
#elif (defined __FREEBSD__)
	// oddly enough, FreeBSD has no _timezone global variable. There is a 
	// function called timezone, but that's nothing for us here. The
	// timezone data you get back from the gettimeofday() call is flakey,
	// but lucky for us the struct tm is extended with a member called tm_gmtoff.
	if ( stm.tm_isdst == 1 )
		t = t + stm.tm_gmtoff - 3600;	
	else	t = t + stm.tm_gmtoff;	
#endif

	// if isdst is on, then add an extra hour in diff from UTC
	if ( stm.tm_isdst == 1 )
		t += 3600;
		
	if ( !stime( &t ) )
		 return TRUE;
	else return FALSE;
}

int strnicmp( const char *string1, const char *string2, size_t count )
{
	char	ch1, ch2;
	size_t	n = 0;
	int		nDiff = 'a' - 'A';	

	// while both not null
	while ( n <= count && *string1 && *string2 )
	{
		ch1 = *string1;
		ch2 = *string2;
		if ( ch1 >= 'a' && ch1 <= 'z' )
			ch1 -= nDiff;
		if ( ch2 >= 'a' && ch2 <= 'z' )
			ch2 -= nDiff;
		if ( ch1 != ch2 )
			return (int)(ch1-ch2);
		n++;
		string1++;
		string2++;
	}
	return 0; // is equal
}
#endif
/////////////////////////////////////////////////////////////////////////////
// 
bool Startup( void )
{
#ifdef WIN32
	RegEventSource();
	hEventLog = RegisterEventSource( 0, STR_EVENTSOURCE );

	// initialize WinSocket
    WSADATA     wsaData;
	BOOL		fRc = FALSE;
    int nRc = WSAStartup( WS_VERSION_REQD,&wsaData );
    if ( nRc == 0 )
    {
		if (LOBYTE(wsaData.wVersion) >= 2)
			fRc = TRUE;
	}
	return (fRc ? true : false);
#endif
#if (defined __LINUX__) || (defined __FREEBSD__)
	openlog( STR_EVENTSOURCE, LOG_PID|LOG_CONS, LOG_USER );
	return true;
#endif
#ifdef __vms
	return true;
#endif
}
/////////////////////////////////////////////////////////////////////////////
// 
void Shutdown( void )
{
#ifdef WIN32
	WSACleanup();
	DeregisterEventSource( hEventLog );
#endif
#if (defined __LINUX__) || (defined __FREEBSD__)
	closelog();
#endif
}
////////////////////////////////////////////////////////////////////////////
//
u_long GetHostAddress (char *pszHost)
{
	u_long lAddr = INADDR_ANY;
  
	if (*pszHost)
	{
		/* check for a dotted-IP address string */
		lAddr = inet_addr(pszHost);

		/* If not an address, then try to resolve it as a hostname */
		if ((lAddr == INADDR_NONE) && ( strcmp (pszHost, "255.255.255.255")))
		{
//#if (!defined __LINUX__) && (!defined __FREEBSD__) && (!defined __APPLE__)
			struct hostent	*pHost = 0;

			if ( ( pHost = gethostbyname(pszHost) ) ) // hosts/DNS lookup
				 lAddr = *((u_long*)(pHost->h_addr));
			else lAddr = INADDR_ANY;   /* failure */
//#else
//			lAddr = INADDR_ANY;
//#endif
		}
	}
	return lAddr; 
}
////////////////////////////////////////////////////////////////////////////
//
void ReportError( char *pszApi )
{
	int		rc;
#ifdef WIN32
	rc = WSAGetLastError();
#else
	rc = GetLastError();
#endif

	if ( gfVerbose )
		printf( "Error %d calling winsock api %s", rc, pszApi );

#ifdef WIN32
	char	szRc[16];
	
	sprintf( szRc, "%d", rc );
	const char	*pszArgs[] = { pszUrl, szRc, pszApi };

	ReportEvent( hEventLog, EVENTLOG_ERROR_TYPE, 0, MSG_E_SOCKET
					, 0, DIM(pszArgs), 0, pszArgs, 0 );
#endif
#if (defined __LINUX__) || (defined __FREEBSD__)
	syslog( LOG_ERR, "Error opening url %s. Ret.code %d calling winsock api %s"
			, pszUrl, GetLastError(), pszApi
			);
#endif
}
////////////////////////////////////////////////////////////////////////////
//
SOCKET connect_tcp( char *pszHost, int nPortNbr, SOCKADDR_IN *pSockAddr )
{
	SOCKET			hSocket;
	int				nRet;

	hSocket = socket( AF_INET, SOCK_STREAM, IPPROTO_IP );
	if ( hSocket == INVALID_SOCKET )
	{
		ReportError( "socket()" );
		return hSocket;
	}

	memset( pSockAddr, 0, sizeof(SOCKADDR_IN) );
	pSockAddr->sin_addr.s_addr = GetHostAddress( pszHost );
	pSockAddr->sin_family = PF_INET;	// internet
	pSockAddr->sin_port   = (u_short)htons( nPortNbr );  

	nRet = connect( hSocket, (const sockaddr*)pSockAddr, sizeof(SOCKADDR_IN) );
	if ( nRet == SOCKET_ERROR )
	{
		ReportError( "connect()" );
		closesocket( hSocket );
		return SOCKET_ERROR;
	}
	else return hSocket;
}
////////////////////////////////////////////////////////////////////////////
//
void close_tcp( SOCKET *socket )
{
	if ( *socket && *socket != SOCKET_ERROR )
		closesocket( *socket );
	*socket = 0;
}
////////////////////////////////////////////////////////////////////////////
//
int socket_transact(  SOCKET socket
				 , const char *sendbuf, int sendsize
				 , char *recvbuf, int *recvsize
				 , int timeout
				 )
{
    int				nRet;

	if ( timeout > 0 )
		setsockopt( socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(int) );

	nRet = send( socket, sendbuf, sendsize, 0 );
	if ( nRet == SOCKET_ERROR )
	{
		ReportError( "send()" );
		return nRet;
	}

	*recvbuf = 0;

	char	*pTail = recvbuf;
	int		cRecv = 0;
	int		cBuf = *recvsize;
	int		nChunk = 2048;

	while( cBuf > 0 )
	{
		nRet = recv( socket, pTail, nChunk, 0 );
		if ( nRet == SOCKET_ERROR )
		{
			if ( cRecv > 0 )
				break; // might be OK anyway
			ReportError( "recv()" );
			*recvsize = 0;
			return nRet;
		}
		if ( nRet == 0 )
			break; // while

		cBuf -= nRet;
		cRecv += nRet;
		pTail += nRet;
		*pTail = 0;

		timeout = 1; // change timeout to 2 seconds for subsequent calls
		setsockopt( socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(int) );

	}
	*recvsize = cRecv;
	return cBuf;
}
////////////////////////////////////////////////////////////////////////////
//
int TcpTransact( char *pszHost, int nPortNbr
				, char *pszMessage
				, char *pszResponse, int *pcbResponse
				, int nTimeout
				)
{
	SOCKET		socket;
	SOCKADDR_IN	sa;
	int			nRc;

	socket = connect_tcp( pszHost, nPortNbr, &sa );
	if ( socket == SOCKET_ERROR )
		return SOCKET_ERROR;

	nRc = socket_transact( socket
						, pszMessage, (int)strlen(pszMessage)
						, pszResponse, pcbResponse
						, nTimeout );
	close_tcp( &socket );
	
	return nRc;
}
////////////////////////////////////////////////////////////////////////////
//
int FormatRequest( char *pszBuffer )
{
	int		cch;
	if ( gfUseProxy )
	{
		cch = sprintf( pszBuffer
					, "HEAD %s%s HTTP/1.0\r\n"
					  "Accept: text/*\r\n"
					  "User-Agent: inettime-tool\r\n"
					  "Content-Type:  application/x-www-form-urlencoded\r\n"
					  "Host: %s\r\n"
					  "Pragma: no-cache\r\n"
					  "Proxy-Connection: Keep-Alive\r\n"
					  "\r\n"
			, STR_HTTP, STR_URL
			, STR_WEBSRV
			);
	}
	else
	{
		cch = sprintf( pszBuffer
					, "HEAD %s HTTP/1.0\r\n"
					  "Accept: text/*\r\n"
					  "User-Agent: inettime-tool\r\n"
					  "Content-Type:  application/x-www-form-urlencoded\r\n"
					  "Host: %s\r\n"
					  "Pragma: no-cache\r\n"
					  "Proxy-Connection: Keep-Alive\r\n"
					  "\r\n"
			, STR_URI, STR_WEBSRV
			);
	}
	return cch;
}
////////////////////////////////////////////////////////////////////////////
//
bool SetProxyAddress( char *pszProxy ) // xxx.yyy.zzz.www:port
{
	char	*pchPort;
	char	szProxy[64];

	strcpy( szProxy, pszProxy );

	if ( ( pchPort = strchr( szProxy, ':' ) ) )
	{
		*pchPort++ = 0;
		gnProxyPort = atoi(pchPort);
	}
	else
	{
		printf( "No proxy port specified - assuming default (%d)\n", gnProxyPort );
	}

	strcpy( gszProxyAddress, szProxy );

	gfUseProxy = true;

	return TRUE;
}
////////////////////////////////////////////////////////////////////////////
//
bool SetTimeout( char *pszTimeout )
{
	char	*pch;
	int		nDef = gnTimeout;

	if ( ( pch = strchr( pszTimeout, ':' ) ) )
	{
		if ( strlen(pch) < 1 )
			return false;
		gnTimeout = atoi(pch+1);
	}
	else
	{
		gnTimeout = atoi(pszTimeout);
	}

	if ( gnTimeout > 0 && gnTimeout < 1000  )
	{
		int	ms = gnTimeout*1000;
		printf( "looks like timeout is specified in seconds (%d) - converting to milliseconds (%d)\n"
			, gnTimeout, ms );
		gnTimeout = ms;
	}
	else
	if ( gnTimeout < 0 || gnTimeout > 60000 )
	{
		printf( "invalid timeout specified (%d) - assuming default %d ms\n"
			, gnTimeout, nDef );
		gnTimeout = nDef;
	}

	return true;
}
////////////////////////////////////////////////////////////////////////////
//
char * DateToString( SYSTEMTIME& st, char *pszDate, int cchDate )
{
	int	cch;
#ifdef WIN32
	cch = GetDateFormat( LOCALE_SYSTEM_DEFAULT, DATE_SHORTDATE, &st, 0, pszDate, cchDate );
	strcat( pszDate, " " );
	GetTimeFormat( LOCALE_SYSTEM_DEFAULT, LOCALE_NOUSEROVERRIDE, &st, 0, pszDate+cch, cchDate-cch );
#else
	cchDate = sprintf( pszDate, "%04d-%02d-%02d %02d:%02d:%02d"
			, st.wYear, st.wMonth, st.wDay
			, st.wHour, st.wMinute, st.wSecond 
			);

#endif	
	return pszDate;
}

void ShowSystemTime( void )
{
	SYSTEMTIME	stCur, stCurL;
	char	szBuf[64];
	char	szBuf2[64];
	GetSystemTime( &stCur );
	GetLocalTime( &stCurL );
	printf( "Current System Time\n"
			"%s (UTC)\n"
			"%s (local)\n"
			, DateToString( stCur, szBuf, sizeof(szBuf) )
			, DateToString( stCurL, szBuf2, sizeof(szBuf2) )
			);
}

void AdjustClock( SYSTEMTIME& st )
{
	SYSTEMTIME	stCur;
	char		szCur[ 64 ];
	char		szNew[ 64 ];

	GetSystemTime( &stCur );
	DateToString( stCur, szCur, sizeof(szCur) );

	printf( "\nBefore adjusting\n" );
	ShowSystemTime();

	DateToString( st, szNew, sizeof(szNew) );

	if ( !SetSystemTime( &st ) )
	{
#ifdef WIN32
		const char	*pszArgs[] = { szCur, szNew };
		sprintf( szCur, "%d", GetLastError() );
		ReportEvent( hEventLog, EVENTLOG_ERROR_TYPE, 0, MSG_E_SETTIME
					, 0, DIM(pszArgs), 0, pszArgs, 0 );
#endif
#if (defined __LINUX__) || (defined __FREEBSD__)
		syslog( LOG_ERR, "Error %d calling SetSystemTime() API\n", GetLastError() );
#endif
		printf( "Error %d calling SetSystemTime() API\n", GetLastError() );
	}
	else
	{
#ifdef WIN32
		const char	*pszArgs[] = { szCur, szNew };
		ReportEvent( hEventLog, EVENTLOG_INFORMATION_TYPE, 0, MSG_I_TIMESET
					, 0, DIM(pszArgs), 0, pszArgs, 0 );
#endif
#if (defined __LINUX__) || (defined __FREEBSD__)
		syslog( LOG_INFO, "System time adjusted from %s to %s. (UTC time)"
				, szCur, szNew );
#endif
		printf( "\nSystem clock adjusted.\n" );
		ShowSystemTime();
	}
}

int GetNextVal( char **ppch )
{
	char	*pch = *ppch;
	char	szVal[ 32 ];
	int		n = 0;
	while( *pch && !isdigit(*pch) )
		pch++;

	while( isdigit(*pch) )
		szVal[n++] = *pch++;

	szVal[n] = 0;
	*ppch = pch;
	return atoi( szVal );
}

char *GetNextDigits( char *pszBuf, WORD *pnValue )
{
	char	*p = pszBuf;

	// find next digit
	while( *p && !isdigit(*p) )
		p++;
	if ( !*p )
		return 0;

	// conv
	*pnValue = (WORD)atoi(p);

	// advance past digits
	while( *p && isdigit(*p) )
		p++;
	if ( !*p )
		return 0;

	return p;
}

bool WebTimeToSystemTime( SYSTEMTIME *pst, char *pchWebTime )
{
	char	*psz = pchWebTime;

	//Aug. 22, 2000,   16:56:21
	//1234567890123456789012345

	//04 Sep 2003 19:48:19 GMT
	//1234567890123456789012345

	//July 05, 22:39:16
	//1234567890123456789012345

	memset( pst, 0, sizeof(SYSTEMTIME) );

	// month or day first?
	int n;
	for( n = 0; n < 2; n++ )
	{
		if ( isdigit(*psz) )
		{
			if ( !( psz = GetNextDigits( psz, &(pst->wDay) ) ) )
				return false;
			while( *psz && isspace(*psz) )
				psz++;
			if ( !*psz )
				return false;
		}
		else
		{
			pst->wMonth = ConvMonthToInt( psz );
			// advance past month to next digit
			//psz += 4;
			//while( *psz && isspace(*psz) )
			//	psz++;
			while( *psz && !isdigit(*psz) )
				psz++;
			if ( !*psz || !isdigit(*psz) )
				return false;
		}
	}
	// day
	//if ( !( psz = GetNextDigits( psz, &(pst->wDay) ) ) )
	//	return false;

	// year
	if ( !( psz = GetNextDigits( psz, &(pst->wYear) ) ) )
		return false;

	// prev was not year, but hour
	if ( *psz == ':' )
	{
		pst->wHour = pst->wYear;

		// get year (assuming current year is OK)
		SYSTEMTIME	st;
		GetSystemTime( &st );
		pst->wYear = st.wYear;
	}
	else
	{
		// hour
		if ( !( psz = GetNextDigits( psz, &(pst->wHour) ) ) )
			return false;
	}
	// minute
	if ( !( psz = GetNextDigits( psz, &(pst->wMinute) ) ) )
		return false;

	// seconds
	if ( !( psz = GetNextDigits( psz, &(pst->wSecond) ) ) )
		return false;

	return true;
}

bool ParseHtml( char *pszBuf )
{
	if ( gfVerbose )
		printf( "HTTP Response:\n%s\n", pszBuf );

// <B> Aug. 22, 2000,   16:56:21   Universal    Time    
// <BR><BR>July 05, 22:39:16 UTC
// <BR><BR>July 05, 22:39:16 U

//	Date: Thu, 04 Sep 2003 19:48:19 GMT

	char	*pszSearchString = "Date:";
	char	*pch = strstr( pszBuf, pszSearchString );

	if ( !pch )
	{
#ifdef WIN32
		const char	*pszArgs[] = { pszUrl, pszSearchString };
		ReportEvent( hEventLog, EVENTLOG_ERROR_TYPE, 0, MSG_E_PARSEHTML
					, 0, DIM(pszArgs), 0, pszArgs, 0 );
#endif
#if (defined __LINUX__) || (defined __FREEBSD__)
		syslog( LOG_ERR, "Cannot find keyword \"%s\" in HTML response", pszSearchString );
#endif
		printf( "Cannot find keyword \"%s\" in HTML response\n", pszSearchString );
		return false;
	}

#if 0
	while( pch > pszBuf && *pch != '>' )
		pch--;

	//pch += 2;
	pch++;
	while( isspace(*pch) )
		pch++;
#else
	pch += strlen(pszSearchString);
	while( isspace(*pch) )
		pch++;
	while( !isdigit(*pch) )
		pch++;
#endif
	//Nov. 1, 2000,   17:36:42
	//Aug. 22, 2000,   16:56:21
	//Thu, 04 Sep 2003 19:48:19 GMT
	//1234567890123456789012345

	if ( gfVerbose )
		printf( "Time: %25.25s\n", pch );
	SYSTEMTIME		st;
	WebTimeToSystemTime( &st, pch );

	pch = strstr( pszBuf, "<H" );
	if ( pch )
	{
		while( *pch != '>' )
			pch++;
		pch++;
		while( isspace(*pch) )
			pch++;
		char *pchEnd = strstr( pch, "</H" );
		int	cch = (int)(pchEnd-pch);
		printf( "\nCorrect UTC Time accoring to %*.*s\n", cch, cch, pch );
	}

	char	szBuf[64];
	printf("%s\n", DateToString( st, szBuf, sizeof(szBuf) ) );

	if ( fSetSystemTime )
	{
		 AdjustClock( st );
	}
	else
	{
		ShowSystemTime();
	}
	return true;
}

#ifdef WIN32
void RegEventSource( void )
{
	char		szKey[ 256 ];
	HKEY		hKey;
	DWORD		dwDisp = 0;

	sprintf( szKey, "SYSTEM\\CurrentControlSet\\Services\\Eventlog\\Application\\%s", STR_EVENTSOURCE );

	if ( ERROR_SUCCESS == RegCreateKeyEx( HKEY_LOCAL_MACHINE, szKey, 0, 0
										, REG_OPTION_NON_VOLATILE
										, KEY_ALL_ACCESS, 0, &hKey, &dwDisp ) )
	{
		if ( dwDisp == REG_CREATED_NEW_KEY )
		{
			char		szModuleName[ FILENAME_MAX ];
			DWORD		dwTypesSupported = 7;

			GetModuleFileName( 0, szModuleName, DIM(szModuleName) );
			RegSetValueEx( hKey, "EventMessageFile", 0, REG_SZ, (const BYTE*)szModuleName, (DWORD)strlen(szModuleName) );
			RegSetValueEx( hKey, "TypesSupported", 0, REG_DWORD, (const BYTE*)&dwTypesSupported, sizeof(dwTypesSupported) );
		}
		RegCloseKey( hKey );
	}
}
#endif

int main(int argc, char* argv[])
{
	int		n;
	char	chBuf[ MAX_BUFSIZE + 1];

	//if ( argc >= 3 )
	//	pszProxy = argv[2];

	for( n = 1; n < argc; n++ )
	{
		if ( (*argv[n] == '-' || *argv[n] == '/' ) )
		{
			if ( !strnicmp(argv[n]+1, "Proxy:", 6 ) )
				SetProxyAddress( argv[n]+7 );
			else
			if ( !strnicmp(argv[n]+1, "timeout:", 8 ) )
				SetTimeout( argv[n] );
			else
			if ( toupper(*(argv[n]+1)) == 'V' ) 
				gfVerbose = true;
			else
			if ( toupper(*(argv[n]+1)) == 'S' ) 
				fSetSystemTime = true;
			else
			if ( *(argv[1]+1) == '?' ) 
			{
				printf( "syntax: inettime [/S] [/Proxy:xxx.yyy.zzz.www:port]\n\n"
					    "Gets Coordinated Universal Time (UTC) Time from US Navy Web site\n\n"
						"url: %s\n"
						"\n"
						"/S switch sets the UTC system time on this machine\n"
						"\n"
						"/Proxy switch sets an address to a proxy server\n"
						"\n"
						"/Timeout switch the HTTP call timeout (default 5 seconds) \n"
						"\n"
						"Note!\n"
						"You must have internet access on the user account running this program\n"
						"and you must have proper OS priviledge to adjust the computers clock!\n"
						, pszUrl
						);
				return 0;
			}
		}
	}

	Startup();

	int	cchResp = DIM(chBuf);
	int	rc = 0;

	memset( chBuf, 0, DIM(chBuf) );
	FormatRequest( chBuf );

	if ( gfUseProxy )
	{
		printf(  "Opening url: %s\n"
				  "using proxy %s:%d\n"
				, pszUrl
				, gszProxyAddress, gnProxyPort
				);
		rc = TcpTransact( gszProxyAddress, gnProxyPort, chBuf, chBuf, &cchResp, gnTimeout );
	}
	else
	{
		printf( "Opening url: %s\n", pszUrl );
		rc = TcpTransact( STR_WEBSRV, 80, chBuf, chBuf, &cchResp, gnTimeout );
	}

	if ( rc == SOCKET_ERROR )
	{
		 printf( "Error opening url\n" );
		 rc = 1;
	}
	else 
	{
		if ( !ParseHtml( chBuf ) )
			 rc = 2;
		else rc = 0;
	}

	Shutdown();

	return rc;
}

