APPNAME=inettime
# remove -@echo when you tested that the makefile works
CC = gcc
LINK = gcc
# add  -D_REENTRANT as define for reentrant code (pthread uses)
DEFS = -D__LINUX__ -DLINUX
# -fPIC -- Position-Independant-Code (.so targets)
#CFLAGS=-Wall
CFLAGS=-W
OUTDIR=./debug
INTDIR=./debug
OBJ=.o
DLL=.so
EXENAME=-o $(OUTDIR)/$(APPNAME)
INC =
LINKLIBS = -L. -ldl -lpthread

NULL=nul

all: linker

#--------------------------------------------------------------------------
#
inettime: $(OUTDIR)/inettime$(OBJ)

# msgs.h is a Windows only file, so just create a dummy one here
msgs.h:
	touch msgs.h

$(OUTDIR)/inettime$(OBJ): $(OUTDIR) inettime.cpp msgs.h
	$(CC) $(DEFS) $(INC) $(CFLAGS) -c inettime.cpp -o $(OUTDIR)/inettime$(OBJ)

#--------------------------------------------------------------------------
#
$(OUTDIR):
	-@mkdir $(OUTDIR)

#--------------------------------------------------------------------------
#
linker: $(OUTDIR) $(OUTDIR)/inettime$(OBJ)

#	link the target

	$(LINK) $(OUTDIR)/inettime$(OBJ) \
	$(LINKLIBS) -o $(OUTDIR)/$(APPNAME)
	cp $(OUTDIR)/$(APPNAME) $(APPNAME)
	-@echo Build completed!

#--------------------------------------------------------------------------
#
clean:
	-@echo clean - removing temporary files and directories...
	rm -f $(OUTDIR)/*$(OBJ)
	rm -f $(OUTDIR)/$(APPNAME)
	rmdir $(OUTDIR)
