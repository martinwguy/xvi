/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    cmd.h
* module function:
    Definitions related to the vi-mode command table.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

/*
 * Values for the flags field in the Cmd and VI_COMMAND structures.
 *
 * If TARGET is set, the command may be used as the target for one of
 * the operators (e.g. 'c'); the default is that targets are character-
 * based unless TGT_LINE is set in which case they are line-based.
 * Similarly, the default is that targets are exclusive, unless the
 * TGT_INCLUSIVE flag is set.
 *
 * Note that there are currently no line-based exclusive targets.
 */
#define		COMMAND		0x1		/* is implemented */
#define		TARGET		0x2		/* can serve as a target */
#define		TGT_LINE	0x4		/* a line-based target */
#define		TGT_CHAR	0		/* a char-based target */
#define		TGT_INCLUSIVE	0x8		/* an inclusive target */
#define		TGT_EXCLUSIVE	0		/* an exclusive target */
#define		TGT_DEFERRED	0x10		/* deferred, e.g. / or ? */
#define		TWO_CHAR	0x20		/* a two-character command */
#define		LONG_DISTANCE	0x40		/* moves cursor a long way */
#define		SET_CURSOR_COL	0x80		/* keep cursor at this column */

#define	IsLineBased(cmd)	((((cmd)->cmd_flags) & TGT_LINE) != 0)
#define	IsCharBased(cmd)	((((cmd)->cmd_flags) & TGT_LINE) == 0)
#define	IsInclusive(cmd)	((((cmd)->cmd_flags) & TGT_INCLUSIVE) != 0)
#define	IsExclusive(cmd)	((((cmd)->cmd_flags) & TGT_INCLUSIVE) == 0)

#define	MakeLineBased(cmd)	(((cmd)->cmd_flags) |= TGT_LINE)
#define	MakeCharBased(cmd)	(((cmd)->cmd_flags) &= ~TGT_LINE)
#define	MakeExclusive(cmd)	(((cmd)->cmd_flags) &= ~TGT_INCLUSIVE)
#define	MakeInclusive(cmd)	(((cmd)->cmd_flags) |= TGT_INCLUSIVE)

typedef struct vi_command {
    void		(*c_func) P((Cmd *));
    unsigned char	c_flags;
} VI_COMMAND;

#define	NUM_VI_CMDS	256
#define	MAX_CMD		(NUM_VI_CMDS - 1)
#define CFLAGS(c)	(((c) > MAX_CMD) ? 0 : cmd_types[c].c_flags)
#define CFUNC(c)	(((c) > MAX_CMD) ? do_badcmd : cmd_types[c].c_func)

extern	VI_COMMAND	cmd_types[NUM_VI_CMDS];
