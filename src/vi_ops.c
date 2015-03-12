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
    /*
     * Do the shift.
     */
    tabinout(cmd->cmd_operator, cmd->cmd_startpos.p_line,
    				cmd->cmd_target.p_line);

    /*
     * Put cursor on first non-white of line; this is good if the
     * cursor is in the range of lines specified, which is normally
     * will be.
     */
    begin_line(curwin, TRUE);
    xvUpdateAllBufferWindows(curbuf);

    /*
     * Construct redo buffer.
     */
    Redo.r_mode = r_normal;
    format_redo(cmd->cmd_operator, cmd);
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
    long	nlines;
    int		n;

    nlines = cntllines(cmd->cmd_startpos.p_line, cmd->cmd_target.p_line);

    /*
     * Do a yank of whatever we're about to delete. If there's too much
     * stuff to fit in the yank buffer, disallow the delete, since we
     * probably wouldn't have enough memory to do it anyway.
     */
    yp_push_deleted();
    if (!do_yank(curbuf, &cmd->cmd_startpos, &cmd->cmd_target,
			    IsCharBased(cmd), cmd->cmd_yp_name)) {
	show_error(curwin, "Not enough memory to perform delete");
	return;
    }

    if (!IsCharBased(cmd)) {
	/*
	 * Put the cursor at the start of the section to be deleted
	 * so that repllines will correctly update it and the screen
	 * pointer, and update the screen.
	 */
	move_cursor(curwin, cmd->cmd_startpos.p_line, 0);
	repllines(curwin, cmd->cmd_startpos.p_line, nlines, (Line *) NULL);
	begin_line(curwin, TRUE);
    } else {
	/*
	 * After a char-based delete, the cursor should always be
	 * on the character following the last character of the
	 * section being deleted. The easiest way to achieve this
	 * is to put it on the character before the section to be
	 * deleted (which will not be affected), and then move one
	 * place right afterwards.
	 */
	move_cursor(curwin, cmd->cmd_startpos.p_line,
			    cmd->cmd_startpos.p_index -
				((cmd->cmd_startpos.p_index > 0) ? 1 : 0));

	if (cmd->cmd_startpos.p_line == cmd->cmd_target.p_line) {
	    /*
	     * Delete characters within line.
	     */
	    n = (cmd->cmd_target.p_index - cmd->cmd_startpos.p_index) + 1;
	    replchars(curwin, cmd->cmd_startpos.p_line,
	    			cmd->cmd_startpos.p_index, n, "");
	} else {
	    /*
	     * Character-based delete between lines.
	     * So we actually have to do three deletes;
	     * one to delete to the end of the top line,
	     * one to delete the intervening lines, and
	     * one to delete up to the target position.
	     */
	    if (!start_command(curwin)) {
		return;
	    }

	    /*
	     * First delete part of the last line.
	     */
	    replchars(curwin, cmd->cmd_target.p_line, 0,
	    			cmd->cmd_target.p_index + 1, "");

	    /*
	     * Now replace the rest of the top line with the
	     * remainder of the bottom line.
	     */
	    replchars(curwin, cmd->cmd_startpos.p_line,
	    			cmd->cmd_startpos.p_index,
				INT_MAX,
				cmd->cmd_target.p_line->l_text);

	    /*
	     * Finally, delete all lines from (top + 1) to bot,
	     * inclusive.
	     */
	    repllines(curwin, cmd->cmd_startpos.p_line->l_next,
			    cntllines(cmd->cmd_startpos.p_line,
			    		cmd->cmd_target.p_line) - 1,
			    (Line *) NULL);

	    end_command(curwin);
	}

	if (cmd->cmd_startpos.p_index > 0) {
	    (void) one_right(curwin, FALSE);
	}
    }

    /*
     * This seems reasonable. It's what vi does anyway.
     */
    curwin->w_set_want_col = TRUE;

    /*
     * Construct redo buffer.
     */
    Redo.r_mode = r_normal;
    format_redo('d', cmd);

#if 0
    /*
     * It would be nice to do this, for efficiency, but because
     * HandleOperator() calls move_cursor() without updating the
     * cursor position, the screen gets messed up if we do.
      */
    if (IsCharBased(cmd) && nlines == 1) {
	updateline(curwin);
    } else
#endif
           {
	xvUpdateAllBufferWindows(curbuf);
    }
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
    if (!start_command(curwin)) {
	return;
    }

    if (!IsCharBased(cmd)) {
	long	nlines;
	Line	*lp;

	/*
	 * This is a bit awkward ... for a line-based change, we don't
	 * actually delete the whole range of lines, but instead leave
	 * the first line in place and delete its text after the cursor
	 * position. However, yanking the whole thing is probably okay.
	 */
	yp_push_deleted();
	if (!do_yank(curbuf, &cmd->cmd_startpos, &cmd->cmd_target,
						FALSE, cmd->cmd_yp_name)) {
	    show_error(curwin, "Not enough memory to perform change");
	    return;
	}

	lp = cmd->cmd_startpos.p_line;

	nlines = cntllines(lp, cmd->cmd_target.p_line);
	if (nlines > 1) {
	    repllines(curwin, lp->l_next, nlines - 1, (Line *) NULL);
	}

	move_cursor(curwin, lp, 0);

	/*
	 * This is not right; it won't do the right thing when
	 * the cursor is in the whitespace of an indented line.
	 * However, it will do for the moment.
	 */
	begin_line(curwin, TRUE);

	replchars(curwin, lp, curwin->w_cursor->p_index,
						strlen(lp->l_text), "");
	xvUpdateAllBufferWindows(curbuf);
    } else {
	bool_t	doappend;	/* true if we should do append, not insert */

	/*
	 * A character-based change really is just a delete and an insert.
	 * So use the deletion code to make things easier.
	 */
	doappend = IsInclusive(cmd) && endofline(&cmd->cmd_target);

	xvOpDelete(cmd);

	if (doappend) {
	    (void) one_right(curwin, TRUE);
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

    /*
     * Report on the number of lines yanked.
     */
    nlines = cntllines(cmd->cmd_startpos.p_line, cmd->cmd_target.p_line);

    if (nlines > Pn(P_report)) {
	show_message(curwin, "%ld lines yanked", nlines);
    }

    (void) do_yank(curbuf, &cmd->cmd_startpos, &cmd->cmd_target,
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
