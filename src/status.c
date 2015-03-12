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
init_sline(win)
Xviwin	*win;
{
    flexclear(&win->w_statusline);
}

/*VARARGS2*/
/*PRINTFLIKE*/
void
show_message
#ifdef	__STDC__
    (Xviwin *window, char *format, ...)
#else /* not __STDC__ */
    (window, format, va_alist)
    Xviwin	*window;
    char	*format;
    va_dcl
#endif	/* __STDC__ */
{
    va_list	argp;

    VA_START(argp, format);
    (void) flexclear(&window->w_statusline);
    (void) vformat(&window->w_statusline, format, argp);
    va_end(argp);

    update_sline(window);
}

/*VARARGS2*/
/*PRINTFLIKE*/
void
show_error
#ifdef	__STDC__
    (Xviwin *window, char *format, ...)
#else /* not __STDC__ */
    (window, format, va_alist)
    Xviwin	*window;
    char	*format;
    va_dcl
#endif	/* __STDC__ */
{
    va_list	argp;

    if (Pb(P_errorbells)) {
	beep(window);
    }
    VA_START(argp, format);
    (void) flexclear(&window->w_statusline);
    (void) vformat(&window->w_statusline, format, argp);
    va_end(argp);

    update_sline(window);
}

void
show_file_info(window, show_numbers)
Xviwin	*window;
bool_t	show_numbers;
{
    if (echo & e_SHOWINFO) {
	Buffer	*buffer;
	Flexbuf	*slp;

	buffer = window->w_buffer;

	slp = &window->w_statusline;
	flexclear(slp);
	if (buffer->b_filename == NULL) {
	    (void) lformat(slp, "No File");
	    if (bufempty(buffer)) {
		show_numbers = FALSE;
	    }
	} else {
	    (void) lformat(slp, "\"%s\"", buffer->b_filename);
	}
	if (is_readonly(buffer))
	    (void) lformat(slp, " [Read only]");
	if (not_editable(buffer))
	    (void) lformat(slp, " [Not editable]");
	if (is_modified(buffer))
	    (void) lformat(slp, " [Modified]");
	if (show_numbers) {
	    long	position, total;
	    long	percentage;

	    position = lineno(buffer, window->w_cursor->p_line);
	    total = lineno(buffer, buffer->b_lastline->l_prev);
	    percentage = (total > 0) ? (position * 100) / total : 0;

	    (void) lformat(slp, " line %ld of %ld --%ld%%--",
			position,
			total,
			percentage);
	}
    }
    update_sline(window);
}

void
info_update(win)
    Xviwin *win;
{
    show_file_info(win, Pen(P_infoupdate) == iu_CONTINUOUS);
}
