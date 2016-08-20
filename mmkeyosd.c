
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdbool.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/XF86keysym.h>
#include <X11/Xft/Xft.h>
#include <X11/Xproto.h>
#include <X11/XKBlib.h>
#include <X11/extensions/Xinerama.h>

#include "config.h"
#include "util.h"

#define CENTER(A, B) (( (A)/2 ) - ( (B)/2 ))
#define CLEANMASK(mask) (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define INTERSECT(x,y,w,h,r)  (MAX(0, MIN((x)+(w),(r).x_org+(r).width)  - MAX((x),(r).x_org)) \
                             * MAX(0, MIN((y)+(h),(r).y_org+(r).height) - MAX((y),(r).y_org)))

struct
font {
	XftFont *xfont;
	int h;
};

char *fontstrbig;
char *fontstrsmall;
char *fgcolor;
char *bgcolor;
char *errcolor;
int bw;           /* border width    */
int ww;           /* window width    */
int wh;           /* window height   */
int barw;         /* bar width       */
int barh;         /* bar height      */
float opacity;    /* window opacity  */
int wtimeout;     /* window time out in milliseconds */
char *shell;
bool silent;      /* silent mode */

Display *dpy;
Window win;
Window root;
int screen;
Visual *visual;
Colormap cmap;
GC gc;
struct font fontbig;
struct font fontsmall;
XftDraw *draw;
Atom NetWMWindowOpacity;
unsigned int numlockmask = 0;
int sw, sh, xoff, yoff;
struct config *config = NULL;
struct settings *settings = NULL;

XftColor fgcol;
XftColor bgcol;
XftColor errcol;

Bool is_mapped = False;
int pid = -1;
int cmdst = 0;

int (*xerrorxlib)(Display *, XErrorEvent *);
int
handle_xerror(Display *dpy, XErrorEvent *ee) {
	if(ee->error_code == BadWindow)
			return 0;
	else if(ee->request_code == X_GrabKey && ee->error_code == BadAccess)
		err("WARNING: Failed to grab key\n");
	else {
		err("fatal error: request code=%d, error code=%d\n",
						ee->request_code, ee->error_code);
		return xerrorxlib(dpy, ee); /* may call exit */
	}
	return 0;
}
/*
int
handle_xerror(Display * display, XErrorEvent * e) {
	fprintf(stderr, "%d  XError caught\n", e->error_code);
	return 0;
}*/

void
setup_font(struct font *font, char *fontstr) {
	if(!(font->xfont = XftFontOpenName(dpy, screen, fontstr))) {
		err("WARNING: Could not setup font: '%s'. Falling back to 'fixed'\n", fontstr);
		if(!(font->xfont = XftFontOpenXlfd(dpy, screen, "fixed")))
			die("ERROR: Could not setup font\n");
	}

	font->h = font->xfont->ascent + font->xfont->descent;
}

void
draw_text(struct font *font, XftColor *col, char *str, int x, int y) {
	XftDrawStringUtf8(draw, col, font->xfont, x, y, (XftChar8 *)str, strlen(str));
}

int
text_width(struct font *font, char *str) {
	XGlyphInfo ext;

	XftTextExtentsUtf8(dpy, font->xfont, (XftChar8 *)str, strlen(str), &ext);
	return ext.xOff;
}

void
moveresizeclear(int ww, int wh) {
	XMoveResizeWindow(dpy, win, CENTER(sw, ww)+xoff, CENTER(sh, wh)+yoff, ww, wh);

	/* Clear window */
	XSetForeground(dpy, gc, bgcol.pixel);
	XFillRectangle(dpy, win, gc, 0, 0, ww, wh);
}

/* Yes I stole this from dmenu */
void
updategeom() {
	int a, j, di, i = 0, area = 0, x, y;
	unsigned int du;
	Window w, pw, dw, *dws;
	XWindowAttributes wa;
	int n;
	XineramaScreenInfo *info;

	if((info = XineramaQueryScreens(dpy, &n))) {

		XGetInputFocus(dpy, &w, &di);
		if(w != root && w != PointerRoot && w != None) {
			/* find top-level window containing current input focus */
			do {
				if(XQueryTree(dpy, (pw = w), &dw, &w, &dws, &du) && dws)
					XFree(dws);
			} while(w != root && w != pw);
			/* find xinerama screen with which the window intersects most */
			if(XGetWindowAttributes(dpy, pw, &wa))
				for(j = 0; j < n; j++)
					if((a = INTERSECT(wa.x, wa.y, wa.width, wa.height, info[j])) > area) {
						area = a;
						i = j;
					}
		}
		/* no focused window is on screen, so use pointer location instead */
		if(!area && XQueryPointer(dpy, root, &dw, &dw, &x, &y, &di, &di, &du))
			for(i = 0; i < n; i++)
				if(INTERSECT(x, y, 1, 1, info[i]))
					break;

		xoff = info[i].x_org;
		yoff = info[i].y_org;
		sw = info[i].width;
		sh = info[i].height;
		XFree(info);
	}
	else {
		xoff = 0;
		yoff = 0;
		sw = DisplayWidth(dpy, screen);
		sh = DisplayHeight(dpy, screen);
	}
}

void 
text_with_text(struct config *c, char *in, int error) {
	int nww, tws, twb;

	/* Calculate window size */
	tws = text_width(&fontsmall, in);
	twb = text_width(&fontbig, c->text);
	nww = MAX(ww, MAX(tws, twb) + 20);

	moveresizeclear(nww, wh);

	draw_text(&fontbig, &fgcol, c->text, CENTER(nww, text_width(&fontbig, c->text)),
			CENTER(wh/2, fontbig.h)+fontbig.h);
	draw_text(&fontsmall, error ? &errcol : &fgcol, in, CENTER(nww, text_width(&fontsmall, in)), (wh/2)+fontsmall.h);
}

void 
text_with_bar(struct config *c, char *in, int error) {
	int nww, twb;

	/* If there was an error, display it instead of drawing a bar */
	if(error) {
		text_with_text(c, in, error);
		return;
	}

	/* Calculate window size */
	twb = text_width(&fontbig, c->text);
	nww = MAX(ww, twb + 30);

	XRectangle r = { CENTER(nww, barw), (wh/2), barw, barh };

	moveresizeclear(nww, wh);

	draw_text(&fontbig, &fgcol, c->text, CENTER(nww, text_width(&fontbig, c->text)),
			CENTER(wh/2, fontbig.h)+fontbig.h);

	XSetForeground(dpy, gc, fgcol.pixel);
	/* border */
	XDrawRectangles(dpy, win, gc, &r, 1);

	/* and bar */
	r.width = (float)atoi(in)/100.0*(float)barw;
	XFillRectangles(dpy, win, gc, &r, 1);
}

void
grabkey(unsigned int modmask, KeySym key) {
	XGrabKey(dpy, XKeysymToKeycode(dpy, key), modmask, root, True,
		GrabModeAsync, GrabModeAsync);
	XGrabKey(dpy, XKeysymToKeycode(dpy, key), LockMask | modmask, root,
		True, GrabModeAsync, GrabModeAsync);

	if(numlockmask) {
		XGrabKey(dpy, XKeysymToKeycode(dpy, key), modmask | numlockmask, root, True,
			GrabModeAsync, GrabModeAsync);
		XGrabKey(dpy, XKeysymToKeycode(dpy, key), LockMask | modmask | numlockmask, root,
			True, GrabModeAsync, GrabModeAsync);
	}
};

void
getcolor(char *colstr, XftColor *color) {
	if(!XftColorAllocName(dpy, visual, cmap, colstr, color))
		die("ERROR: cannot allocate color '%s'\n", colstr);
}

void
sigalrm(int i) {
	signal(SIGALRM, sigalrm);
	XUnmapWindow(dpy, win);
	XFlush(dpy);
	is_mapped = False;
	//puts("Caught sigalrm");
}

void
sigchld(int i) {
	int p;

	p = wait(NULL);
	if(p == pid)
		pid = -1;

	signal(SIGCHLD, sigchld);
}

void
sigusr1(int i) {
	silent = true;
}

void
sigusr2(int i) {
	silent = false;
}

void
setup() {
	int i, j;
	struct config *c;

	XSetWindowAttributes wattr;
	XModifierKeymap *modmap;

	xerrorxlib = XSetErrorHandler(handle_xerror);

	dpy = XOpenDisplay(NULL);
	root = DefaultRootWindow(dpy);
	screen = DefaultScreen(dpy);
	visual = DefaultVisual(dpy, screen);
	cmap = DefaultColormap(dpy, screen);
	gc = DefaultGC(dpy, screen);

	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);

	silent = false;

	wattr.override_redirect = True;
	wattr.background_pixel = bgcol.pixel; 
	wattr.border_pixel = fgcol.pixel;
	win = XCreateWindow(dpy, root, 0, 0, ww, wh, bw,
			CopyFromParent, InputOutput, CopyFromParent,
			CWOverrideRedirect | CWBackPixel | CWBorderPixel, &wattr);

	/* Grab keys */
	/* modifier stuff taken from evilwm */
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++) {
		for (j = 0; j < modmap->max_keypermod; j++) {
			if (modmap->modifiermap[i * modmap->max_keypermod + j] ==
			XKeysymToKeycode(dpy, XK_Num_Lock)) {
				numlockmask = (1 << i);
			}
		}
	}
	for(c=config; c; c=c->next)
		grabkey(c->mod, c->key);

	XSelectInput(dpy, root, KeyPressMask | SubstructureNotifyMask);
	XSelectInput(dpy, win, ButtonPressMask);

	setup_font(&fontbig, fontstrbig);
	setup_font(&fontsmall, fontstrsmall);

	getcolor(fgcolor, &fgcol);
	getcolor(bgcolor, &bgcol);
	getcolor(errcolor, &errcol);

	NetWMWindowOpacity = XInternAtom(dpy, "_NET_WM_WINDOW_OPACITY", False);
	if(opacity > 1.0 || opacity < 0.0)
		opacity = 0.0;
	unsigned long real_opacity[] = { opacity * 0xffffffff };
	XChangeProperty(dpy, win, NetWMWindowOpacity, XA_CARDINAL, 32,
			PropModeReplace, (unsigned char *)real_opacity, 1);

	draw = XftDrawCreate(dpy, win, visual, cmap);

	signal(SIGALRM, sigalrm);
	signal(SIGCHLD, sigchld);
	signal(SIGUSR1, sigusr1);
	signal(SIGUSR2, sigusr2);
}

void
start_timer() {
	struct itimerval t;

	memset(&t, 0, sizeof t);
	t.it_value.tv_sec = wtimeout/1000;
	t.it_value.tv_usec = (wtimeout%1000)*1000;
	if(setitimer(ITIMER_REAL, &t, 0) == -1)
		die("Could not set timer: %s\n", strerror(errno));
}

/* this function reads both stdout and stderr
 * from cmd. this way we can capture error messages.
 * it also has a time out to prevent lock ups */
void
readcmd(char *cmd, char *stdoutbuf, int stdoutbuflen, char *stderrbuf, int stderrbuflen) {
	FILE *stdoutf, *stderrf;
	int stdoutp[2], stderrp[2];
	int r;

	if(pipe(stdoutp) == -1)
		die("pipe: %s\n", strerror(errno));

	if(pipe(stderrp) == -1)
		die("pipe: %s\n", strerror(errno));

	pid = fork();
	if(pid == -1)
		die("fork: %s\n", strerror(errno));
	else if(pid == 0) {
		close(STDOUT_FILENO);
		dup(stdoutp[1]);
		close(STDERR_FILENO);
		dup(stderrp[1]);

		execl(shell, shell, "-c", cmd, NULL);
		die("%s: %s", shell, strerror(errno));
	}

	struct pollfd ufds[2] = {
		{ stdoutp[0], POLLIN, 0 },
		{ stderrp[0], POLLIN, 0 },
	};

	stdoutf = fdopen(stdoutp[0], "r");
	stderrf = fdopen(stderrp[0], "r");
	stdoutbuf[0] = '\0';
	stderrbuf[0] = '\0';

	for(;;) {
		if((r = poll(ufds, 2, 2000)) == -1) {
			if(errno != EINTR)
				die("poll: %s\n", strerror(errno));
			if(pid == -1)
				break;
		}
		
		if(r == 0) /* timed out */
			break;

		/* TODO: check for error in .revents */
		if(ufds[0].revents & POLLIN) {
			if(!strlen(stdoutbuf)) {
				fgets(stdoutbuf, stdoutbuflen, stdoutf);
				/* only need one line; break
				 * NOTE: if command is broken the process might
				 * continue forever. */
				break;
			}
		}
		if(ufds[1].revents & POLLIN) {
			fgets(stderrbuf, stderrbuflen, stderrf);
		}

		/* if process died we are finished (see sigchld())*/
		if(pid == -1)
			break;
	}

	/* Clean up */
	fclose(stdoutf);
	fclose(stderrf);
	close(stdoutp[0]);
	close(stdoutp[1]);
	close(stderrp[0]);
	close(stderrp[1]);

}

void
run() {
	XEvent ev;
	KeySym keysym;
	char buf[256];
	char errbuf[256] = {"ERROR: "};
	char *errmsg = NULL;
	/*struct timeval t1, t2;*/
	struct config *c;

	XSync(dpy, False);
	while(!XNextEvent(dpy, &ev)) {

		if(ev.type == ButtonPress) {
			if(ev.xbutton.window == win && is_mapped) {
				XUnmapWindow(dpy, win);
				XFlush(dpy);
				is_mapped = False;
			}
			continue;
		}

		if(ev.type != KeyPress)
			continue;

		keysym = XkbKeycodeToKeysym(dpy, (KeyCode)ev.xkey.keycode, 0, 0);
		for(c=config; c; c=c->next) {
			if(c->key == keysym &&
					CLEANMASK(c->mod) == CLEANMASK(ev.xkey.state)) {
				break;
			}
		}

		if(!c)
			continue;

		if(!is_mapped && !silent) {
			updategeom();
			moveresizeclear(ww, wh);
			XMapRaised(dpy, win);
			/* Wait for window to be mapped */
			while(1) {
				XMaskEvent(dpy, SubstructureNotifyMask, &ev);
				if ((ev.type == CreateNotify || ev.type == MapNotify)
					&& ev.xcreatewindow.window == win)
				{
					break;
				}
			}
			is_mapped = True;
		}
		/* gettimeofday(&t1, NULL); */
		readcmd(c->cmd, buf, sizeof buf,
				errbuf+7/*skip the 'ERROR: ' part of the buffer */,
				sizeof(errbuf)-7);
		/*
		gettimeofday(&t2, NULL);
		printf("cmd finished after %li ms\n", ((t2.tv_sec-t1.tv_sec)*1000) + ((t2.tv_usec-t1.tv_usec)/1000));
		*/

		if(buf[strlen(buf)-1] == '\n')
			buf[strlen(buf)-1] = '\0';
		if(errbuf[strlen(errbuf)-1] == '\n')
			errbuf[strlen(errbuf)-1] = '\0';

		if(strlen(buf))
			errmsg = NULL;
		else if(strlen(errbuf+7))
			errmsg = errbuf;
		else
			errmsg = "ERROR: Command failed";

		start_timer();
		c->disp(c, errmsg ? errmsg : buf, errmsg ? True : False);
		XSync(dpy, False);
	}
}

int
main(int argc, char *argv[]) {
	if(argc > 1 && strcmp(argv[1], "-v") == 0) {
		puts("mmkeyosd "VERSION);
		exit(0);
	}
	if(chdir(getenv("HOME")) == -1)
		die("chdir: %s\n", strerror(errno));

	config = config_read(".mmkeyosd/keys");
	settings = settings_read(".mmkeyosd/settings");

	fontstrbig   = settings_find_str(   settings, "fontbig",       "Dejavu Sans-15");
	fontstrsmall = settings_find_str(   settings, "fontsmall",     "Dejavu Sans-10");
	fgcolor      = settings_find_str(   settings, "fgcolor",       "white");
	bgcolor      = settings_find_str(   settings, "bgcolor",       "black");
	errcolor     = settings_find_str(   settings, "errcolor",      "red");
	bw           = settings_find_int(   settings, "borderwidth",   0);
	ww           = settings_find_int(   settings, "windowwidth",   300);
	wh           = settings_find_int(   settings, "windowheight",  150);
	barw         = settings_find_int(   settings, "barwidth",      150);
	barh         = settings_find_int(   settings, "barheight",     15);
	opacity      = settings_find_double(settings, "opacity",       0.8);
	wtimeout     = settings_find_int(   settings, "windowtimeout", 2000);
	shell        = settings_find_str(   settings, "shell",         "/bin/sh");

	setup();
	run();
	return 0;
}

