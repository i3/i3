/* See LICENSE file for copyright and license details. */
#define _BSD_SOURCE
#include <ctype.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

/* macros */
#define CLEANMASK(mask)         (mask & ~(numlockmask | LockMask))
#define INRECT(X,Y,RX,RY,RW,RH) ((X) >= (RX) && (X) < (RX) + (RW) && (Y) >= (RY) && (Y) < (RY) + (RH))
#define MIN(a, b)               ((a) < (b) ? (a) : (b))

/* enums */
enum { ColFG, ColBG, ColLast };

/* typedefs */
typedef struct {
	int x, y, w, h;
	unsigned long norm[ColLast];
	unsigned long sel[ColLast];
	Drawable drawable;
	GC gc;
	struct {
		XFontStruct *xfont;
		XFontSet set;
		int ascent;
		int descent;
		int height;
	} font;
} DC; /* draw context */

typedef struct Item Item;
struct Item {
	char *text;
	Item *next;		/* traverses all items */
	Item *left, *right;	/* traverses items matching current search pattern */
};

/* forward declarations */
static void appenditem(Item *i, Item **list, Item **last);
static void calcoffsets(void);
static char *cistrstr(const char *s, const char *sub);
static void cleanup(void);
static void drawmenu(void);
static void drawtext(const char *text, unsigned long col[ColLast]);
static void eprint(const char *errstr, ...);
static unsigned long getcolor(const char *colstr);
static Bool grabkeyboard(void);
static void initfont(const char *fontstr);
static void kpress(XKeyEvent * e);
static void match(char *pattern);
static void readstdin(void);
static void run(void);
static void setup(Bool topbar);
static int textnw(const char *text, unsigned int len);
static int textw(const char *text);

#include "config.h"

/* variables */
static char *maxname = NULL;
static char *prompt = NULL;
static char text[4096];
static int cmdw = 0;
static int promptw = 0;
static int ret = 0;
static int screen;
static unsigned int mw, mh;
static unsigned int numlockmask = 0;
static Bool running = True;
static Display *dpy;
static DC dc;
static Item *allitems = NULL;	/* first of all items */
static Item *item = NULL;	/* first of pattern matching items */
static Item *sel = NULL;
static Item *next = NULL;
static Item *prev = NULL;
static Item *curr = NULL;
static Window root, win;
static int (*fstrncmp)(const char *, const char *, size_t n) = strncmp;
static char *(*fstrstr)(const char *, const char *) = strstr;

void
appenditem(Item *i, Item **list, Item **last) {
	if(!(*last))
		*list = i;
	else
		(*last)->right = i;
	i->left = *last;
	i->right = NULL;
	*last = i;
}

void
calcoffsets(void) {
	int tw;
	unsigned int w;

	if(!curr)
		return;
	w = promptw + cmdw + 2 * spaceitem;
	for(next = curr; next; next=next->right) {
		tw = textw(next->text);
		if(tw > mw / 3)
			tw = mw / 3;
		w += tw;
		if(w > mw)
			break;
	}
	w = promptw + cmdw + 2 * spaceitem;
	for(prev = curr; prev && prev->left; prev=prev->left) {
		tw = textw(prev->left->text);
		if(tw > mw / 3)
			tw = mw / 3;
		w += tw;
		if(w > mw)
			break;
	}
}

char *
cistrstr(const char *s, const char *sub) {
	int c, csub;
	unsigned int len;

	if(!sub)
		return (char *)s;
	if((c = *sub++) != 0) {
		c = tolower(c);
		len = strlen(sub);
		do {
			do {
				if((csub = *s++) == 0)
					return (NULL);
			}
			while(tolower(csub) != c);
		}
		while(strncasecmp(s, sub, len) != 0);
		s--;
	}
	return (char *)s;
}

void
cleanup(void) {
	Item *itm;

	while(allitems) {
		itm = allitems->next;
		free(allitems->text);
		free(allitems);
		allitems = itm;
	}
	if(dc.font.set)
		XFreeFontSet(dpy, dc.font.set);
	else
		XFreeFont(dpy, dc.font.xfont);
	XFreePixmap(dpy, dc.drawable);
	XFreeGC(dpy, dc.gc);
	XDestroyWindow(dpy, win);
	XUngrabKeyboard(dpy, CurrentTime);
}

void
drawmenu(void) {
	Item *i;

	dc.x = 0;
	dc.y = 0;
	dc.w = mw;
	dc.h = mh;
	drawtext(NULL, dc.norm);
	/* print prompt? */
	if(promptw) {
		dc.w = promptw;
		drawtext(prompt, dc.sel);
	}
	dc.x += promptw;
	dc.w = mw - promptw;
	/* print command */
	if(cmdw && item)
		dc.w = cmdw;
	drawtext(text[0] ? text : NULL, dc.norm);
	dc.x += cmdw;
	if(curr) {
		dc.w = spaceitem;
		drawtext((curr && curr->left) ? "<" : NULL, dc.norm);
		dc.x += dc.w;
		/* determine maximum items */
		for(i = curr; i != next; i=i->right) {
			dc.w = textw(i->text);
			if(dc.w > mw / 3)
				dc.w = mw / 3;
			drawtext(i->text, (sel == i) ? dc.sel : dc.norm);
			dc.x += dc.w;
		}
		dc.x = mw - spaceitem;
		dc.w = spaceitem;
		drawtext(next ? ">" : NULL, dc.norm);
	}
	XCopyArea(dpy, dc.drawable, win, dc.gc, 0, 0, mw, mh, 0, 0);
	XFlush(dpy);
}

void
drawtext(const char *text, unsigned long col[ColLast]) {
	char buf[256];
	int i, x, y, h, len, olen;
	XRectangle r = { dc.x, dc.y, dc.w, dc.h };

	XSetForeground(dpy, dc.gc, col[ColBG]);
	XFillRectangles(dpy, dc.drawable, dc.gc, &r, 1);
	if(!text)
		return;
	olen = strlen(text);
	h = dc.font.ascent + dc.font.descent;
	y = dc.y + (dc.h / 2) - (h / 2) + dc.font.ascent;
	x = dc.x + (h / 2);
	/* shorten text if necessary */
	for(len = MIN(olen, sizeof buf); len && textnw(text, len) > dc.w - h; len--);
	if(!len)
		return;
	memcpy(buf, text, len);
	if(len < olen)
		for(i = len; i && i > len - 3; buf[--i] = '.');
	XSetForeground(dpy, dc.gc, col[ColFG]);
	if(dc.font.set)
		XmbDrawString(dpy, dc.drawable, dc.font.set, dc.gc, x, y, buf, len);
	else
		XDrawString(dpy, dc.drawable, dc.gc, x, y, buf, len);
}

void
eprint(const char *errstr, ...) {
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

unsigned long
getcolor(const char *colstr) {
	Colormap cmap = DefaultColormap(dpy, screen);
	XColor color;

	if(!XAllocNamedColor(dpy, cmap, colstr, &color, &color))
		eprint("error, cannot allocate color '%s'\n", colstr);
	return color.pixel;
}

Bool
grabkeyboard(void) {
	unsigned int len;

	for(len = 1000; len; len--) {
		if(XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime)
		== GrabSuccess)
			break;
		usleep(1000);
	}
	return len > 0;
}

void
initfont(const char *fontstr) {
	char *def, **missing;
	int i, n;

	if(!fontstr || fontstr[0] == '\0')
		eprint("error, cannot load font: '%s'\n", fontstr);
	missing = NULL;
	dc.font.set = XCreateFontSet(dpy, fontstr, &missing, &n, &def);
	if(missing)
		XFreeStringList(missing);
	if(dc.font.set) {
		XFontSetExtents *font_extents;
		XFontStruct **xfonts;
		char **font_names;
		dc.font.ascent = dc.font.descent = 0;
		font_extents = XExtentsOfFontSet(dc.font.set);
		n = XFontsOfFontSet(dc.font.set, &xfonts, &font_names);
		for(i = 0, dc.font.ascent = 0, dc.font.descent = 0; i < n; i++) {
			if(dc.font.ascent < (*xfonts)->ascent)
				dc.font.ascent = (*xfonts)->ascent;
			if(dc.font.descent < (*xfonts)->descent)
				dc.font.descent = (*xfonts)->descent;
			xfonts++;
		}
	}
	else {
		if(!(dc.font.xfont = XLoadQueryFont(dpy, fontstr))
		&& !(dc.font.xfont = XLoadQueryFont(dpy, "fixed")))
			eprint("error, cannot load font: '%s'\n", fontstr);
		dc.font.ascent = dc.font.xfont->ascent;
		dc.font.descent = dc.font.xfont->descent;
	}
	dc.font.height = dc.font.ascent + dc.font.descent;
}

void
kpress(XKeyEvent * e) {
	char buf[32];
	int i, num;
	unsigned int len;
	KeySym ksym;

	len = strlen(text);
	buf[0] = 0;
	num = XLookupString(e, buf, sizeof buf, &ksym, 0);
	if(IsKeypadKey(ksym)) {
		if(ksym == XK_KP_Enter)
			ksym = XK_Return;
		else if(ksym >= XK_KP_0 && ksym <= XK_KP_9)
			ksym = (ksym - XK_KP_0) + XK_0;
	}
	if(IsFunctionKey(ksym) || IsKeypadKey(ksym)
	   || IsMiscFunctionKey(ksym) || IsPFKey(ksym)
	   || IsPrivateKeypadKey(ksym))
		return;
	/* first check if a control mask is omitted */
	if(e->state & ControlMask) {
		switch (ksym) {
		default:	/* ignore other control sequences */
			return;
		case XK_bracketleft:
			ksym = XK_Escape;
			break;
		case XK_h:
		case XK_H:
			ksym = XK_BackSpace;
			break;
		case XK_i:
		case XK_I:
			ksym = XK_Tab;
			break;
		case XK_j:
		case XK_J:
			ksym = XK_Return;
			break;
		case XK_u:
		case XK_U:
			text[0] = 0;
			match(text);
			drawmenu();
			return;
		case XK_w:
		case XK_W:
			if(len) {
				i = len - 1;
				while(i >= 0 && text[i] == ' ')
					text[i--] = 0;
				while(i >= 0 && text[i] != ' ')
					text[i--] = 0;
				match(text);
				drawmenu();
			}
			return;
		}
	}
	if(CLEANMASK(e->state) & Mod1Mask) {
		switch(ksym) {
		default: return;
		case XK_h:
			ksym = XK_Left;
			break;
		case XK_l:
			ksym = XK_Right;
			break;
		case XK_j:
			ksym = XK_Next;
			break;
		case XK_k:
			ksym = XK_Prior;
			break;
		case XK_g:
			ksym = XK_Home;
			break;
		case XK_G:
			ksym = XK_End;
			break;
		}
	}
	switch(ksym) {
	default:
		if(num && !iscntrl((int) buf[0])) {
			buf[num] = 0;
			if(len > 0)
				strncat(text, buf, sizeof text);
			else
				strncpy(text, buf, sizeof text);
			match(text);
		}
		break;
	case XK_BackSpace:
		if(len) {
			text[--len] = 0;
			match(text);
		}
		break;
	case XK_End:
		if(!item)
			return;
		while(next) {
			sel = curr = next;
			calcoffsets();
		}
		while(sel && sel->right)
			sel = sel->right;
		break;
	case XK_Escape:
		ret = 1;
		running = False;
		break;
	case XK_Home:
		if(!item)
			return;
		sel = curr = item;
		calcoffsets();
		break;
	case XK_Left:
		if(!(sel && sel->left))
			return;
		sel=sel->left;
		if(sel->right == curr) {
			curr = prev;
			calcoffsets();
		}
		break;
	case XK_Next:
		if(!next)
			return;
		sel = curr = next;
		calcoffsets();
		break;
	case XK_Prior:
		if(!prev)
			return;
		sel = curr = prev;
		calcoffsets();
		break;
	case XK_Return:
		if((e->state & ShiftMask) && *text)
			fprintf(stdout, "%s", text);
		else if(sel)
			fprintf(stdout, "%s", sel->text);
		else if(*text)
			fprintf(stdout, "%s", text);
		fflush(stdout);
		running = False;
		break;
	case XK_Right:
		if(!(sel && sel->right))
			return;
		sel=sel->right;
		if(sel == next) {
			curr = next;
			calcoffsets();
		}
		break;
	case XK_Tab:
		if(!sel)
			return;
		strncpy(text, sel->text, sizeof text);
		match(text);
		break;
	}
	drawmenu();
}

void
match(char *pattern) {
	unsigned int plen;
	Item *i, *itemend, *lexact, *lprefix, *lsubstr, *exactend, *prefixend, *substrend;

	if(!pattern)
		return;
	plen = strlen(pattern);
	item = lexact = lprefix = lsubstr = itemend = exactend = prefixend = substrend = NULL;
	for(i = allitems; i; i = i->next)
		if(!fstrncmp(pattern, i->text, plen + 1))
			appenditem(i, &lexact, &exactend);
		else if(!fstrncmp(pattern, i->text, plen))
			appenditem(i, &lprefix, &prefixend);
		else if(fstrstr(i->text, pattern))
			appenditem(i, &lsubstr, &substrend);
	if(lexact) {
		item = lexact;
		itemend = exactend;
	}
	if(lprefix) {
		if(itemend) {
			itemend->right = lprefix;
			lprefix->left = itemend;
		}
		else
			item = lprefix;
		itemend = prefixend;
	}
	if(lsubstr) {
		if(itemend) {
			itemend->right = lsubstr;
			lsubstr->left = itemend;
		}
		else
			item = lsubstr;
	}
	curr = prev = next = sel = item;
	calcoffsets();
}

void
readstdin(void) {
	char *p, buf[1024];
	unsigned int len = 0, max = 0;
	Item *i, *new;

	i = 0;
	while(fgets(buf, sizeof buf, stdin)) {
		len = strlen(buf);
		if (buf[len - 1] == '\n')
			buf[len - 1] = 0;
		if(!(p = strdup(buf)))
			eprint("fatal: could not strdup() %u bytes\n", strlen(buf));
		if(max < len) {
			maxname = p;
			max = len;
		}
		if((new = (Item *)malloc(sizeof(Item))) == NULL)
			eprint("fatal: could not malloc() %u bytes\n", sizeof(Item));
		new->next = new->left = new->right = NULL;
		new->text = p;
		if(!i)
			allitems = new;
		else 
			i->next = new;
		i = new;
	}
}

void
run(void) {
	XEvent ev;

	/* main event loop */
	while(running && !XNextEvent(dpy, &ev))
		switch (ev.type) {
		default:	/* ignore all crap */
			break;
		case KeyPress:
			kpress(&ev.xkey);
			break;
		case Expose:
			if(ev.xexpose.count == 0)
				drawmenu();
			break;
		}
}

void
setup(Bool topbar) {
	int i, j, x, y;
#if XINERAMA
	int n;
	XineramaScreenInfo *info = NULL;
#endif
	XModifierKeymap *modmap;
	XSetWindowAttributes wa;

	/* init modifier map */
	modmap = XGetModifierMapping(dpy);
	for(i = 0; i < 8; i++)
		for(j = 0; j < modmap->max_keypermod; j++) {
			if(modmap->modifiermap[i * modmap->max_keypermod + j]
			== XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
		}
	XFreeModifiermap(modmap);

	/* style */
	dc.norm[ColBG] = getcolor(normbgcolor);
	dc.norm[ColFG] = getcolor(normfgcolor);
	dc.sel[ColBG] = getcolor(selbgcolor);
	dc.sel[ColFG] = getcolor(selfgcolor);
	initfont(font);

	/* menu window */
	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ExposureMask | ButtonPressMask | KeyPressMask;

	/* menu window geometry */
	mh = dc.font.height + 2;
#if XINERAMA
	if(XineramaIsActive(dpy) && (info = XineramaQueryScreens(dpy, &n))) {
		i = 0;
		if(n > 1) {
			int di;
			unsigned int dui;
			Window dummy;
			if(XQueryPointer(dpy, root, &dummy, &dummy, &x, &y, &di, &di, &dui))
				for(i = 0; i < n; i++)
					if(INRECT(x, y, info[i].x_org, info[i].y_org, info[i].width, info[i].height))
						break;
		}
		x = info[i].x_org;
		y = topbar ? info[i].y_org : info[i].y_org + info[i].height - mh;
		mw = info[i].width;
		XFree(info);
	}
	else
#endif
	{
		x = 0;
		y = topbar ? 0 : DisplayHeight(dpy, screen) - mh;
		mw = DisplayWidth(dpy, screen);
	}

	win = XCreateWindow(dpy, root, x, y, mw, mh, 0,
			DefaultDepth(dpy, screen), CopyFromParent,
			DefaultVisual(dpy, screen),
			CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);

	/* pixmap */
	dc.drawable = XCreatePixmap(dpy, root, mw, mh, DefaultDepth(dpy, screen));
	dc.gc = XCreateGC(dpy, root, 0, 0);
	XSetLineAttributes(dpy, dc.gc, 1, LineSolid, CapButt, JoinMiter);
	if(!dc.font.set)
		XSetFont(dpy, dc.gc, dc.font.xfont->fid);
	if(maxname)
		cmdw = textw(maxname);
	if(cmdw > mw / 3)
		cmdw = mw / 3;
	if(prompt)
		promptw = textw(prompt);
	if(promptw > mw / 5)
		promptw = mw / 5;
	text[0] = 0;
	match(text);
	XMapRaised(dpy, win);
}

int
textnw(const char *text, unsigned int len) {
	XRectangle r;

	if(dc.font.set) {
		XmbTextExtents(dc.font.set, text, len, NULL, &r);
		return r.width;
	}
	return XTextWidth(dc.font.xfont, text, len);
}

int
textw(const char *text) {
	return textnw(text, strlen(text)) + dc.font.height;
}

int
main(int argc, char *argv[]) {
	unsigned int i;
	Bool topbar = True;

	/* command line args */
	for(i = 1; i < argc; i++)
		if(!strcmp(argv[i], "-i")) {
			fstrncmp = strncasecmp;
			fstrstr = cistrstr;
		}
		else if(!strcmp(argv[i], "-b"))
			topbar = False;
		else if(!strcmp(argv[i], "-fn")) {
			if(++i < argc) font = argv[i];
		}
		else if(!strcmp(argv[i], "-nb")) {
			if(++i < argc) normbgcolor = argv[i];
		}
		else if(!strcmp(argv[i], "-nf")) {
			if(++i < argc) normfgcolor = argv[i];
		}
		else if(!strcmp(argv[i], "-p")) {
			if(++i < argc) prompt = argv[i];
		}
		else if(!strcmp(argv[i], "-sb")) {
			if(++i < argc) selbgcolor = argv[i];
		}
		else if(!strcmp(argv[i], "-sf")) {
			if(++i < argc) selfgcolor = argv[i];
		}
		else if(!strcmp(argv[i], "-v"))
			eprint("dmenu-"VERSION", © 2006-2008 dmenu engineers, see LICENSE for details\n");
		else
			eprint("usage: dmenu [-i] [-b] [-fn <font>] [-nb <color>] [-nf <color>]\n"
			       "             [-p <prompt>] [-sb <color>] [-sf <color>] [-v]\n");
	if(!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fprintf(stderr, "warning: no locale support\n");
	if(!(dpy = XOpenDisplay(0)))
		eprint("dmenu: cannot open display\n");
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

	if(isatty(STDIN_FILENO)) {
		readstdin();
		running = grabkeyboard();
	}
	else { /* prevent keypress loss */
		running = grabkeyboard();
		readstdin();
	}

	setup(topbar);
	drawmenu();
	XSync(dpy, False);
	run();
	cleanup();
	XCloseDisplay(dpy);
	return ret;
}
