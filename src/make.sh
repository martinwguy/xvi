# Copyright (c) 1992,1993 Chris and John Downey
#
# program name:
#	xvi
# function:
#	Portable version of UNIX "vi" editor, with extensions.
# module name:
#	make.sh
# module function:
#	Shell script to build for POSIX when you don't have "make"
# history:
#	STEVIE - ST Editor for VI Enthusiasts, Version 3.10
#	Originally by Tim Thompson (twitch!tjt)
#	Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
#	Heavily modified by Chris & John Downey
#	Modified by Martin Guy <martinwguy@gmail.com>

CC=cc

SYSDEFS="-DUNIX -DTERMIOS -DPOSIX"
INCDIRS=

LIBS=-ltermcap
LDFLAGS=

OPTFLAG=-O
CFLAGS="$SYSDEFS $INCDIRS $OPTFLAG"

MACHSRC="unix.c tcapmain.c tcap_scr.c"
MACHOBJ="unix.o tcapmain.o tcap_scr.o"
MACHINC="unix.h termcap.h"

GENINC="ascii.h change.h cmd.h param.h ptrfunc.h regexp.h regmagic.h \
		virtscr.h xvi.h"

GENSRC="alloc.c altstack.c ascii.c buffers.c \
		cmdline.c cmdmode.c cmdtab.c cursor.c dispmode.c \
		edit.c ex_cmds1.c ex_cmds2.c events.c \
		fileio.c find.c flexbuf.c \
		map.c mark.c misccmds.c mouse.c movement.c normal.c \
		param.c pipe.c preserve.c ptrfunc.c regexp.c \
		screen.c search.c signal.c startup.c status.c \
		tag.c targets.c undo.c update.c \
		version.c vi_cmds.c vi_ops.c virtscr.c \
		windows.c yankput.c"

GENOBJ="alloc.o altstack.o ascii.o buffers.o \
		cmdline.o cmdmode.o cmdtab.o cursor.o dispmode.o \
		edit.o ex_cmds1.o ex_cmds2.o events.o \
		fileio.o find.o flexbuf.o \
		map.o mark.o misccmds.o mouse.o movement.o normal.o \
		param.o pipe.o preserve.o ptrfunc.o regexp.o \
		screen.o search.o signal.o startup.o status.o \
		tag.o targets.o undo.o update.o \
		version.o vi_cmds.o vi_ops.o virtscr.o \
		windows.o yankput.o"

set -x

for c in $GENSRC $MACHSRC
do
	$CC $CFLAGS $INCDIRS -c $c
done

$CC $CFLAGS $LDFLAGS -o xvi $GENOBJ $MACHOBJ $LIBS
