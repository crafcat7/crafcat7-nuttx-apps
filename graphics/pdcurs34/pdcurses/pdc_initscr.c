/****************************************************************************
 * apps/graphics/pdcurs34/pdcurses/pdc_initscr.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Adapted from the original public domain pdcurses by Gregory Nutt
 ****************************************************************************/

/* Name: initscr
 *
 * Synopsis:
 *       WINDOW *initscr(void);
 *       WINDOW *Xinitscr(int argc, char *argv[]);
 *       int endwin(void);
 *       bool isendwin(void);
 *       SCREEN *newterm(const char *type, FILE *outfd, FILE *infd);
 *       SCREEN *set_term(SCREEN *new);
 *       void delscreen(SCREEN *sp);
 *
 *       int resize_term(int nlines, int ncols);
 *       bool is_termresized(void);
 *       const char *curses_version(void);
 *
 * Description:
 *       initscr() should be the first curses routine called.  It will
 *       initialize all curses data structures, and arrange that the
 *       first call to refresh() will clear the screen.  In case of
 *       error, initscr() will write a message to standard error and end
 *       the program.
 *
 *       endwin() should be called before exiting or escaping from curses
 *       mode temporarily.  It will restore tty modes, move the cursor to
 *       the lower left corner of the screen and reset the terminal into
 *       the proper non-visual mode.  To resume curses after a temporary
 *       escape, call refresh() or doupdate().
 *
 *       isendwin() returns true if endwin() has been called without a
 *       subsequent refresh, unless SP is NULL.
 *
 *       In some implementations of curses, newterm() allows the use of
 *       multiple terminals. Here, it's just an alternative interface for
 *       initscr(). It always returns SP, or NULL.
 *
 *       delscreen() frees the memory allocated by newterm() or
 *       initscr(), since it's not freed by endwin(). This function is
 *       usually not needed. In PDCurses, the parameter must be the
 *       value of SP, and delscreen() sets SP to NULL.
 *
 *       set_term() does nothing meaningful in PDCurses, but is included
 *       for compatibility with other curses implementations.
 *
 *       resize_term() is effectively two functions: When called with
 *       nonzero values for nlines and ncols, it attempts to resize the
 *       screen to the given size. When called with (0, 0), it merely
 *       adjusts the internal structures to match the current size after
 *       the screen is resized by the user. On the currently supported
 *       platforms, this functionality is mutually exclusive: X11 allows
 *       user resizing, while DOS, OS/2 and Win32 allow programmatic
 *       resizing. If you want to support user resizing, you should check
 *       for getch() returning KEY_RESIZE, and/or call is_termresized()
 *       at appropriate times; if either condition occurs, call
 *       resize_term(0, 0). Then, with either user or programmatic
 *       resizing, you'll have to resize any windows you've created, as
 *       appropriate; resize_term() only handles stdscr and curscr.
 *
 *       is_termresized() returns true if the curses screen has been
 *       resized by the user, and a call to resize_term() is needed.
 *       Checking for KEY_RESIZE is generally preferable, unless you're
 *       not handling the keyboard.
 *
 *       curses_version() returns a string describing the version of
 *       PDCurses.
 *
 * Return Value:
 *       All functions return NULL on error, except endwin(), which
 *       returns ERR on error.
 *
 * Portability                                X/Open    BSD    SYS V
 *       initscr                                 Y       Y       Y
 *       endwin                                  Y       Y       Y
 *       isendwin                                Y       -      3.0
 *       newterm                                 Y       -       Y
 *       set_term                                Y       -       Y
 *       delscreen                               Y       -      4.0
 *       resize_term                             -       -       -
 *       is_termresized                          -       -       -
 *       curses_version                          -       -       -
 */

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdlib.h>

#include "curspriv.h"

/****************************************************************************
 * Public Data
 ****************************************************************************/

const char *_curses_notice = "PDCurses 3.4 - Public Domain 2008";

#ifndef CONFIG_PDCURSES_MULTITHREAD

char ttytype[128];

SCREEN *SP = NULL;          /* curses variables */
WINDOW *curscr = NULL;      /* the current screen image */
WINDOW *stdscr = NULL;      /* the default screen window */
WINDOW *pdc_lastscr = NULL; /* the last screen image */

int LINES = 0;              /* current terminal height */
int COLS = 0;               /* current terminal width */
int TABSIZE = 8;

MOUSE_STATUS Mouse_status, pdc_mouse_status;

extern RIPPEDOFFLINE linesripped[5];
extern char linesrippedoff;

#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

WINDOW *Xinitscr(int argc, char *argv[])
{
  int i;
#ifdef CONFIG_PDCURSES_MULTITHREAD
  FAR struct pdc_context_s *ctx = PDC_ctx();
#endif

  PDC_LOG(("Xinitscr() - called\n"));

  if (SP && SP->alive)
    {
      return NULL;
    }

  if (PDC_scr_open(argc, argv) == ERR)
    {
      fprintf(stderr, "initscr(): Unable to create SP\n");
      return NULL;
    }

  SP->autocr               = true;   /* cr -> lf by default */
  SP->raw_out              = false;  /* tty I/O modes */
  SP->raw_inp              = false;  /* tty I/O modes */
  SP->cbreak               = true;
  SP->save_key_modifiers   = false;
  SP->return_key_modifiers = false;
  SP->echo                 = true;
  SP->visibility           = 1;
  SP->resized              = false;
  SP->_trap_mbe            = 0L;
  SP->_map_mbe_to_key      = 0L;
  SP->linesrippedoffcnt    = 0;
  SP->linesrippedoffontop  = 0;
  SP->delaytenths          = 0;
  SP->line_color           = -1;
  SP->orig_cursor          = PDC_get_cursor_mode();

  LINES                    = SP->lines;
  COLS                     = SP->cols;

  if (LINES < 2 || COLS < 2)
    {
      fprintf(stderr, "initscr(): LINES=%d COLS=%d: too small.\n",
              LINES, COLS);
      exit(4);
    }

  if ((curscr = newwin(LINES, COLS, 0, 0)) == NULL)
    {
      fprintf(stderr, "initscr(): Unable to create curscr.\n");
      exit(2);
    }

  if ((pdc_lastscr = newwin(LINES, COLS, 0, 0)) == NULL)
    {
      fprintf(stderr, "initscr(): Unable to create pdc_lastscr.\n");
      exit(2);
    }

  wattrset(pdc_lastscr, (chtype) (-1));
  werase(pdc_lastscr);

  PDC_slk_initialize();
  LINES -= SP->slklines;

  /* We have to sort out ripped off lines here, and reduce the height of
   * stdscr by the number of lines ripped off
   */

  for (i = 0; i < linesrippedoff; i++)
    {
      if (linesripped[i].line < 0)
        {
          (*linesripped[i].init) (newwin(1, COLS, LINES - 1, 0), COLS);
        }
      else
        {
          (*linesripped[i].init) (newwin(1, COLS,
                                  SP->linesrippedoffontop++, 0), COLS);
        }

      SP->linesrippedoffcnt++;
      LINES--;
    }

  linesrippedoff = 0;

  if (!(stdscr = newwin(LINES, COLS, SP->linesrippedoffontop, 0)))
    {
      fprintf(stderr, "initscr(): Unable to create stdscr.\n");
      exit(1);
    }

  wclrtobot(stdscr);

  /* If preserving the existing screen, don't allow a screen clear */

  if (SP->_preserve)
    {
      untouchwin(curscr);
      untouchwin(stdscr);
      stdscr->_clear = false;
      curscr->_clear = false;
    }
  else
    {
      curscr->_clear = true;
    }

  PDC_init_atrtab();            /* set up default colors */

  MOUSE_X_POS = MOUSE_Y_POS = -1;
  BUTTON_STATUS(1) = BUTTON_RELEASED;
  BUTTON_STATUS(2) = BUTTON_RELEASED;
  BUTTON_STATUS(3) = BUTTON_RELEASED;
  Mouse_status.changes = 0;

  SP->alive = true;

  def_shell_mode();

  snprintf(ttytype, sizeof(ttytype),
           "pdcurses|PDCurses for %s", PDC_sysname());

  return stdscr;
}

WINDOW *initscr(void)
{
  PDC_LOG(("initscr() - called\n"));

  return Xinitscr(0, NULL);
}

int endwin(void)
{
#ifdef CONFIG_PDCURSES_MULTITHREAD
  FAR struct pdc_context_s *ctx = PDC_ctx();
#endif
  PDC_LOG(("endwin() - called\n"));

  /* Allow temporary exit from curses using endwin() */

  def_prog_mode();
  PDC_scr_close();

  SP->alive = false;

  return OK;
}

bool isendwin(void)
{
#ifdef CONFIG_PDCURSES_MULTITHREAD
  FAR struct pdc_context_s *ctx = PDC_ctx();
#endif
  PDC_LOG(("isendwin() - called\n"));

  return SP ? !(SP->alive) : false;
}

SCREEN *newterm(const char *type, FILE *outfd, FILE *infd)
{
#ifdef CONFIG_PDCURSES_MULTITHREAD
  FAR struct pdc_context_s *ctx = PDC_ctx();
#endif
  PDC_LOG(("newterm() - called\n"));

  return Xinitscr(0, NULL) ? SP : NULL;
}

SCREEN *set_term(SCREEN *new)
{
#ifdef CONFIG_PDCURSES_MULTITHREAD
  FAR struct pdc_context_s *ctx = PDC_ctx();
#endif
  PDC_LOG(("set_term() - called\n"));

  /* We only support one screen */

  return (new == SP) ? SP : NULL;
}

void delscreen(SCREEN *sp)
{
#ifdef CONFIG_PDCURSES_MULTITHREAD
  FAR struct pdc_context_s *ctx = PDC_ctx();
#endif
  PDC_LOG(("delscreen() - called\n"));

  if (sp != SP)
    {
      return;
    }

  PDC_slk_free();               /* Free the soft label keys, if needed */

  delwin(stdscr);
  delwin(curscr);
  delwin(pdc_lastscr);
  stdscr      = NULL;
  curscr      = NULL;
  pdc_lastscr = NULL;

  SP->alive   = false;

  PDC_scr_free();               /* Free SP and pdc_atrtab */

  SP = NULL;

#ifdef CONFIG_PDCURSES_MULTITHREAD

  /* Free the context */

  PDC_ctx_free();
#endif
}

int resize_term(int nlines, int ncols)
{
#ifdef CONFIG_PDCURSES_MULTITHREAD
  FAR struct pdc_context_s *ctx = PDC_ctx();
#endif
  PDC_LOG(("resize_term() - called: nlines %d\n", nlines));

  if (!stdscr || PDC_resize_screen(nlines, ncols) == ERR)
    {
      return ERR;
    }

  SP->lines = PDC_get_rows();
  LINES     = SP->lines - SP->linesrippedoffcnt - SP->slklines;
  SP->cols  = COLS = PDC_get_columns();

  if (wresize(curscr, SP->lines, SP->cols) == ERR ||
      wresize(stdscr, LINES, COLS) == ERR ||
      wresize(pdc_lastscr, SP->lines, SP->cols) == ERR)
    {
      return ERR;
    }

  werase(pdc_lastscr);
  curscr->_clear = true;

  if (SP->slk_winptr)
    {
      if (wresize(SP->slk_winptr, SP->slklines, COLS) == ERR)
        {
          return ERR;
        }

      wmove(SP->slk_winptr, 0, 0);
      wclrtobot(SP->slk_winptr);
      PDC_slk_initialize();
      slk_noutrefresh();
    }

  touchwin(stdscr);
  wnoutrefresh(stdscr);
  return OK;
}

bool is_termresized(void)
{
#ifdef CONFIG_PDCURSES_MULTITHREAD
  FAR struct pdc_context_s *ctx = PDC_ctx();
#endif
  PDC_LOG(("is_termresized() - called\n"));

  return SP->resized;
}

const char *curses_version(void)
{
  return _curses_notice;
}
