DEFS = -D__LINUX__ -DLINUX -DNOWIN32
INC = -I.
CFLAGS = -W -Wno-write-strings -Wunused-value

all:	app

inettime.o: inettime.cpp
	cc $(DEFS) $(INC) $(CFLAGS) inettime.cpp -o inettime
	chmod +x inettime

app: inettime.o