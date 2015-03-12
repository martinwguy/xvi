/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    virtscr.h
* module function:
    Definition of the VirtScr class.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

/*
 * This is used for the internal representation of a screen line.
 * Each VirtScr holds two pointers to vectors of these, one which
 * holds the external state of the display, and the other which
 * represents the internal state. The two are diff'ed when the
 * window is updated. The vector is resized when the window size
 * is changed.
 *
 * The s_line part is guaranteed to be always null-terminated.
 */
typedef	struct line_struct {
    char		*s_line;	/* storage for character cells */
    unsigned char	*s_colour;	/* storage for character colours */
    int			s_used;		/* number of bytes actually used */
    unsigned int	s_flags;	/* information bits */
} Sline;

/*
 * Bit definitions for s_flags.
 */
#define	S_TEXT		0x01		/* is an ordinary text line */
#define	S_MARKER	0x02		/* is a marker line ('@' or '~') */
#define	S_DIRTY		0x04		/* has been modified */
#define	S_MESSAGE	0x08		/* is a message line */
#define	S_COMMAND	0x10		/* is a command line */
#define	S_READONLY	0x20		/* message line for readonly buffer */

#define	S_STATUS	(S_MESSAGE | S_COMMAND | S_READONLY)
							/* is a status line */

typedef struct virtscr {
    genptr	*pv_sys_ptr;	/* used by implementation for private data */

    unsigned	pv_rows;	/* rows and cols for this screen; filled */
    unsigned	pv_cols;	/* in by the virtscr implementation */

    genptr	*pv_window;	/* pointer to current Xviwin for this virtscr */
    Sline	*pv_int_lines;	/* state of internal display lines */
    Sline	*pv_ext_lines;	/* state of external display lines */

    int		pv_colours[5];
#define	VSCcolour	0
#define	VSCstatuscolour	1
#define	VSCroscolour	2
#define	VSCsyscolour	3
#define VSCtagcolour	4

/* public: */
    struct virtscr
		*(*v_open) P((struct virtscr *, genptr *));
    void	(*v_close) P((struct virtscr *));

    void	(*v_clear_all) P((struct virtscr *));
    void	(*v_clear_rows) P((struct virtscr *, int, int));
    void	(*v_clear_line) P((struct virtscr *, int, int));

    void	(*v_goto) P((struct virtscr *, int, int));
    void	(*v_advise) P((struct virtscr *, int, int, int, char *));

    void	(*v_write) P((struct virtscr *, int, int, char *));
    void	(*v_putc) P((struct virtscr *, int, int, int));

    void	(*v_set_colour) P((struct virtscr *, int));
    int		(*v_colour_cost) P((struct virtscr *));
    int		(*v_decode_colour) P((struct virtscr *, int, char *));

    void	(*v_flush) P((struct virtscr *));

    void	(*v_beep) P((struct virtscr *));

/* optional: do not use if NULL */
    void	(*v_insert) P((struct virtscr *, int, int, char *));

    int		(*v_scroll) P((struct virtscr *, int, int, int));

    void	(*v_flash) P((struct virtscr *));

    int		(*v_can_scroll) P((struct virtscr *, int, int, int));

} VirtScr;

#define	VSrows(vs)			(vs->pv_rows)
#define	VScols(vs)			(vs->pv_cols)

#define	VSopen(vs, win)			((*(vs->v_open))(vs, win))
#define	VSclose(vs)			((*(vs->v_close))(vs))
#define	VSresize(vs, top, bot, l, r)	((*(vs->v_resize))(vs, top, bot, l, r))
#define	VSclear_all(vs)			((*(vs->v_clear_all))(vs))
#define	VSclear_rows(vs, start, end)	((*(vs->v_clear_rows))(vs, start, end))
#define	VSclear_line(vs, row, col)	((*(vs->v_clear_line))(vs, row, col))
#define	VSgoto(vs, row, col)		((*(vs->v_goto))(vs, row, col))
#define	VSadvise(vs, r, c, i, str)	((*(vs->v_advise))(vs, r, c, i, str))
#define	VSwrite(vs, row, col, str)	((*(vs->v_write))(vs, row, col, str))
#define	VSputc(vs, row, col, c)		((*(vs->v_putc))(vs, row, col, c))
#define	VSset_colour(vs, colind)	((*((vs)->v_set_colour))(vs, \
						(vs)->pv_colours[colind]))
#define	VSdecode_colour(vs, ind, str)	((*(vs->v_decode_colour))(vs, ind, str))
#define	VScolour_cost(vs)		((*(vs->v_colour_cost))(vs))
#define	VSflush(vs)			((*(vs->v_flush))(vs))
#define	VSbeep(vs)			((*(vs->v_beep))(vs))
#define	VSinsert(vs, row, col, str)	((*(vs->v_insert))(vs, row, col, str))
#define	VSscroll(vs, start, end, n)	((*(vs->v_scroll))(vs, start, end, n))
#define	VScan_scroll(vs, start, end, n)	(\
				(vs->v_can_scroll != NULL) ? \
				    (*(vs->v_can_scroll))(vs, start, end, n) : \
				    (vs->v_scroll != NULL) \
			    )

/*
 * Editor input events are of type xvEvent.
 */
typedef struct xvevent {
    enum {
	Ev_char,
	Ev_timeout,
	Ev_refresh,
	Ev_resize,
	Ev_mouseclick,
	Ev_mousedrag,
	Ev_mousemove,
	Ev_breakin,			/* stop, I didn't mean that! I said stop! */
	Ev_suspend_request,		/* hold it, I want to do something else */
	Ev_terminate,			/* please just go away now */
	Ev_disconnected			/* sod this, I'm going down the pub */
    }			ev_type;	/* type of event */

    VirtScr		*ev_vs;		/* VirtScr it applies to */

    union {				/* special data associated with event */
	/* Ev_char: */
	int	evu_inchar;

	/* Ev_timeout: */

	/* Ev_reresh: */
	int	evu_do_clear;

	/* Ev_resize: */
	struct {
	    int	     evur_rows;
	    int      evur_columns;
	} evu_r;

	/* Ev_mouseclick: */
	/* Ev_mousedrag: */
	/* Ev_mousemove: */
	struct {
	    int	     evum_row;
	    int      evum_col;
	    int	     evum_endrow;
	    int      evum_endcol;
	} evu_m;
    }			ev_u;
} xvEvent;

#define	ev_inchar	ev_u.evu_inchar
#define	ev_do_clear	ev_u.evu_do_clear
#define	ev_rows		ev_u.evu_r.evur_rows
#define	ev_columns	ev_u.evu_r.evur_columns
#define	ev_m_row	ev_u.evu_m.evum_row
#define	ev_m_col	ev_u.evu_m.evum_col
#define	ev_m_endrow	ev_u.evu_m.evum_endrow
#define	ev_m_endcol	ev_u.evu_m.evum_endcol

/*
 * The editor responds to an event with one of these:
 */
typedef struct xvresponse {
    enum {
	Xvr_timed_input,
	Xvr_exit
    }		xvr_type;	/* type of event */

    long	xvr_timeout;	/* num milliseconds for timeout */
} xvResponse;
