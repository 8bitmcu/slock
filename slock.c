/* See LICENSE file for license details. */
#define _XOPEN_SOURCE 500
#if HAVE_SHADOW_H
#include <shadow.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xinerama.h>
#include <X11/extensions/dpms.h>
#include <X11/keysym.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>
#include <X11/Xmd.h>
#include <Imlib2.h>
#include <security/pam_appl.h>
#include <security/pam_misc.h>

#include "arg.h"
#include "util.h"
#include "tomlc99/toml.h"

char *argv0;

static int pam_conv(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr);
struct pam_conv pamc = {pam_conv, NULL};
char passwd[256];

static time_t locktime;

/* global count to prevent repeated error messages */
int count_error = 0;

int failtrack = 0;

enum {
  FOREGROUND,
	INIT,
	INPUT,
	INPUT_ALT,
	FAILED,
	CAPS,
	PAM,
	NUMCOLS
};

#include "config.h"

struct lock {
	int screen;
	Window root, win;
	Pixmap pmap;
	Pixmap bgmap;
	unsigned long colors[NUMCOLS];
  unsigned int x, y;
  unsigned int xoff, yoff, mw, mh;
  Drawable drawable;
  GC gc;
};

struct xrandr {
	int active;
	int evbase;
	int errbase;
};

Imlib_Image image;

static void 
die(const char *errstr, ...)
{
  va_list ap;

  va_start(ap, errstr);
  vfprintf(stderr, errstr, ap);
  va_end(ap);
  exit(1);
}

static int
pam_conv(int num_msg, const struct pam_message **msg,
		struct pam_response **resp, void *appdata_ptr)
{
	int retval = PAM_CONV_ERR;
	for(int i=0; i<num_msg; i++) {
		if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF &&
				strncmp(msg[i]->msg, "Password: ", 10) == 0) {
			struct pam_response *resp_msg = malloc(sizeof(struct pam_response));
			if (!resp_msg)
				die("malloc failed\n");
			char *password = malloc(strlen(passwd) + 1);
			if (!password)
				die("malloc failed\n");
			memset(password, 0, strlen(passwd) + 1);
			strcpy(password, passwd);
			resp_msg->resp_retcode = 0;
			resp_msg->resp = password;
			resp[i] = resp_msg;
			retval = PAM_SUCCESS;
		}
	}
	return retval;
}


#ifdef __linux__
#include <fcntl.h>
#include <linux/oom.h>

static void
dontkillme(void)
{
	FILE *f;
	const char oomfile[] = "/proc/self/oom_score_adj";

	if (!(f = fopen(oomfile, "w"))) {
		if (errno == ENOENT)
			return;
		die("slock: fopen %s: %s\n", oomfile, strerror(errno));
	}
	fprintf(f, "%d", OOM_SCORE_ADJ_MIN);
	if (fclose(f)) {
		if (errno == EACCES)
			die("slock: unable to disable OOM killer. "
			    "Make sure to suid or sgid slock.\n");
		else
			die("slock: fclose %s: %s\n", oomfile, strerror(errno));
	}
}
#endif


static void
writemessage(Display *dpy, Window win, int screen, const char* font_name, const char* message, int offset, const char * color)
{
	int len, line_len, width, height, s_width, s_height, i, j, k, tab_replace, tab_size;
	XftFont *fontinfo;
	XftColor xftcolor;
	XftDraw *xftdraw;
	XGlyphInfo ext_msg, ext_space;
	XineramaScreenInfo *xsi;
	xftdraw = XftDrawCreate(dpy, win, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen));
	fontinfo = XftFontOpenName(dpy, screen, font_name);
	XftColorAllocName(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), color, &xftcolor);

	if (fontinfo == NULL) {
		if (count_error == 0) {
			fprintf(stderr, "slock: Unable to load font \"%s\"\n", font_name);
			count_error++;
		}
		return;
	}

	XftTextExtentsUtf8(dpy, fontinfo, (XftChar8 *) " ", 1, &ext_space);
	tab_size = 8 * ext_space.width;

	/*  To prevent "Uninitialized" warnings. */
	xsi = NULL;

	/*
	 * Start formatting and drawing text
	 */

	len = strlen(message);

	/* Max max line length (cut at '\n') */
	line_len = 0;
	k = 0;
	for (i = j = 0; i < len; i++) {
		if (message[i] == '\n') {
			if (i - j > line_len)
				line_len = i - j;
			k++;
			i++;
			j = i;
		}
	}
	/* If there is only one line */
	if (line_len == 0)
		line_len = len;

	if (XineramaIsActive(dpy)) {
		xsi = XineramaQueryScreens(dpy, &i);
		s_width = xsi[0].width;
		s_height = xsi[0].height;
	} else {
		s_width = DisplayWidth(dpy, screen);
		s_height = DisplayHeight(dpy, screen);
	}

	XftTextExtentsUtf8(dpy, fontinfo, (XftChar8 *)message, line_len, &ext_msg);
	height = s_height*3/7 - (k*20)/3;
	width  = (s_width - ext_msg.width)/2;

	/* Look for '\n' and print the text between them. */
	for (i = j = k = 0; i <= len; i++) {
		/* i == len is the special case for the last line */
		if (i == len || message[i] == '\n') {
			tab_replace = 0;
			while (message[j] == '\t' && j < i) {
				tab_replace++;
				j++;
			}

			XftDrawStringUtf8(xftdraw, &xftcolor, fontinfo, width + tab_size*tab_replace, height + 20*k + offset, (XftChar8 *)(message + j), i - j);
			while (i < len && message[i] == '\n') {
				i++;
				j = i;
				k++;
			}
		}
	}

	/* xsi should not be NULL anyway if Xinerama is active, but to be safe */
	if (XineramaIsActive(dpy) && xsi != NULL)
			XFree(xsi);

	XftFontClose(dpy, fontinfo);
	XftColorFree(dpy, DefaultVisual(dpy, screen), DefaultColormap(dpy, screen), &xftcolor);
	XftDrawDestroy(xftdraw);
}

static const char *
gethash(void)
{
	const char *hash;
	struct passwd *pw;

	/* Check if the current user has a password entry */
	errno = 0;
	if (!(pw = getpwuid(getuid()))) {
		if (errno)
			die("slock: getpwuid: %s\n", strerror(errno));
		else
			die("slock: cannot retrieve password entry\n");
	}
	hash = pw->pw_passwd;

#if HAVE_SHADOW_H
	if (!strcmp(hash, "x")) {
		struct spwd *sp;
		if (!(sp = getspnam(pw->pw_name)))
			die("slock: getspnam: cannot retrieve shadow entry. "
			    "Make sure to suid or sgid slock.\n");
		hash = sp->sp_pwdp;
	}
#else
	if (!strcmp(hash, "*")) {
#ifdef __OpenBSD__
		if (!(pw = getpwuid_shadow(getuid())))
			die("slock: getpwnam_shadow: cannot retrieve shadow entry. "
			    "Make sure to suid or sgid slock.\n");
		hash = pw->pw_passwd;
#else
		die("slock: getpwuid: cannot retrieve shadow entry. "
		    "Make sure to suid or sgid slock.\n");
#endif /* __OpenBSD__ */
	}
#endif /* HAVE_SHADOW_H */

	/* pam, store user name */
  if (enablepam) 
    hash = pw->pw_name;

	return hash;
}

static void
readpw(Display *dpy, struct xrandr *rr, struct lock **locks, int nscreens,
       const char *hash)
{
	XRRScreenChangeNotifyEvent *rre;
	char buf[32], passwd[256], *inputhash;
	int caps, num, screen, running, failure, oldc, retval;
	unsigned int len, color, indicators;
	KeySym ksym;
	XEvent ev;
	pam_handle_t *pamh;

	len = 0;
	caps = 0;
	running = 1;
	failure = 0;
	oldc = INIT;

	if (!XkbGetIndicatorState(dpy, XkbUseCoreKbd, &indicators))
		caps = indicators & 1;

	while (running && !XNextEvent(dpy, &ev)) {
		running = !((time(NULL) - locktime < timetocancel) && (ev.type == MotionNotify || ev.type == KeyPress));
    if (!running)
      return;
		if (ev.type == KeyPress) {
			explicit_bzero(&buf, sizeof(buf));
			num = XLookupString(&ev.xkey, buf, sizeof(buf), &ksym, 0);
			if (IsKeypadKey(ksym)) {
				if (ksym == XK_KP_Enter)
					ksym = XK_Return;
				else if (ksym >= XK_KP_0 && ksym <= XK_KP_9)
					ksym = (ksym - XK_KP_0) + XK_0;
			}
			if (IsFunctionKey(ksym) ||
			    IsKeypadKey(ksym) ||
			    IsMiscFunctionKey(ksym) ||
			    IsPFKey(ksym) ||
			    IsPrivateKeypadKey(ksym))
				continue;
			switch (ksym) {
      case XF86XK_AudioPlay:
      case XF86XK_AudioStop:
      case XF86XK_AudioPrev:
      case XF86XK_AudioNext:
      case XF86XK_AudioRaiseVolume:
      case XF86XK_AudioLowerVolume:
      case XF86XK_AudioMute:
      case XF86XK_AudioMicMute:
      case XF86XK_MonBrightnessDown:
      case XF86XK_MonBrightnessUp:
        XSendEvent(dpy, DefaultRootWindow(dpy), True, KeyPressMask, &ev);
        break;
			case XK_Return:
				passwd[len] = '\0';
				errno = 0;

        if (enablepam) {
          retval = pam_start(pam_service, hash, &pamc, &pamh);
          for (screen = 0; screen < nscreens; screen++) {
            XClearWindow(dpy, locks[screen]->win);
            writemessage(dpy, locks[screen]->win, screen, icon_font, display_icon, 0, colorname[PAM]);
            writemessage(dpy, locks[screen]->win, screen, text_font, display_text, 100, colorname[FOREGROUND]);
          }
          XSync(dpy, False);

          if (retval == PAM_SUCCESS)
            retval = pam_authenticate(pamh, 0);
          if (retval == PAM_SUCCESS)
            retval = pam_acct_mgmt(pamh, 0);

          running = 1;
          if (retval == PAM_SUCCESS)
            running = 0;
          else
            fprintf(stderr, "slock: %s\n", pam_strerror(pamh, retval));
          pam_end(pamh, retval);
        }
        else {
          if (!(inputhash = crypt(passwd, hash)))
            fprintf(stderr, "slock: crypt: %s\n", strerror(errno));
          else
            running = !!strcmp(inputhash, hash);
        }

				if (running) {
					XBell(dpy, 100);
					failure = 1;
					failtrack++;

					if (failtrack >= failcount && failcount != 0){
						system(failcommand);
					}
				}
				explicit_bzero(&passwd, sizeof(passwd));
				len = 0;
				break;
			case XK_Escape:
				explicit_bzero(&passwd, sizeof(passwd));
				len = 0;
				break;
			case XK_BackSpace:
				if (len)
					passwd[--len] = '\0';
				break;
			case XK_Caps_Lock:
				caps = !caps;
				break;
			default:
				if (controlkeyclear && iscntrl((int)buf[0]))
					continue;
				if (num && (len + num < sizeof(passwd))) {
					memcpy(passwd + len, buf, num);
					len += num;
				}
				break;
			}

			color = len ? (caps ? CAPS : (len%2 ? INPUT : INPUT_ALT)) : ((failure || failonclear) ? FAILED : INIT);
			if (running && oldc != color) {
				for (screen = 0; screen < nscreens; screen++) {
          if(locks[screen]->bgmap)
              XSetWindowBackgroundPixmap(dpy, locks[screen]->win, locks[screen]->bgmap);
          else
              XSetWindowBackground(dpy, locks[screen]->win, locks[screen]->colors[0]);
					XClearWindow(dpy, locks[screen]->win);
          writemessage(dpy, locks[screen]->win, screen, icon_font, display_icon, 0, colorname[color]);
          writemessage(dpy, locks[screen]->win, screen, text_font, display_text, 100, colorname[FOREGROUND]);
				}
				oldc = color;
			}
		} else if (rr->active && ev.type == rr->evbase + RRScreenChangeNotify) {
			rre = (XRRScreenChangeNotifyEvent*)&ev;
			for (screen = 0; screen < nscreens; screen++) {
				if (locks[screen]->win == rre->window) {
					if (rre->rotation == RR_Rotate_90 ||
					    rre->rotation == RR_Rotate_270)
						XResizeWindow(dpy, locks[screen]->win,
						              rre->height, rre->width);
					else
						XResizeWindow(dpy, locks[screen]->win,
						              rre->width, rre->height);
					XClearWindow(dpy, locks[screen]->win);
					break;
				}
			}
		} else {
			for (screen = 0; screen < nscreens; screen++)
				XRaiseWindow(dpy, locks[screen]->win);
		}
	}
}

static struct lock *
lockscreen(Display *dpy, struct xrandr *rr, int screen)
{
	char curs[] = {0, 0, 0, 0, 0, 0, 0, 0};
	int i, ptgrab, kbgrab;
	struct lock *lock;
	XColor color, dummy;
	XSetWindowAttributes wa;
	Cursor invisible;
#ifdef XINERAMA
	XineramaScreenInfo *info;
	int n;
#endif

	if (dpy == NULL || screen < 0 || !(lock = malloc(sizeof(struct lock))))
		return NULL;

	lock->screen = screen;
	lock->root = RootWindow(dpy, lock->screen);

  if(image) {
      lock->bgmap = XCreatePixmap(dpy, lock->root, DisplayWidth(dpy, lock->screen), DisplayHeight(dpy, lock->screen), DefaultDepth(dpy, lock->screen));
      imlib_context_set_image(image);
      imlib_context_set_display(dpy);
      imlib_context_set_visual(DefaultVisual(dpy, lock->screen));
      imlib_context_set_colormap(DefaultColormap(dpy, lock->screen));
      imlib_context_set_drawable(lock->bgmap);
      imlib_render_image_on_drawable(0, 0);
      imlib_free_image();
  }

	for (i = 0; i < NUMCOLS; i++) {
		XAllocNamedColor(dpy, DefaultColormap(dpy, lock->screen),
		                 colorname[i], &color, &dummy);
		lock->colors[i] = color.pixel;
	}

	lock->x = DisplayWidth(dpy, lock->screen);
	lock->y = DisplayHeight(dpy, lock->screen);
#ifdef XINERAMA
	if ((info = XineramaQueryScreens(dpy, &n))) {
		lock->xoff = info[0].x_org;
		lock->yoff = info[0].y_org;
		lock->mw = info[0].width;
		lock->mh = info[0].height;
	} else
#endif
	{
		lock->xoff = lock->yoff = 0;
		lock->mw = lock->x;
		lock->mh = lock->y;
	}
	lock->drawable = XCreatePixmap(dpy, lock->root,
            lock->x, lock->y, DefaultDepth(dpy, screen));
	lock->gc = XCreateGC(dpy, lock->root, 0, NULL);
	XSetLineAttributes(dpy, lock->gc, 1, LineSolid, CapButt, JoinMiter);

	/* init */
	wa.override_redirect = 1;
	wa.background_pixel = lock->colors[FOREGROUND];
	lock->win = XCreateWindow(dpy, lock->root, 0, 0,
	                          lock->x, lock->y,
	                          0, DefaultDepth(dpy, lock->screen),
	                          CopyFromParent,
	                          DefaultVisual(dpy, lock->screen),
	                          CWOverrideRedirect | CWBackPixel, &wa);
  if(lock->bgmap)
     XSetWindowBackgroundPixmap(dpy, lock->win, lock->bgmap);
	lock->pmap = XCreateBitmapFromData(dpy, lock->win, curs, 8, 8);
	invisible = XCreatePixmapCursor(dpy, lock->pmap, lock->pmap,
	                                &color, &color, 0, 0);
	XDefineCursor(dpy, lock->win, invisible);

	/* Try to grab mouse pointer *and* keyboard for 600ms, else fail the lock */
	for (i = 0, ptgrab = kbgrab = -1; i < 6; i++) {
		if (ptgrab != GrabSuccess) {
			ptgrab = XGrabPointer(dpy, lock->root, False,
			                      ButtonPressMask | ButtonReleaseMask |
			                      PointerMotionMask, GrabModeAsync,
			                      GrabModeAsync, None, invisible, CurrentTime);
		}
		if (kbgrab != GrabSuccess) {
			kbgrab = XGrabKeyboard(dpy, lock->root, True,
			                       GrabModeAsync, GrabModeAsync, CurrentTime);
		}

		/* input is grabbed: we can lock the screen */
		if (ptgrab == GrabSuccess && kbgrab == GrabSuccess) {
			XMapRaised(dpy, lock->win);
			if (rr->active)
				XRRSelectInput(dpy, lock->win, RRScreenChangeNotifyMask);

			XSelectInput(dpy, lock->root, SubstructureNotifyMask);
			locktime = time(NULL);
			return lock;
		}

		/* retry on AlreadyGrabbed but fail on other errors */
		if ((ptgrab != AlreadyGrabbed && ptgrab != GrabSuccess) ||
		    (kbgrab != AlreadyGrabbed && kbgrab != GrabSuccess))
			break;

		usleep(100000);
	}

	/* we couldn't grab all input: fail out */
	if (ptgrab != GrabSuccess)
		fprintf(stderr, "slock: unable to grab mouse pointer for screen %d\n",
		        screen);
	if (kbgrab != GrabSuccess)
		fprintf(stderr, "slock: unable to grab keyboard for screen %d\n",
		        screen);
	return NULL;
}

static void
usage(void)
{
	die("usage: slock [-v] [cmd [arg ...]]\n");
}

static void
cfg_read_str(toml_table_t* conf, char* key, const char** dest)
{
  toml_datum_t d = toml_string_in(conf, key);
  if (d.ok)
    *dest = strdup(d.u.s);
  free(d.u.s);
}

static void
cfg_read_int(toml_table_t* conf, char* key, int* dest)
{
  toml_datum_t d = toml_int_in(conf, key);
  if (d.ok)
    *dest = d.u.i;
}

int
main(int argc, char **argv) {
	struct xrandr rr;
	struct lock **locks;
	struct passwd *pwd;
	struct group *grp;
	uid_t duid;
	gid_t dgid;
	const char *hash;
	Display *dpy;
	CARD16 standby, suspend, off;
	BOOL dpms_state;
	int s, nlocks, nscreens;

  const char *config_file = strcat(getenv("XDG_CONFIG_HOME"), slock_cfg);
  FILE* fp = fopen(config_file, "r");
  if(fp) {
    char errbuf[200];
    toml_table_t* conf = toml_parse_file(fp, errbuf, sizeof(errbuf));
    fclose(fp);

    if (conf) {
      cfg_read_int(conf, "failonclear", &failonclear);
      cfg_read_int(conf, "enableblur", &enableblur);
      cfg_read_int(conf, "blurradius", &blurradius);
      cfg_read_int(conf, "enablepixel", &enablepixel);
      cfg_read_int(conf, "pixelsize", &pixelsize);
      cfg_read_int(conf, "timetocancel", &timetocancel);
      cfg_read_int(conf, "failcount", &failcount);
      cfg_read_int(conf, "controlkeyclear", &controlkeyclear);
      cfg_read_int(conf, "enabledpms", &enabledpms);
      cfg_read_int(conf, "monitortime", &monitortime);
      cfg_read_int(conf, "enablepam", &enablepam);
      cfg_read_str(conf, "user", &user);
      cfg_read_str(conf, "group", &group);
      cfg_read_str(conf, "color_foreground", &colorname[FOREGROUND]);
      cfg_read_str(conf, "color_init", &colorname[INIT]);
      cfg_read_str(conf, "color_input", &colorname[INPUT]);
      cfg_read_str(conf, "color_inputalt", &colorname[INPUT_ALT]);
      cfg_read_str(conf, "color_failed", &colorname[FAILED]);
      cfg_read_str(conf, "color_caps", &colorname[CAPS]);
      cfg_read_str(conf, "icon_font", &icon_font);
      cfg_read_str(conf, "display_icon", &display_icon);
      cfg_read_str(conf, "text_font", &text_font);
      cfg_read_str(conf, "display_text", &display_text);
      cfg_read_str(conf, "failcommand", &failcommand);
      cfg_read_str(conf, "pam_service", &pam_service);
      cfg_read_str(conf, "bgimage", &bgimage);
      toml_free(conf);
    }
  }

	ARGBEGIN {
	case 'v':
		puts("slock-"VERSION);
		return 0;
	case 'm':
		display_text = EARGF(usage());
		break;
	default:
		usage();
	} ARGEND

	/* validate drop-user and -group */
	errno = 0;
	if (!(pwd = getpwnam(user)))
		die("slock: getpwnam %s: %s\n", user,
		    errno ? strerror(errno) : "user entry not found");
	duid = pwd->pw_uid;
	errno = 0;
	if (!(grp = getgrnam(group)))
		die("slock: getgrnam %s: %s\n", group,
		    errno ? strerror(errno) : "group entry not found");
	dgid = grp->gr_gid;

#ifdef __linux__
	dontkillme();
#endif

	/* the contents of hash are used to transport the current user name */
	hash = gethash();
	errno = 0;
  if (enablepam && !crypt("", hash))
    die("slock: crypt: %s\n", strerror(errno));

	if (!(dpy = XOpenDisplay(NULL)))
		die("slock: cannot open display\n");

	/* drop privileges */
	if (setgroups(0, NULL) < 0)
		die("slock: setgroups: %s\n", strerror(errno));
	if (setgid(dgid) < 0)
		die("slock: setgid: %s\n", strerror(errno));
	if (setuid(duid) < 0)
		die("slock: setuid: %s\n", strerror(errno));

	/*Create screenshot Image*/
  Screen *scr = ScreenOfDisplay(dpy, DefaultScreen(dpy));

  if (strcmp(bgimage, "")) {
    image = imlib_load_image(bgimage);
    imlib_context_set_image(image);
    int w = imlib_image_get_width();
    int h = imlib_image_get_height();
    image = imlib_create_cropped_scaled_image(0, 0, w, h, scr->width, scr->height);
    imlib_context_set_image(image);
  }
  else {
    image = imlib_create_image(scr->width,scr->height);
    imlib_context_set_image(image);
    imlib_context_set_display(dpy);
    imlib_context_set_visual(DefaultVisual(dpy,0));
    imlib_context_set_drawable(RootWindow(dpy,XScreenNumberOfScreen(scr)));	
    imlib_copy_drawable_to_image(0,0,0,scr->width,scr->height,0,0,1);
  }


  if (enableblur > 0) {
    /*Blur function*/
    imlib_image_blur(blurradius);
  }

  if (enablepixel > 0) {
    /*Pixelation*/
    int width = scr->width;
    int height = scr->height;
    
    for(int y = 0; y < height; y += pixelsize)
    {
      for(int x = 0; x < width; x += pixelsize)
      {
        int red = 0;
        int green = 0;
        int blue = 0;

        Imlib_Color pixel; 
        Imlib_Color* pp;
        pp = &pixel;
        for(int j = 0; j < pixelsize && j < height; j++)
        {
          for(int i = 0; i < pixelsize && i < width; i++)
          {
            imlib_image_query_pixel(x+i,y+j,pp);
            red += pixel.red;
            green += pixel.green;
            blue += pixel.blue;
          }
        }
        red /= (pixelsize*pixelsize);
        green /= (pixelsize*pixelsize);
        blue /= (pixelsize*pixelsize);
        imlib_context_set_color(red,green,blue,pixel.alpha);
        imlib_image_fill_rectangle(x,y,pixelsize,pixelsize);
        red = 0;
        green = 0;
        blue = 0;
      }
    }
  }
	
	/* check for Xrandr support */
	rr.active = XRRQueryExtension(dpy, &rr.evbase, &rr.errbase);

	/* get number of screens in display "dpy" and blank them */
	nscreens = ScreenCount(dpy);
	if (!(locks = calloc(nscreens, sizeof(struct lock *))))
		die("slock: out of memory\n");
	for (nlocks = 0, s = 0; s < nscreens; s++) {
		if ((locks[s] = lockscreen(dpy, &rr, s)) != NULL) {
      writemessage(dpy, locks[s]->win, s, icon_font, display_icon, 0, colorname[FOREGROUND]);
      writemessage(dpy, locks[s]->win, s, text_font, display_text, 100, colorname[FOREGROUND]);
			nlocks++;
    }
		else
			break;
	}
	XSync(dpy, 0);

	/* did we manage to lock everything? */
	if (nlocks != nscreens)
		return 1;

  if (enabledpms) {
    /* DPMS magic to disable the monitor */
    if (!DPMSCapable(dpy))
      die("slock: DPMSCapable failed\n");
    if (!DPMSInfo(dpy, &standby, &dpms_state))
      die("slock: DPMSInfo failed\n");
    if (!DPMSEnable(dpy) && !dpms_state)
      die("slock: DPMSEnable failed\n");
    if (!DPMSGetTimeouts(dpy, &standby, &suspend, &off))
      die("slock: DPMSGetTimeouts failed\n");
    if (!standby || !suspend || !off)
      die("slock: at least one DPMS variable is zero\n");
    if (!DPMSSetTimeouts(dpy, monitortime, monitortime, monitortime))
      die("slock: DPMSSetTimeouts failed\n");

    XSync(dpy, 0);
  }



	/* run post-lock command */
	if (argc > 0) {
		switch (fork()) {
		case -1:
			die("slock: fork failed: %s\n", strerror(errno));
		case 0:
			if (close(ConnectionNumber(dpy)) < 0)
				die("slock: close: %s\n", strerror(errno));
			execvp(argv[0], argv);
			fprintf(stderr, "slock: execvp %s: %s\n", argv[0], strerror(errno));
			_exit(1);
		}
	}

	/* everything is now blank. Wait for the correct password */
	readpw(dpy, &rr, locks, nscreens, hash);

	for (nlocks = 0, s = 0; s < nscreens; s++) {
		XFreePixmap(dpy, locks[s]->drawable);
		XFreeGC(dpy, locks[s]->gc);
	}

  if (enabledpms) {
    /* reset DPMS values to inital ones */
    DPMSSetTimeouts(dpy, standby, suspend, off);
    if (!dpms_state)
      DPMSDisable(dpy);
    XSync(dpy, 0);
  }

	XCloseDisplay(dpy);
	return 0;
}
