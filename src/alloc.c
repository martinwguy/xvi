/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    alloc.c
* module function:
    Various routines dealing with allocation
    and deallocation of data structures.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

/* #define DPRINT(args)	fprintf args */
#define DPRINT(args)	

/* One string for all "Out of memory" messages in all files */
char out_of_memory[] = "Not enough memory!";

/*
 * We use a special strategy for the allocation & freeing of Change &
 * Line objects to make these operations as fast as possible (since we
 * have to allocate one Change object for each input character in
 * INSERT & REPLACE states, & one Line object for every line of text
 * in each buffer).
 *
 * So we have a linked list of reusable objects, each of which can be
 * either a Change or a Line; freeing one means just adding it to this
 * list, so a later request for an allocation of a Change or a Line
 * can be satisfied very quickly by taking one item from the front of
 * the list.
 *
 * On some systems, a Line is slightly bigger than a Change, but then
 * we try to allocate them in reasonably large blocks, so we avoid
 * most of the overhead that we would get if we used a separate heap
 * block for each one.
 */
typedef union reusable
{
    Line		ru_l;
    Change		ru_c;
    union reusable     *ru_next;
} Reusable;

static Reusable		*reuselist = NULL;

/*
 * Add a single object to the list of Reusables.
 */
#define RECYCLE(p)	(((Reusable *) (p))->ru_next = reuselist, \
			reuselist = ((Reusable *) (p)))

/*
 * Recycle a Change object.
 */
void
chfree(ch)
    Change *ch;
{
    RECYCLE(ch);
}

/*
 * Future plan for MS-DOS is to use some EMS memory to store
 * Reusables; this function will be needed for that.
 */
#ifndef MSDOS
static
#endif
void
usemem(p, nbytes)
    void *p;
    register int nbytes;
{
    if (nbytes >= sizeof(Reusable)) {
	register Reusable * rp1;
	register Reusable * rp2;

	rp2 = p;
	do {
	    rp1 = rp2;
	    rp1->ru_next = rp2 = &rp1[1];
	} while ((nbytes -= sizeof(Reusable)) >= sizeof (Reusable));
	rp1->ru_next = reuselist;
	reuselist = p;
    }
}

/*
 * Allocate a single Reusable object.
 */
static Reusable *
ralloc()
{
    Reusable *p;

    if (reuselist == NULL) {
	/*
	 * Try to allocate a block of 16 Reusable objects.
	 *
	 * We turn off e_ALLOCFAIL for the first alloc to
	 * prevent getting the same error message twice.
	 */
	unsigned savecho = echo;

#define RBLOCKSIZE (16 * sizeof (Reusable))
	echo &= ~e_ALLOCFAIL;
	if ((p = alloc(RBLOCKSIZE)) != NULL) {
	    usemem((char *) p, RBLOCKSIZE);
#undef  RBLOCKSIZE
	}
	echo = savecho;
    }
    if (reuselist) {
	p = reuselist;
	reuselist = p->ru_next;
	return p;
    }
    /*
     * If we get to here, either there isn't much memory left or it's
     * very badly fragmented, so we just try for a single Reusable.
     */
    return alloc(sizeof(Reusable));
}

Change *
challoc()
{
    return (Change *) ralloc();
}

void *
alloc(size)
size_t size;
{
    void *p;		/* pointer to new storage space */

    if ((p = malloc(size)) == NULL) {
	if (echo & e_ALLOCFAIL) {
	    show_error(out_of_memory);
	}
    }
    return(p);
}

void *
re_alloc(ref, size)
void *ref;
size_t size;
{
    void *p;		/* pointer to new storage space */

    if ((p = realloc(ref, size)) == NULL) {
	if (echo & e_ALLOCFAIL) {
	    show_error(out_of_memory);
	}
    }
    return(p);
}

void *
clr_alloc(num, size)
size_t num, size;
{
    void *p;		/* pointer to new storage space */
#if 0
    size_t total;	/* total size to allocate */

    total = num * size;
    if ((size != 0) && ((total / size) != num)) {
	/* overflow */
	if (echo & e_ALLOCFAIL) {
	    show_error("Allocation size overflow!");
	}
	return(NULL);
    }

    if ((p = malloc(total)) == NULL) {
	if (echo & e_ALLOCFAIL) {
	    show_error(out_of_memory);
	}
    } else {
	memset(p, 0, total);
    }
#else
    if ((p = calloc(num, size)) == NULL) {
	if (echo & e_ALLOCFAIL) {
	    show_error(out_of_memory);
	}
    }
#endif
    return(p);
}

char *
strsave(string)
const char *string;
{
    char	*space;

    space = alloc(strlen(string) + 1);
    if (space != NULL) {
	(void) strcpy(space, string);
    }
    return(space);
}

/*
 * newline(): allocate and initialize a new line object with room
 * for at least the number of characters specified.
 *
 * In fact, we round this number up to the next multiple of MEMCHUNK
 * (which is currently 8, but could be tuned for different operating
 * environments) so that we don't have to call realloc() too many
 * times if the line gets longer.
 */

#define MC_SHIFT	3
#define MEMCHUNK	(1 << MC_SHIFT)
#define MC_MASK		(~(unsigned)0 << MC_SHIFT)
#define MC_ROUNDUP(n)	(((n) + MEMCHUNK - 1) & MC_MASK)

Line *
newline(nchars)
int	nchars;
{
    register Line	*l;
    char		*ltp;

    /*
     * It is okay for newline() to be called with a 0
     * parameter - but we must never call malloc(0) as
     * this will break on many systems.
     */
    nchars = (nchars == 0) ? MEMCHUNK : MC_ROUNDUP(nchars);
    ltp = alloc(nchars);
    if (ltp == NULL) {
	return(NULL);
    }
    if ((l = (Line *) ralloc()) == NULL) {
	free(ltp);
	return NULL;
    }
    ltp[0] = '\0';
    l->l_text = ltp;
    l->l_size = nchars;
    l->l_prev = NULL;
    l->l_next = NULL;

    return(l);
}

/*
 * snewline(): allocate and initialize a new line object from a string
 * whose contents were obtained from alloc().
 */
Line *
snewline(str)
char *str;
{
    register Line	*l;

    if ((l = (Line *) ralloc()) == NULL) {
	return NULL;
    }
    l->l_text = str;
    l->l_size = strlen(str) + 1;
    l->l_prev = NULL;
    l->l_next = NULL;

    return(l);
}

/*
 * bufempty() - return TRUE if the current buffer is empty
 */
bool_t
bufempty()
{
    /* TRUE if the buffer has exactly one line and the line has no text */
    return(curbuf->b_file->l_next == curbuf->b_lastline &&
	   curbuf->b_file->l_text[0] == '\0');
}

/*
 * endofline() - return TRUE if the given position is at end of line
 *
 * This routine will probably never be called with a position resting
 * on the zero byte, but handle it correctly in case it happens.
 */
bool_t
endofline(p)
Posn	*p;
{
    register char	*endtext = p->p_line->l_text + p->p_index;

    return(*endtext == '\0' || *(endtext + 1) == '\0');
}

/*
 * Adjust the text size of a Line.
 *
 * See comments for newline().
 */
bool_t
lnresize(lp, newsize)
Line	*lp;
register unsigned	newsize;
{
    register char	*oldtext;
    register char	*newtext;
    register unsigned	oldsize;

    oldsize = (unsigned) lp->l_size;

    if ((newsize = MC_ROUNDUP(newsize)) == 0) {
	return(TRUE);
    }

    if (newsize == oldsize) {
	return(TRUE);
    }

    oldtext = lp->l_text;

    if (newsize < oldsize && oldtext != NULL) {
	if ((newtext = re_alloc(oldtext, newsize)) == NULL) {
	    newtext = oldtext;
	}
    } else {
	if ((newtext = alloc(newsize)) == NULL) {
	    return(FALSE);
	}
	if (oldtext) {
	    (void) strncpy(newtext, oldtext, oldsize - 1);
	    newtext[oldsize - 1] = '\0';
	    free(oldtext);
	}
    }
    lp->l_text = newtext;
    lp->l_size = (int) newsize;
    return(TRUE);
}

/*
 * grow_line(lp, n)
 *	- increase the size of the space allocated for the line by n bytes.
 *
 * This routine returns TRUE immediately if the requested space is available.
 * If not, it attempts to allocate the space and adjust the data structures
 * accordingly, and returns TRUE if this worked.
 * If everything fails it returns FALSE.
 */
bool_t
grow_line(lp, n)
Line	*lp;
int	n;
{
    int		nsize;
    char	*ctp;

    ctp = lp->l_text;
    nsize = (ctp ? strlen(ctp) : 0) + 1 + n;	/* size required */

    if (nsize <= lp->l_size) {
	return(TRUE);
    }

    return lnresize(lp, nsize);
}

/*
 * Free up space used by the given list of lines.
 *
 * Note that the Line structures themselves are just added to the list
 * of Reusables.
 */
void
throw(lineptr)
register Line	*lineptr;
{
    while (lineptr != NULL) {
	register Line	*nextline;

	if (lineptr->l_text != NULL) {
	    free(lineptr->l_text);
	}
	nextline = lineptr->l_next;
	RECYCLE(lineptr);
	lineptr = nextline;
    }
}
