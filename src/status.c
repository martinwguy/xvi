/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    status.c
* module function:
    Routines to print status line messages.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

/*
 * Set up the "slinetype" field in the given buffer structure,
 * according to the number of columns available.
 */
void
init_sline()
{
    flexclear(&curwin->w_statusline);
}

/*VARARGS2*/
/*PRINTFLIKE*/
void
show_message
#ifdef	__STDC__
    (char *format, ...)
#else /* not __STDC__ */
    (format, va_alist)
    char	*format;
    va_dcl
#endif	/* __STDC__ */
{
    va_list	argp;

    VA_START(argp, format);
    flexclear(&curwin->w_statusline);
    (void) vformat(&curwin->w_statusline, format, argp);
    va_end(argp);

    update_sline();
}

/*VARARGS2*/
/*PRINTFLIKE*/
void
show_error
#ifdef	__STDC__
    (char *format, ...)
#else /* not __STDC__ */
    (format, va_alist)
    char	*format;
    va_dcl
#endif	/* __STDC__ */
{
    va_list	argp;

    if (Pb(P_errorbells)) {
	beep();
    }
    VA_START(argp, format);
    flexclear(&curwin->w_statusline);
    (void) vformat(&curwin->w_statusline, format, argp);
    va_end(argp);

    update_sline();
}

void
show_file_info(show_numbers)
bool_t	show_numbers;
{
    if (echo & e_SHOWINFO) {
	Flexbuf	*slp;

	slp = &curwin->w_statusline;
	flexclear(slp);
	if (curbuf->b_filename == NULL) {
	    (void) lformat(slp, "No File");
	    if (bufempty()) {
		show_numbers = FALSE;
	    }
	} else {
	    (void) lformat(slp, "\"%s\"", curbuf->b_filename);
	}
	if (is_readonly(curbuf))
	    (void) lformat(slp, " [Read only]");
	if (is_modified(curbuf))
	    (void) lformat(slp, " [Modified]");
	if (show_numbers) {
	    long	position, total;
	    long	percentage;

	    position = lineno(curwin->w_cursor->p_line);
	    total = lineno(b_last_line_of(curbuf));
	    percentage = (total > 0) ? (position * 100) / total : 0;

	    (void) lformat(slp, " line %ld of %ld --%ld%%--",
			position,
			total,
			percentage);
	}
    }
    update_sline();
}

void
info_update()
{
    show_file_info(Pen(P_infoupdate) == iu_CONTINUOUS);
}
