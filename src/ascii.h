/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    ascii.h
* module function:
    Keycode definitions for special keys - e.g. help, cursor arrow
    keys, page up, page down etc., & test & conversion macros for
    single characters.

    On systems that have any special keys, the routine 'inchar' in
    the terminal interface code should return one of the codes
    here.

    This file is specific to the ASCII character set; versions for
    other character sets could be implemented if required.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include <ctype.h>

/*
 * Tests on single characters.
 */
#define is_alpha(c)	(isascii(c) && isalpha(c))
#define is_upper(c)	(isascii(c) && isupper(c))
#define is_lower(c)	(isascii(c) && islower(c))
#define is_digit(c)	(isascii(c) && isdigit(c))
#define is_xdigit(c)	(isascii(c) && isxdigit(c))
#define is_octdigit(c)	((c) >= '0' && (c) <= '7')
#define is_punct(c)	(isascii(c) && ispunct(c))
#define is_alnum(c)	(isascii(c) && isalnum(c))
#define is_print(c)	(isascii(c) && isprint(c))
#define is_graph(c)	(isascii(c) && isgraph(c))
#define is_cntrl(c)	(isascii(c) && iscntrl(c))

/*
 * This is not like the usual isspace() macro; it only recognises space
 * and tab characters as being whitespace. This is important for the
 * editor, as other control characters do not normally occur, and if they
 * do they should not be treated as whitespace but as control characters.
 */
#define is_space(c)	((c) == ' ' || (c) == '\t')

/*
 * Conversions.
 *
 * Note that no argument validity checking is performed.
 */
/*
 * Upper case to lower case.
 */
#define to_lower(c)	((c) | 040)
/*
 * Lower case to upper case.
 */
#define to_upper(c)	((c) & 0137)
/*
 * Hexadecimal digit to binary integer.
 */
#define hex_to_bin(h)	(is_digit(h) ? (h) & 017 : ((h) & 7) + 9)

/*
 * Key codes.
 */
#define K_HELP		0x80
#define K_UNDO		0x81
#define K_INSERT	0x82
#define K_HOME		0x83
#define K_UARROW	0x84
#define K_DARROW	0x85
#define K_LARROW	0x86
#define K_RARROW	0x87
#define K_CGRAVE	0x88	/* control grave accent */
#define K_PGDOWN	0x89
#define K_PGUP		0x8a
#define K_END		0x8b

/*
 * Function keys.
 */
#define K_FUNC(n)	(0xa0 + (n))

/*
 * Some common control characters.
 */
#define ESC		'\033'
#define DEL		'\177'

#undef	CTRL
#define CTRL(x) 	((x) & 0x1f)

/*
 * Convert a command character to an ASCII character.
 *
 * This is needed for normal(), which uses the mapped value as an
 * index into a table.
 *
 * QNX is the only ASCII-based system which gives us a minor problem
 * here because its newline character is the same as control-^; so we
 * convert this value to an ASCII linefeed.
 */
#ifdef QNX
#   define ascii_map(n)	((n) == '\n' ? 012 : (n))
#else
#   define ascii_map(n)	(n)
#endif

/*
 * The top bit for extended character sets.
 * Note that this is NOT related to the size of a char in bits,
 * but to the ASCII character set - i.e. it is always 128.
 */
#define	TOP_BIT		128
