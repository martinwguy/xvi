/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    map.c
* module function:
    Keyboard input/pushback routines, "map" command.

    Note that we provide key mapping through a different interface, so that
    cursor key mappings etc do not show up to the user. This works by having
    a two-stage process; first keymapping is done, and then the result is fed
    through the normal mapping process.

    The intent of the keymapping stage is to convert terminal-specific keys
    into a canonical form. The second mapping stage is used for user-set maps.
    Note, however, that the actual mapping process is exactly the same for
    both stages; it is only the data that are different.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

/*
 * This is the fundamental structure type which is used
 * to hold mappings from one string to another.
 */
typedef struct map {
    struct map	    *m_next;
    char	    *m_lhs;		/* lhs of map */
    char	    *m_rhs;		/* rhs of map */
    unsigned int    m_same;		/* no characters same as next map */
} Map;

/*
 * This structure holds a current position while scanning a map list.
 * It is also effectively used to form a chain of mapping structures,
 * interconnected with flexbufs.
 */
typedef struct mpos {
    Map		*mp_map;
    int		mp_index;
    Flexbuf	*mp_src;
    Flexbuf	*mp_dest;
} Mpos;

/*
 * Three queues exist: the raw queue (containing characters directly
 * obtained form the keyboard), the canonical queue (where some escape
 * sequences will have been translated into their "canonical" form,
 * and the mapped queue (containing the characters which will be read
 * by the editor).
 *
 * The three queues are connected together by translation data structures.
 * While a translation is being processed, this is represented by an Mpos
 * structure, and these are defined below.
 */
static	Flexbuf	mapped_queue;
static	Flexbuf canon_queue;
static	Flexbuf raw_queue;
static	Mpos	npos = { NULL, 0, &canon_queue,	&mapped_queue };
static	Mpos	kpos = { NULL, 0, &raw_queue,	&canon_queue };

/*
 * These map structures are used for NORMAL mode and for INSERT/REPLACE modes,
 * respectively. They translate between the canonical and mapped queues.
 */
static	Map	*cmd_map = NULL;
static	Map	*ins_map = NULL;

/*
 * This map structure holds translations from the raw to the canonical form.
 * Note that this is set up by calls to xvi_keymap() from the terminal-specific
 * module, as this module has knowledge of the escape sequence which must be
 * mapped into the right canonical forms.
 */
static	Map	*key_map = NULL;

/*
 * This is used for "display" mode; it records the current map which
 * is being displayed. It is used by show_map().
 */
static	Map	*curmap;
static	char	*show_map P((void));

static	void	mapthrough P((Mpos *, Map *));
static	bool_t	process_map P((int, Mpos *));
static	void	calc_same P((Map *));
static	void	map_failed P((Mpos *));
static	void	insert_map P((Map **, char *, char *));
static	void	delete_map P((Map **, char *));

/*VARARGS1*/
/*PRINTFLIKE*/
void
stuff
#ifdef	__STDC__
    (char *format, ...)
#else /* not __STDC__ */
    (format, va_alist)
    char	*format;
    va_dcl
#endif
{
    va_list	argp;

    VA_START(argp, format);
    (void) vformat(npos.mp_dest, format, argp);
    va_end(argp);
}

/*
 * This routine inserts a newly-arrived character into the raw input queue.
 */
void
map_char(c)
register int	c;
{
    (void) flexaddch(kpos.mp_src, c);
}

/*
 * This function returns the next input character to the editor,
 * if there is one; otherwise it returns EOF.
 */
int
map_getc()
{
    /*
     * Keep pushing any characters through the map lookup tables until
     * we get at least one out this end. Then stop. Notice that we only
     * touch the raw input queue if there are no characters already
     * awaiting translation in the canonical queue.
     */
    while (flexempty(npos.mp_dest)) {
	if (!flexempty(npos.mp_src)) {
	    mapthrough(&npos,
			(State == NORMAL) ? cmd_map :
			(State == INSERT ||
			 State == REPLACE ||
			 State == CMDLINE) ? ins_map :
			NULL);
	} else if (!flexempty(kpos.mp_src)) {
	    mapthrough(&kpos, key_map);
	} else {
	    break;
	}
    }
    return(flexempty(npos.mp_dest) ? EOF : flexpopch(npos.mp_dest));
}

/*
 * Process any characters in the canonical queue through the cmd_map/ins_map
 * lists into the mapped queue, whence characters go into the editor itself.
 */
static void
mapthrough(mp, map)
Mpos	*mp;
Map	*map;
{
    if (mp->mp_map == NULL) {
	mp->mp_map = map;
	mp->mp_index = 0;
    }
    if (process_map(flexpopch(mp->mp_src), mp) == FALSE) {
	/*
	 * Failed; reset key mapping for next try.
	 */
	mp->mp_map = NULL;
    }
}

/*
 * Process the given character through the maplist pointed to
 * by the given position. Returns TRUE if we should continue,
 * or FALSE if this attempt at mapping has terminated (either
 * due to success or definite failure).
 */
static bool_t
process_map(c, pos)
register int	c;
register Mpos	*pos;
{
    register Map	*tmp;
    register int	ind;

    ind = pos->mp_index;
    for (tmp = pos->mp_map; tmp != NULL; tmp = tmp->m_next) {
	if (tmp->m_lhs[ind] == c) {
	    if (tmp->m_lhs[ind + 1] == '\0') {
		/*
		 * Found complete match. Insert the result into the
		 * appropriate buffer, according to whether "remap"
		 * is set or not.
		 *
		 * If we're remapping, we need to insert the rhs into the
		 * beginning of the input queue. Flexbufs don't support
		 * such inserts, so we have to shuffle the data around.
		 */
		if (Pb(P_remap) && pos->mp_src != NULL) {
		    Flexbuf flextmp;
		    int tmpch;

		    /*
		     * Get any characters waiting in the queue
		     */
		    flexnew(&flextmp);
		    while ((tmpch = flexpopch(pos->mp_src)) != 0) {
			flexaddch(&flextmp, tmpch);
		    }
		    /*
		     * Input queue is now the new rhs followed by the
		     * characters that were there before (if any)
		     */
		    (void) lformat(pos->mp_src, "%s%s", tmp->m_rhs, flexgetstr(&flextmp));
		    flexdelete(&flextmp);
		} else {
		    (void) lformat(pos->mp_dest, "%s", tmp->m_rhs);
		}
		return(FALSE);
	    } else {
		/*
		 * Found incomplete match,
		 * keep going.
		 */
		pos->mp_map = tmp;
		pos->mp_index++;
	    }
	    return(TRUE);
	}

	/*
	 * Can't move on to next map entry unless the m_same
	 * field is sufficient that the match so far would
	 * have worked.
	 */
	if (tmp->m_same < ind) {
	    break;
	}
    }

    map_failed(pos);

    /*
     * Don't forget to re-stuff the character we have just received.
     */
    if (pos->mp_src != NULL && ind > 0) {
	(void) flexaddch(pos->mp_src, c);
    } else {
	(void) flexaddch(pos->mp_dest, c);
    }
    return(FALSE);
}

void
map_timeout()
{
    if (kpos.mp_map != NULL) {
	map_failed(&kpos);
    } else {
    	map_failed(&npos);
    }
}

bool_t
map_waiting()
{
    return(kpos.mp_map != NULL || npos.mp_map != NULL);
}

/*
 * This routine is called when a map has failed. We transfer the first
 * input character into the destination flexbuf, and all the others into
 * the src flexbuf. This gives us a change to retry maps which fail on
 * the first input character at the next input character.
 */
static void
map_failed(pos)
Mpos	*pos;
{
    register char	*cp;
    register Flexbuf	*fbp;
    register int	i;

    if (pos->mp_map != NULL) {

	cp = pos->mp_map->m_lhs;
	if (pos->mp_index > 0) {
	    (void) flexaddch(pos->mp_dest, cp[0]);
	}

	fbp = (pos->mp_src != NULL) ? pos->mp_src : pos->mp_dest;
	for (i = 1; i < pos->mp_index; i++) {
	    (void) flexaddch(fbp, cp[i]);
	}
	pos->mp_map = NULL;
    }
}

/*
 * Insert the key map lhs as mapping into rhs.
 */
void
xvi_keymap(left, right)
char	*left;
char	*right;
{
    char *lhs, *rhs;

    lhs = strsave(left);
    if (lhs == NULL || (rhs = strsave(right)) == NULL) {
	if (lhs != NULL) {
	    free(lhs);
	}
	return;
    }
    insert_map(&key_map, lhs, rhs);
}

/*
 * Interpret escape sequences in :map commands.
 *
 * Also split the command line (following a :map command) into two
 * parts: the left-hand side & the right-hand side, & return
 * appropriate values through the pointers given. *rhsp is set to NULL
 * if we don't get a right-hand side (as in an :unmap command).
 *
 * Return FALSE if we run out of dynamic memory.
 */
static bool_t
mapescape(lhsp, rhsp)
    char	      **lhsp,
		      **rhsp;
{
    Flexbuf		f;
    register int	c;
    register char      *s;
    char	      **spp;

    flexnew(&f);
    s = *lhsp;
    spp = lhsp;
    *rhsp = (char *) NULL;
    while ((c = *s++) != '\0') {
	register int	bc;

	switch (bc = c) {
	case '\\':
	    switch (c = *s++) {
	    case ' ':
	    case '\t':
		bc = c;
		break;

	    case '0': case '1': case '2': case '3':
	    case '4': case '5': case '6': case '7':
		bc = c - '0';

		c = *s;
		if (is_octdigit(c))
		{
		    bc <<= 3;
		    bc |= (c - '0');
		    c = *++s;
		    if (is_octdigit(c))
		    {
			bc <<= 3;
			bc |= (c - '0');
			s++;
		    }
		}
		break;

	    case 'e':
	    case 'E':	bc = ESC; break;

	    case 'b':	bc = '\b'; break;
	    case 'f':	bc = '\f'; break;
	    case 'n':	bc = '\n'; break;
	    case 'r':	bc = '\r'; break;
	    case 't':	bc = '\t'; break;
	    case 'v':	bc = '\v'; break;
	    case '\\':	break;
	    default:	s--;
	    }
	    break;

	case ' ':
	case '\t':
	    while ((c = *s) != '\0' && is_space(c)) {
		s++;
	    }
	    if (spp == lhsp) {
		if ((*lhsp = flexdetach(&f)) == NULL) {
		    return FALSE;
		}
		spp = rhsp;
		continue;
	    }
	}
	if ((unsigned char) bc != 0 && !flexaddch(&f, bc)) {
	    if (spp == rhsp) {
		free(*lhsp);
	    }
	    flexdelete(&f);
	    return FALSE;
	}
    }
    if ((*spp = flexdetach(&f)) == NULL) {
	if (spp == rhsp) {
	    free(*lhsp);
	}
	return FALSE;
    }
    return TRUE;
}

/*
 * Insert the entry "lhs" as mapping into "rhs".
 */
void
xvi_map(arg, exclam, inter)
    register char      *arg;
    bool_t		exclam;
    bool_t		inter;
{
    register int	c;

    if (arg) {
	while ((c = *arg) != '\0' && is_space(c)) {
	    arg++;
	}
    }
    if (arg == NULL || arg[0] == '\0') {
	curmap = exclam ? ins_map : cmd_map;
	disp_init(curwin, show_map, (int) curwin->w_ncols, FALSE);
    } else {
	char	       *lhs;
	char	       *rhs;

	lhs = arg;
	if (!mapescape(&lhs, &rhs)) {
	    if (inter) {
		show_message(curwin, "No memory for that map");
	    }
	    return;
	}
	if (rhs == NULL) {
	    if (inter) {
		show_message(curwin, "Usage: :map lhs rhs");
	    }
	    free(lhs);
	    return;
	}
	insert_map(exclam ? &ins_map : &cmd_map, lhs, rhs);
    }
}

static void
insert_map(maplist, lhs, rhs)
Map		**maplist;
char		*lhs;
char		*rhs;
{
    Map		*mptr;			/* new map element */
    Map		**p;			/* used for loop to find position */
    int		rel;

    mptr = (Map *) alloc(sizeof(Map));
    if (mptr == NULL) {
	free(lhs);
	free(rhs);
	return;
    }

    mptr->m_lhs = lhs;
    mptr->m_rhs = rhs;

    p = maplist;
    if ((*p) == NULL || strcmp((*p)->m_lhs, lhs) > 0) {
	/*
	 * Either there are no maps yet, or the one
	 * we want to enter should go at the start.
	 */
	mptr->m_next = *p;
	*p = mptr;
	calc_same(mptr);
    } else if (strcmp((*p)->m_lhs, lhs) == 0) {
	/*
	 * We need to replace the rhs of the first map.
	 */
	free(lhs);
	free((char *) mptr);
	free((*p)->m_rhs);
	(*p)->m_rhs = rhs;
	calc_same(*p);
    } else {
	for ( ; (*p) != NULL; p = &((*p)->m_next)) {
	    /*
	     * Set "rel" to +ve if the next element is greater
	     * than the current one, -ve if it is less, or 0
	     * if they are the same (if the lhs is the same).
	     */
	    rel = ((*p)->m_next == NULL) ? 1 :
		strcmp((*p)->m_next->m_lhs, lhs);

	    if (rel >= 0) {
		if (rel > 0) {
		    /*
		     * The right place to insert
		     * the new map.
		     */
		    mptr->m_next = (*p)->m_next;
		    (*p)->m_next = mptr;
		    calc_same(*p);
		    calc_same(mptr);
		} else /* rel == 0 */ {
		    /*
		     * The lhs of the new map is identical
		     * to that of an existing map.
		     * Replace the old rhs with the new.
		     */
		    free(lhs);
		    free((char *) mptr);
		    mptr = (*p)->m_next;
		    free(mptr->m_rhs);
		    mptr->m_rhs = rhs;
		    calc_same(mptr);
		}
		return;
	    }
	}
    }
}

void
xvi_unmap(argc, argv, exclam, inter)
int	argc;
char	*argv[];
bool_t	exclam;
bool_t	inter;
{
    int	count;

    if (argc < 1) {
	if (inter) {
	    show_message(curwin, "But what do you want me to unmap?");
	}
	return;
    }

    for (count = 0; count < argc; count++) {
	char *lhs;
	char *rhs;
		/* This isn't actually used, but mapescape() needs it. */

	lhs = argv[count];
	if (mapescape(&lhs, &rhs)) {
	    delete_map(exclam ? &ins_map : &cmd_map, lhs);
	    free(lhs);
	}
    }
}

static void
delete_map(maplist, lhs)
Map	**maplist;
char	*lhs;
{
    Map	*p;

    p = *maplist;
    if (p != NULL && strcmp(p->m_lhs, lhs) == 0) {
	*maplist = p->m_next;
    } else {
	for (; p != NULL; p = p->m_next) {
	    if (p->m_next != NULL && strcmp(lhs, p->m_next->m_lhs) == 0) {
		Map	*tmp;

		tmp = p->m_next;
		p->m_next = p->m_next->m_next;
		free(tmp->m_lhs);
		free(tmp->m_rhs);
		free((char *) tmp);
		calc_same(p);
	    }
	}
    }
}

static void
calc_same(mptr)
Map	*mptr;
{
    register char	*a, *b;

    mptr->m_same = 0;
    if (mptr->m_next != NULL) {
	for (a = mptr->m_lhs, b = mptr->m_next->m_lhs; *a == *b; a++, b++) {
	    mptr->m_same++;
	}
    }
}

static char *
show_map()
{
    static Flexbuf	buf;

    /*
     * Have we reached the end?
     */
    if (curmap == NULL) {
	return(NULL);
    }

    flexclear(&buf);
    (void) lformat(&buf, "%-18.18s %-s", curmap->m_lhs, curmap->m_rhs);
    curmap = curmap->m_next;
    return flexgetstr(&buf);
}
