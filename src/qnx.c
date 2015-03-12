/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    qnx.c
* module function:
    QNX system interface module.

    Note that this module assumes the C86 compiler,
    which is an ANSI compiler, rather than the standard
    QNX compiler, which is not.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

/*
 * QNX-specific include files.
 *
 * <stdio.h> etc. get included by "xvi.h".
 */
#include <io.h>
#include <dev.h>
#include <timer.h>
#include <process.h>
#include <systids.h>
#include <sys/stat.h>
#include <taskmsgs.h>

#include "xvi.h"

#define	PIPE_MASK_1	"/tmp/xvipipe1XXXXXX"
#define	PIPE_MASK_2	"/tmp/xvipipe2XXXXXX"
#define	EXPAND_MASK	"/tmp/xvexpandXXXXXX"

static	int		old_options;

/*
 * Returns TRUE if file does not exist or exists and is writeable.
 */
bool_t
can_write(file)
char	*file;
{
    return(access(file, W_OK) == 0 || access(file, F_OK) != 0);
}

bool_t
exists(file)
char *file;
{
    return(access(file, F_OK) == 0);
}

int
call_shell(sh)
char	*sh;
{
    return(spawnlp(0, sh, sh, (char *) NULL));
}

void
Wait200ms()
{
    (void) set_timer(TIMER_WAKEUP, RELATIVE, 2, 0, NULL);
}

void
sys_exit(r)
int	r;
{
    sys_endv();
    exit(r);
}

void
sys_startv()
{
    /*
     * Turn off:
     *	EDIT	to get "raw" mode
     *	ECHO	to stop characters being echoed
     *	MAPCR	to stop mapping of \r into \n
     */
    old_options = get_option(stdin);
    set_option(stdin, old_options & ~(EDIT | ECHO | MAPCR));
}

void
sys_endv()
{
    tty_endv();

    /*
     * This flushes the typeahead buffer so that recall of previous
     * commands will not work - this is desirable because some of
     * the commands typed to vi (i.e. control characters) will
     * have a deleterious effect if given to the shell.
     */
    flush_input(stdin);

    set_option(stdin, old_options);
}

/*
 * Construct unique name for temporary file,
 * to be used as a backup file for the named file.
 */
char *
tempfname(srcname)
char	*srcname;
{
    char	*newname;		/* ptr to allocated new pathname */
    char	*last_slash;		/* ptr to last slash in srcname */
    char	*last_hat;		/* ptr to last slash in srcname */
    int		tail_length;		/* length of last component */
    int		head_length;		/* length up to (including) '/' */

    /*
     * Obtain the length of the last component of srcname.
     */
    last_slash = strrchr(srcname, '/');
    last_hat = strrchr(srcname, '^');
    if ((last_slash == NULL) ||
	(last_slash != NULL && last_hat != NULL && last_hat > last_slash)) {
	last_slash = last_hat;
    }

    /*
     * last_slash now points at the last delimiter in srcname,
     * or is NULL if srcname contains no delimiters.
     */
    if (last_slash != NULL) {
	tail_length = strlen(last_slash + 1);
	head_length = last_slash - srcname;
    } else {
	tail_length = strlen(srcname);
	head_length = 0;
    }

    /*
     * We want to add ".tmp" onto the end of the name,
     * and QNX restricts filenames to 16 characters.
     * So we must ensure that the last component
     * of the pathname is not more than 12 characters
     * long, or truncate it so it isn't.
     */
    if (tail_length > 12)
	tail_length = 12;

    /*
     * Create the new pathname. We need the total length
     * of the path as is, plus ".tmp" plus a null byte.
     */
    newname = alloc(head_length + tail_length + 5);
    if (newname != NULL) {
	(void) strncpy(newname, srcname, head_length + tail_length);
    }

    {
	char	*endp;
	int	indexnum;

	endp = newname + head_length + tail_length;
	indexnum = 0;
	do {
	    /*
	     * Keep trying this until we get a unique file name.
	     */
	    if (indexnum > 0) {
		(void) sprintf(endp, ".%03d", indexnum);
	    } else {
		(void) strcpy(endp, ".tmp");
	    }
	    indexnum++;
	} while (access(newname, 0) == 0 && indexnum < 1000);
    }

    return(newname);
}

/*
 * Fake out a pipe by writing output to temp file, running a process with
 * i/o redirected from this file to another temp file, and then reading
 * the second temp file back in.
 *
 * Either (but not both) of writefunc or readfunc may be NULL, indicating
 * that no output or input is necessary.
 */
bool_t
sys_pipe(cmd, writefunc, readfunc)
char	*cmd;
int	(*writefunc) P((FILE *));
long	(*readfunc) P((FILE *));
{
    Flexbuf		cmdbuf;
    static char	*args[] = { NULL, "+s", NULL, NULL };
    char		*temp1;
    char		*temp2;
    char		buffer[512];
    FILE		*fp;
    bool_t		retval = FALSE;

    if (Ps(P_shell) == NULL) {
	return(FALSE);
    }
    args[0] = Ps(P_shell);

    flexnew(&cmdbuf);
    temp1 = temp2 = NULL;

    /*
     * From this point, any failures must go through
     * the cleanup code at the end of the routine.
     */

    /*
     * Open output/input files as necessary.
     */
    if (writefunc != NULL) {
	temp1 = mktemp(PIPE_MASK_1);
	if (temp1 == NULL) {
	    goto ret;
	}

	fp = fopen(temp1, "w+");
	if (fp == NULL) {
	    goto ret;
	}
	(void) (*writefunc)(fp);
	(void) fclose(fp);
    }
    if (readfunc != NULL) {
	temp2 = mktemp(PIPE_MASK_2);
	if (temp2 == NULL) {
	    goto ret;
	}
    }

    /*
     * Run the command, with appropriate redirection.
     */
    (void) lformat(&cmdbuf, "%s < %s > %s", cmd,
    			(writefunc != NULL) ? temp1 : "$null",
			(readfunc != NULL) ? temp2 : "$null");
    args[2] = flexgetstr(&cmdbuf);

    sys_endv();
    if (createv(buffer, 0, -1, 0, BEQUEATH_TTY, 0, args, NULL) != 0) {
	goto ret;
    }

    /*
     * Read the output from the command back in, if necessary.
     */
    if (readfunc != NULL) {
	fp = fopen(temp2, "r");
	if (fp == NULL) {
	    goto ret;
	}
	(void) (*readfunc)(fp);
	(void) fclose(fp);
    }

    retval = TRUE;			/* success! */

ret:
    flexdelete(&cmdbuf);

    if (temp1 != NULL) {
	(void) remove(temp1);
	free(temp1);
    }
    if (temp2 != NULL) {
	(void) remove(temp2);
	free(temp2);
    }

    sys_startv();

    return(retval);
}

/*
 * Expand filename.
 * Have to do this using the shell, using a /tmp filename in the vain
 * hope that it will be on a ramdisk and hence reasonably fast.
 */
char *
fexpand(name, do_completion)
char	*name;
bool_t	do_completion;
{
    static char	meta[] = "*?#{}";
    int		has_meta;
    static char	*args[] = { NULL, "+s", NULL, NULL };
    static char	*io[4];
    Flexbuf	cmdbuf;
    char	*temp = NULL;
    FILE	*fp;
    static char	newname[256];
    char	*cp;

    if (name == NULL || *name == '\0') {
	return(name);
    }

    has_meta = do_completion;
    for (cp = meta; *cp != '\0'; cp++) {
	if (strchr(name, *cp) != NULL) {
	    has_meta = TRUE;
	    break;
	}
    }
    if (!has_meta) {
	return(name);
    }

    if (Ps(P_shell) == NULL) {
	goto fail;
    }
    args[0] = Ps(P_shell);

    temp = mktemp(EXPAND_MASK);

    if (temp == NULL) {
	goto fail;
    }

    flexnew(&cmdbuf);
    (void) lformat(&cmdbuf, "echo %s%c > %s", name, do_completion ? '*' : ' ',
    							temp);
    args[2] = flexgetstr(&cmdbuf);

    io[0] = io[1] = io[2] = "$tty99";
    if (createv(NULL, 0, -1, 0, 0, 0, args, NULL) != 0) {
	goto fail;
    }
    flexdelete(&cmdbuf);

    fp = fopen(temp, "r");
    if (fp == NULL) {
	goto fail;
    }

    if (fgets(newname, sizeof(newname), fp) == NULL) {
	(void) fclose(fp);
	goto fail;
    }
    (void) fclose(fp);
    (void) remove(temp);
    free(temp);

    cp = strchr(newname, '\n');
    if (cp != NULL) {
	*cp = '\0';
    }

    /*
     * Check for no expansion if do_completion option is set.
     */
    if (do_completion && strncmp(newname, name, strlen(name)) == 0 &&
			    newname[strlen(name)] == '*') {
	/*
	 * Unable to complete filename - return zero length string.
	 */
	return("");
    } else {
	return(newname);
    }
    
fail:
    if (temp != NULL) {
	free(temp);
	(void) remove(temp);
    }
    return(name);
}
