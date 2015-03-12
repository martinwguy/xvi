/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    change.h
* module function:
    Definitions related to changes to a buffer - used mainly
    in undo.c, but also in alloc.c.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

/*
 * Structure used to record a single change to a buffer.
 * A change may either be a number of lines replaced with a
 * new set, a number of characters removed or a number of
 * characters replaced with a new set. Character changes
 * never straddle line boundaries. A list of these
 * structures forms a complex change. There is also a fourth
 * type of "change", which does not actually change the
 * buffer, but is simply a record of the cursor position at
 * the time of the start of the change. This is needed so
 * that the cursor returns to the correct position after an
 * "undo".
 *
 * This entire structure is only used in undo.c and alloc.c, and
 * no other code should touch it.
 */
typedef	struct change {
    struct change	*c_next;
    enum {
	C_LINE,
	C_CHAR,
	C_DEL_CHAR,
	C_POSITION
    }			c_type;
    unsigned long	c_lineno;
    union {
	struct {
	    long    cul_nlines;
	    Line    *cul_lines;
	}	cu_l;
	struct {
	    int	    cuc_index;
	    int	    cuc_nchars;
	    char    *cuc_chars;
	}	cu_c;
	struct {
	    long    cup_line;
	    int	    cup_index;
	}	cu_p;
    }			c_u;
} Change;

#define	c_nlines	c_u.cu_l.cul_nlines
#define	c_lines		c_u.cu_l.cul_lines
#define	c_index		c_u.cu_c.cuc_index
#define	c_nchars	c_u.cu_c.cuc_nchars
#define	c_chars		c_u.cu_c.cuc_chars
#define	c_pline		c_u.cu_p.cup_line
#define	c_pindex	c_u.cu_p.cup_index

typedef struct changestack {
    Change	*cs_stack[MAX_UNDO];
    int		cs_size;
} ChangeStack;

/*
 * One of these data structures exists for every buffer.
 * It contains the undo and redo stacks, and temporary
 * variables used during a composite command.
 */
typedef	struct changedata {
    /*
     * Number of brackets surrounding current change to buffer.
     */
    unsigned int	cd_nlevels;

    /*
     * This field holds the total number of added/deleted lines
     * for a change; it is used for reporting (the "report" parameter).
     */
    long		cd_total_lines;

    /*
     * Array of pointers to LIFO lists of changes made. Each element
     * of the array represents a composite command, and the elements
     * are themselves stored as a LIFO, so that they may be replayed
     * to return to any previous buffer configuration.
     *
     * The maximum number of undo levels retained is governed by two
     * numeric parameters, maxundo and minundo; we always discard any
     * changes once maxundo is reached, but will retain minundo even
     * if it means we fail to execute a command. The MAX_UNDO define
     * limits the value of Pn(P_maxundo).
     */
    ChangeStack		*cd_undo;
    ChangeStack		*cd_redo;

} ChangeData;

/*
 * alloc.c
 */
extern	struct change	*challoc P((void));
extern	void		chfree P((struct change *));
