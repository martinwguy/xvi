/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    param.c
* module function:
    Code to handle user-settable parameters. This is all pretty much
    table-driven. To add a new parameter, put it in the params array,
    and add a macro for it in param.h.

    The idea of the parameter table is that access to any particular
    parameter has to be fast, so it is done with a table lookup. This
    unfortunately means that the index of each parameter is recorded
    as a macro in param.h, so that file must be changed at the same
    time as the table below, and in the same way.

    When a parameter is changed, a function is called to do the actual
    work; this function is part of the parameter structure.  For many
    parameters, it's just a simple function that prints "not implemented";
    for most others, there are "standard" functions to set bool, numeric
    and string parameters, with a certain amount of checking.

    No bounds checking is done here; we should really include limits
    to numeric parameters in the table. Maybe this will come later.

    The data structures will be changed again shortly to enable
    buffer- and window-local parameters to be implemented.

    One problem with numeric parameters is that they are of type "int";
    this obviously places some restrictions on the sort of things they
    may be used for, and it may be necessary at some point to change
    this type to something like "unsigned long".
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"

#define nofunc	PFUNCADDR(NOFUNC)

/*
 * Default settings for string parameters.
 * These are set by the exported function init_params(),
 * which must be called before any parameters are accessed.
 */
#define	DEF_TAGS	"tags,/usr/lib/tags"
#define	DEF_PARA	"^($|\\.([ILPQ]P|LI|[plib]p))"
#define	DEF_SECTIONS	"^({|\\.([NS]H|HU|nh|sh))"
#define	DEF_SENTENCES	"([.!?][])\"']*(  |$))|^$"

/*
 * Default setting for roscolour and tagcolour parameters is
 * the same as the statuscolour if not specified otherwise.
 */
#ifndef	DEF_ROSCOLOUR
#define	DEF_ROSCOLOUR	DEF_STCOLOUR
#endif
#ifndef	DEF_TAGCOLOUR
#define	DEF_TAGCOLOUR	DEF_STCOLOUR
#endif

/*
 * Default settings for showing control- and meta-characters are
 * as for "normal" vi, i.e. "old" xvi without SHOW_META_CHARS set.
 */
#ifndef	DEF_CCHARS
#   define	DEF_CCHARS	FALSE
#endif
#ifndef	DEF_MCHARS
#   define	DEF_MCHARS	FALSE
#endif

/*
 * This is returned by the internal function findparam().
 */
typedef struct {
    Param	      *	mi_param;
    bool_t		mi_bool;
    char	      *	mi_str;
    bool_t		mi_query;
} matchinfo_t;

/*
 * This is returned by the internal function matchname().
 */
typedef enum {
    m_FAIL,
    m_PARTIAL,
    m_FULL
} match_t;

/*
 * Internal functions.
 */
static	bool_t	_do_set P((Xviwin *, matchinfo_t *, bool_t));
static	char	*parmstring P((Param *, int));
static	void	enum_usage P((Xviwin*, Param *));
static	bool_t	not_imp P((Xviwin *, Paramval, bool_t));
static	bool_t	xvpSetMagic P((Xviwin *, Paramval, bool_t));
static	bool_t	xvpSetRT P((Xviwin *, Paramval, bool_t));
static	bool_t	xvpSetTS P((Xviwin *, Paramval, bool_t));
static	bool_t	xvpSetColour P((Xviwin *, Paramval, bool_t));
static	bool_t	xvpSetVBell P((Xviwin *, Paramval, bool_t));
static	char	*par_show P((void));
static	match_t		matchname P((char *, char **, int));
static	matchinfo_t	*findparam P((Xviwin *, char *));

/*
 * These are the available parameters. The following are non-standard:
 *
 *	autodetect autogrow autonoedit autosplit colour edit equalsize
 *	format helpfile infoupdate jumpscroll preserve preservetime
 *	regextype roscolour statuscolour systemcolour tabindent vbell
 */
Param	params[] = {
/*  fullname        shortname       flags       value           function ... */
{   "ada",          "ada",          P_BOOL,     0,              not_imp,   },
{   "adapath",      "adap",         P_STRING,   0,              not_imp,   },
{   "autodetect",   "ad",           P_BOOL,     0,              nofunc,    },
{   "autogrow",     "ag",           P_BOOL,     TRUE,           nofunc,    },
{   "autoindent",   "ai",           P_BOOL,     0,              nofunc,    },
{   "autonoedit",   "an",           P_BOOL,     0,              nofunc,    },
{   "autoprint",    "ap",           P_BOOL,     0,              not_imp,   },
{   "autosplit",    "as",           P_NUM,      2,              nofunc,    },
{   "autowrite",    "aw",           P_BOOL,     0,              not_imp,   },
{   "beautify",     "bf",           P_BOOL,     0,              not_imp,   },
{   "cchars",       "cc",           P_BOOL,     DEF_CCHARS,     nofunc,    },
{   "colour",       "co",           P_STRING,   0,              xvpSetColour,},
{   "directory",    "di",           P_STRING,   0,              not_imp,   },
{   "edcompatible", "edc",          P_BOOL,     0,              not_imp,   },
{   "edit",         "edi",          P_BOOL,     TRUE,           set_edit,  },
{   "equalsize",    "eq",           P_BOOL,     TRUE,           nofunc,    },
{   "errorbells",   "eb",           P_BOOL,     TRUE,           nofunc,    },
{   "flash",        "flash",        P_BOOL,     FALSE,          xvpSetVBell, },
{   "format",       "fmt",          P_ENUM,     0,              set_format,},
{   "hardtabs",     "ht",           P_NUM,      0,              not_imp,   },
{   "helpfile",     "hf",           P_STRING,   0,              nofunc,    },
{   "ignorecase",   "ic",           P_BOOL,     0,              nofunc,    },
{   "infoupdate",   "iu",	    P_ENUM,     0,		nofunc,    },
{   "jumpscroll",   "js",           P_ENUM,     0,              nofunc,    },
{   "lisp",         "lisp",         P_BOOL,     0,              not_imp,   },
{   "list",         "ls",           P_BOOL,     0,              nofunc,    },
{   "magic",        "ma",           P_BOOL,     TRUE,           xvpSetMagic, },
{   "mchars",       "mc",           P_BOOL,     DEF_MCHARS,     nofunc,    },
{   "mesg",         "me",           P_BOOL,     0,              not_imp,   },
{   "minrows",      "mi",           P_NUM,      2,              nofunc,    },
{   "modeline",     "mo",           P_BOOL,     0,              not_imp,   },
{   "number",       "nu",           P_BOOL,     0,              nofunc,    },
{   "open",         "ope",          P_BOOL,     0,              not_imp,   },
{   "optimize",     "opt",          P_BOOL,     0,              not_imp,   },
{   "paragraphs",   "pa",           P_STRING,   0,              nofunc,    },
{   "preserve",     "psv",          P_ENUM,     0,              nofunc,    },
{   "preservetime", "psvt",         P_NUM,      5,              nofunc,    },
{   "prompt",       "pro",          P_BOOL,     0,              not_imp,   },
{   "readonly",     "ro",           P_BOOL,     0,              nofunc,	   },
{   "redraw",       "red",          P_BOOL,     0,              not_imp,   },
{   "regextype",    "rt",           P_ENUM,     0,              xvpSetRT,  },
{   "remap",        "rem",          P_BOOL,     0,              nofunc,    },
{   "report",       "rep",          P_NUM,      5,              nofunc,    },
{   "roscolour",    "rst",          P_STRING,   0,              xvpSetColour,},
{   "scroll",       "sc",           P_NUM,      0,              not_imp,   },
{   "sections",     "sec",          P_STRING,   0,              nofunc,    },
{   "sentences",    "sen",          P_STRING,   0,              nofunc,    },
{   "shell",        "sh",           P_STRING,   0,              nofunc,    },
{   "shiftwidth",   "sw",           P_NUM,      8,              nofunc,    },
{   "showmatch",    "sm",           P_BOOL,     0,              nofunc,    },
{   "slowopen",     "sl",           P_BOOL,     0,              not_imp,   },
{   "sourceany",    "so",           P_BOOL,     0,              not_imp,   },
{   "statuscolour", "st",           P_STRING,   0,              xvpSetColour,},
{   "systemcolour", "sy",           P_STRING,   0,              xvpSetColour,},
{   "tabindent",    "tabindent",    P_BOOL,     TRUE,           nofunc,    },
{   "tabs",         "tabs",         P_BOOL,     TRUE,           nofunc,    },
{   "tabstop",      "ts",           P_NUM,      8,              xvpSetTS,  },
{   "tagcolour",    "tc",           P_STRING,   0,              xvpSetColour,},
{   "taglength",    "tlh",          P_NUM,      0,              tagSetParam,},
{   "tags",         "tags",         P_LIST,     0,              tagSetParam,},
{   "term",         "term",         P_STRING,   0,              not_imp,   },
{   "terse",        "ters",         P_BOOL,     0,              not_imp,   },
{   "timeout",      "ti",           P_NUM,      DEF_TIMEOUT,    nofunc,    },
{   "ttytype",      "tt",           P_STRING,   0,              not_imp,   },
{   "undolevels",   "ul",           P_NUM,      MAX_UNDO,      set_undolevels,},
{   "vbell",        "vb",           P_BOOL,     FALSE,          xvpSetVBell, },
{   "warn",         "war",          P_BOOL,     TRUE,           nofunc,    },
{   "window",       "wi",           P_NUM,      0,              not_imp,   },
{   "wrapmargin",   "wm",           P_NUM,      0,              nofunc,    },
{   "wrapscan",     "ws",           P_BOOL,     TRUE,           nofunc,    },
{   "writeany",     "wa",           P_BOOL,     0,              not_imp,   },

{   (char *) NULL,  (char *) NULL,  0,          0,              nofunc,    },

};

/*
 * Special initialisations for string, list and enum parameters,
 * which we cannot put in the table above because C does not
 * allow the initialisation of unions.
 */
static struct {
    int		index;
    char	*value;
} init_str[] = {	/* strings and lists */
    P_colour,		DEF_COLOUR,
    P_helpfile,		HELPFILE,
    P_paragraphs,	DEF_PARA,
    P_roscolour,	DEF_ROSCOLOUR,
    P_sections,		DEF_SECTIONS,
    P_sentences,	DEF_SENTENCES,
    P_statuscolour,	DEF_STCOLOUR,
    P_systemcolour,	DEF_SYSCOLOUR,
    P_tags,		DEF_TAGS,
    P_tagcolour,	DEF_TAGCOLOUR,
};
#define	NSTRS	(sizeof(init_str) / sizeof(init_str[0]))

/*
 * Names of values for the P_jumpscroll enumerated parameter.
 *
 * It is essential that these are in the same order as the js_...
 * symbolic constants defined in xvi.h.
 */
static char *js_strings[] =
{
    "off",		/* js_OFF */
    "auto",		/* js_AUTO */
    "on",		/* js_ON */
    NULL
};

/*
 * Names of values for the P_infoupdate enumerated parameter.
 *
 * It is essential that these are in the same order as the iu_...
 * symbolic constants defined in xvi.h.
 */
static char *iu_strings[] =
{
    "terse",		/* iu_TERSE */
    "continuous",	/* iu_CONTINUOUS */
    NULL
};

static struct {
    int		index;
    int		value;
    char	**elist;
} init_enum[] = {	/* enumerations */
    P_format,		DEF_TFF,	fmt_strings,
    P_infoupdate,	iu_TERSE,	iu_strings,
    P_jumpscroll,	js_AUTO,	js_strings,
    P_preserve,		psv_STANDARD,	psv_strings,
    P_regextype,	rt_GREP,	rt_strings,
};
#define	NENUMS	(sizeof(init_enum) / sizeof(init_enum[0]))

/*
 * These are used by par_show().
 */
static	bool_t	show_all;
static	Param	*curparam;

/*
 * Initialise parameters.
 *
 * This function is called once from startup().
 */
void
init_params()
{
    Paramval	pv;
    Param	*pp;
    int		i;

    /*
     * First go through the special string and enum initialisation
     * tables, setting the values into the union field in the
     * parameter structures.
     */
    for (i = 0; i < NSTRS; i++) {
	set_param(init_str[i].index, init_str[i].value);
    }
    for (i = 0; i < NENUMS; i++) {
	set_param(init_enum[i].index, init_enum[i].value,
			init_enum[i].elist);
    }

    /*
     * Call any special functions that have been set up for
     * any parameters, using the default values included in
     * the parameter table.
     */
    for (pp = &params[0]; pp->p_fullname != NULL; pp++) {
	if (pp->p_func != nofunc && pp->p_func != PFUNCADDR(not_imp)) {
	    pv.pv_index = pp - &params[0];
	    switch (pp->p_flags & P_TYPE) {
	    case P_NUM:
	    case P_ENUM:
		pv.pv_i = pp->p_value;
		break;
	    case P_BOOL:
		pv.pv_b = pp->p_value;
		break;
	    case P_STRING:
		pv.pv_s = pp->p_str;
		break;
	    case P_LIST:
		pv.pv_l = pp->p_list;
	    }
	    (void) (*pp->p_func)(curwin, pv, FALSE);
	}
    }
}

void
exSet(window, argc, argv, inter)
Xviwin	*window;
int	argc;		/* number of parameters to set */
char	*argv[];	/* vector of parameter strings */
bool_t	inter;		/* TRUE if called interactively */
{
    Flexbuf		to_show;
    int	count;

    /*
     * First check to see if there were any parameters to set,
     * or if the user just wants us to display the parameters.
     */
    if (argc == 0 || (argc == 1 && (argv[0][0] == '\0' ||
				strncmp(argv[0], "all", 3) == 0))) {
	if (inter) {
	    int pcwidth;

	    show_all = (argc != 0 && argv[0][0] != '\0');
	    curparam = &params[0];
	    pcwidth = (window->w_ncols < 90 ?
			window->w_ncols :
			(window->w_ncols < 135 ?
			    window->w_ncols / 2 :
			    window->w_ncols / 3));

	    disp_init(window, par_show, pcwidth, FALSE);
	}

	return;
    }

    flexnew(&to_show);
    for (count = 0; count < argc; count++) {
	matchinfo_t	*mip;

	mip = findparam(window, argv[count]);
	if (mip == NULL) {
	    break;
	}
	if (mip->mi_query) {
	    (void) lformat(&to_show, "%s ", parmstring(mip->mi_param, 0));
	} else {
	    if (!_do_set(window, mip, inter)) {
		break;
	    }
	}
    }

    if (!flexempty(&to_show)) {
	show_message(window, "%s", flexgetstr(&to_show));
    }
    flexdelete(&to_show);

    if (inter) {

	/*
	 * Finally, update the screen in case we changed
	 * something like "tabstop" or "list" that will change
	 * its appearance. We don't always have to do this,
	 * but it's easier for now.
	 */
	redraw_all(window, TRUE);
    }
}

/*
 * Convert a string to an integer. The string may encode the integer
 * in octal, decimal or hexadecimal form, in the same style as C. On
 * return, make the string pointer, which is passed to us by
 * reference, point to the first character which isn't valid for the
 * base the number seems to be in.
 */
int
xv_strtoi(sp)
char	**sp;
{
    register char	*s;
    register int	i, c;
    bool_t		neg;

    i = 0;
    neg = FALSE;
    s = *sp;
    c = *s;
    while (is_space(c)) {
	c = *++s;
    }
    if (c == '-') {
	neg = TRUE;
	c = *++s;
    }
    while (is_space(c)) {
	c = *++s;
    }
    if (c == '0') {
	switch (c = *++s) {
	case 'x': case 'X':
	    /*
	     * We've got 0x ... or 0X ..., so it
	     * looks like a hex. number.
	     */
	    while ((c = *++s) != '\0' && is_xdigit(c)) {
		i = (i * 16) + hex_to_bin(c);
	    }
	    break;

	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
	    /*
	     * It looks like an octal number.
	     */
	    do {
		i = (i * 8) + c - '0';
	    } while ((c = *++s) != '\0' && is_octdigit(c));
	    break;

	default:
	    *sp = s;
	    return(0);
	}
    } else {
	/*
	 * Assume it's decimal.
	 */
	while (c != '\0' && is_digit(c)) {
	    i = (i * 10) + c - '0';
	    c = *++s;
	}
    }
    *sp = s;
    return(neg ? -i : i);
}

/*
 * matchname(): attempt to match parameter name with string terminated
 * by specified delimiter, looking for either a full or partial match.
 * If successful, & if delimiter is not '\0', return pointer to string
 * following delimiter.
 */
static match_t
matchname(name, argp, delim)
    register char *name;
    char	**argp;
    int		delim;
{
    register char *arg;
    register int nc;
    register int ac;

    arg = *argp;
    while ((nc = *name++) == (ac = *arg++) && nc != '\0') {
	;
    }
    if (ac != delim) {
	return m_FAIL;
    }
    if (ac != '\0') {
	*argp = arg;
    }
    if (nc != '\0') {
	return m_PARTIAL;
    }
    return m_FULL;
}

/*
 * findparam(): find entry in parameter table, given a complete match
 * of its full name, a complete match of its short name, or a unique
 * partial match of its full name.
 *
 * Also return boolean value to be assigned, for a boolean parameter,
 * & string value to be assigned - after decoding, if required - for
 * any other type.
 *
 * If the field mi_query is TRUE, the parameter is not being set; we
 * are to show its current value. This is true when no value at all
 * is given for a non-Boolean parameter, or when a Boolean parameter
 * name is immediately followed by a '?'.
 */
static matchinfo_t *
findparam(win, arg)
    Xviwin	      *win;
    char	      *arg;
{
    register Param	*pp;
    int			nfull;		/* number of full matches */
    int			npartial;	/* number of partial matches */
    static matchinfo_t	lastfull;
    static matchinfo_t	lastpartial;
    register int	c0;

    c0 = arg[0];
    nfull = npartial = 0;
    for (pp = &params[0]; pp->p_fullname != NULL; pp++) {
	int		result;
	int		delim;
	char		*fullname;
	char		*shortname;
	char		*arg1;
	char		*arg2;
	register bool_t	gotfull;
	register bool_t	gotpartial;
	bool_t		isquery;

	fullname = pp->p_fullname;
	shortname = pp->p_shortname;
	isquery = FALSE;

	if ((pp->p_flags & P_TYPE) == P_BOOL) {
	    gotfull = gotpartial = FALSE;
	    /*
	     * If it's a boolean parameter, look for leading "no"
	     * followed by parameter name.
	     */
	    if (c0 == 'n' && arg[1] == 'o') {
		arg1 = &arg[2];
		if (matchname(shortname, &arg1, '\0') == m_FULL) {
		    gotfull = TRUE;
		}
		arg1 = &arg[2];
		if ((result = matchname(fullname, &arg1, '\0')) != m_FAIL) {
		    if (result == m_PARTIAL) {
			gotpartial = TRUE;
		    } else {
			gotfull = TRUE;
		    }
		}
		if (gotfull) {
		    nfull++;
		    lastfull.mi_param = pp;
		    lastfull.mi_bool = FALSE;
		    lastfull.mi_query = FALSE;
		}
		if (gotpartial) {
		    npartial++;
		    lastpartial.mi_param = pp;
		    lastpartial.mi_bool = FALSE;
		    lastpartial.mi_query = FALSE;
		}
	    }
	    delim = '\0';
	} else {
	    delim = '=';
	}
	gotfull = gotpartial = FALSE;
	if (c0 == shortname[0]) {
	    arg1 = arg;
	    if (matchname(shortname, &arg1, delim) == m_FULL) {
		gotfull = TRUE;
	    } else if (
		    /*
		     * See if it's a query.
		     */
		    matchname(shortname, &arg1, '?') == m_FULL
		    ||
		    (
			(pp->p_flags & P_TYPE) != P_BOOL
			&&
			matchname(shortname, &arg1, '\0') == m_FULL
		    )
	    ) {
		gotfull = TRUE;
		isquery = TRUE;
	    }
	}
	if (c0 == fullname[0]) {
	    arg2 = arg;
	    result = matchname(fullname, &arg2, delim);
	    if (result == m_FAIL) {
		/*
		 * Didn't match with the setting delimiter but
		 * it might be a query ...
		 */
		result = matchname(fullname, &arg2, '?');
		if (result == m_FAIL && (pp->p_flags & P_TYPE) != P_BOOL) {
		    result = matchname(fullname, &arg2, '\0');
		}
		if (result != m_FAIL) {
		    isquery = TRUE;
		}
	    }
	    if (result != m_FAIL) {
		if (result == m_PARTIAL) {
		    gotpartial = TRUE;
		} else {
		    gotfull = TRUE;
		    arg1 = arg2;
		}
	    }
	}

	if (gotfull) {
	    nfull++;
	    lastfull.mi_param = pp;
	    lastfull.mi_str = arg1;
	    lastfull.mi_bool = TRUE;
	    lastfull.mi_query = isquery;
	}
	if (gotpartial) {
	    npartial++;
	    lastpartial.mi_param = pp;
	    lastpartial.mi_str = arg2;
	    lastpartial.mi_bool = TRUE;
	    lastpartial.mi_query = isquery;
	}
    }
    switch (nfull) {
	case 1:
	    return &lastfull;
	case 0:
	    if (npartial == 1) {
		return &lastpartial;
	    }
	    if (win) {
		show_error(win, npartial == 0 ?
				    "Invalid parameter setting" :
				    "Ambiguous parameter name");
	    }
	    break;
	default:
	    if (win) {
		show_error(win, "INTERNAL ERROR: duplicate parameter names");
	    }
    }
    return NULL;
}

/*
 * Internal version of exSet(). Returns TRUE if set of specified
 * parameter was successful, otherwise FALSE.
 */
static bool_t
_do_set(window, mip, inter)
Xviwin			*window;	/* NULL if not called interactively */
matchinfo_t		*mip;		/* details of the parameter */
bool_t			inter;		/* TRUE if called interactively */
{
    Paramval		val;		/* value to set */
    Param		*pp;
    char		*cp;

    pp = mip->mi_param;

    /*
     * Do any type-specific checking, and set up the
     * "val" union to contain the (decoded) value.
     */

    cp = mip->mi_str;

    switch (pp->p_flags & P_TYPE) {
    case P_NUM:

	val.pv_i = xv_strtoi(&cp);
	/*
	 * If there are extra characters after the number,
	 * don't accept it.
	 */
	if (*cp != '\0') {
	    if (window) {
		show_error(window, "Invalid numeric parameter");
	    }
	    return(FALSE);
	}
	break;

    case P_ENUM:
    {
	char	     **	ep;

	for (ep = pp->p_elist; *ep != NULL; ep++) {
	    if (strcmp(*ep, cp) == 0)
		break;
	}

	if (*ep == NULL) {
	    if (window != NULL && inter) {
		enum_usage(window, pp);
	    }
	    return(FALSE);
	}

	val.pv_i = ep - pp->p_elist;
	break;
    }

    case P_STRING:
    case P_LIST:
	val.pv_s = cp;
	break;

    case P_BOOL:
	val.pv_b = mip->mi_bool;
	break;
    }

    /*
     * Call the check function if there is one.
     */
    val.pv_index = pp - &params[0];
    if (pp->p_func != nofunc &&
	(*pp->p_func)(window, val, (window != NULL)) == FALSE) {
	return(FALSE);
    }

    /*
     * Set the value.
     */
    switch (pp->p_flags & P_TYPE) {
    case P_NUM:
    case P_ENUM:
	pp->p_value = val.pv_i;
	break;

    case P_BOOL:
	pp->p_value = val.pv_b;
	break;

    case P_STRING:
    case P_LIST:
	set_param(pp - params, val.pv_s);
	break;
    }
    pp->p_flags |= P_CHANGED;

    /*
     * Got through all the checking, so we're okay.
     */
    return TRUE;
}

/*
 * Internal parameter setting - parameters are not considered
 * to have been changed unless they are set by the user.
 *
 * All types of parameter are handled by this function.
 * Enumerated types are special, because two arguments
 * are expected: the first, always present, is the index,
 * but the second may be either a valid list of strings
 * a NULL pointer, the latter indicating that the list
 * of strings is not to be changed.
 *
 * The P_LIST type accepts a string argument and converts
 * it to a vector of strings which is then stored.
 */
/*VARARGS1*/
void
#ifdef	__STDC__
    set_param(int n, ...)
#else
    set_param(n, va_alist)
    int		n;
    va_dcl
#endif
{
    va_list		argp;
    Param		*pp;
    char		*cp;
    char		**elist;

    pp = &params[n];

    VA_START(argp, n);

    switch (pp->p_flags & P_TYPE) {
    case P_ENUM:
	pp->p_value = va_arg(argp, int);

	/*
	 * If the second argument is a non-NULL list of
	 * strings, set it into the parameter structure.
	 * Note that this is not dependent on the return
	 * value from the check function being TRUE,
	 * since the check cannot be made until the
	 * array of strings is in place.
	 */
	elist = va_arg(argp, char **);
	if (elist != NULL) {
	    pp->p_elist = elist;
	}
	break;

    case P_BOOL:
    case P_NUM:
	pp->p_value = va_arg(argp, int);
	break;

    case P_LIST:
	{
	    int	argc;
	    char	**argv;

	    cp = strsave(va_arg(argp, char *));
	    if (cp == NULL) {
		/*
		 * This is not necessarily a good idea.
		 */
		show_error(curwin, "Can't allocate memory for parameter");
		return;
	    }

	    makeargv(cp, &argc, &argv, " \t,");
	    if (argc == 0 || argv == NULL) {
		free(cp);
		return;
	    }
	    if (pp->p_list != NULL) {
		if (pp->p_list[0] != NULL) {
		    free(pp->p_list[0]);
		}
		free((char *) pp->p_list);
	    }
	    pp->p_list = argv;
	}
	break;

    case P_STRING:
	cp = strsave(va_arg(argp, char *));
	if (cp == NULL) {
	    /*
	     * This is not necessarily a good idea.
	     */
	    show_error(curwin, "Can't allocate memory for parameter");
	} else {
	    /*
	     * We always free up the old string, because
	     * it must have been allocated at some point.
	     */
	    if (pp->p_str != NULL) {
		free(pp->p_str);
	    }
	    pp->p_str = cp;
	}
	break;
    }

    va_end(argp);
}

/*
 * Display helpful usage message for an enumerated parameter, listing
 * the legal values for it.
 */
static void
enum_usage(w, pp)
    Xviwin		*w;
    Param		*pp;
{
    Flexbuf		s;
    char		**sp;

    flexnew(&s);
    (void) lformat(&s, "Must be one of:");
    for (sp = (char **) pp->p_str; *sp != NULL; sp++) {
	(void) lformat(&s, " %s", *sp);
    }
    show_error(w, "%s", flexgetstr(&s));
    flexdelete(&s);
}

/*
 * Return a string representation for a single parameter.
 */
static char *
parmstring(pp, leading)
Param	*pp;			/* parameter */
int	leading;		/* number of leading spaces in string */
{
    static Flexbuf	b;

    flexclear(&b);
    while (leading-- > 0) {
	(void) flexaddch(&b, ' ');
    }
    switch (pp->p_flags & P_TYPE) {
    case P_BOOL:
	(void) lformat(&b, "%s%s", (pp->p_value ? "" : "no"), pp->p_fullname);
	break;

    case P_NUM:
	(void) lformat(&b, "%s=%d", pp->p_fullname, pp->p_value);
	break;

    case P_ENUM:
    {
	int	n;
	char	*estr;

	for (n = 0; ; n++) {
	    if ((estr = ((char **) pp->p_str)[n]) == NULL) {
		estr = "INTERNAL ERROR";
		break;
	    }
	    if (n == pp->p_value)
		break;
	}
	(void) lformat(&b, "%s=%s", pp->p_fullname, estr);
	break;
    }

    case P_STRING:
	(void) lformat(&b, "%s=%s", pp->p_fullname,
				    (pp->p_str != NULL) ? pp->p_str : "");
	break;

    case P_LIST:
	{
	    register char	**cpp;

	    (void) lformat(&b, "%s=%s", pp->p_fullname, pp->p_list[0]);
	    for (cpp = pp->p_list + 1; *cpp != NULL; cpp++) {
		(void) lformat(&b, " %s", *cpp);
	    }
	}
	break;
    }
    return(flexgetstr(&b));
}

static char *
par_show()
{
    static bool_t	started = FALSE;

    if (!started) {
	started = TRUE;
	return("Parameters:");
    }

    for ( ; curparam->p_fullname != NULL; curparam++) {

	/*
	 * Ignore unimplemented parameters.
	 */
	if (curparam->p_func != PFUNCADDR(not_imp)) {
	    /*
	     * Display all parameters if show_all is set;
	     * otherwise, just display changed parameters.
	     */
	    if (show_all || (curparam->p_flags & P_CHANGED) != 0) {
		break;
	    }
	}
    }

    if (curparam->p_fullname == NULL) {
	started = FALSE;		/* reset for next time */
	return(NULL);
    } else {
	char	*retval;

	retval = parmstring(curparam, 3);
	curparam++;
	return(retval);
    }
}

/*ARGSUSED*/
static bool_t
not_imp(window, new_value, interactive)
Xviwin		*window;
Paramval	new_value;
bool_t		interactive;
{
    if (interactive) {
	show_message(window, "That parameter is not implemented!");
    }
    return(TRUE);
}

/*ARGSUSED*/
static bool_t
xvpSetMagic(window, new_value, interactive)
Xviwin			*window;
Paramval		new_value;
bool_t			interactive;
{
    static int	prev_rt = rt_GREP;

    if (new_value.pv_b) {
	/*
	 * Turn magic on.
	 */
	if (Pn(P_regextype) == rt_TAGS) {
	    set_param(P_regextype, prev_rt, (char **) NULL);
	    P_setchanged(P_regextype);
	}
    } else {
	/*
	 * Turn magic off.
	 */
	if (Pn(P_regextype) != rt_TAGS) {
	    prev_rt = Pn(P_regextype);
	    set_param(P_regextype, rt_TAGS, (char **) NULL);
	    P_setchanged(P_regextype);
	}
    }
    return(TRUE);
}

/*ARGSUSED*/
static bool_t
xvpSetRT(window, new_value, interactive)
Xviwin		*window;
Paramval	new_value;
bool_t		interactive;
{
    switch (new_value.pv_i) {
    case rt_TAGS:
    case rt_GREP:
    case rt_EGREP:
	set_param(P_magic, (new_value.pv_i != rt_TAGS));
	return(TRUE);
    default:
	return(FALSE);
    }
}

/*ARGSUSED*/
static bool_t
xvpSetTS(window, new_value, interactive)
Xviwin		*window;
Paramval	new_value;
bool_t		interactive;
{
    int i;

    if ((i = new_value.pv_i) <= 0 || i > MAX_TABSTOP) {
	if (interactive) {
	    show_error(window, "Invalid tab size specified");
	}
	return(FALSE);
    }
    return TRUE;
}

/*ARGSUSED*/
static bool_t
xvpSetColour(w, new_value, interactive)
Xviwin		*w;
Paramval	new_value;
bool_t		interactive;
{
    int		which;

    switch (new_value.pv_index) {
    case P_colour:		which = VSCcolour;		break;
    case P_tagcolour:		which = VSCtagcolour;		break;
    case P_statuscolour:	which = VSCstatuscolour;	break;
    case P_systemcolour:	which = VSCsyscolour;		break;
    case P_roscolour:		which = VSCroscolour;
    }
    return(VSdecode_colour(w->w_vs, which, new_value.pv_s));
}

/*
 * "flash" and "vbell" are synonyms, from different historical streams.
 */
/*ARGSUSED*/
static bool_t
xvpSetVBell(w, new_value, interactive)
Xviwin		*w;
Paramval	new_value;
bool_t		interactive;
{
    if (new_value.pv_b != Pb(P_vbell)) {
	set_param(P_flash, new_value.pv_b);
	set_param(P_vbell, new_value.pv_b);
	P_setchanged(P_flash);
	P_setchanged(P_vbell);
    }
    return(TRUE);
}
