/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    virtscr.c
* module function:
    Functions to handle initialisation and freeing up of memory used
    by VirtScrs.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

bool_t
vs_init(vs)
VirtScr	*vs;
{
    int		count;

    /*
     * Allocate space for the lines. Notice that we allocate an
     * extra byte at the end of each line for null termination.
     */
    vs->pv_int_lines = (Sline *) malloc(vs->pv_rows * sizeof(Sline));
    vs->pv_ext_lines = (Sline *) malloc(vs->pv_rows * sizeof(Sline));

    if (vs->pv_int_lines == NULL || vs->pv_ext_lines == NULL) {
	/* What to do now? */
	return(FALSE);
    }

    /*
     * Now assign all the rows ...
     */
    for (count = 0; count < vs->pv_rows; count++) {
	register Sline	*ip, *ep;

	ip = vs->pv_int_lines + count;
	ep = vs->pv_ext_lines + count;

    	ip->s_line = malloc((vs->pv_cols + 1) * sizeof(ip->s_line[0]));
    	ep->s_line = malloc((vs->pv_cols + 1) * sizeof(ip->s_line[0]));
    	ip->s_colour = malloc((vs->pv_cols + 1) * sizeof(ip->s_colour[0]));
    	ep->s_colour = malloc((vs->pv_cols + 1) * sizeof(ip->s_colour[0]));
	if (ip->s_line == NULL || ep->s_line == NULL ||
		ip->s_colour == NULL || ep->s_colour == NULL) {
	    return(FALSE);
	}

	ip->s_line[0] = '\0';
	ep->s_line[0] = '\0';
	ip->s_used = ep->s_used = 0;
	ip->s_flags = ep->s_flags = 0;
    }

    return(TRUE);
}

/*
 * Re-initialise data structures for the new size of this window.
 * Returns TRUE if we did it, FALSE for failure.
 */
bool_t
vs_resize(vs, rows, columns)
VirtScr	*vs;
int	rows, columns;
{
    int		count;
    int		old_rows, new_rows;
    int		new_cols;
    Sline	*ip, *ep;

    if (rows == 0 && columns == 0) {
    	return(TRUE);
    }

    new_rows = VSrows(vs);
    old_rows = new_rows - rows;
    new_cols = VScols(vs);

    /*
     * Free up rows no longer required.
     */
    for (count = new_rows; count < old_rows; count++) {
	free((genptr *) vs->pv_int_lines[count].s_line);
	free((genptr *) vs->pv_ext_lines[count].s_line);
    }

    /*
     * Redimension the screen line arrays. Because it is possible
     * for new_rows to be 0, we add a single byte to the amount
     * allocated. This ensures that the allocation will succeed.
     */
    vs->pv_int_lines = (Sline *) realloc((genptr *) vs->pv_int_lines,
				(unsigned) new_rows * sizeof(Sline) + 1);
    vs->pv_ext_lines = (Sline *) realloc((genptr *) vs->pv_ext_lines,
				(unsigned) new_rows * sizeof(Sline) + 1);
    if (vs->pv_int_lines == NULL || vs->pv_ext_lines == NULL) {
    	return(FALSE);
    }

    /*
     * Allocate space for newly acquired rows.
     */
    for (count = old_rows; count < new_rows; count++) {
	ip = vs->pv_int_lines + count;
	ep = vs->pv_ext_lines + count;

	ip->s_line = malloc((unsigned) (new_cols + 1) * sizeof(ip->s_line[0]));
	ep->s_line = malloc((unsigned) (new_cols + 1) * sizeof(ip->s_line[0]));
	ip->s_colour = malloc((unsigned)
				(new_cols + 1) * sizeof(ip->s_colour[0]));
	ep->s_colour = malloc((unsigned)
				(new_cols + 1) * sizeof(ip->s_colour[0]));
	if (ip->s_line == NULL || ep->s_line == NULL ||
		ip->s_colour == NULL || ep->s_colour == NULL) {
	    return(FALSE);
	}

	ip->s_line[0] = '\0';
	ep->s_line[0] = '\0';
	ip->s_used = ep->s_used = 0;
	ip->s_flags = ep->s_flags = S_DIRTY;
    }

    /*
     * If the number of columns has changed, change the size of the
     * screen rows for the smaller of the old and new screen sizes.
     */
    count = new_rows - 1;
    if (count > old_rows - 1) {
    	count = old_rows - 1;
    }
    for ( ; count >= 0; --count) {
	ip = vs->pv_int_lines + count;
	ep = vs->pv_ext_lines + count;

	ip->s_line = realloc(ip->s_line,
			(unsigned) (new_cols + 1) * sizeof(ip->s_line[0]));
	ep->s_line = realloc(ep->s_line,
			(unsigned) (new_cols + 1) * sizeof(ep->s_line[0]));
	ip->s_colour = realloc(ip->s_colour,
			(unsigned) (new_cols + 1) * sizeof(ip->s_colour[0]));
	ep->s_colour = realloc(ep->s_colour,
			(unsigned) (new_cols + 1) * sizeof(ep->s_colour[0]));
	if (ip->s_line == NULL || ep->s_line == NULL ||
		ip->s_colour == NULL || ep->s_colour == NULL) {
	    return(FALSE);
	}
    }

    return(TRUE);
}

void
vs_free(vs)
VirtScr	*vs;
{
    int		count;

    /*
     * Now delete the memory used for the screen image.
     */
    for (count = 0; count < vs->pv_rows; count++) {
	free((genptr *) vs->pv_int_lines[count].s_line);
	free((genptr *) vs->pv_ext_lines[count].s_line);
	free((genptr *) vs->pv_int_lines[count].s_colour);
	free((genptr *) vs->pv_ext_lines[count].s_colour);
    }
    free((genptr *) vs->pv_int_lines);
    free((genptr *) vs->pv_ext_lines);
    VSclose(vs);
}

/*
 * Default colour-decoding function.
 */
int
xv_decode_colour(vs, which, colstr)
VirtScr	*vs;
int	which;
char	*colstr;
{
    int		i;

    i = xv_strtoi(&colstr);
    if (*colstr != '\0') {
	return(FALSE);
    }
    vs->pv_colours[which] = i;
    return(TRUE);
}
