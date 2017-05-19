/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    param.h
* module function:
    Definitions for parameter access.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

/*
 * Settable parameters.
 */

typedef struct {
    int		pv_index;	/* index of parameter */
    union {
	int	pvu_i;		/* numeric value or enumeration index */
	bool_t	pvu_b;		/* boolean value */
	char	*pvu_s;		/* string value */
	char	**pvu_l;	/* enumeration: list of possible values */
    } pvu;
} Paramval;
#define	pv_i	pvu.pvu_i
#define	pv_b	pvu.pvu_b
#define	pv_s	pvu.pvu_s
#define	pv_l	pvu.pvu_l

typedef	struct	param {
    char	*p_fullname;	/* full parameter name */
    char	*p_shortname;	/* permissible abbreviation */
    int		p_flags;	/* type of parameter, and whether implemented */

    /*
     * This field is used for numeric and boolean values,
     * and for storing the numeric representation for enums.
     */
    int		p_value;

    /*
     * Special function to set parameter.
     * This is called whenever the parameter is set by the user.
     */
    bool_t	(*p_func) P((Paramval, bool_t));

    /*
     * This field is used for strings, lists and enums.
     * Note that making this the last field allows us to initialise the
     * parameter table, since we can just leave this field uninitialised.
     */
    union {
	char	*pu_str;
	char	**pu_list;
    } pu;
} Param;

#define	p_num	p_value
#define	p_bool	p_value
#define	p_eval	p_value

#define	p_str	pu.pu_str
#define	p_list	pu.pu_list
#define	p_elist	pu.pu_list

#ifdef __ZTC__
#   define	PFUNCADDR(f)	((bool_t (*)(Xviwin *, Paramval, bool_t))(f))
#else
#   define	PFUNCADDR(f)	(f)
#endif

extern	Param	params[];

/*
 * Flags
 */
#define	P_BOOL		0x01	/* the parameter is boolean */
#define	P_NUM		0x02	/* the parameter is numeric */
#define	P_STRING	0x04	/* the parameter is a string */
#define	P_ENUM		0x08	/* the parameter is an enumerated type */
#define	P_LIST		0x20	/* the parameter is a list of strings */

#define	P_TYPE		0x2f	/* used to mask out type from other bits */

#define	P_CHANGED	0x10	/* the parameter has been changed */

/*
 * Macros to set/get the value of a parameter.
 * One each for the four parameter types.
 * May change implementation later to do more checking.
 */
#define	Pn(n)		(params[n].p_num)
#define	Pb(n)		(params[n].p_bool)
#define	Ps(n)		(params[n].p_str)
#define	Pl(n)		(params[n].p_list)
#define	Pen(n)		(params[n].p_value)
#define	Pes(n)		(params[n].p_elist[params[n].p_eval])

#define P_ischanged(n)	(params[n].p_flags & P_CHANGED)
#define P_setchanged(n)	(params[n].p_flags |= P_CHANGED)

/*
 * The following are the indices in the params array for each parameter.
 * They must not be changed without also changing the table in "param.c".
 */
enum {
    P_ada = 0,
    P_adapath,
    P_autodetect,
    P_autoindent,
    P_autoprint,
    P_autosplit,
    P_autowrite,
    P_beautify,
    P_cchars,
    P_colour,
    P_directory,
    P_edcompatible,
    P_errorbells,
    P_exrc,
    P_flash,
    P_format,
    P_hardtabs,
    P_helpfile,
    P_ignorecase,
    P_infoupdate,
    P_jumpscroll,
    P_lisp,
    P_list,
    P_magic,
    P_mchars,
    P_mesg,
    P_minrows,
    P_modeline,
    P_number,
    P_open,
    P_optimize,
    P_paragraphs,
    P_posix,
    P_preserve,
    P_preservetime,
    P_prompt,
    P_readonly,
    P_redraw,
    P_regextype,
    P_remap,
    P_report,
    P_roscolour,
    P_scroll,
    P_sections,
    P_sentences,
    P_shell,
    P_shiftwidth,
    P_showmatch,
    P_showmode,
    P_slowopen,
    P_sourceany,
    P_statuscolour,
    P_systemcolour,
    P_tabindent,
    P_tabs,
    P_tabstop,
    P_taglength,
    P_tags,
    P_term,
    P_terse,
    P_timeout,
    P_ttytype,
    P_vbell,
    P_warn,
    P_window,
    P_wrapmargin,
    P_wrapscan,
    P_writeany,
    P_sentinel	/* for check in param.c */
};
