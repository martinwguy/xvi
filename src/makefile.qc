# Copyright (c) 1990,1991,1992,1993 Chris and John Downey
#***
#
# @(#)makefile.qc	2.14 (Chris & John Downey) 7/27/93
#
# program name:
#	xvi
# function:
#	Portable version of UNIX "vi" editor, with extensions.
# module name:
#	makefile.qc
# module function:
#	Makefile for MS-DOS, using Microsoft Quick C (standalone
#	version) & Microsoft's make utility.
#
#	Microsoft Macro Assembler (or a compatible assembler) is also
#	required.
# history:
#	STEVIE - ST Editor for VI Enthusiasts, Version 3.10
#	Originally by Tim Thompson (twitch!tjt)
#	Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
#	Heavily modified by Chris & John Downey
#***

#
# Name of this file.
#
THISFILE=	makefile.qc
MEMMODEL=	L
CFLAGS=		-A$(MEMMODEL) -D__STDC__=1 -Ol
LDFLAGS=	/NOI /NOE /STACK:20480
LINKFILE=	xvi.lnk

MACHSRC=	defmain.c defscr.c \
		ibmpc_c.c msdos_c.c ibmpc_a.c msdos_a.c
MACHOBJ=	defmain.obj defscr.obj \
		ibmpc_c.obj msdos_c.obj ibmpc_a.obj msdos_a.obj
MACHINC=	msdos.h ibmpc.h 8086mm.inc

GENINC=		ascii.h change.h param.h ptrfunc.h regexp.h regmagic.h xvi.h \
		virtscr.h

GENSRC=		alloc.c altstack.c ascii.c buffers.c \
		cmdline.c cmdmode.c cmdtab.c cursor.c dispmode.c \
		edit.c ex_cmds1.c ex_cmds2.c events.c \
		fileio.c find.c flexbuf.c \
		map.c mark.c misccmds.c mouse.c movement.c \
		normal.c param.c pipe.c preserve.c ptrfunc.c \
		regexp.c screen.c search.c startup.c status.c \
		tags.c targets.c undo.c update.c \
		version.c vi_cmds.c vi_ops.c virtscr.c \
		windows.c yankput.c

GENOBJ=		alloc.obj altstack.obj ascii.obj buffers.obj \
		cmdline.obj cmdmode.obj cmdtab.obj cursor.obj dispmode.obj \
		edit.obj ex_cmds1.obj ex_cmds2.obj events.obj \
		fileio.obj find.obj flexbuf.obj \
		map.obj mark.obj misccmds.obj mouse.obj movement.obj \
		normal.obj param.obj pipe.obj preserve.obj ptrfunc.obj \
		regexp.obj screen.obj search.obj startup.obj status.obj \
		tags.obj targets.obj undo.obj update.obj \
		version.obj vi_cmds.obj vi_ops.obj virtscr.obj \
		windows.obj yankput.obj

.c.obj:
		qcl -c $(CFLAGS) $*.c

.asm.obj:
		masm -Mx -DMEMMODEL=$(MEMMODEL) $* ;

defmain.obj:	defmain.c

defscr.obj:	defscr.c

alloc.obj:	alloc.c

altstack.obj:	altstack.c

ascii.obj:	ascii.c

buffers.obj:	buffers.c

cmdline.obj:	cmdline.c

cmdmode.obj:	cmdmode.c

cmdtab.obj:	cmdtab.c

cursor.obj:	cursor.c

dispmode.obj:	dispmode.c

edit.obj:	edit.c

ex_cmds1.obj:	ex_cmds1.c

ex_cmds2.obj:	ex_cmds2.c

events.obj:	events.c

fileio.obj:	fileio.c

find.obj:	find.c

flexbuf.obj:	flexbuf.c

ibmpc_c.obj:	ibmpc_c.c

map.obj:	map.c

mark.obj:	mark.c

misccmds.obj:	misccmds.c

mouse.obj:	mouse.c

movement.obj:	movement.c

msdos_c.obj:	msdos_c.c

normal.obj:	normal.c

param.obj:	param.c

pipe.obj:	pipe.c

preserve.obj:	preserve.c

ptrfunc.obj:	ptrfunc.c

regexp.obj:	regexp.c

screen.obj:	screen.c

search.obj:	search.c

startup.obj:	startup.c

status.obj:	status.c

tags.obj:	tags.c

targets.obj:	targets.c

undo.obj:	undo.c

update.obj:	update.c

vi_cmds.obj:	vi_cmds.c

vi_ops.obj:	vi_ops.c

virtscr.obj:	virtscr.c

windows.obj:	windows.c

yankput.obj:	yankput.c

ibmpc_a.obj:	ibmpc_a.asm

msdos_a.obj:	msdos_a.asm

$(LINKFILE):	$(THISFILE)
		+echo $(LIB)\c0l + > $@
		+echo defmain + defscr + >> $@
		+echo alloc + altstack + ascii + buffers + cmdline + >> $@
		+echo cmdmode + cmdtab + cursor + dispmode + edit + >> $@
		+echo ex_cmds1 + ex_cmds2 + events + fileio + >> $@
		+echo find + flexbuf + map + mark + misccmds + mouse + >> $@
		+echo movement + normal + param + pipe + preserve + >> $@
		+echo ptrfunc + regexp + screen + search + startup + >> $@
		+echo status + tags + targets + undo + update + version + >> $@
		+echo vi_cmds + vi_ops + virtscr + windows + yankput + >> $@
		+echo msdos_c + msdos_a + ibmpc_a + ibmpc_c + >> $(LINKFILE)
		+echo $(LIB)\setargv >> $(LINKFILE)
		+echo xvi.exe $(LDFLAGS) ; >> $(LINKFILE)

xvi.exe:	$(GENOBJ) $(MACHOBJ) $(LINKFILE)
		qcl -c $(CFLAGS) version.c
		link @$(LINKFILE)
