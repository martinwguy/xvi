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
 { /* ^@ */	do_badcmd,	0 },
 { /* ^A */	do_badcmd,	0 },
 { /* ^B */	do_page,	0 },
 { /* ^C */	do_badcmd,	0 },
 { /* ^D */	do_scroll,	0 },
 { /* ^E */	do_scroll,	0 },
 { /* ^F */	do_page,	0 },
 { /* ^G */	do_cmd,		0 },
 { /* ^H */	do_left_right,	TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL },
 { /* ^I */	do_badcmd,	0 },
 { /* ^J */	do_target,	TARGET | TGT_LINE | TGT_INCLUSIVE },
 { /* ^K */	do_badcmd,	0 },
 { /* ^L */	do_cmd,		0 },
 { /* ^M */	do_target,	TARGET | TGT_LINE | TGT_INCLUSIVE | SET_CURSOR_COL },
 { /* ^N */	do_target,	TARGET | TGT_LINE | TGT_INCLUSIVE },
 { /* ^O */	do_cmd,		0 },
 { /* ^P */	do_target,	TARGET | TGT_LINE | TGT_INCLUSIVE },
 { /* ^Q */	do_badcmd,	0 },
 { /* ^R */	do_cmd,		0 },
 { /* ^S */	do_badcmd,	0 },
 { /* ^T */	do_cmd,		0 },
 { /* ^U */	do_scroll,	0 },
 { /* ^V */	do_badcmd,	0 },
 { /* ^W */	do_cmd,		0 },
 { /* ^X */	do_badcmd,	0 },
 { /* ^Y */	do_scroll,	0 },
 { /* ^Z */	do_cmd,		0 },
 { /* ESCAPE */	do_cmd,		0 },
 { /* ^\ */	do_badcmd,	0 },
 { /* ^] */	do_cmd,		0 },
 { /* ^^ */	do_cmd,		0 },
 { /* ^_ */	do_rchar,	0 },
 { /* space */	do_left_right,	TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL },
 { /* ! */	do_operator,	0 },
 { /* " */	do_quote,	TWO_CHAR },
 { /* # */	do_badcmd,	0 },
 { /* $ */	do_target,	TARGET | TGT_CHAR | TGT_INCLUSIVE },
 { /* % */	do_target,	TARGET | TGT_CHAR | TGT_INCLUSIVE
					| LONG_DISTANCE | SET_CURSOR_COL },
 { /* & */	do_cmd,		0 },
 { /* ' */	do_target,	TARGET | TGT_LINE | TGT_INCLUSIVE
					| TWO_CHAR | LONG_DISTANCE | SET_CURSOR_COL },
 { /* ( */	do_target,	TARGET | TGT_CHAR | TGT_EXCLUSIVE
					| LONG_DISTANCE | SET_CURSOR_COL },
 { /* ) */	do_target,	TARGET | TGT_CHAR | TGT_EXCLUSIVE
					| LONG_DISTANCE | SET_CURSOR_COL },
 { /* * */	do_badcmd,	0 },
 { /* + */	do_target,	TARGET | TGT_LINE | TGT_INCLUSIVE | SET_CURSOR_COL },
 { /* , */	do_csearch,	TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL },
 { /* - */	do_target,	TARGET | TGT_LINE | TGT_INCLUSIVE | SET_CURSOR_COL },
 { /* . */	do_cmd,		0 },
 { /* / */	do_target,	TARGET | TGT_CHAR | TGT_EXCLUSIVE
					| TGT_DEFERRED },
 { /* 0 */	do_target,	TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL },
 { /* 1 */	do_badcmd,	0 },
 { /* 2 */	do_badcmd,	0 },
 { /* 3 */	do_badcmd,	0 },
 { /* 4 */	do_badcmd,	0 },
 { /* 5 */	do_badcmd,	0 },
 { /* 6 */	do_badcmd,	0 },
 { /* 7 */	do_badcmd,	0 },
 { /* 8 */	do_badcmd,	0 },
 { /* 9 */	do_badcmd,	0 },
 { /* : */	do_cmd,		0 },
 { /* ; */	do_csearch,	TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL },
 { /* < */	do_operator,	0 },
 { /* = */	do_badcmd,	0 },
 { /* > */	do_operator,	0 },
 { /* ? */	do_target,	TARGET | TGT_CHAR | TGT_EXCLUSIVE
					| TGT_DEFERRED },
 { /* @ */	do_cmd,		TWO_CHAR },
 { /* A */	do_ins,		0 },
 { /* B */	do_word,	TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL },
 { /* C */	do_cmd,		0 },
 { /* D */	do_cmd,		0 },
 { /* E */	do_word,	TARGET | TGT_CHAR | TGT_INCLUSIVE | SET_CURSOR_COL },
 { /* F */	do_csearch,	TARGET | TGT_CHAR | TGT_EXCLUSIVE
					| TWO_CHAR | SET_CURSOR_COL },
 { /* G */	do_target,	TARGET | TGT_LINE | TGT_INCLUSIVE
					| LONG_DISTANCE | SET_CURSOR_COL },
 { /* H */	do_HLM,		TARGET | TGT_LINE | TGT_INCLUSIVE | SET_CURSOR_COL },
 { /* I */	do_ins,		0 },
 { /* J */	do_cmd,		0 },
 { /* K */	do_badcmd,	0 },
 { /* L */	do_HLM,		TARGET | TGT_LINE | TGT_INCLUSIVE | SET_CURSOR_COL },
 { /* M */	do_HLM,		TARGET | TGT_LINE | TGT_INCLUSIVE | SET_CURSOR_COL },
 { /* N */	do_target,	TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL },
 { /* O */	do_ins,		0 },
 { /* P */	do_cmd,		0 },
 { /* Q */	do_badcmd,	0 },
 { /* R */	do_cmd,		0 },
 { /* S */	do_cmd,		0 },
 { /* T */	do_csearch,	TARGET | TGT_CHAR | TGT_EXCLUSIVE
					| TWO_CHAR | SET_CURSOR_COL },
 { /* U */	do_cmd,		0 },
 { /* V */	do_badcmd,	0 },
 { /* W */	do_word,	TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL },
 { /* X */	do_x,		0 },
 { /* Y */	do_cmd,		0 },
 { /* Z */	do_cmd,		TWO_CHAR },
 { /* [ */	do_target,	TARGET | TGT_LINE | TGT_INCLUSIVE
					| TWO_CHAR | LONG_DISTANCE | SET_CURSOR_COL },
 { /* \ */	do_badcmd,	0 },
 { /* ] */	do_target,	TARGET | TGT_LINE | TGT_INCLUSIVE
					| TWO_CHAR | LONG_DISTANCE | SET_CURSOR_COL },
 { /* ^ */	do_target,	TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL },
 { /* _ */	do_target,	TARGET | TGT_LINE | TGT_INCLUSIVE },
 { /* ` */	do_target,	TARGET | TGT_CHAR | TGT_EXCLUSIVE
					| TWO_CHAR | LONG_DISTANCE },
 { /* a */	do_ins,		0 },
 { /* b */	do_word,	TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL },
 { /* c */	do_operator,	0 },
 { /* d */	do_operator,	0 },
 { /* e */	do_word,	TARGET | TGT_CHAR | TGT_INCLUSIVE | SET_CURSOR_COL },
 { /* f */	do_csearch,	TARGET | TGT_CHAR | TGT_INCLUSIVE
					| TWO_CHAR | SET_CURSOR_COL },
 { /* g */	do_cmd,		0 },
 { /* h */	do_left_right,	TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL },
 { /* i */	do_ins,		0 },
 { /* j */	do_target,	TARGET | TGT_LINE | TGT_INCLUSIVE },
 { /* k */	do_target,	TARGET | TGT_LINE | TGT_INCLUSIVE },
 { /* l */	do_left_right,	TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL },
 { /* m */	do_cmd,		TWO_CHAR },
 { /* n */	do_target,	TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL },
 { /* o */	do_ins,		0 },
 { /* p */	do_cmd,		0 },
 { /* q */	do_badcmd,	0 },
 { /* r */	do_cmd,		0 },
 { /* s */	do_cmd,		0 },
 { /* t */	do_csearch,	TARGET | TGT_CHAR | TGT_INCLUSIVE
					| TWO_CHAR | SET_CURSOR_COL },
 { /* u */	do_cmd,		0 },
 { /* v */	do_badcmd,	0 },
 { /* w */	do_word,	TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL },
 { /* x */	do_x,		0 },
 { /* y */	do_operator,	0 },
 { /* z */	do_z,		TWO_CHAR },
 { /* { */	do_target,	TARGET | TGT_CHAR | TGT_EXCLUSIVE
					| LONG_DISTANCE | SET_CURSOR_COL },
 { /* | */	do_target,	TARGET | TGT_CHAR | TGT_EXCLUSIVE },
 { /* } */	do_target,	TARGET | TGT_CHAR | TGT_EXCLUSIVE
					| LONG_DISTANCE | SET_CURSOR_COL },
 { /* ~ */	do_rchar,	0 },
 { /* DEL */	do_badcmd,	0 },
 { /* K_HELP */		do_cmd,		0 },
 { /* K_UNDO */		do_cmd,		0 },
 { /* K_INSERT */	do_ins,		0 },
 { /* K_HOME */		do_target,	TARGET | TGT_CHAR | TGT_INCLUSIVE },
 { /* K_UARROW */	do_target,	TARGET | TGT_LINE | TGT_INCLUSIVE },
 { /* K_DARROW */	do_target,	TARGET | TGT_LINE | TGT_INCLUSIVE },
 { /* K_LARROW */	do_left_right,	TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL },
 { /* K_RARROW */	do_left_right,	TARGET | TGT_CHAR | TGT_EXCLUSIVE | SET_CURSOR_COL },
 { /* K_CGRAVE */	do_cmd,		0 },
 { /* K_PGDOWN */	do_page,	0 },
 { /* K_PGUP */		do_page,	0 },
 { /* K_END */		do_target,	TARGET | TGT_CHAR | TGT_INCLUSIVE },
 { /* K_DELETE */	do_x,		0 },
};
