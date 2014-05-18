/* See LICENSE for licence details. */
#include <limits.h> /* UINT_MAX */
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xlocale.h>

#define XK_NO_MOD UINT_MAX

static const char *im_font = "-*-*-medium-r-*-*-16-*-*-*-*-*-*-*";

enum x_misc { /* default terminal size */
    TERM_WIDTH = 640,
    TERM_HEIGHT = 384,
};

struct keydef {
	KeySym k;
	unsigned int mask;
	char s[BUFSIZE];
};

const struct keydef key[] = {
    { XK_BackSpace, XK_NO_MOD, "\177"    },
    { XK_Up,        XK_NO_MOD, "\033[A"  },
    { XK_Down,      XK_NO_MOD, "\033[B"  },
    { XK_Right,     XK_NO_MOD, "\033[C"  },
    { XK_Left,      XK_NO_MOD, "\033[D"  },
    { XK_Begin,     XK_NO_MOD, "\033[G"  },
    { XK_Home,      XK_NO_MOD, "\033[1~" },
    { XK_Insert,    XK_NO_MOD, "\033[2~" },
    { XK_Delete,    XK_NO_MOD, "\033[3~" },
    { XK_End,       XK_NO_MOD, "\033[4~" },
    { XK_Prior,     XK_NO_MOD, "\033[5~" },
    { XK_Next,      XK_NO_MOD, "\033[6~" },
    { XK_F1,        XK_NO_MOD, "\033[[A" },
    { XK_F2,        XK_NO_MOD, "\033[[B" },
    { XK_F3,        XK_NO_MOD, "\033[[C" },
    { XK_F4,        XK_NO_MOD, "\033[[D" },
    { XK_F5,        XK_NO_MOD, "\033[[E" },
    { XK_F6,        XK_NO_MOD, "\033[17~"},
    { XK_F7,        XK_NO_MOD, "\033[18~"},
    { XK_F8,        XK_NO_MOD, "\033[19~"},
    { XK_F9,        XK_NO_MOD, "\033[20~"},
    { XK_F10,       XK_NO_MOD, "\033[21~"},
    { XK_F11,       XK_NO_MOD, "\033[23~"},
    { XK_F12,       XK_NO_MOD, "\033[24~"},
    { XK_F13,       XK_NO_MOD, "\033[25~"},
    { XK_F14,       XK_NO_MOD, "\033[26~"},
    { XK_F15,       XK_NO_MOD, "\033[28~"},
    { XK_F16,       XK_NO_MOD, "\033[29~"},
    { XK_F17,       XK_NO_MOD, "\033[31~"},
    { XK_F18,       XK_NO_MOD, "\033[32~"},
    { XK_F19,       XK_NO_MOD, "\033[33~"},
    { XK_F20,       XK_NO_MOD, "\033[34~"},
};

/* not assigned:
    kcbt=\E[Z,kmous=\E[M,kspd=^Z,
*/

struct xwindow {
    Display *display;
    Window window;
    Pixmap pixbuf;
    GC gc;
	int width, height;
    int screen;
	XIM im;
	XIC ic;
	XIMStyle input_style;
};

XIMStyle select_better_style(XIMStyle im_style, XIMStyle best_style)
{
	XIMStyle i, b;
	XIMStyle preedit, status;

	preedit = XIMPreeditArea | XIMPreeditCallbacks |
		XIMPreeditPosition | XIMPreeditNothing | XIMPreeditNone;
	status = XIMStatusArea | XIMStatusCallbacks |
		XIMStatusNothing | XIMStatusNone;

	/* if one style == 0, select another */
	if (im_style == 0)
		return best_style;

	if (best_style == 0)
		return im_style;

	/* im_style and best_style are the same */
	if ((im_style & (preedit | status)) == (best_style & (preedit | status)))
		return im_style;

	/* ok, now we select better style */
	i = im_style & preedit;
	b = best_style & preedit;

	if (i != b) { /* different preedit style */
		if (i | b | XIMPreeditCallbacks)
			return (i == XIMPreeditCallbacks) ? im_style: best_style;
		else if (i | b | XIMPreeditPosition)
			return (i == XIMPreeditPosition) ? im_style: best_style;
		else if (i | b | XIMPreeditArea)
			return (i == XIMPreeditArea) ? im_style: best_style;
		else if (i | b | XIMPreeditNothing)
			return (i == XIMPreeditNothing) ? im_style: best_style;
	}
	else { /* if preedit flags are the same, compare status flags */
		i = im_style & status;
		b = best_style & status;

		if (i | b | XIMStatusCallbacks)
			return (i == XIMStatusCallbacks) ? im_style: best_style;
		else if (i | b | XIMStatusArea)
			return (i == XIMStatusArea) ? im_style: best_style;
		else if (i | b | XIMStatusNothing)
			return (i == XIMStatusNothing) ? im_style: best_style;
	}
}

void im_init(struct xwindow *xw)
{
	int i;
	int num_missing_charsets = 0;
	char **missing_charsets;
	char *default_string;
	long im_event_mask;
	XFontSet fontset;
	XVaNestedList list;
	XIMStyles *im_styles;
	XIMStyle app_styles, style;

	if ((xw->im = XOpenIM(xw->display, NULL, NULL, NULL)) == NULL)
		fatal("couldn't open input method");

	/* check available input style */
	app_styles = XIMPreeditNone | XIMPreeditNothing | XIMPreeditPosition
		| XIMStatusNone | XIMStatusNothing;

	XGetIMValues(xw->im, XNQueryInputStyle, &im_styles, NULL);
	xw->input_style = 0;

	for (i = 0; i < im_styles->count_styles; i++) {
		style = im_styles->supported_styles[i];

		if ((style & app_styles) == style)
			xw->input_style = select_better_style(style, xw->input_style);
	}
	if (xw->input_style == 0)
		fatal("couldn't get valid input method style");

	XFree(im_styles);

	/* create ic */
	fontset = XCreateFontSet(xw->display, im_font,
		&missing_charsets, &num_missing_charsets, &default_string);
	list = XVaCreateNestedList(0, XNSpotLocation, &(XPoint){.x = 0, .y = 0}, XNFontSet, fontset, NULL);

	if ((xw->ic = XCreateIC(xw->im, XNInputStyle, xw->input_style,
		XNPreeditAttributes, list, XNClientWindow, xw->window, NULL)) == NULL)
		fatal("couldn't create ic");

	XFree(list);

	XGetICValues(xw->ic, XNFilterEvents, &im_event_mask, NULL);
	XSelectInput(xw->display, xw->window, im_event_mask
		| FocusChangeMask | ExposureMask | KeyPressMask | StructureNotifyMask);
	XSetICFocus(xw->ic);
}

void xw_init(struct xwindow *xw)
{
	if ((xw->display = XOpenDisplay(NULL)) == NULL)
		fatal("XOpenDisplay failed");

	if (!XSupportsLocale())
		fatal("X does not support locale\n");

	if (XSetLocaleModifiers("") == NULL)
		warn("cannot set locale modifiers");

	xw->screen = DefaultScreen(xw->display);
	xw->window = XCreateSimpleWindow(xw->display, DefaultRootWindow(xw->display),
		0, 0, TERM_WIDTH, TERM_HEIGHT, 0, color_list[DEFAULT_FG], color_list[DEFAULT_BG]);

	XSetWMProperties(xw->display, xw->window, &(XTextProperty)
		{.value = (unsigned char *) "yaftx", .encoding = XA_STRING, .format = 8, .nitems = 5},
		NULL, NULL, 0, NULL, NULL, NULL);
	xw->gc = XCreateGC(xw->display, xw->window, 0, NULL);
	XSetGraphicsExposures(xw->display, xw->gc, False);

	xw->width = DisplayWidth(xw->display, xw->screen);
	xw->height = DisplayHeight(xw->display, xw->screen);
	xw->pixbuf = XCreatePixmap(xw->display, xw->window,
		xw->width, xw->height, XDefaultDepth(xw->display, xw->screen));

	im_init(xw);
	XMapWindow(xw->display, xw->window);
}

void xw_die(struct xwindow *xw)
{
	XFreeGC(xw->display, xw->gc);
	XFreePixmap(xw->display, xw->pixbuf);
	XDestroyIC(xw->ic);
	XCloseIM(xw->im);
	XDestroyWindow(xw->display, xw->window);
	XCloseDisplay(xw->display);
}

void draw_line(struct xwindow *xw, struct terminal *term, int line)
{
	int bit_shift, margin_right;
	int col, glyph_width_offset, glyph_height_offset;
	struct color_pair color;
	struct cell *cp;
	const struct static_glyph_t *gp;

	/* at first, fill all pixels of line in backgournd color */
	XSetForeground(xw->display, xw->gc, color_list[DEFAULT_BG]);
	XFillRectangle(xw->display, xw->pixbuf, xw->gc, 0, line * CELL_HEIGHT, term->width, CELL_HEIGHT);

	for (col = term->cols - 1; col >= 0; col--) {
		margin_right = (term->cols - 1 - col) * CELL_WIDTH;

		/* get cell color and glyph */
		cp = &term->cells[col + line * term->cols];
		color = cp->color;
		gp = cp->gp;

		/* check cursor positon */
		if ((term->mode & MODE_CURSOR && line == term->cursor.y)
			&& (col == term->cursor.x
			|| (cp->width == WIDE && (col + 1) == term->cursor.x)
			|| (cp->width == NEXT_TO_WIDE && (col - 1) == term->cursor.x))) {
			color.fg = DEFAULT_BG;
			color.bg = (!tty.visible && tty.background_draw) ? PASSIVE_CURSOR_COLOR: ACTIVE_CURSOR_COLOR;
		}

		for (glyph_height_offset = 0; glyph_height_offset < CELL_HEIGHT; glyph_height_offset++) {
			/* if UNDERLINE attribute on, swap bg/fg */
			if ((glyph_height_offset == (CELL_HEIGHT - 1)) && (cp->attribute & attr_mask[UNDERLINE]))
				color.bg = color.fg;

			for (glyph_width_offset = 0; glyph_width_offset < CELL_WIDTH; glyph_width_offset++) {
				/* check wide character or not */
				if (cp->width == WIDE)
					bit_shift = glyph_width_offset + CELL_WIDTH;
				else
					bit_shift = glyph_width_offset;

				/* set color palette */
				if (gp->bitmap[glyph_height_offset] & (0x01 << bit_shift))
					XSetForeground(xw->display, xw->gc, color_list[color.fg]);
				else if (color.bg != DEFAULT_BG)
					XSetForeground(xw->display, xw->gc, color_list[color.bg]);
				else /* already draw */
					continue;

				/* update copy buffer */
				XDrawPoint(xw->display, xw->pixbuf, xw->gc, term->width - 1 - margin_right - glyph_width_offset,
					line * CELL_HEIGHT + glyph_height_offset);
			}
		}
	}
	/* actual display update */
	XCopyArea(xw->display, xw->pixbuf, xw->window, xw->gc, 0, line * CELL_HEIGHT,
		term->width, CELL_HEIGHT, 0, line * CELL_HEIGHT);

	term->line_dirty[line] = ((term->mode & MODE_CURSOR) && term->cursor.y == line) ? true: false;
}

void set_preedit_pos(struct xwindow *xw, struct terminal *term)
{
	XVaNestedList list;
	XPoint pos;

	pos.x = term->cursor.x * CELL_WIDTH;
	pos.y = term->cursor.y * CELL_HEIGHT;

	list = XVaCreateNestedList(0, XNSpotLocation, &pos, NULL);

	XSetICValues(xw->ic, XNPreeditAttributes, list, NULL);

	XFree(list);
}

void refresh(struct xwindow *xw, struct terminal *term)
{
	int line;

	if (term->mode & MODE_CURSOR)
		term->line_dirty[term->cursor.y] = true;

	for (line = 0; line < term->lines; line++) {
		if (term->line_dirty[line])
			draw_line(xw, term, line);
	}

	if (xw->input_style & XIMPreeditPosition)
		set_preedit_pos(xw, term);
}
