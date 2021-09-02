/* vi:set ts=8 sts=4 sw=4:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * os_plan9.c
 *
 * Plan 9 system-dependent routines.
 */

#include <signal.h>
#include <setjmp.h>
#include <fcntl.h>
#include <lib9.h>
#include <draw.h>
#include <keyboard.h>
#include <event.h>
#include <plumb.h>
#include "vim.h"

/* Vim defines Display.  We need it for libdraw. */
#undef Display

/* We need to access the native Plan 9 alarm() since
 * it takes milliseconds rather than seconds. */
extern int _ALARM(unsigned long);

enum {
    /* Text modes are in sync with term.c */
    TMODE_NORMAL  = 0,
    TMODE_REVERSE = 1,
    TMODE_BOLD    = 2,

    NCOLORS       = 16
};

static int scr_inited;
static Point fontsize;
static Point shellsize;
static Rectangle scrollregion;
static int currow; /* TODO use a Point */
static int curcol;
static Image *cursorsave;
static Image *colortab[NCOLORS];
static Image *fgcolor;
static Image *bgcolor;
static Font *normalfont;
static Font *boldfont;
static Font *curfont;
static int curmode; /* TODO remove this, use cterm_normal_fg_color instead for comparing */
static int plumbkey;
static int done;

/* Timeouts are handled using alarm() and a signal handler.
 * When an alarm happens during a syscall, the syscall is
 * interrupted.  We use setjmp() to handle control flow from
 * interrupted library functions. */
static int interruptable;
static jmp_buf timeoutjmpbuf;

static void err9(char *msg) {
    char errs[256];
    /* TODO */
    errstr(errs, sizeof errs);
    fprintf(stderr, "%s: %s\n", msg, errs);
    exit(1);
}

/* Error handler for libdraw */
static void derr(Display*, char *msg) {
    if (interruptable && strcmp(msg, "eof on event pipe") == 0) {
        longjmp(timeoutjmpbuf, 1);
    }
    err9(msg);
}

static void drain_plumb_events(void) {
    Event e;
    Plumbmsg *m;
    char *addr;
    int l = ECMD_ONE;

    while(ecanread(plumbkey)){
	eread(plumbkey, &e);
	m = e.v;
	addr = plumblookup(m->attr, "addr");
	if(addr)
		l = atoi(addr);
	do_ecmd(0, (char_u*)m->data, NULL, NULL, (linenr_T)l, ECMD_HIDE);
	shell_resized();
    }
}

static void start_plumber_thread(void)
{
	switch (rfork(RFPROC|RFMEM)){
	case -1:
		fprintf(stderr, "rfork failed\n");
		return;
	case 0:
		while(!done){
			drain_plumb_events();
			_SLEEP(100);
		}
		exit(0);
	default:
		break;
	}
}


int mch_has_wildcard(char_u *p) {
    for (; *p; mb_ptr_adv(p)) {
        if (*p == '\\' && p[1] != NUL) {
            ++p;
        } else if (vim_strchr((char_u *)"*?[{`'$", *p) != NULL ||
                (*p == '~' && p[1] != NUL)) {
            return TRUE;
        }
    }
    return FALSE;
}

int mch_has_exp_wildcard(char_u *p) {
    for (; *p; mb_ptr_adv(p))
    {
        if (*p == '\\' && p[1] != NUL)
            ++p;
        else
            if (vim_strchr((char_u *) "*?[{'", *p) != NULL)
                return TRUE;
    }
    return FALSE;
}

int mch_expandpath(garray_T *gap, char_u *pat, int flags) {
    return unix_expandpath(gap, pat, 0, flags, FALSE);
}

int mch_isdir(char_u *name) {
    struct stat st;
    if (stat((char*)name, &st) != 0) {
        return FALSE;
    }
    return S_ISDIR(st.st_mode) ? TRUE : FALSE;
}

static int executable_file(char_u *name)
{
    struct stat	st;

    if (stat((char *)name, &st))
        return 0;
    return S_ISREG(st.st_mode) && mch_access((char *)name, X_OK) == 0;
}

int mch_isFullName(char_u *fname) {
    return fname[0] == '/' || fname[0] == '#';
}

int mch_can_exe(char_u *name) {
    char_u	*buf;
    char_u	*p, *e;
    int		retval;

    /* If it's an absolute or relative path don't need to use $path. */
    if (mch_isFullName(name) || (name[0] == '.' && (name[1] == '/'
				      || (name[1] == '.' && name[2] == '/'))))
        return executable_file(name);

    p = (char_u *)getenv("path");
    if (p == NULL || *p == NUL)
        return -1;
    buf = alloc((unsigned)(STRLEN(name) + STRLEN(p) + 2));
    if (buf == NULL)
        return -1;

    /*
     * Walk through all entries in $PATH to check if "name" exists there and
     * is an executable file.
     */
    for (;;)
    {
        e = (char_u *)strchr((char *)p, '\01');
        if (e == NULL)
            e = p + STRLEN(p);
        if (e - p <= 1)		/* empty entry means current dir */
            STRCPY(buf, "./");
        else
        {
            vim_strncpy(buf, p, e - p);
            add_pathsep(buf);
        }
        STRCAT(buf, name);
        retval = executable_file(buf);
        if (retval == 1)
            break;

        if (*e != '\01')
            break;
        p = e + 1;
    }

    vim_free(buf);
    return retval;
}

int mch_dirname(char_u *buf, int len) {
    return (getcwd((char*)buf, len) ? OK : FAIL);
}

long mch_getperm(char_u *name) {
    struct stat st;
    if (stat((char*)name, &st) < 0) {
        return -1;
    }
    return st.st_mode;
}

int mch_setperm(char_u *name, long perm) {
    return chmod((char*)name, (mode_t)perm) == 0 ? OK : FAIL;
}

int mch_remove(char_u *name) {
    return remove((char*)name);
}

void mch_hide(char_u*) {
    /* Do nothing */
}

/* getuser() and sysname() can be implemented using this. */
static int read_value_from_file(char *fname, char_u *s, int len) {
    int fd;
    int n;
    fd = open(fname, O_RDONLY);
    if (fd < 0) {
        vim_strncpy(s, (char_u*)"none", len - 1);
        return FAIL;
    }
    n = read(fd, s, len - 1);
    if (n <= 0) {
        vim_strncpy(s, (char_u*)"none", len - 1);
    } else {
        s[n] = '\0';
    }
    close(fd);
    return (n > 0) ? OK : FAIL;
}

int mch_get_user_name(char_u *s, int len) {
    return read_value_from_file("/dev/user", s, len);
}

void mch_get_host_name(char_u *s, int len) {
    read_value_from_file("/dev/sysname", s, len);
}

void mch_settmode(int) {
    /* Do nothing */
}

int mch_screenmode(char_u*) {
    /* Always fail */
    EMSG(_(e_screenmode));
    return FAIL;
}

static void scr_stamp_cursor(void) {
    if (cursorsave) {
        draw(cursorsave, Rect(0, 0, fontsize.x, fontsize.y),
                screen, nil, Pt(screen->clipr.min.x + curcol * fontsize.x,
                    screen->clipr.min.y + currow * fontsize.y));
        border(screen, Rect(screen->clipr.min.x + curcol * fontsize.x,
                    screen->clipr.min.y + currow * fontsize.y,
                    screen->clipr.min.x + (curcol + 1) * fontsize.x,
                    screen->clipr.min.y + (currow + 1) * fontsize.y),
                2, colortab[cterm_normal_fg_color - 1], ZP);
    }
}

static void scr_unstamp_cursor(void) {
    if (cursorsave) {
        Rectangle r;
        r = Rect(screen->clipr.min.x + curcol * fontsize.x,
                screen->clipr.min.y + currow * fontsize.y,
                screen->clipr.min.x + (curcol + 1) * fontsize.x,
                screen->clipr.min.y + (currow + 1) * fontsize.y);
        draw(screen, r, cursorsave, nil, ZP);
    }
}

static void scr_pos(int row, int col) {
    currow = row;
    curcol = col;
}

static void scr_clear(void) {
    scr_pos(0, 0);
    draw(screen, screen->clipr, bgcolor, nil, ZP);
    draw(cursorsave, Rect(0, 0, fontsize.x, fontsize.y), bgcolor, nil, ZP);
}

static void scr_scroll_down(int nlines) {
    Rectangle r;
    Point pt;

    /* Copy up visible part of scroll region */
    r = Rect(screen->clipr.min.x + scrollregion.min.x * fontsize.x,
            screen->clipr.min.y + scrollregion.min.y * fontsize.y,
            screen->clipr.min.x + scrollregion.max.x * fontsize.x,
            screen->clipr.min.y + (scrollregion.max.y - nlines) * fontsize.y);
    pt = Pt(screen->clipr.min.x + scrollregion.min.x * fontsize.x,
            screen->clipr.min.y + (scrollregion.min.y + nlines) * fontsize.y);
    draw(screen, r, screen, nil, pt);

    /* Clear newly exposed part of scroll region */
    r.min.y = r.max.y;
    r.max.y = screen->clipr.min.y + scrollregion.max.y * fontsize.y;
    draw(screen, r, bgcolor, nil, ZP);
}

static void scr_scroll_up(int nlines) {
    Rectangle r;
    Point pt;

    /* Copy down visible part of scroll region */
    r = Rect(screen->clipr.min.x + scrollregion.min.x * fontsize.x,
            screen->clipr.min.y + (scrollregion.min.y + nlines) * fontsize.y,
            screen->clipr.min.x + scrollregion.max.x * fontsize.x,
            screen->clipr.min.y + scrollregion.max.y * fontsize.y);
    pt = Pt(screen->clipr.min.x + scrollregion.min.x * fontsize.x,
            screen->clipr.min.y + scrollregion.min.y * fontsize.y);
    draw(screen, r, screen, nil, pt);

    /* Clear newly exposed part of scroll region */
    r.max.y = r.min.y;
    r.min.y = screen->clipr.min.y + scrollregion.min.y * fontsize.y;
    draw(screen, r, bgcolor, nil, ZP);
}

static void scr_set_color(Image **cp, int c) {
    if (c >= 0 && c < NCOLORS) {
        *cp = colortab[c];
    }
}

void mch_set_normal_colors(void) {
    char_u	*p;
    int		n;

    if (cterm_normal_fg_color == 0) {
	cterm_normal_fg_color = 1;
    }
    if (cterm_normal_bg_color == 0) {
	cterm_normal_bg_color = 16;
    }
    if (T_ME[0] == ESC && T_ME[1] == '|')
    {
	p = T_ME + 2;
	n = getdigits(&p);
	if (*p == 'm' && n > 0)
	{
	    cterm_normal_fg_color = (n & 0xf) + 1;
	    cterm_normal_bg_color = ((n >> 4) & 0xf) + 1;
	}
    }

    scr_set_color(&bgcolor, cterm_normal_bg_color - 1);
    scr_set_color(&fgcolor, cterm_normal_fg_color - 1);
}

static void scr_tmode(int mode) {
    Image *tmp;
    if ((curmode & TMODE_REVERSE) != (mode & TMODE_REVERSE)) {
        tmp = fgcolor;
        fgcolor = bgcolor;
        bgcolor = tmp;
    }
    if (mode & TMODE_BOLD) {
        curfont = boldfont;
    } else {
        curfont = normalfont;
    }
    if (mode == TMODE_NORMAL) {
	scr_set_color(&bgcolor, cterm_normal_bg_color - 1);
	scr_set_color(&fgcolor, cterm_normal_fg_color - 1);
    }
    curmode = mode;
}

/* Read a number and return bytes consumed. */
static int scr_escape_number(char *p, int len, int *n) {
    int num;
    int chlen;
    if (len == 0) {
        return -1;
    }
    num = 0;
    chlen = 0;
    while (len && isdigit(*p)) {
        num = num * 10 + (*p - '0');
        p++;
        len--;
        chlen++;
    }
    *n = num;
    return chlen;
}

/* Handle escape sequence and return number of bytes consumed. */
static int scr_escape_sequence(char *p, int len) {
    int nlines;
    int n1;
    int n2;
    int i;
    int chlen;

    if (len == 0) {
        return 0;
    }

    chlen = 1;
    switch (*p) {
        case 'J': /* clear screen */
            scr_clear();
            break;

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            n1 = -1;
            n2 = -1;

            /* First number */
            i = scr_escape_number(p, len, &n1);
            if (i == -1) {
                /* TODO */
                fprintf(stderr, "scr_escape_sequence: escape at end of string\n");
                exit(1);
            }
            p += i;
            len -= i;
            chlen += i;

            /* Optional second number */
            if (len && *p == ';') {
                p += 1;
                len -= 1;
                chlen += 1;
                i = scr_escape_number(p, len, &n2);
                if (i == -1) {
                    /* TODO */
                    fprintf(stderr, "scr_escape_sequence: missing second number\n");
                    exit(1);
                }
                p += i;
                len -= i;
                chlen += i;
            }

            if (len == 0) {
                /* TODO */
                fprintf(stderr, "scr_escape_sequence: early end of escape sequence\n");
                exit(1);
            }

            switch (*p) {
                case 'b': /* background color */
                    scr_set_color(&bgcolor, n1);
                    break;

                case 'f': /* foreground color */
                    scr_set_color(&fgcolor, n1);
                    break;

                case 'H': /* cursor motion */
                    scr_pos(n1, n2);
                    break;

                case 'm': /* mode */
                    scr_tmode(n1);
                    break;

                case 'R': /* scroll region */
                    scrollregion.min.y = n1;
                    scrollregion.max.y = n2 + 1;
                    break;

                case 'V': /* scroll region vertical */
                    scrollregion.min.x = n1;
                    scrollregion.max.x = n2 + 1;
                    break;

                default:
                    /* TODO */
                    fprintf(stderr, "scr_escape_sequence: unimplemented (p=%c)\n", *p);
                    exit(1);
            }
            break;

        case 'K': /* clear to end of line */
            draw(screen, Rect(screen->clipr.min.x + curcol * fontsize.x,
                        screen->clipr.min.y + currow * fontsize.y,
                        screen->clipr.max.x,
                        screen->clipr.min.y + (currow + 1) * fontsize.y),
                    bgcolor, nil, ZP);
            break;

        case 'L': /* add new blank line */
            p++;
            nlines = 1;
            while (len >= 3 && p[0] == '\x1b' && p[1] == '[' && p[2] == 'L') {
                nlines++;
                len -= 3;
                p += 3;
                chlen += 3;
            }
            /* TODO what if nlines >= scroll region size */
            scr_scroll_up(nlines);
            break;

        default:
            /* TODO */
            fprintf(stderr, "scr_escape_sequence: unhandled sequence (p=%c)\n", *p);
            exit(1);
    }

    return chlen;
}

/* Handle special characters. */
static int write_special(char *p, int len) {
    int n;
    int nbytes;
    if (len == 0) {
        return 0;
    }
    nbytes = 0;
    while (len > 0) {
        switch (*p) {
            case '\a': /* Bell */
                /* There is no bell on Plan 9. */
                break;

            case '\b': /* Backspace */
                if (curcol > scrollregion.min.x) {
                    scr_pos(currow, curcol - 1);
                } else if (currow > scrollregion.min.y) {
                    scr_pos(currow - 1, scrollregion.max.x - 1);
                }
                break;

            case '\n': /* New line */
                for (n = 0; n < len && *p == '\n'; n++) {
                    p++;
                }
                if (currow + n >= scrollregion.max.y) {
                    scr_scroll_down(currow + n - scrollregion.max.y + 1);
                    scr_pos(scrollregion.max.y - 1, scrollregion.min.x);
                } else {
                    scr_pos(currow + n, scrollregion.min.x);
                }
                len -= n;
                nbytes += n;
                continue; /* process next character */

            case '\r': /* Carriage return */
                curcol = scrollregion.min.x;
                break;

            case '\x1b': /* Escape sequences */
                if (len > 1 && p[1] == '[') {
                    n = 2;
                    n += scr_escape_sequence(p + 2, len - 2);
                    p += n;
                    len -= n;
                    nbytes += n;
                    continue; /* process next character */
                }
                break;

            default: /* Normal character */
                return nbytes;
        }
        p++;
        len--;
        nbytes++;
    }
    return nbytes;
}

/* Draw normal characters. */
static int write_str(char *p, int len) {
    int nbytes;
    int n;
    int m;
    if (len == 0) {
        return 0;
    }
    for (nbytes = 0; nbytes < len &&
            p[nbytes] != '\a' && p[nbytes] != '\b' &&
            p[nbytes] != '\n' && p[nbytes] != '\r' &&
            p[nbytes] != '\x1b';
            len--, nbytes++)
        ;
    n = nbytes;
    while (n > 0) {
        m = (curcol + n >= scrollregion.max.x) ?
            scrollregion.max.x - curcol : n;
        if (m == 0) {
            break;
        }
        stringnbg(screen, Pt(screen->clipr.min.x + curcol * fontsize.x,
                    screen->clipr.min.y + currow * fontsize.y),
                fgcolor, ZP, curfont, p, m, bgcolor, ZP);
        curcol += m;
        if (curcol == scrollregion.max.x) {
            curcol = scrollregion.min.x;
            if (currow == scrollregion.max.y - 1) {
                scr_scroll_down(1);
            } else {
                currow++;
            }
        }
        p += m;
        n -= m;
    }
    return nbytes;
}

void mch_write(char_u *p, int len) {
    int slen;
    scr_unstamp_cursor();
    while (len > 0) {
        /* Handle special characters. */
        slen = write_special((char*)p, len);
        p += slen;
        len -= slen;

        /* Write as many normal characters as possible. */
        slen = write_str((char*)p, len);
        p += slen;
        len -= slen;
    }
    scr_stamp_cursor();
    flushimage(display, 1);
}

static void sigalrm(int, char*, void*) {
    /* Do nothing */
}

/* Don't allow mouse events to accumulate. */
static void drain_mouse_events(void) {
    while (ecanmouse()) {
        emouse();
    }
}

int RealWaitForChar(int, long msec, int*) {
    Rune rune;
    char utf[UTFmax];
    int len;

    drain_mouse_events();
    if (msec == 0 && !ecankbd()) {
        return 0;
    }
    if (msec > 0) {
        if (setjmp(timeoutjmpbuf)) {
            /* We arrive here if the alarm occurred and a library
             * function called drawerror() due to an interrupted
             * syscall. */
            _ALARM(0);
            interruptable = 0;
            return 0;
        }
        interruptable = 1;
        _ALARM((unsigned long)msec);
    }
    /* TODO garbage collect */
    rune = ekbd();
    if (msec > 0) {
        _ALARM(0);
        interruptable = 0;
    }
    if (rune == Ctrl_C && ctrl_c_interrupts) {
	got_int = TRUE;
	return 0;
    }
    len = runetochar(utf, &rune);
    add_to_input_buf_csi((char_u*)utf, len); /* TODO escape K_SPECIAL? */
    return len > 0;
}

int mch_inchar(char_u *buf, int maxlen, long msec, int) {
    if (vim_is_input_buf_empty() && RealWaitForChar(0, msec, NULL) == 0) {
	return 0;
    }
    return read_from_input_buf(buf, maxlen);
}

int mch_char_avail(void) {
    return ecankbd();
}

void mch_delay(long msec, int ignoreinput) {
    if (ignoreinput) {
        sleep(msec / 1000);
    } else {
        RealWaitForChar(0, msec, NULL);
    }
}

int mch_nodetype(char_u *name) {
    struct stat st;
    if (stat((char*)name, &st) < 0) {
        return NODE_NORMAL;
    }
    if (S_ISREG(st.st_mode) || S_ISDIR(st.st_mode)) {
        return NODE_NORMAL;
    }
    return NODE_WRITABLE;
}

int mch_FullName(char_u *fname, char_u *buf, int len, int force) {
    int		l;
    char_u	olddir[MAXPATHL];
    char_u	*p;
    int		retval = OK;

    /* expand it if forced or not an absolute path */
    if (force || !mch_isFullName(fname))
    {
        /*
         * If the file name has a path, change to that directory for a moment,
         * and then do the getwd() (and get back to where we were).
         * This will get the correct path name with "../" things.
         */
        if ((p = vim_strrchr(fname, '/')) != NULL)
        {
            /* Only change directory when we are sure we can return to where
             * we are now.  After doing "su" chdir(".") might not work. */
            if ( (mch_dirname(olddir, MAXPATHL) == FAIL ||
                        mch_chdir((char *)olddir) != 0))
            {
                p = NULL;	/* can't get current dir: don't chdir */
                retval = FAIL;
            }
            else
            {
                /* The directory is copied into buf[], to be able to remove
                 * the file name without changing it (could be a string in
                 * read-only memory) */
                if (p - fname >= len)
                    retval = FAIL;
                else
                {
                    vim_strncpy(buf, fname, p - fname);
                    if (mch_chdir((char *)buf))
                        retval = FAIL;
                    else
                        fname = p + 1;
                    *buf = NUL;
                }
            }
        }
        if (mch_dirname(buf, len) == FAIL)
        {
            retval = FAIL;
            *buf = NUL;
        }
        if (p != NULL)
        {
            l = mch_chdir((char *)olddir);
            if (l != 0)
                EMSG(_(e_prev_dir));
        }

        l = STRLEN(buf);
        if (l >= len)
            retval = FAIL;
        else
        {
            if (l > 0 && buf[l - 1] != '/' && *fname != NUL
                    && STRCMP(fname, ".") != 0)
                STRCAT(buf, "/");
        }
    }

    /* Catch file names which are too long. */
    if (retval == FAIL || STRLEN(buf) + STRLEN(fname) >= len)
        return FAIL;

    /* Do not append ".", "/dir/." is equal to "/dir". */
    if (STRCMP(fname, ".") != 0)
        STRCAT(buf, fname);

    return OK;
}

long mch_get_pid(void) {
    return (long)getpid();
}

int mch_input_isatty(void) {
    return isatty(0) ? TRUE : FALSE;
}

int mch_setenv(char *var, char *value, int x) {
    char buf[100];
    int fd;
    int len;

    snprintf(buf, sizeof buf, "/env/%s", var);

    /* Check for overwrite */
    if (!x) {
        struct stat st;
        if (stat(buf, &st) == 0) {
            return -1;
        }
    }

    /* Write the value */
    fd = creat(buf, 0666);
    if (fd < 0) {
        return -1;
    }
    len = strlen(value);
    if (write(fd, value, len) != len) {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

void mch_suspend(void) {
    suspend_shell();
}

static void update_shellsize(void) {
    shellsize = Pt((screen->clipr.max.x - screen->clipr.min.x) / fontsize.x,
                   (screen->clipr.max.y - screen->clipr.min.y) / fontsize.y);
}

int mch_get_shellsize(void) {
    update_shellsize();
    Rows = shellsize.y;
    Columns = shellsize.x;
    scrollregion.min.x = 0;
    scrollregion.min.y = 0;
    scrollregion.max.x = shellsize.x;
    scrollregion.max.y = shellsize.y;
    return OK;
}

void mch_set_shellsize(void) {
    /* Do nothing */
}

void mch_new_shellsize(void) {
    /* Do nothing */
}

void mch_breakcheck(void) {
    if (scr_inited) {
	/* Read into input buffer and check for Ctrl-C. */
	RealWaitForChar(0, 0, NULL);
    }
}

/* Called by libevent whenever a resize event occurs. */
void eresized(int renew) {
    if (renew) {
        if (getwindow(display, Refnone) < 0) {
            err9("resize failed");
        }
    }
    shell_resized();
}

static void init_colors(void) {
    int i;
    int colors[NCOLORS] = {
        /* Copied from hardcopy.c and adjusted for libdraw. */
        0x000000ff, 0x0000c0ff, 0x008000ff, 0x004080ff,
        0xc00000ff, 0xc000c0ff, 0x808000ff, 0xc0c0c0ff,
        0x808080ff, 0x6060ffff, 0x00ff00ff, 0x00ffffff,
        0xff8080ff, 0xff40ffff, 0xffff00ff, 0xffffffff
    };
    for (i = 0; i < NCOLORS; i++) {
        colortab[i] = allocimage(display, Rect(0, 0, 1, 1), screen->chan, 1, colors[i]);
        if (colortab[i] == nil) {
            err9("allocimage failed");
        }
    }
    mch_set_normal_colors();
}

static void free_colors(void) {
    int i;
    bgcolor = nil;
    fgcolor = nil;
    for (i = 0; i < NCOLORS; i++) {
        freeimage(colortab[i]);
        colortab[i] = nil;
    }
}

static void init_fonts(void) {
    normalfont = openfont(display, "/lib/font/bit/fixed/unicode.9x18.font");
    if (normalfont == nil) {
        err9("openfont normal failed");
    }
    boldfont = openfont(display, "/lib/font/bit/fixed/unicode.9x18B.font");
    if (boldfont == nil) {
        err9("openfont bold failed");
    }
    curfont = normalfont;
}

static void free_fonts(void) {
    freefont(normalfont);
    freefont(boldfont);
    curfont = nil;
}

void mch_early_init(void) {
    rfork(RFENVG | RFNOTEG);
}

int mch_check_win(int, char**) {
    return OK;
}

static void scr_init(void) {
    if (initdraw(derr, nil, "Vim") == -1) {
        err9("initdraw failed");
    }

    init_colors();
    init_fonts();
    fontsize = stringsize(curfont, "A");
    cursorsave = allocimage(display, Rect(0, 0, fontsize.x, fontsize.y), screen->chan, 0, DBlack);
    mch_get_shellsize();
    scr_clear();
    scr_stamp_cursor();
    flushimage(display, 1);

    /* Mouse events must be enabled to receive window resizes. */
    einit(Emouse | Ekeyboard);
    plumbkey = eplumb(512, "edit");
    start_plumber_thread();
    scr_inited = TRUE;
}

void mch_init(void) {
    done = FALSE;
    signal(SIGALRM, sigalrm);
    scr_init();

    /*
     * Force UTF-8 output no matter what the value of 'encoding' is.
     * did_set_string_option() in option.c prohibits changing 'termencoding'
     * to something else than UTF-8.
     */
    set_option_value((char_u *)"termencoding", 0L, (char_u *)"utf-8", 0);

#if defined(FEAT_CLIPBOARD)
    clip_init(TRUE);
#endif
}

void mch_exit(int r) {
    done = TRUE;
    ml_close_all(TRUE);    /* remove all memfiles */
    /* libdraw shuts down automatically on exit */
    exit(r);
}

#if defined(FEAT_TITLE)
void mch_settitle(char_u *title, char_u *) {
    int fd;
    fd = open("/dev/label", O_WRONLY);
    if (fd < 0) {
        /* Not running under rio. */
        return;
    }
    write(fd, (char*)title, strlen((char*)title));
    close(fd);
}

void mch_restore_title(int) {
    /* No need to restore - libdraw does this automatically. */
}

int mch_can_restore_title(void) {
    return TRUE;
}

int mch_can_restore_icon(void) {
    return FALSE;
}
#endif

#if defined(FEAT_CLIPBOARD)
int clip_mch_own_selection(VimClipboard*) {
    /* We never own the clipboard. */
    return FAIL;
}

void clip_mch_lose_selection(VimClipboard*) {
    /* Do nothing */
}

void clip_mch_request_selection(VimClipboard *cbd) {
    int fd;
    char_u *data;
    char_u *newdata;
    long_u len;  /* used length */
    long_u mlen; /* max allocated length */
    ssize_t n;
    int type;
    fd = open("/dev/snarf", O_RDONLY);
    if (fd < 0) {
        /* Not running under rio. */
        return;
    }
    n = 0;
    len = 0;
    mlen = 0;
    data = NULL;
    do {
        len += n;
        mlen += 4096;
        newdata = vim_realloc(data, mlen);
        if (!newdata) {
            n = -1;
            break;
        }
        data = newdata;
        n = read(fd, data, 4096);
        /* TODO breakcheck? */
    } while (n == 4096);
    if (n >= 0) {
        len += n;
        type = memchr(data, '\n', len) ? MLINE : MCHAR;
        clip_yank_selection(type, data, len, cbd);
    }
    vim_free(data);
    close(fd);
}

void clip_mch_set_selection(VimClipboard *cbd) {
    char_u *data;
    long_u len;
    int type;
    int fd;

    /* If the '*' register isn't already filled in, fill it in now. */
    cbd->owned = TRUE;
    clip_get_selection(cbd);
    cbd->owned = FALSE;

    type = clip_convert_selection(&data, &len, cbd);
    if (type < 0) {
        return;
    }
    fd = open("/dev/snarf", O_WRONLY);
    if (fd >= 0) {
        write(fd, data, len);
        close(fd);
    }
    vim_free(data);
}
#endif

#if defined(FEAT_MOUSE)
void mch_setmouse(int) {
    /* Do nothing.  Mouse needed for clipboard. */
}
#endif

static pid_t
forkwin(int hide){
	char spec[256];
	char *wsys;
	int wfd;
	pid_t pid;

	/* Fork child. */
	pid = fork();
	if(pid != 0)
	    return pid;

	/* We need a separate namespace from parent process. */
	rfork(RFNAMEG);

	/* Mounting the window system creates a new window. */
	wsys = getenv("wsys");
	if(!wsys){
		fprintf(stderr, "wsys not set\n");
		exit(1);
	}
	wfd = open(wsys, O_RDWR);
	if(wfd < 0){
		fprintf(stderr, "unable to open \"%s\"\n", wsys);
		exit(1);
	}
	snprintf(spec, sizeof spec, "new -pid %d -scroll %s", getpid(), hide ? "-hide" : "");
	if(mount(wfd, -1, "/mnt/wsys", MREPL, spec) < 0){
		fprintf(stderr, "unable to mount\n");
		exit(1);
	}
	if(bind("/mnt/wsys", "/dev", MBEFORE) < 0){
		fprintf(stderr, "unable to bind\n");
		exit(1);
	}

	/* Now reopen standard input, output, and error. */
	freopen("/dev/cons", "r", stdin);
	setbuf(stdin, NULL);
	freopen("/dev/cons", "w", stdout);
	setbuf(stdout, NULL);
	freopen("/dev/cons", "w", stderr);
	setbuf(stderr, NULL);

	return pid;
}

int mch_call_shell(char_u *cmd, int options) {
    char ch;
    pid_t pid;
    int status;
    int hide;

    /* Non-interactive commands run in a hidden window. */
    hide = options & SHELL_FILTER || options & SHELL_DOOUT;
    pid = forkwin(hide);
    if (pid == -1) {
	MSG_PUTS(_("\nCannot fork\n"));
	return -1;
    }
    if (pid == 0) {
	/* Fork again so that we can prompt the user to
	 * press a key before the window closes. */
	pid = fork();
	if (pid == -1) {
	    exit(255);
	}
	if (pid == 0) {
	    if (cmd) {
		execl("/bin/rc", "rc", "-c", cmd, NULL);
	    } else {
		execl("/bin/rc", "rc", NULL);
	    }
	    exit(122); /* same as on UNIX Vim */
	} else {
	    waitpid(pid, &status, 0);
	    if (!hide) {
		printf("\nPress RETURN to close this window...");
		read(0, &ch, 1);
	    }
	    exit(status);
	}
    }
    waitpid(pid, &status, 0);
    return status;
}
