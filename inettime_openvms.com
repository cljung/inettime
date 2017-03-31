$! *
$! * This is a build script in DCL targeted for the OpenVMS environment
$! *
$! * Since I don't want to rely on MMS being installed, the build 
$! * procedure is a simple DCL script
$! *
$! * Run it by typing the commands
$! *
$! *   prompt>set def <instdir>
$! *   prompt>@inettime_openvms
$! *
$! * Test it by typing
$! *
$! *   prompt>set def <instdir>
$! *   prompt>inettime:==$<instdir>:inettime.exe
$! *   prompt>inettime -v -proxy:x.y.z.w:port -timeout:10
$! *
$!
$Initialization:
$ SET NOON
$!
$ Echo		= "Write Sys$Output"     ! Print a line to the user
$ EchoBig	= "Type  Sys$Input"      ! Print multiple line output
$ Invoker_message = f$environment("MESSAGE")
$ Current_default = F$ENVIRONMENT ("DEFAULT")
$!
$ projname = "inettime"
$ objfiles = "inettime.OBJ"
$ linklibs = "TCPIP$LIBRARY:TCPIP$LIB/lib"
$ Include_directories = "[]"
$ Define_list = "__VMS__"
$ dbg_flag = "NDEBUG"   ! else _DEBUG
$ dbg_switch = ""       ! else "/DEBUG"
$ strCppStep = "N"
$ strLinkStep = "N"
$!
$! *****************************************************************
$!
$! Build common files:  If building any or all of the above options, always
$! compiler the IDL file.
$!
$Compile_component:
$ ECHO "[Compiling]"
$ cc_dir = ""
$ cc_fil = "inettime"
$ gosub Compile_CC
$!
$ goto LINK_COMPONENT
$!
$! *****************************************************************
$!
$! Link debug and non-debug version of the out-of-process server.
$!
$LINK_COMPONENT:
$ ECHO "[Linking] ''projname'.EXE"
$ CXXLINK/EXE='projname'.EXE -
  'objfiles',-
  'linklibs'
$ IF (F$SEARCH("''projname'.EXE") .NES. "") THEN $ SET PROT=W:RE 'projname'.EXE;0
$!
$! *****************************************************************
$Exit:
$ pur *.*
$ Set message'invoker_message
$ EXIT
$!
$! *****************************************************************
$Compile_CC:
$ tid = f$edit(f$time(),"trim,compress")
$ ECHO "C++ compiling ''cc_dir'''cc_fil'.cpp..."
$ CXX/INCLUDE=('Include_directories')/DEFINE=('Define_list','dbg_flag') -
  /EXCEPTIONS=CLEANUP/STANDARD=(CFRONT)'dbg_switch'/NOOPT 'cc_dir''cc_fil'.CPP -
  /OBJECT='cc_fil'.OBJ
$ pur 'cc_fil'.obj
$ return
$!
