/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    altstack.c
* module function:
    Simple implementation of a stack of alternate files.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews	(onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

/*
 * If the alternate file stack becomes full, it's better to make room
 * for new elements to be pushed on to the top of it by discarding
 * elements from the bottom, so we implement it (effectively) as a
 * circular stack.
 */
struct element {
    char	*filename;
    long	line;
};

static	struct element	stack[32];
static	int		n_elements;	/* number of elements on stack */
static	int		top;		/* index of next element to use */

#define	STACK_SIZE	(sizeof(stack) / sizeof(stack[0]))

#define	stack_full()	FALSE
#define	stack_empty()	(n_elements == 0)

#define prev_element	(top > 0 ? top - 1 : STACK_SIZE - 1)
#define next_element	(top < STACK_SIZE - 1 ? top + 1 : 0)

/*
 * Return the current alternate filename, or NULL if there isn't one.
 */
char *
alt_file_name()
{
    if (stack_empty()) {
    	return(NULL);
    } else {
    	return(stack[prev_element].filename);
    }
}

/*
 * Push a new alternate filename and line number,
 * if there is room. If there isn't room, don't.
 */
void
push_alternate(altfile, altline)
char	*altfile;
long	altline;
{
    struct element *p = &stack[top];

    p->filename = strsave(altfile);
    if (p->filename != NULL) {
    	p->line = altline;
	top = next_element;
	if (n_elements < STACK_SIZE)
	    n_elements++;
    }
}

/*
 * Pop the alternate file/line off the stack, returning a
 * pointer to the filename in allocated memory, and filling
 * in *linep with the associated alternate line number.
 */
char *
pop_alternate(linep)
long	*linep;
{
    struct element *p;

    if (stack_empty()) {
    	return(NULL);
    }
    --n_elements;
    top = prev_element;
    p = &stack[top];
    *linep = p->line;
    return(p->filename);
}
