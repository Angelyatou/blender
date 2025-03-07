/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spconsole
 */

#include <ctype.h> /* #ispunct */
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "MEM_guardedalloc.h"

#include "DNA_userdef_types.h"

#include "BLI_dynstr.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_string_cursor_utf8.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "UI_view2d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "console_intern.h"

/* TODO: Text operations not yet supported for console:
 * Mac KM_OSKEY-arrow to beginning/end of line
 * Mac KM_OSKEY-backspace to start of line
 * Mac KM_OSKEY-delete to end of line
 * Text cursor insertion by mouse
 * Mouse drag to select does not change text cursor position.
 * Shift-ctrl-arrow to select word
 * ctrl-x to copy to clipboard and delete.
 * ctrl-a to select all
 * ctrl-z，shift-crtrl-z undo/redo
 */

/* -------------------------------------------------------------------- */
/** \name Utilities
 * \{ */

static char *console_select_to_buffer(SpaceConsole *sc)
{
  if (sc->sel_start == sc->sel_end) {
    return NULL;
  }

  ConsoleLine cl_dummy = {NULL};
  console_scrollback_prompt_begin(sc, &cl_dummy);

  int offset = 0;
  for (ConsoleLine *cl = sc->scrollback.first; cl; cl = cl->next) {
    offset += cl->len + 1;
  }

  char *buf_str = NULL;
  if (offset != 0) {
    offset -= 1;
    int sel[2] = {offset - sc->sel_end, offset - sc->sel_start};
    DynStr *buf_dyn = BLI_dynstr_new();
    for (ConsoleLine *cl = sc->scrollback.first; cl; cl = cl->next) {
      if (sel[0] <= cl->len && sel[1] >= 0) {
        int sta = max_ii(sel[0], 0);
        int end = min_ii(sel[1], cl->len);

        if (BLI_dynstr_get_len(buf_dyn)) {
          BLI_dynstr_append(buf_dyn, "\n");
        }

        BLI_dynstr_nappend(buf_dyn, cl->line + sta, end - sta);
      }

      sel[0] -= cl->len + 1;
      sel[1] -= cl->len + 1;
    }

    buf_str = BLI_dynstr_get_cstring(buf_dyn);

    BLI_dynstr_free(buf_dyn);
  }
  console_scrollback_prompt_end(sc, &cl_dummy);

  return buf_str;
}

static void console_select_update_primary_clipboard(SpaceConsole *sc)
{
  if ((WM_capabilities_flag() & WM_CAPABILITY_PRIMARY_CLIPBOARD) == 0) {
    return;
  }
  if (sc->sel_start == sc->sel_end) {
    return;
  }
  char *buf = console_select_to_buffer(sc);
  if (buf == NULL) {
    return;
  }
  WM_clipboard_text_set(buf, true);
  MEM_freeN(buf);
}

/** \} */

/* so when we type - the view scrolls to the bottom */
static void console_scroll_bottom(ARegion *region)
{
  View2D *v2d = &region->v2d;
  v2d->cur.ymin = 0.0;
  v2d->cur.ymax = (float)v2d->winy;
}

void console_textview_update_rect(SpaceConsole *sc, ARegion *region)
{
  View2D *v2d = &region->v2d;

  UI_view2d_totRect_set(v2d, region->winx - 1, console_textview_height(sc, region));
}

static void console_select_offset(SpaceConsole *sc, const int offset)
{
  sc->sel_start += offset;
  sc->sel_end += offset;
}

void console_history_free(SpaceConsole *sc, ConsoleLine *cl)
{
  BLI_remlink(&sc->history, cl);
  MEM_freeN(cl->line);
  MEM_freeN(cl);
}
void console_scrollback_free(SpaceConsole *sc, ConsoleLine *cl)
{
  BLI_remlink(&sc->scrollback, cl);
  MEM_freeN(cl->line);
  MEM_freeN(cl);
}

static void console_scrollback_limit(SpaceConsole *sc)
{
  int tot;

  for (tot = BLI_listbase_count(&sc->scrollback); tot > U.scrollback; tot--) {
    console_scrollback_free(sc, sc->scrollback.first);
  }
}

static ConsoleLine *console_history_find(SpaceConsole *sc, const char *str, ConsoleLine *cl_ignore)
{
  ConsoleLine *cl;

  for (cl = sc->history.last; cl; cl = cl->prev) {
    if (cl == cl_ignore) {
      continue;
    }

    if (STREQ(str, cl->line)) {
      return cl;
    }
  }

  return NULL;
}

/* return 0 if no change made, clamps the range */
static bool console_line_cursor_set(ConsoleLine *cl, int cursor)
{
  int cursor_new;

  if (cursor < 0) {
    cursor_new = 0;
  }
  else if (cursor > cl->len) {
    cursor_new = cl->len;
  }
  else {
    cursor_new = cursor;
  }

  if (cursor_new == cl->cursor) {
    return false;
  }

  cl->cursor = cursor_new;
  return true;
}

#if 0 /* XXX unused */
static void console_lb_debug__internal(ListBase *lb)
{
  ConsoleLine *cl;

  printf("%d: ", BLI_listbase_count(lb));
  for (cl = lb->first; cl; cl = cl->next) {
    printf("<%s> ", cl->line);
  }
  printf("\n");
}

static void console_history_debug(const bContext *C)
{
  SpaceConsole *sc = CTX_wm_space_console(C);

  console_lb_debug__internal(&sc->history);
}
#endif

static ConsoleLine *console_lb_add__internal(ListBase *lb, ConsoleLine *from)
{
  ConsoleLine *ci = MEM_callocN(sizeof(ConsoleLine), "ConsoleLine Add");

  if (from) {
    BLI_assert(strlen(from->line) == from->len);
    ci->line = BLI_strdupn(from->line, from->len);
    ci->len = ci->len_alloc = from->len;
    ci->cursor = from->cursor;
    ci->type = from->type;
  }
  else {
    ci->line = MEM_callocN(64, "console-in-line");
    ci->len_alloc = 64;
    ci->len = 0;
  }

  BLI_addtail(lb, ci);
  return ci;
}

static ConsoleLine *console_history_add(SpaceConsole *sc, ConsoleLine *from)
{
  return console_lb_add__internal(&sc->history, from);
}

#if 0 /* may use later ? */
static ConsoleLine *console_scrollback_add(const bContext *C, ConsoleLine *from)
{
  SpaceConsole *sc = CTX_wm_space_console(C);

  return console_lb_add__internal(&sc->scrollback, from);
}
#endif

static ConsoleLine *console_lb_add_str__internal(ListBase *lb, char *str, bool own)
{
  ConsoleLine *ci = MEM_callocN(sizeof(ConsoleLine), "ConsoleLine Add");
  if (own) {
    ci->line = str;
  }
  else {
    ci->line = BLI_strdup(str);
  }

  ci->len = ci->len_alloc = strlen(str);

  BLI_addtail(lb, ci);
  return ci;
}
ConsoleLine *console_history_add_str(SpaceConsole *sc, char *str, bool own)
{
  return console_lb_add_str__internal(&sc->history, str, own);
}
ConsoleLine *console_scrollback_add_str(SpaceConsole *sc, char *str, bool own)
{
  ConsoleLine *ci = console_lb_add_str__internal(&sc->scrollback, str, own);
  console_select_offset(sc, ci->len + 1);
  return ci;
}

ConsoleLine *console_history_verify(const bContext *C)
{
  SpaceConsole *sc = CTX_wm_space_console(C);
  ConsoleLine *ci = sc->history.last;
  if (ci == NULL) {
    ci = console_history_add(sc, NULL);
  }

  return ci;
}

static void console_line_verify_length(ConsoleLine *ci, int len)
{
  /* resize the buffer if needed */
  if (len >= ci->len_alloc) {
    /* new length */
#ifndef NDEBUG
    int new_len = len + 1;
#else
    int new_len = (len + 1) * 2;
#endif
    ci->line = MEM_recallocN_id(ci->line, new_len, "console line");
    ci->len_alloc = new_len;
  }
}

static int console_line_insert(ConsoleLine *ci, char *str)
{
  int len = strlen(str);

  if (len > 0 && str[len - 1] == '\n') { /* stop new lines being pasted at the end of lines */
    str[len - 1] = '\0';
    len--;
  }

  if (len == 0) {
    return 0;
  }

  console_line_verify_length(ci, len + ci->len);

  memmove(ci->line + ci->cursor + len, ci->line + ci->cursor, (ci->len - ci->cursor) + 1);
  memcpy(ci->line + ci->cursor, str, len);

  ci->len += len;
  ci->cursor += len;

  return len;
}

/**
 * Take an absolute index and give the line/column info.
 *
 * \note be sure to call console_scrollback_prompt_begin first
 */
static bool console_line_column_from_index(
    SpaceConsole *sc, const int pos, ConsoleLine **r_cl, int *r_cl_offset, int *r_col)
{
  ConsoleLine *cl;
  int offset = 0;

  for (cl = sc->scrollback.last; cl; cl = cl->prev) {
    offset += cl->len + 1;
    if (offset >= pos) {
      break;
    }
  }

  if (cl) {
    offset -= 1;
    *r_cl = cl;
    *r_cl_offset = offset;
    *r_col = offset - pos;
    return true;
  }

  *r_cl = NULL;
  *r_cl_offset = -1;
  *r_col = -1;
  return false;
}

/* Static functions for text editing. */

/* similar to the text editor, with some not used. keep compatible */
static const EnumPropertyItem console_move_type_items[] = {
    {LINE_BEGIN, "LINE_BEGIN", 0, "Line Begin", ""},
    {LINE_END, "LINE_END", 0, "Line End", ""},
    {PREV_CHAR, "PREVIOUS_CHARACTER", 0, "Previous Character", ""},
    {NEXT_CHAR, "NEXT_CHARACTER", 0, "Next Character", ""},
    {PREV_WORD, "PREVIOUS_WORD", 0, "Previous Word", ""},
    {NEXT_WORD, "NEXT_WORD", 0, "Next Word", ""},
    {0, NULL, 0, NULL, NULL},
};

static int console_move_exec(bContext *C, wmOperator *op)
{
  ConsoleLine *ci = console_history_verify(C);

  int type = RNA_enum_get(op->ptr, "type");
  bool done = false;
  int pos;

  switch (type) {
    case LINE_BEGIN:
      pos = ci->cursor;
      BLI_str_cursor_step_utf8(ci->line, ci->len, &pos, STRCUR_DIR_PREV, STRCUR_JUMP_ALL, true);
      done = console_line_cursor_set(ci, pos);
      break;
    case LINE_END:
      pos = ci->cursor;
      BLI_str_cursor_step_utf8(ci->line, ci->len, &pos, STRCUR_DIR_NEXT, STRCUR_JUMP_ALL, true);
      done = console_line_cursor_set(ci, pos);
      break;
    case PREV_CHAR:
      pos = ci->cursor;
      BLI_str_cursor_step_utf8(ci->line, ci->len, &pos, STRCUR_DIR_PREV, STRCUR_JUMP_NONE, true);
      done = console_line_cursor_set(ci, pos);
      break;
    case NEXT_CHAR:
      pos = ci->cursor;
      BLI_str_cursor_step_utf8(ci->line, ci->len, &pos, STRCUR_DIR_NEXT, STRCUR_JUMP_NONE, true);
      done = console_line_cursor_set(ci, pos);
      break;

    /* - if the character is a delimiter then skip delimiters (including white space)
     * - when jump over the word */
    case PREV_WORD:
      pos = ci->cursor;
      BLI_str_cursor_step_utf8(ci->line, ci->len, &pos, STRCUR_DIR_PREV, STRCUR_JUMP_DELIM, true);
      done = console_line_cursor_set(ci, pos);
      break;
    case NEXT_WORD:
      pos = ci->cursor;
      BLI_str_cursor_step_utf8(ci->line, ci->len, &pos, STRCUR_DIR_NEXT, STRCUR_JUMP_DELIM, true);
      done = console_line_cursor_set(ci, pos);
      break;
  }

  if (done) {
    ScrArea *area = CTX_wm_area(C);
    ARegion *region = CTX_wm_region(C);

    ED_area_tag_redraw(area);
    console_scroll_bottom(region);
  }

  return OPERATOR_FINISHED;
}

void CONSOLE_OT_move(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Move Cursor";
  ot->description = "Move cursor position";
  ot->idname = "CONSOLE_OT_move";

  /* api callbacks */
  ot->exec = console_move_exec;
  ot->poll = ED_operator_console_active;

  /* properties */
  RNA_def_enum(
      ot->srna, "type", console_move_type_items, LINE_BEGIN, "Type", "Where to move cursor to");
}

#define TAB_LENGTH 4
static int console_insert_exec(bContext *C, wmOperator *op)
{
  SpaceConsole *sc = CTX_wm_space_console(C);
  ARegion *region = CTX_wm_region(C);
  ConsoleLine *ci = console_history_verify(C);
  char *str = RNA_string_get_alloc(op->ptr, "text", NULL, 0, NULL);
  int len;

  if (str[0] == '\t' && str[1] == '\0') {
    len = TAB_LENGTH;
    MEM_freeN(str);
    str = MEM_mallocN(len + 1, "insert_exec");
    memset(str, ' ', len);
    str[len] = '\0';
  }

  len = console_line_insert(ci, str);

  MEM_freeN(str);

  if (len == 0) {
    return OPERATOR_CANCELLED;
  }

  console_select_offset(sc, len);

  console_textview_update_rect(sc, region);
  ED_area_tag_redraw(CTX_wm_area(C));

  console_scroll_bottom(region);

  return OPERATOR_FINISHED;
}

static int console_insert_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  /* NOTE: the "text" property is always set from key-map,
   * so we can't use #RNA_struct_property_is_set, check the length instead. */
  if (!RNA_string_length(op->ptr, "text")) {
    /* If alt/control/super are pressed pass through except for UTF8 character event
     * (when input method are used for UTF8 inputs, the user may assign key event
     * including alt/control/super like control-m to commit UTF8 string.
     * in such case, the modifiers in the UTF8 character event make no sense.) */
    if ((event->modifier & (KM_CTRL | KM_OSKEY)) && !event->utf8_buf[0]) {
      return OPERATOR_PASS_THROUGH;
    }

    char str[BLI_UTF8_MAX + 1];
    const size_t len = BLI_str_utf8_size_safe(event->utf8_buf);
    memcpy(str, event->utf8_buf, len);
    str[len] = '\0';
    RNA_string_set(op->ptr, "text", str);
  }
  return console_insert_exec(C, op);
}

void CONSOLE_OT_insert(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Insert";
  ot->description = "Insert text at cursor position";
  ot->idname = "CONSOLE_OT_insert";

  /* api callbacks */
  ot->exec = console_insert_exec;
  ot->invoke = console_insert_invoke;
  ot->poll = ED_operator_console_active;

  /* properties */
  prop = RNA_def_string(
      ot->srna, "text", NULL, 0, "Text", "Text to insert at the cursor position");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

/* -------------------------------------------------------------------- */
/** \name Indent or Autocomplete Operator
 * \{ */

static int console_indent_or_autocomplete_exec(bContext *C, wmOperator *UNUSED(op))
{
  ConsoleLine *ci = console_history_verify(C);
  bool text_before_cursor = false;

  /* Check any text before cursor (not just the previous character) as is done for
   * #TEXT_OT_indent_or_autocomplete because Python auto-complete operates on import
   * statements such as completing possible sub-modules: `from bpy import `. */
  for (int i = 0; i < ci->cursor; i += BLI_str_utf8_size_safe(&ci->line[i])) {
    if (!ELEM(ci->line[i], ' ', '\t')) {
      text_before_cursor = true;
      break;
    }
  }

  if (text_before_cursor) {
    WM_operator_name_call(C, "CONSOLE_OT_autocomplete", WM_OP_INVOKE_DEFAULT, NULL, NULL);
  }
  else {
    WM_operator_name_call(C, "CONSOLE_OT_indent", WM_OP_EXEC_DEFAULT, NULL, NULL);
  }
  return OPERATOR_FINISHED;
}

void CONSOLE_OT_indent_or_autocomplete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Indent or Autocomplete";
  ot->idname = "CONSOLE_OT_indent_or_autocomplete";
  ot->description = "Indent selected text or autocomplete";

  /* api callbacks */
  ot->exec = console_indent_or_autocomplete_exec;
  ot->poll = ED_operator_console_active;

  /* flags */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Indent Operator
 * \{ */

static int console_indent_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceConsole *sc = CTX_wm_space_console(C);
  ARegion *region = CTX_wm_region(C);
  ConsoleLine *ci = console_history_verify(C);
  int spaces;
  int len;

  for (spaces = 0; spaces < ci->len; spaces++) {
    if (ci->line[spaces] != ' ') {
      break;
    }
  }

  len = TAB_LENGTH - spaces % TAB_LENGTH;

  console_line_verify_length(ci, ci->len + len);

  memmove(ci->line + len, ci->line, ci->len + 1);
  memset(ci->line, ' ', len);
  ci->len += len;
  BLI_assert(ci->len >= 0);
  console_line_cursor_set(ci, ci->cursor + len);
  console_select_offset(sc, len);

  console_textview_update_rect(sc, region);
  ED_area_tag_redraw(CTX_wm_area(C));

  console_scroll_bottom(region);

  return OPERATOR_FINISHED;
}

void CONSOLE_OT_indent(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Indent";
  ot->description = "Add 4 spaces at line beginning";
  ot->idname = "CONSOLE_OT_indent";

  /* api callbacks */
  ot->exec = console_indent_exec;
  ot->poll = ED_operator_console_active;
}

/** \} */

static int console_unindent_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceConsole *sc = CTX_wm_space_console(C);
  ARegion *region = CTX_wm_region(C);
  ConsoleLine *ci = console_history_verify(C);
  int spaces;
  int len;

  for (spaces = 0; spaces < ci->len; spaces++) {
    if (ci->line[spaces] != ' ') {
      break;
    }
  }

  if (spaces == 0) {
    return OPERATOR_CANCELLED;
  }

  len = spaces % TAB_LENGTH;
  if (len == 0) {
    len = TAB_LENGTH;
  }

  console_line_verify_length(ci, ci->len - len);

  memmove(ci->line, ci->line + len, (ci->len - len) + 1);
  ci->len -= len;
  BLI_assert(ci->len >= 0);

  console_line_cursor_set(ci, ci->cursor - len);
  console_select_offset(sc, -len);

  console_textview_update_rect(sc, region);
  ED_area_tag_redraw(CTX_wm_area(C));

  console_scroll_bottom(region);

  return OPERATOR_FINISHED;
}

void CONSOLE_OT_unindent(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Unindent";
  ot->description = "Delete 4 spaces from line beginning";
  ot->idname = "CONSOLE_OT_unindent";

  /* api callbacks */
  ot->exec = console_unindent_exec;
  ot->poll = ED_operator_console_active;
}

static const EnumPropertyItem console_delete_type_items[] = {
    {DEL_NEXT_CHAR, "NEXT_CHARACTER", 0, "Next Character", ""},
    {DEL_PREV_CHAR, "PREVIOUS_CHARACTER", 0, "Previous Character", ""},
    {DEL_NEXT_WORD, "NEXT_WORD", 0, "Next Word", ""},
    {DEL_PREV_WORD, "PREVIOUS_WORD", 0, "Previous Word", ""},
    {0, NULL, 0, NULL, NULL},
};

static int console_delete_exec(bContext *C, wmOperator *op)
{
  SpaceConsole *sc = CTX_wm_space_console(C);
  ARegion *region = CTX_wm_region(C);
  ConsoleLine *ci = console_history_verify(C);
  int pos;
  int stride;

  const short type = RNA_enum_get(op->ptr, "type");
  bool done = false;

  if (ci->len == 0) {
    return OPERATOR_CANCELLED;
  }

  switch (type) {
    case DEL_NEXT_CHAR:
    case DEL_NEXT_WORD:
      if (ci->cursor < ci->len) {
        pos = ci->cursor;
        BLI_str_cursor_step_utf8(ci->line,
                                 ci->len,
                                 &pos,
                                 STRCUR_DIR_NEXT,
                                 (type == DEL_NEXT_CHAR) ? STRCUR_JUMP_NONE : STRCUR_JUMP_DELIM,
                                 true);
        stride = pos - ci->cursor;
        if (stride) {
          memmove(ci->line + ci->cursor,
                  ci->line + ci->cursor + stride,
                  (ci->len - (ci->cursor + stride)) + 1);
          ci->len -= stride;
          BLI_assert(ci->len >= 0);
          done = true;
        }
      }
      break;
    case DEL_PREV_CHAR:
    case DEL_PREV_WORD:
      if (ci->cursor > 0) {
        pos = ci->cursor;
        BLI_str_cursor_step_utf8(ci->line,
                                 ci->len,
                                 &pos,
                                 STRCUR_DIR_PREV,
                                 (type == DEL_PREV_CHAR) ? STRCUR_JUMP_NONE : STRCUR_JUMP_DELIM,
                                 true);
        stride = ci->cursor - pos;
        if (stride) {
          ci->cursor -= stride; /* same as above */
          memmove(ci->line + ci->cursor,
                  ci->line + ci->cursor + stride,
                  (ci->len - (ci->cursor + stride)) + 1);
          ci->len -= stride;
          BLI_assert(ci->len >= 0);
          done = true;
        }
      }
      break;
  }

  if (!done) {
    return OPERATOR_CANCELLED;
  }

  console_select_offset(sc, -stride);

  console_textview_update_rect(sc, region);
  ED_area_tag_redraw(CTX_wm_area(C));

  console_scroll_bottom(region);

  return OPERATOR_FINISHED;
}

void CONSOLE_OT_delete(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Delete";
  ot->description = "Delete text by cursor position";
  ot->idname = "CONSOLE_OT_delete";

  /* api callbacks */
  ot->exec = console_delete_exec;
  ot->poll = ED_operator_console_active;

  /* properties */
  RNA_def_enum(ot->srna,
               "type",
               console_delete_type_items,
               DEL_NEXT_CHAR,
               "Type",
               "Which part of the text to delete");
}

static int console_clear_line_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceConsole *sc = CTX_wm_space_console(C);
  ARegion *region = CTX_wm_region(C);
  ConsoleLine *ci = console_history_verify(C);

  if (ci->len == 0) {
    return OPERATOR_CANCELLED;
  }

  console_history_add(sc, ci);
  console_history_add(sc, NULL);
  console_select_offset(sc, -ci->len);

  console_textview_update_rect(sc, region);

  ED_area_tag_redraw(CTX_wm_area(C));

  console_scroll_bottom(region);

  return OPERATOR_FINISHED;
}

void CONSOLE_OT_clear_line(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Line";
  ot->description = "Clear the line and store in history";
  ot->idname = "CONSOLE_OT_clear_line";

  /* api callbacks */
  ot->exec = console_clear_line_exec;
  ot->poll = ED_operator_console_active;
}

/* the python exec operator uses this */
static int console_clear_exec(bContext *C, wmOperator *op)
{
  SpaceConsole *sc = CTX_wm_space_console(C);
  ARegion *region = CTX_wm_region(C);

  const bool scrollback = RNA_boolean_get(op->ptr, "scrollback");
  const bool history = RNA_boolean_get(op->ptr, "history");

  /* ConsoleLine *ci = */ console_history_verify(C);

  if (scrollback) { /* Last item in history. */
    while (sc->scrollback.first) {
      console_scrollback_free(sc, sc->scrollback.first);
    }
  }

  if (history) {
    while (sc->history.first) {
      console_history_free(sc, sc->history.first);
    }
    console_history_verify(C);
  }

  console_textview_update_rect(sc, region);
  ED_area_tag_redraw(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

void CONSOLE_OT_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear All";
  ot->description = "Clear text by type";
  ot->idname = "CONSOLE_OT_clear";

  /* api callbacks */
  ot->exec = console_clear_exec;
  ot->poll = ED_operator_console_active;

  /* properties */
  RNA_def_boolean(ot->srna, "scrollback", 1, "Scrollback", "Clear the scrollback history");
  RNA_def_boolean(ot->srna, "history", 0, "History", "Clear the command history");
}

/* the python exec operator uses this */
static int console_history_cycle_exec(bContext *C, wmOperator *op)
{
  SpaceConsole *sc = CTX_wm_space_console(C);
  ARegion *region = CTX_wm_region(C);

  /* TODO: stupid, just prevents crashes when no command line. */
  ConsoleLine *ci = console_history_verify(C);
  const bool reverse = RNA_boolean_get(op->ptr, "reverse"); /* assumes down, reverse is up */
  int prev_len = ci->len;

  /* keep a copy of the line above so when history is cycled
   * this is the only function that needs to know about the double-up */
  if (ci->prev) {
    ConsoleLine *ci_prev = (ConsoleLine *)ci->prev;

    if (STREQ(ci->line, ci_prev->line)) {
      console_history_free(sc, ci_prev);
    }
  }

  if (reverse) { /* last item in history */
    ci = sc->history.last;
    BLI_remlink(&sc->history, ci);
    BLI_addhead(&sc->history, ci);
  }
  else {
    ci = sc->history.first;
    BLI_remlink(&sc->history, ci);
    BLI_addtail(&sc->history, ci);
  }

  { /* add a duplicate of the new arg and remove all other instances */
    ConsoleLine *cl;
    while ((cl = console_history_find(sc, ci->line, ci))) {
      console_history_free(sc, cl);
    }

    console_history_add(sc, (ConsoleLine *)sc->history.last);
  }

  ci = sc->history.last;
  console_select_offset(sc, ci->len - prev_len);

  /* could be wrapped so update scroll rect */
  console_textview_update_rect(sc, region);
  ED_area_tag_redraw(CTX_wm_area(C));

  console_scroll_bottom(region);

  return OPERATOR_FINISHED;
}

void CONSOLE_OT_history_cycle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "History Cycle";
  ot->description = "Cycle through history";
  ot->idname = "CONSOLE_OT_history_cycle";

  /* api callbacks */
  ot->exec = console_history_cycle_exec;
  ot->poll = ED_operator_console_active;

  /* properties */
  RNA_def_boolean(ot->srna, "reverse", 0, "Reverse", "Reverse cycle history");
}

/* the python exec operator uses this */
static int console_history_append_exec(bContext *C, wmOperator *op)
{
  SpaceConsole *sc = CTX_wm_space_console(C);
  ARegion *region = CTX_wm_region(C);
  ScrArea *area = CTX_wm_area(C);
  ConsoleLine *ci = console_history_verify(C);
  /* own this text in the new line, don't free */
  char *str = RNA_string_get_alloc(op->ptr, "text", NULL, 0, NULL);
  int cursor = RNA_int_get(op->ptr, "current_character");
  const bool rem_dupes = RNA_boolean_get(op->ptr, "remove_duplicates");
  int prev_len = ci->len;

  if (rem_dupes) {
    ConsoleLine *cl;

    while ((cl = console_history_find(sc, ci->line, ci))) {
      console_history_free(sc, cl);
    }

    if (STREQ(str, ci->line)) {
      MEM_freeN(str);
      return OPERATOR_FINISHED;
    }
  }

  ci = console_history_add_str(sc, str, 1); /* own the string */
  console_select_offset(sc, ci->len - prev_len);
  console_line_cursor_set(ci, cursor);

  ED_area_tag_redraw(area);

  /* when calling render modally this can be NULL when calling:
   * bpy.ops.render.render('INVOKE_DEFAULT') */
  if (region) {
    console_scroll_bottom(region);
  }

  return OPERATOR_FINISHED;
}

void CONSOLE_OT_history_append(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "History Append";
  ot->description = "Append history at cursor position";
  ot->idname = "CONSOLE_OT_history_append";

  /* api callbacks */
  ot->exec = console_history_append_exec;
  ot->poll = ED_operator_console_active;

  /* properties */
  RNA_def_string(ot->srna, "text", NULL, 0, "Text", "Text to insert at the cursor position");
  RNA_def_int(
      ot->srna, "current_character", 0, 0, INT_MAX, "Cursor", "The index of the cursor", 0, 10000);
  RNA_def_boolean(ot->srna,
                  "remove_duplicates",
                  0,
                  "Remove Duplicates",
                  "Remove duplicate items in the history");
}

/* the python exec operator uses this */
static int console_scrollback_append_exec(bContext *C, wmOperator *op)
{
  SpaceConsole *sc = CTX_wm_space_console(C);
  ARegion *region = CTX_wm_region(C);
  ConsoleLine *ci;

  /* own this text in the new line, don't free */
  char *str = RNA_string_get_alloc(op->ptr, "text", NULL, 0, NULL);
  int type = RNA_enum_get(op->ptr, "type");

  console_history_verify(C);

  ci = console_scrollback_add_str(sc, str, 1); /* own the string */
  ci->type = type;

  console_scrollback_limit(sc);

  /* 'region' can be null depending on the operator that runs
   * rendering with invoke default for eg causes this */
  if (region) {
    console_textview_update_rect(sc, region);
  }

  ED_area_tag_redraw(CTX_wm_area(C));

  return OPERATOR_FINISHED;
}

void CONSOLE_OT_scrollback_append(wmOperatorType *ot)
{
  /* defined in DNA_space_types.h */
  static const EnumPropertyItem console_line_type_items[] = {
      {CONSOLE_LINE_OUTPUT, "OUTPUT", 0, "Output", ""},
      {CONSOLE_LINE_INPUT, "INPUT", 0, "Input", ""},
      {CONSOLE_LINE_INFO, "INFO", 0, "Information", ""},
      {CONSOLE_LINE_ERROR, "ERROR", 0, "Error", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* identifiers */
  ot->name = "Scrollback Append";
  ot->description = "Append scrollback text by type";
  ot->idname = "CONSOLE_OT_scrollback_append";

  /* api callbacks */
  ot->exec = console_scrollback_append_exec;
  ot->poll = ED_operator_console_active;

  /* properties */
  RNA_def_string(ot->srna, "text", NULL, 0, "Text", "Text to insert at the cursor position");
  RNA_def_enum(ot->srna,
               "type",
               console_line_type_items,
               CONSOLE_LINE_OUTPUT,
               "Type",
               "Console output type");
}

static int console_copy_exec(bContext *C, wmOperator *UNUSED(op))
{
  SpaceConsole *sc = CTX_wm_space_console(C);
  char *buf = console_select_to_buffer(sc);
  if (buf == NULL) {
    return OPERATOR_CANCELLED;
  }

  WM_clipboard_text_set(buf, 0);
  MEM_freeN(buf);
  return OPERATOR_FINISHED;
}

void CONSOLE_OT_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Copy to Clipboard";
  ot->description = "Copy selected text to clipboard";
  ot->idname = "CONSOLE_OT_copy";

  /* api callbacks */
  ot->poll = ED_operator_console_active;
  ot->exec = console_copy_exec;

  /* properties */
}

static int console_paste_exec(bContext *C, wmOperator *op)
{
  const bool selection = RNA_boolean_get(op->ptr, "selection");
  SpaceConsole *sc = CTX_wm_space_console(C);
  ARegion *region = CTX_wm_region(C);
  ConsoleLine *ci = console_history_verify(C);
  int buf_len;

  char *buf_str = WM_clipboard_text_get(selection, true, &buf_len);
  char *buf_step, *buf_next;

  if (buf_str == NULL) {
    return OPERATOR_CANCELLED;
  }

  buf_step = buf_str;

  while ((buf_next = buf_step) && buf_next[0] != '\0') {
    buf_step = strchr(buf_next, '\n');
    if (buf_step) {
      *buf_step = '\0';
      buf_step++;
    }

    if (buf_next != buf_str) {
      WM_operator_name_call(C, "CONSOLE_OT_execute", WM_OP_EXEC_DEFAULT, NULL, NULL);
      ci = console_history_verify(C);
    }

    console_select_offset(sc, console_line_insert(ci, buf_next));
  }

  MEM_freeN(buf_str);

  console_textview_update_rect(sc, region);
  ED_area_tag_redraw(CTX_wm_area(C));

  console_scroll_bottom(region);

  return OPERATOR_FINISHED;
}

void CONSOLE_OT_paste(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Paste from Clipboard";
  ot->description = "Paste text from clipboard";
  ot->idname = "CONSOLE_OT_paste";

  /* api callbacks */
  ot->poll = ED_operator_console_active;
  ot->exec = console_paste_exec;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna,
                         "selection",
                         0,
                         "Selection",
                         "Paste text selected elsewhere rather than copied (X11/Wayland only)");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

typedef struct SetConsoleCursor {
  int sel_old[2];
  int sel_init;
} SetConsoleCursor;

/* TODO: cursor placement without selection. */
static void console_cursor_set_to_pos(
    SpaceConsole *sc, ARegion *region, SetConsoleCursor *scu, const int mval[2], int UNUSED(sel))
{
  int pos;
  pos = console_char_pick(sc, region, mval);

  if (scu->sel_init == INT_MAX) {
    scu->sel_init = pos;
    sc->sel_start = sc->sel_end = pos;
    return;
  }

  if (pos < scu->sel_init) {
    sc->sel_start = pos;
    sc->sel_end = scu->sel_init;
  }
  else if (pos > sc->sel_start) {
    sc->sel_start = scu->sel_init;
    sc->sel_end = pos;
  }
  else {
    sc->sel_start = sc->sel_end = pos;
  }
}

static void console_modal_select_apply(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceConsole *sc = CTX_wm_space_console(C);
  ARegion *region = CTX_wm_region(C);
  SetConsoleCursor *scu = op->customdata;
  int mval[2];
  int sel_prev[2];

  mval[0] = event->mval[0];
  mval[1] = event->mval[1];

  sel_prev[0] = sc->sel_start;
  sel_prev[1] = sc->sel_end;

  console_cursor_set_to_pos(sc, region, scu, mval, true);

  /* only redraw if the selection changed */
  if (sel_prev[0] != sc->sel_start || sel_prev[1] != sc->sel_end) {
    ED_area_tag_redraw(CTX_wm_area(C));
  }
}

static void console_cursor_set_exit(bContext *C, wmOperator *op)
{
  SpaceConsole *sc = CTX_wm_space_console(C);
  SetConsoleCursor *scu = op->customdata;

  console_select_update_primary_clipboard(sc);

  MEM_freeN(scu);
}

static int console_modal_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceConsole *sc = CTX_wm_space_console(C);
  //  ARegion *region = CTX_wm_region(C);
  SetConsoleCursor *scu;

  op->customdata = MEM_callocN(sizeof(SetConsoleCursor), "SetConsoleCursor");
  scu = op->customdata;

  scu->sel_old[0] = sc->sel_start;
  scu->sel_old[1] = sc->sel_end;

  scu->sel_init = INT_MAX;

  WM_event_add_modal_handler(C, op);

  console_modal_select_apply(C, op, event);

  return OPERATOR_RUNNING_MODAL;
}

static int console_modal_select(bContext *C, wmOperator *op, const wmEvent *event)
{
  switch (event->type) {
    case LEFTMOUSE:
    case MIDDLEMOUSE:
    case RIGHTMOUSE:
      if (event->val == KM_RELEASE) {
        console_cursor_set_exit(C, op);
        return OPERATOR_FINISHED;
      }
      break;
    case MOUSEMOVE:
      console_modal_select_apply(C, op, event);
      break;
  }

  return OPERATOR_RUNNING_MODAL;
}

static void console_modal_select_cancel(bContext *C, wmOperator *op)
{
  console_cursor_set_exit(C, op);
}

void CONSOLE_OT_select_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Set Selection";
  ot->idname = "CONSOLE_OT_select_set";
  ot->description = "Set the console selection";

  /* api callbacks */
  ot->invoke = console_modal_select_invoke;
  ot->modal = console_modal_select;
  ot->cancel = console_modal_select_cancel;
  ot->poll = ED_operator_console_active;
}

static int console_selectword_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *event)
{
  SpaceConsole *sc = CTX_wm_space_console(C);
  ARegion *region = CTX_wm_region(C);

  ConsoleLine cl_dummy = {NULL};
  ConsoleLine *cl;
  int ret = OPERATOR_CANCELLED;
  int pos, offset, n;

  pos = console_char_pick(sc, region, event->mval);

  console_scrollback_prompt_begin(sc, &cl_dummy);

  if (console_line_column_from_index(sc, pos, &cl, &offset, &n)) {
    int sel[2] = {n, n};

    BLI_str_cursor_step_bounds_utf8(cl->line, cl->len, n, &sel[1], &sel[0]);

    sel[0] = offset - sel[0];
    sel[1] = offset - sel[1];

    if ((sel[0] != sc->sel_start) || (sel[1] != sc->sel_end)) {
      sc->sel_start = sel[0];
      sc->sel_end = sel[1];
      ED_area_tag_redraw(CTX_wm_area(C));
      ret = OPERATOR_FINISHED;
    }
  }

  console_scrollback_prompt_end(sc, &cl_dummy);

  if (ret & OPERATOR_FINISHED) {
    console_select_update_primary_clipboard(sc);
  }

  return ret;
}

void CONSOLE_OT_select_word(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Word";
  ot->description = "Select word at cursor position";
  ot->idname = "CONSOLE_OT_select_word";

  /* api callbacks */
  ot->invoke = console_selectword_invoke;
  ot->poll = ED_operator_console_active;
}
