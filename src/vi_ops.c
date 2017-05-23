/* Copyright (c) 1990,1991,1992,1993,1994 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    vi_ops.c
* module function:
    Handling of operators.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"
#include "cmd.h"

static	void	format_redo P((int, Cmd *));

void
xvOpShift(cmd)
register Cmd	*cmd;
{
    if (!start_command(cmd)) {
	return;
    }

    /*
     * Do the shift.
     */
    tabinout(cmd->cmd_operator, cmd->cmd_startpos.p_line,
				cmd->cmd_target.p_line, cmd);

    /*
     * Put cursor on first non-white of line; this is good if the
     * cursor is in the range of lines specified, which is normally
     * will be.
     */
    begin_line(TRUE);
    xvUpdateAllBufferWindows();

    /*
     * Construct redo buffer.
     */
    Redo.r_mode = r_normal;
    format_redo(cmd->cmd_operator, cmd);

    end_command();
}

/*
 * xvOpDelete - handle a delete operation
 * The characters cmd->cmd_ch1 and cmd->cmd_ch2 are the target character and
 * its argument (if it takes one) respectively. E.g. 'f', '/'.
 */
void
xvOpDelete(cmd)
register Cmd	*cmd;
{
    long	nlines;	    /* Number of logical lines the deletion affects */
    long	nplines;    /* If nlines == 1, nplines is the number of
			     * screen lines it occupied before the deletion.
			     */
    int		n;

    /* Don't do it if the motion was not valid */
    if (cmd->cmd_startpos.p_line == NULL) {
	return;
    }

    nlines = cntllines(cmd->cmd_startpos.p_line, cmd->cmd_target.p_line);
    if (nlines == 1) {
        nplines = plines(cmd->cmd_startpos.p_line);
    }

    if (!start_command(cmd)) {
	return;
    }

    /*
     * Do a yank of whatever we're about to delete. If there's too much
     * stuff to fit in the yank buffer, disallow the delete, since we
     * probably wouldn't have enough memory to do it anyway.
     */
    yp_push_deleted();
    if (!do_yank(&cmd->cmd_startpos, &cmd->cmd_target,
			    IsCharBased(cmd), cmd->cmd_yp_name)) {
	return;
    }

    if (IsLineBased(cmd)) {
	/*
	 * Put the cursor at the start of the section to be deleted
	 * so that repllines will correctly update it and the screen
	 * pointer, and update the screen.
	 */
	move_cursor(cmd->cmd_startpos.p_line, 0);
	repllines(cmd->cmd_startpos.p_line, nlines, (Line *) NULL);
	begin_line(TRUE);
	info_update();	/* bcos the current line might have changed */
    } else {
	if (cmd->cmd_startpos.p_line == cmd->cmd_target.p_line) {
	    /*
	     * Delete characters within line.
	     */
	    n = (cmd->cmd_target.p_index - cmd->cmd_startpos.p_index) + 1;
	    replchars(cmd->cmd_startpos.p_line,
		      cmd->cmd_startpos.p_index, n, "");
	} else {
	    /*
	     * Character-based delete between lines.
	     * So we actually have to do three deletes;
	     * one to delete to the end of the top line,
	     * one to delete the intervening lines, and
	     * one to delete up to the target position.
	     */
	    /*
	     * First delete part of the last line.
	     */
	    replchars(cmd->cmd_target.p_line, 0,
		      cmd->cmd_target.p_index + 1, "");

	    /*
	     * Now replace the rest of the top line with the
	     * remainder of the bottom line.
	     */
	    replchars(cmd->cmd_startpos.p_line,
		      cmd->cmd_startpos.p_index,
		      INT_MAX,
		      cmd->cmd_target.p_line->l_text);

	    /*
	     * Finally, delete all lines from (top + 1) to bot,
	     * inclusive.
	     */
	    repllines(cmd->cmd_startpos.p_line->l_next,
			    cntllines(cmd->cmd_startpos.p_line,
				      cmd->cmd_target.p_line) - 1,
			    (Line *) NULL);
	}

	/*
	 * After a char-based delete, the cursor should always be on the
	 * character following the last character of the section being deleted,
	 * i.e. the char now at what was the starting position.
	 * if that has been deleted, it goes on the last character of the line.
	 */
	/* If past end of line, move to end of line */
	{
	    Posn *p = &(cmd->cmd_startpos);
	    if (p->p_index > 0 && p->p_line->l_text[p->p_index] == '\0') {
		p->p_index--;
	    }
	}
	move_cursor(cmd->cmd_startpos.p_line,
			    cmd->cmd_startpos.p_index);
    }
    end_command();

    /*
     * This seems reasonable. It's what vi does anyway.
     */
    curwin->w_set_want_col = TRUE;

    /*
     * Construct redo buffer.
     */
    Redo.r_mode = r_normal;
    format_redo('d', cmd);

    /*
     * If the deletion only affected a single screen line,
     * don't bother updating the screen lines below it.
     */
    updateline(!(IsCharBased(cmd) && nlines == 1 && nplines == 1));
}

/*
 * xvOpChange - handle a change operation
 */
void
xvOpChange(cmd)
register Cmd	*cmd;
{
    /*
     * Start the command here so the initial delete gets
     * included in the meta-command and hence undo will
     * work properly.
     */
    if (!start_command(cmd)) {
	return;
    }

    if (IsLineBased(cmd)) {
	long	nlines;
	Line	*lp;

	/*
	 * This is a bit awkward ... for a line-based change, we don't
	 * actually delete the whole range of lines, but instead leave
	 * the first line in place and delete its text after the cursor
	 * position. However, yanking the whole thing is probably okay.
	 */
	yp_push_deleted();
	if (!do_yank(&cmd->cmd_startpos, &cmd->cmd_target,
						FALSE, cmd->cmd_yp_name)) {
	    return;
	}

	lp = cmd->cmd_startpos.p_line;

	nlines = cntllines(lp, cmd->cmd_target.p_line);
	if (nlines > 1) {
	    repllines(lp->l_next, nlines - 1, (Line *) NULL);
	}

	move_cursor(lp, 0);

	/*
	 * This is not right; it won't do the right thing when
	 * the cursor is in the whitespace of an indented line.
	 * However, it will do for the moment.
	 */
	begin_line(TRUE);

	replchars(lp, curwin->w_cursor->p_index, strlen(lp->l_text), "");
	xvUpdateAllBufferWindows();
    } else {
	bool_t	doappend;	/* true if we should do append, not insert */

	/*
	 * A character-based change really is just a delete and an insert.
	 * So use the deletion code to make things easier.
	 *
	 * We need to add text after the resulting cursor position if the
	 * changed text ended at the end of a line.  This happens when you
	 * c$, or when the target is a search that matches at the start of
	 * a line (so the change includes up the end of the previous line).
	 */
	doappend = endofline(&cmd->cmd_target);

	xvOpDelete(cmd);

	if (doappend) {
	    (void) one_right(TRUE);
	}
    }

    Redo.r_mode = r_insert;
    format_redo('c', cmd);

    startinsert(FALSE, 0);
}

void
xvOpYank(cmd)
register Cmd	*cmd;
{
    register long	nlines;

    if (cmd->cmd_target.p_line == NULL) {
	return;
    }

    /*
     * Report on the number of lines yanked.
     */
    nlines = cntllines(cmd->cmd_startpos.p_line, cmd->cmd_target.p_line);

    if (nlines > Pn(P_report)) {
	show_message("%ld lines yanked", nlines);
    }

    (void) do_yank(&cmd->cmd_startpos, &cmd->cmd_target,
		   IsCharBased(cmd), cmd->cmd_yp_name);
}

static void
format_redo(opchar, cmd)
int		opchar;
register Cmd	*cmd;
{
    flexclear(&Redo.r_fb);
    if (cmd->cmd_prenum != 0) {
	(void) lformat(&Redo.r_fb, "%c%ld%c%c",
				opchar, cmd->cmd_prenum,
				cmd->cmd_ch1, cmd->cmd_ch2);
    } else {
	(void) lformat(&Redo.r_fb, "%c%c%c", opchar,
				cmd->cmd_ch1, cmd->cmd_ch2);
    }
}
