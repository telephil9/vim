/* vi:set ts=8 sts=4 sw=4:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 */

/*
 * Plan 9 Machine-dependent things
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>

#define SPACE_IN_FILENAME
#define USE_TERM_CONSOLE
#define USE_UNIXFILENAME

#define HAVE_STDLIB_H
#define HAVE_STDARG_H
#define HAVE_STRING_H
#define HAVE_FCNTL_H
#define HAVE_STRCSPN
#define HAVE_STRFTIME	    /* guessed */
#define HAVE_SETENV
#define HAVE_MEMSET
#define HAVE_PATHDEF
#define HAVE_QSORT
#if defined(__DATE__) && defined(__TIME__)
# define HAVE_DATE_TIME
#endif

#define DFLT_ERRORFILE		"errors.err"

#define DFLT_RUNTIMEPATH	"$home/lib/vim/vimfiles,$VIM/vimfiles,$VIMRUNTIME,$VIM/vimfiles/after,$home/lib/vim/vimfiles/after"

#if !defined(MAXNAMLEN)
# define MAXNAMLEN 512		    /* for all other Unix */
#endif

#define BASENAMELEN	(MAXNAMLEN - 5)

#define TEMPDIRNAMES  "$TMPDIR", "/tmp", ".", "$HOME"
#define TEMPNAMELEN    256

/*
 * Names for the EXRC, HELP and temporary files.
 * Some of these may have been defined in the makefile.
 */
#ifndef SYS_VIMRC_FILE
# define SYS_VIMRC_FILE "$VIM/vimrc"
#endif
#ifndef SYS_GVIMRC_FILE
# define SYS_GVIMRC_FILE "$VIM/gvimrc"
#endif
#ifndef SYS_MENU_FILE
# define SYS_MENU_FILE	"$VIMRUNTIME/menu.vim"
#endif
#ifndef DFLT_HELPFILE
# define DFLT_HELPFILE	"$VIMRUNTIME/doc/help.txt"
#endif
#ifndef FILETYPE_FILE
# define FILETYPE_FILE	"filetype.vim"
#endif
#ifndef FTPLUGIN_FILE
# define FTPLUGIN_FILE	"ftplugin.vim"
#endif
#ifndef INDENT_FILE
# define INDENT_FILE	"indent.vim"
#endif
#ifndef FTOFF_FILE
# define FTOFF_FILE	"ftoff.vim"
#endif
#ifndef FTPLUGOF_FILE
# define FTPLUGOF_FILE	"ftplugof.vim"
#endif
#ifndef INDOFF_FILE
# define INDOFF_FILE	"indoff.vim"
#endif
#ifndef SYNTAX_FNAME
# define SYNTAX_FNAME	"$VIMRUNTIME/syntax/%s.vim"
#endif

#ifndef USR_EXRC_FILE
# define USR_EXRC_FILE	"$home/lib/exrc"
#endif

#ifndef USR_VIMRC_FILE
# define USR_VIMRC_FILE	"$home/lib/vimrc"
#endif
#ifndef EVIM_FILE
# define EVIM_FILE	"$VIMRUNTIME/evim.vim"
#endif

#ifndef USR_GVIMRC_FILE
# define USR_GVIMRC_FILE "$home/lib/gvimrc"
#endif

#ifdef FEAT_VIMINFO
# ifndef VIMINFO_FILE
#  define VIMINFO_FILE	"$home/lib/viminfo"
# endif
#endif /* FEAT_VIMINFO */

#ifndef EXRC_FILE
# define EXRC_FILE	"exrc"
#endif

#ifndef VIMRC_FILE
# define VIMRC_FILE	"vimrc"
#endif

#ifndef GVIMRC_FILE
# define GVIMRC_FILE	"gvimrc"
#endif

#ifndef DFLT_BDIR
#define DFLT_BDIR    ".,/tmp,$home"    /* default for 'backupdir' */
#endif

#ifndef DFLT_DIR
# define DFLT_DIR     ".,/tmp" /* default for 'directory' */
#endif

#ifndef DFLT_VDIR
# define DFLT_VDIR    "$home/lib/vim/view"       /* default for 'viewdir' */
#endif

#ifndef DFLT_MAXMEM
# define DFLT_MAXMEM	(5*1024)	 /* use up to 5 Mbyte for a buffer */
#endif
#ifndef DFLT_MAXMEMTOT
# define DFLT_MAXMEMTOT	(10*1024)    /* use up to 10 Mbyte for Vim */
#endif

#define mch_rename(src, dst) rename(src, dst)
#define mch_chdir(s) chdir(s)
#define vim_mkdir(x, y) mkdir((char*)(x), (y))
#define mch_rmdir(x) rmdir((char*)(x))
#define mch_getenv(x) (char_u *)getenv((char *)(x))
