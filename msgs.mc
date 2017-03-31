;#ifndef _INC_INETTIME_MSGS_
;#define _INC_INETTIME_MSGS_

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
				Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
				Warning=0x2:STATUS_SEVERITY_WARNING
				Error=0x3:STATUS_SEVERITY_ERROR
				)

;//LanguageNames=(English=0x0009:MSG00001 )

MessageID=5001
Severity=Informational
SymbolicName=MSG_I_TIMESET
Language=English
System time adjusted
%n
%nfrom%t%1
%nto%t%2.
%n
%nTime is expressed in Universal Time Coordinate.
.
MessageID=5002
Severity=Error
SymbolicName=MSG_E_INTERNETOPEN
Language=English
Error opening preconfigured internet connection.
.
MessageID=5003
Severity=Error
SymbolicName=MSG_E_INTERNETOPENURL
Language=English
Error opening opening url %1.
.
MessageID=5004
Severity=Error
SymbolicName=MSG_E_PARSEHTML
Language=English
Error parsing HTML for UTC Time in url %1.
%nExcpected keyword %2 not found.
.
MessageID=5005
Severity=Error
SymbolicName=MSG_E_SETTIME
Language=English
Error %1 calling Win32 API SetSystemTime( "%2" )
.
MessageID=5006
Severity=Error
SymbolicName=MSG_E_SOCKET
Language=English
Error opening url %1.
%n
Ret.code %2 calling winsock api %3
.
;#endif // _INC_INETTIME_MSGS_