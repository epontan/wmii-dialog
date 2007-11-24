/* See LICENSE file for copyright and license details. */
#include <ctype.h>
#include <locale.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

/* enums */
enum { ColFG, ColBG, ColBR, ColLast };

/* typedefs */
typedef struct {
	int x, y, w, h;
	unsigned long colors[ColLast];
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

/* forward declarations */
void calcoffsets(void);
void cleanup(void);
void drawdialog(void);
void drawmessage(void);
void *emalloc(unsigned int size);
void eprint(const char *errstr, ...);
unsigned long getcolor(const char *colstr);
void initfont(const char *fontstr);
void run(void);
void setup(void);
unsigned int textnw(const char *text, unsigned int len);
unsigned int textw(const char *text);
int parse_int(const char *str);
void split_up(char *text);
void start_timer(void);

#include "config.h"

/* variables */
char *font = FONT;
char *bg = BGCOLOR;
char *fg = FGCOLOR;
char *br = BRCOLOR;
char *message[MAX_LINES];
int count;
int hs = HSPACE;
int vs = VSPACE;
int timeout = 0;
int screen;
unsigned int dw, dh;
Bool running = True;
Display *dpy;
DC dc = {0};
Window root, win;

void
cleanup(void) {
	if(dc.font.set)
		XFreeFontSet(dpy, dc.font.set);
	else
		XFreeFont(dpy, dc.font.xfont);
	XFreePixmap(dpy, dc.drawable);
	XFreeGC(dpy, dc.gc);
	XDestroyWindow(dpy, win);
}

void
drawdialog(void) {
	dc.x = 0;
	dc.y = 0;
	dc.w = dw;
	dc.h = dh;
	drawmessage();
	XCopyArea(dpy, dc.drawable, win, dc.gc, 0, 0, dw, dh, 0, 0);
	XFlush(dpy);
}

void
drawmessage(void) {
	int i, y;

	y = dc.font.ascent + dc.font.descent;
	XSetForeground(dpy, dc.gc, dc.colors[ColBG]);
	XFillRectangle(dpy, dc.drawable, dc.gc, 0, 0, dw, dh);
	XSetForeground(dpy, dc.gc, dc.colors[ColFG]);
	if(dc.font.set)
		for(i = 0; i < count; i++)
			XmbDrawString(dpy, dc.drawable, dc.font.set, dc.gc, hs,
					vs+(y*(i+1)), message[i], strlen(message[i]));
	else
		for(i = 0; i < count; i++)
			XDrawString(dpy, dc.drawable, dc.gc, hs, vs+(y*(i+1)),
					message[i], strlen(message[i]));
}

void *
emalloc(unsigned int size) {
	void *res = malloc(size);

	if(!res)
		eprint("fatal: could not malloc() %u bytes\n", size);
	return res;
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

void
initfont(const char *fontstr) {
	char *def, **missing;
	int i, n;

	if(!fontstr || fontstr[0] == '\0')
		eprint("error, cannot load font: '%s'\n", fontstr);
	missing = NULL;
	if(dc.font.set)
		XFreeFontSet(dpy, dc.font.set);
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
		if(dc.font.xfont)
			XFreeFont(dpy, dc.font.xfont);
		dc.font.xfont = NULL;
		if(!(dc.font.xfont = XLoadQueryFont(dpy, fontstr))
		&& !(dc.font.xfont = XLoadQueryFont(dpy, "fixed")))
			eprint("error, cannot load font: '%s'\n", fontstr);
		dc.font.ascent = dc.font.xfont->ascent;
		dc.font.descent = dc.font.xfont->descent;
	}
	dc.font.height = dc.font.ascent + dc.font.descent;
}

void
run(void) {
	XEvent ev;

	/* main event loop */
	while(running && !XNextEvent(dpy, &ev))
		switch (ev.type) {
		default:	/* ignore all crap */
			break;
		case ButtonRelease:
			return;
		case Expose:
			if(ev.xexpose.count == 0)
				drawdialog();
			break;
		case VisibilityNotify:
			XMapRaised(dpy, win);
			break;
		}
}

void
setup(void) {
	XSetWindowAttributes wa;
	int i, w;

	/* style */
	dc.colors[ColFG] = getcolor(fg);
	dc.colors[ColBG] = getcolor(bg);
	dc.colors[ColBR] = getcolor(br);
	initfont(font);

	/* calculate window size */
	w = 0;
	for(i = 0; i < count; i++) {
		dw = (hs*2) + textw(message[i]);
		if(dw > w)
			w = dw;
	}
	dw = w;
	dh = (vs*3) + ((dc.font.ascent + dc.font.descent) * count);

	/* dialog window */
	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.border_pixel = dc.colors[ColBR];
	wa.event_mask = ExposureMask | ButtonReleaseMask | VisibilityChangeMask;
	win = XCreateWindow(dpy, root,
			DisplayWidth(dpy, screen) - (dw + 2),
			DisplayHeight(dpy, screen) - dh - (dc.font.height + 4), dw, dh, 1,
			DefaultDepth(dpy, screen), CopyFromParent,
			DefaultVisual(dpy, screen),
			CWOverrideRedirect | CWBackPixmap | CWBorderPixel | CWEventMask,
			&wa);

	/* pixmap */
	dc.drawable = XCreatePixmap(dpy, root, dw, dh, DefaultDepth(dpy, screen));
	dc.gc = XCreateGC(dpy, root, 0, 0);
	XSetLineAttributes(dpy, dc.gc, 1, LineSolid, CapButt, JoinMiter);
	if(!dc.font.set)
		XSetFont(dpy, dc.gc, dc.font.xfont->fid);
	XMapRaised(dpy, win);
}

unsigned int
textnw(const char *text, unsigned int len) {
	XRectangle r;

	if(dc.font.set) {
		XmbTextExtents(dc.font.set, text, len, NULL, &r);
		return r.width;
	}
	return XTextWidth(dc.font.xfont, text, len);
}

unsigned int
textw(const char *text) {
	return textnw(text, strlen(text));
}

int
parse_int(const char *str) {
	int d;
	return sscanf(str, "%d", &d) == 1 ? d : 0;
}

void
split_up(char *text) {
	int i;
	char *cur, *cp;
	cur = text;
	for(i = 0; i < MAX_LINES; i++) {
		if((cp = strchr(cur, '\n')) != NULL) {
			message[i] = cur;
		   	*cp++ = '\0';
	   		cur = cp;
		} else if((cp = strchr(cur, '\\')) != NULL && *(cp+1) == 'n') {
			message[i] = cur;
			*cp++ = '\0';
			*cp++ = '\0';
			cur = cp;
		} else {
			break;
		}	   
	}
	message[i] = cur;
	count = i + 1;
}

void
start_timer() {
	sleep(timeout);
	cleanup();
	XCloseDisplay(dpy);
	exit(EXIT_SUCCESS);
}

int
main(int argc, char *argv[]) {
	pthread_t timer;
	unsigned int i;

	if(argc < 2)
		eprint("usage: wmii-dialog [-fn <font>] [-fg <color>] "
				"[-bg <color>] [-br <color>]\n                   "
				"[-hs <pixels>] [-vs <pixels>] [-to <seconds>] [-v]"
				" message\n");

	/* command line args */
	for(i = 1; i < argc; i++)
		if(!strcmp(argv[i], "-fn")) {
			if(++i < argc) font = argv[i];
		}
		else if(!strcmp(argv[i], "-fg")) {
			if(++i < argc) fg = argv[i];
		}
		else if(!strcmp(argv[i], "-bg")) {
			if(++i < argc) bg = argv[i];
		}
		else if(!strcmp(argv[i], "-br")) {
			if(++i < argc) br = argv[i];
		}
		else if(!strcmp(argv[i], "-hs")) {
			if(++i < argc) hs = parse_int(argv[i]);
		}
		else if(!strcmp(argv[i], "-vs")) {
			if(++i < argc) vs = parse_int(argv[i]);
		}
		else if(!strcmp(argv[i], "-to")) {
			if(++i < argc) timeout = parse_int(argv[i]);
		}
		else if(!strcmp(argv[i], "-v"))
			eprint("wmii-dialog-"VERSION", © 2007 Pontus Andersson\n");

	split_up(argv[i-1]);
	setlocale(LC_CTYPE, "");
	dpy = XOpenDisplay(getenv("DISPLAY"));
	if(!dpy)
		eprint("wmii-dialog: cannot open display\n");
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);

	setup();
	drawdialog();
	XSync(dpy, False);
	if(timeout > 0)
		pthread_create(&timer, NULL, (void *)&start_timer, NULL);
	run();
	cleanup();
	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
