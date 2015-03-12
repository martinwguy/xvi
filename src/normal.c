/* Copyright (c) 1990,1991,1992,1993 Chris and John Downey */

/***

* program name:
    xvi
* function:
    Portable version of UNIX "vi" editor, with extensions.
* module name:
    normal.c
* module function:
    Main routine for processing characters in command mode
    as well as routines for handling the operators.
* history:
    STEVIE - ST Editor for VI Enthusiasts, Version 3.10
    Originally by Tim Thompson (twitch!tjt)
    Extensive modifications by Tony Andrews (onecom!wldrdg!tony)
    Heavily modified by Chris & John Downey
    Last modified by Martin Guy

***/

#include "xvi.h"
#include "cmd.h"

static	bool_t	HandleCommand P((Cmd *));
static	void	HandleOperator P((Cmd *));

/*
 * Global - should really be referenced from VirtScr or something.
 */
struct redo	Redo;

/*
 * Execute a command in "normal" (i.e. command) mode.
 */
bool_t
normal(c)
register int	c;
{
    register Cmd	*cmd;

    cmd = curwin->w_cmd;

    /*
     * If the character is a digit, and it is not a leading '0',
     * compute cmd->cmd_prenum instead of doing a command.  Leading
     * zeroes are treated specially because '0' is a valid command.
     *
     * If two_char is set, don't treat digits this way; they are
     * passed in as the second character. This is because none of
     * the two-character commands are allowed to take prenums in
     * the middle; if you want a prenum, you have to type it before
     * the command. So "t3" works as you might expect it to.
     */
    if (!cmd->cmd_two_char && is_digit(c) &&
				    (c != '0' || cmd->cmd_prenum > 0)) {
	cmd->cmd_prenum = cmd->cmd_prenum * 10 + (c - '0');
	return(FALSE);
    }

    /*
     * If we're in the middle of an operator AND we had a count before
     * the operator, then that count overrides the current value of
     * cmd->cmd_prenum. What this means effectively, is that commands like
     * "3dw" get turned into "d3w" which makes things fall into place
     * pretty neatly.
     */
    if (cmd->cmd_operator != NOP || cmd->cmd_two_char) {
	if (cmd->cmd_opnum != 0) {
	    cmd->cmd_prenum = cmd->cmd_opnum;
	}
    } else {
	cmd->cmd_opnum = 0;
    }

    /*
     * If we got given a buffer name last time around, it is only
     * good for one operation; so at the start of each new command
     * we set or clear the yankput module's idea of the buffer name.
     * We don't do this if cmd_operator is not NOP because it is not
     * the start of a new command.
     */
    if (cmd->cmd_operator == NOP) {
	cmd->cmd_yp_name = cmd->cmd_got_yp_name ? cmd->cmd_buffer_name : '@';
	cmd->cmd_got_yp_name = FALSE;
    }

    /*
     * If two_char is set, it means we got the first character of
     * a two-character command last time. So check the second char,
     * and set cflags appropriately.
     */
    if (cmd->cmd_two_char) {

	cmd->cmd_ch2 = c;

	State = NORMAL;
	cmd->cmd_two_char = FALSE;

	/*
	 * This seems to be a universal rule - if a two-character
	 * command has ESC as the second character, it means "abort".
	 */
	if (cmd->cmd_ch2 == ESC) {
	    cmd->cmd_operator = NOP;
	    cmd->cmd_prenum = 0;
	    return(FALSE);
	}

    } else {

	cmd->cmd_ch1 = c;
	cmd->cmd_ch2 = '\0';

	if (CFLAGS(cmd->cmd_ch1) & TWO_CHAR) {
	    /*
	     * It's the start of a two-character command. So wait
	     * until we get the second character before proceeding.
	     */
	    cmd->cmd_two_char = TRUE;
	    State = SUBNORMAL;
	    if (cmd->cmd_prenum != 0) {
		cmd->cmd_opnum = cmd->cmd_prenum;
		cmd->cmd_prenum = 0;
	    }
	    return(FALSE);
	}

	/*
	 * If we got an operator last time, and the user typed
	 * the same character again, we fake out the default
	 * "apply to this line" rule by changing the input
	 * character to a '_' which means "the current line."
	 */
	if (cmd->cmd_operator != NOP && cmd->cmd_operator == cmd->cmd_ch1) {
	    cmd->cmd_ch1 = '_';
	}
    }

    return(HandleCommand(cmd));
}

static bool_t
HandleCommand(cmd)
Cmd	*cmd;
{
    cmd->cmd_flags = CFLAGS(cmd->cmd_ch1);

    if ((cmd->cmd_flags & COMMAND) == 0) {
	cmd->cmd_operator = NOP;
	cmd->cmd_prenum = 0;
    	beep(curwin);
	return(FALSE);
    }

    if (cmd->cmd_flags & TARGET) {

	/*
	 * A cursor movement command. Set the target position to NULL
	 * before calling the function, so we know if it was successful.
	 */
	cmd->cmd_target.p_line = NULL;
	(*CFUNC(cmd->cmd_ch1))(cmd);

	if (cmd->cmd_flags & TGT_DEFERRED) {
	    /*
	     * Can't handle this yet; someone else will get it later.
	     */
	    return(FALSE);
	}

	/*
	 * If target is not deferred, we expect a good position to
	 * have been returned. If it wasn't, this is an error and
	 * no operators need apply.
	 */
	if (cmd->cmd_target.p_line == NULL) {
	    beep(curwin);
	    cmd->cmd_operator = NOP;
	    cmd->cmd_prenum = 0;
	    return(FALSE);
	}

	/*
	 * If the command will move us a long way, set the previous
	 * context mark now (we only do this if the target worked).
	 */
	if (cmd->cmd_flags & LONG_DISTANCE) {
	    setpcmark(curwin);
	}

	if (cmd->cmd_flags & SET_CURSOR_COL) {
	    curwin->w_set_want_col = TRUE;
	}

	/*
	 * We have a good position, so move to it. This will change
	 * later, as we do not really need to move the cursor in the
	 * case where an operator is being used.
	 */
	if (cmd->cmd_operator != NOP) {
	    HandleOperator(cmd);
	} else {
	    move_cursor(curwin, cmd->cmd_target.p_line,
	    			cmd->cmd_target.p_index);
	}

    } else {
	/*
	 * A command that does something.
	 * Since it isn't a target, no operators need apply.
	 */
	if (cmd->cmd_operator != NOP) {
	    beep(curwin);
	    cmd->cmd_operator = NOP;
	    return(FALSE);
	}

	(void) (*CFUNC(cmd->cmd_ch1))(cmd);
    }

    cmd->cmd_prenum = 0;
    return(TRUE);
}

static void
HandleOperator(cmd)
Cmd	*cmd;
{
    /*
     * Don't do an operation if the target is at the same cursor
     * position as the starting position and the target's type is
     * exclusive - there's no point.
     */
    if (IsCharBased(cmd) && IsExclusive(cmd) &&
			    eq(&cmd->cmd_startpos, &cmd->cmd_target)) {
	beep(curwin);
	cmd->cmd_operator = NOP;
	return;
    }

    if (IsExclusive(cmd) && IsLineBased(cmd)) {
	show_error(curwin,
		    "Oops! I can't handle exclusive line-based operations!");
	cmd->cmd_operator = NOP;
	return;
    }

    /*
     * Ensure start and target positions are correctly ordered.
     */
    if (lt(&cmd->cmd_target, &cmd->cmd_startpos)) {
	pswap(&cmd->cmd_startpos, &cmd->cmd_target);
    }

    if (IsExclusive(cmd)) {
	/*
	 * For character-based exclusive targets, we always decrement the
	 * end position, even when the direction of the target is effectively
	 * backwards in the buffer. Character-based operators work this way,
	 * i.e. we do not affect the character under the cursor when the
	 * target is before the starting position.
	 *
	 * Careful here; only decrement the end position if it is different
	 * from the starting position.
	 */
	if (!eq(&cmd->cmd_startpos, &cmd->cmd_target)) {
	    (void) dec(&cmd->cmd_target);
	}
    }

    switch (cmd->cmd_operator) {
    case 'c':
	xvOpChange(cmd);
	break;

    case 'd':
	xvOpDelete(cmd);
	break;

    case 'y':
	xvOpYank(cmd);
	break;

    case '<':
    case '>':
	xvOpShift(cmd);
	break;

    case '!':
	specify_pipe_range(curwin, cmd->cmd_startpos.p_line,
				    cmd->cmd_target.p_line);
	cmd_init(curwin, '!');
	break;

    default:
	beep(curwin);
    }
    cmd->cmd_operator = NOP;
}

/*
 * Command-line mode has read in a search. Process the search,
 * treating it as the target of any pending operator if necessary.
 *
 * Note the careful hack whereby we pass 'n' instead of the actual
 * search initiation character to HandleOperator(); this is so that
 * repeat-command will work properly. This will always be safe as
 * the 'n' command must have the same semantics as '/' and '?'.
 */
bool_t
xvProcessSearch(type, pattern)
int	type;
char	*pattern;
{
    Cmd		*cmd;
    Posn	*p;

    cmd = curwin->w_cmd;

    p = xvDoSearch(curwin, pattern, type);
    if (p == NULL) {
	cmd->cmd_operator = NOP;
	cmd->cmd_prenum = 0;
	return(FALSE);
    }

    if (cmd->cmd_operator != NOP) {
	cmd->cmd_ch1 = 'n';
	cmd->cmd_ch2 = '\0';
	cmd->cmd_flags = CFLAGS('n');
	cmd->cmd_target = *p;
	HandleOperator(cmd);
    } else {
	curwin->w_set_want_col = TRUE;
	setpcmark(curwin);
	move_cursor(curwin, p->p_line, p->p_index);
	move_window_to_cursor(curwin);
    }
    return(TRUE);
}

/*
 * Handle operator commands. This just means recording that they
 * have been typed, for later use.
 */
void
do_operator(cmd)
Cmd	*cmd;
{
    if (cmd->cmd_prenum != 0) {
	cmd->cmd_opnum = cmd->cmd_prenum;
    }
    cmd->cmd_startpos = *curwin->w_cursor;
    cmd->cmd_operator = cmd->cmd_ch1;
}

void
do_quote(cmd)
Cmd	*cmd;
{
    cmd->cmd_got_yp_name = TRUE;
    cmd->cmd_buffer_name = cmd->cmd_ch2;
}

/*ARGSUSED*/
void
do_badcmd(cmd)
Cmd	*cmd;
{
    beep(curwin);
}

void
xvInitialiseCmd(cmd)
Cmd	*cmd;
{
    cmd->cmd_two_char = FALSE;
    cmd->cmd_flags = 0;
    cmd->cmd_operator = NOP;
    cmd->cmd_prenum = 0;
    cmd->cmd_opnum = 0;
    cmd->cmd_startpos.p_line = NULL;
    cmd->cmd_target.p_line = NULL;
    cmd->cmd_ch1 = '\0';
    cmd->cmd_ch2 = '\0';
    cmd->cmd_yp_name = '\0';
    cmd->cmd_buffer_name = '\0';
}
