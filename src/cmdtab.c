/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    cmdtab.c
* module function:
    Table of vi-mode commands.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"
#include "cmd.h"

VI_COMMAND	cmd_types[NUM_VI_CMDS] = {
 /* ^@ */	do_badcmd,	0,
 /* ^A */	do_badcmd,	0,
 /* ^B */	do_page,	COMMAND,
 /* ^C */	do_badcmd,	0,
 /* ^D */	do_scroll,	COMMAND,
 /* ^E */	do_scroll,	COMMAND,
 /* ^F */	do_page,	COMMAND,
 /* ^G */	do_cmd,		COMMAND,
 /* ^H */	do_left_right,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL,
 /* ^I */	do_badcmd,	0,
 /* ^J */	do_target,	COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE,
 /* ^K */	do_badcmd,	0,
 /* ^L */	do_cmd,		COMMAND,
 /* ^M */	do_target,	COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE | SET_CURSOR_COL,
 /* ^N */	do_target,	COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE,
 /* ^O */	do_cmd,		COMMAND,
 /* ^P */	do_target,	COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE,
 /* ^Q */	do_badcmd,	0,
 /* ^R */	do_cmd,		COMMAND,
 /* ^S */	do_badcmd,	0,
 /* ^T */	do_cmd,		COMMAND,
 /* ^U */	do_scroll,	COMMAND,
 /* ^V */	do_badcmd,	0,
 /* ^W */	do_cmd,		COMMAND,
 /* ^X */	do_badcmd,	0,
 /* ^Y */	do_scroll,	COMMAND,
 /* ^Z */	do_cmd,		COMMAND,
 /* ESCAPE */	do_cmd,		COMMAND,
 /* ^\ */	do_badcmd,	0,
 /* ^] */	do_cmd,		COMMAND,
 /* ^^ */	do_cmd,		COMMAND,
 /* ^_ */	do_rchar,	COMMAND,
 /* space */	do_left_right,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL,
 /* ! */	do_operator,	COMMAND,
 /* " */	do_quote,	COMMAND | TWO_CHAR,
 /* # */	do_badcmd,	0,
 /* $ */	do_target,	COMMAND | TARGET | TGT_CHAR | TGT_INCLUSIVE,
 /* % */	do_target,	COMMAND | TARGET | TGT_CHAR | TGT_INCLUSIVE
					| LONG_DISTANCE | SET_CURSOR_COL,
 /* & */	do_cmd,		COMMAND,
 /* ' */	do_target,	COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE
					| TWO_CHAR | LONG_DISTANCE | SET_CURSOR_COL,
 /* ( */	do_target,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE
					| LONG_DISTANCE | SET_CURSOR_COL,
 /* ) */	do_target,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE
					| LONG_DISTANCE | SET_CURSOR_COL,
 /* * */	do_badcmd,	0,
 /* + */	do_target,	COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE | SET_CURSOR_COL,
 /* , */	do_csearch,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL,
 /* - */	do_target,	COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE | SET_CURSOR_COL,
 /* . */	do_cmd,		COMMAND,
 /* / */	do_target,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE
 					| TGT_DEFERRED,
 /* 0 */	do_target,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL,
 /* 1 */	do_badcmd,	0,
 /* 2 */	do_badcmd,	0,
 /* 3 */	do_badcmd,	0,
 /* 4 */	do_badcmd,	0,
 /* 5 */	do_badcmd,	0,
 /* 6 */	do_badcmd,	0,
 /* 7 */	do_badcmd,	0,
 /* 8 */	do_badcmd,	0,
 /* 9 */	do_badcmd,	0,
 /* : */	do_cmd,		COMMAND,
 /* ; */	do_csearch,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL,
 /* < */	do_operator,	COMMAND,
 /* = */	do_badcmd,	0,
 /* > */	do_operator,	COMMAND,
 /* ? */	do_target,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE
 					| TGT_DEFERRED,
 /* @ */	do_cmd,		COMMAND | TWO_CHAR,
 /* A */	do_ins,		COMMAND,
 /* B */	do_word,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL,
 /* C */	do_cmd,		COMMAND,
 /* D */	do_cmd,		COMMAND,
 /* E */	do_word,	COMMAND | TARGET | TGT_CHAR | TGT_INCLUSIVE | SET_CURSOR_COL,
 /* F */	do_csearch,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE
					| TWO_CHAR | SET_CURSOR_COL,
 /* G */	do_target,	COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE
					| LONG_DISTANCE | SET_CURSOR_COL,
 /* H */	do_HLM,		COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE | SET_CURSOR_COL,
 /* I */	do_ins,		COMMAND,
 /* J */	do_cmd,		COMMAND,
 /* K */	do_badcmd,	0,
 /* L */	do_HLM,		COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE | SET_CURSOR_COL,
 /* M */	do_HLM,		COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE | SET_CURSOR_COL,
 /* N */	do_target,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL,
 /* O */	do_ins,		COMMAND,
 /* P */	do_cmd,		COMMAND,
 /* Q */	do_badcmd,	0,
 /* R */	do_cmd,		COMMAND,
 /* S */	do_cmd,		COMMAND,
 /* T */	do_csearch,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE
					| TWO_CHAR | SET_CURSOR_COL,
 /* U */	do_badcmd,	0,
 /* V */	do_badcmd,	0,
 /* W */	do_word,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL,
 /* X */	do_x,		COMMAND,
 /* Y */	do_cmd,		COMMAND,
 /* Z */	do_cmd,		COMMAND | TWO_CHAR,
 /* [ */	do_target,	COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE
					| TWO_CHAR | LONG_DISTANCE | SET_CURSOR_COL,
 /* \ */	do_badcmd,	0,
 /* ] */	do_target,	COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE
					| TWO_CHAR | LONG_DISTANCE | SET_CURSOR_COL,
 /* ^ */	do_target,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL,
 /* _ */	do_target,	COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE,
 /* ` */	do_target,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE
					| TWO_CHAR | LONG_DISTANCE,
 /* a */	do_ins,		COMMAND,
 /* b */	do_word,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL,
 /* c */	do_operator,	COMMAND,
 /* d */	do_operator,	COMMAND,
 /* e */	do_word,	COMMAND | TARGET | TGT_CHAR | TGT_INCLUSIVE | SET_CURSOR_COL,
 /* f */	do_csearch,	COMMAND | TARGET | TGT_CHAR | TGT_INCLUSIVE
					| TWO_CHAR | SET_CURSOR_COL,
 /* g */	do_cmd,		COMMAND,
 /* h */	do_left_right,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL,
 /* i */	do_ins,		COMMAND,
 /* j */	do_target,	COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE,
 /* k */	do_target,	COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE,
 /* l */	do_left_right,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL,
 /* m */	do_cmd,		COMMAND | TWO_CHAR,
 /* n */	do_target,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL,
 /* o */	do_ins,		COMMAND,
 /* p */	do_cmd,		COMMAND,
 /* q */	do_badcmd,	0,
 /* r */	do_cmd,		COMMAND,
 /* s */	do_cmd,		COMMAND,
 /* t */	do_csearch,	COMMAND | TARGET | TGT_CHAR | TGT_INCLUSIVE
					| TWO_CHAR | SET_CURSOR_COL,
 /* u */	do_cmd,		COMMAND,
 /* v */	do_badcmd,	0,
 /* w */	do_word,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL,
 /* x */	do_x,		COMMAND,
 /* y */	do_operator,	COMMAND,
 /* z */	do_z,		COMMAND | TWO_CHAR,
 /* { */	do_target,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE
					| LONG_DISTANCE | SET_CURSOR_COL,
 /* | */	do_target,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE,
 /* } */	do_target,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE
					| LONG_DISTANCE | SET_CURSOR_COL,
 /* ~ */	do_rchar,	COMMAND,
 /* DEL */	do_badcmd,	0,
 /* K_HELP */	do_cmd,		COMMAND,
 /* K_UNDO */	do_cmd,		COMMAND,
 /* K_INSERT */	do_ins,		COMMAND,
 /* K_HOME */	do_HLM,		COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE,
 /* K_UARROW */	do_target,	COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE,
 /* K_DARROW */	do_target,	COMMAND | TARGET | TGT_LINE | TGT_INCLUSIVE,
 /* K_LARROW */	do_left_right,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL,
 /* K_RARROW */	do_left_right,	COMMAND | TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL,
 /* K_CGRAVE */	do_cmd,		COMMAND,
};
