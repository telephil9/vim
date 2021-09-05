#
# Makefile for VIM on Plan 9
#

#>>>>> choose options:

### See feature.h for a list of optionals.

# The root directory for resources.
VIMRCLOC=/sys/lib/vim

# The runtime files.
VIMRUNTIMEDIR=$VIMRCLOC/vimfiles

#>>>>> end of choices
###########################################################################

APE=/sys/src/ape
<$APE/config

BIN=/$objtype/bin

TARG=\
	vim\
	xxd\

VOFILES=\
	buffer.$O\
	charset.$O\
	diff.$O\
	digraph.$O\
	edit.$O\
	eval.$O\
	ex_cmds.$O\
	ex_cmds2.$O\
	ex_docmd.$O\
	ex_eval.$O\
	ex_getln.$O\
	fileio.$O\
	fold.$O\
	getchar.$O\
	hardcopy.$O\
	hashtab.$O\
	main.$O\
	mark.$O\
	mbyte.$O\
	memfile.$O\
	memline.$O\
	menu.$O\
	message.$O\
	misc1.$O\
	misc2.$O\
	move.$O\
	normal.$O\
	ops.$O\
	option.$O\
	os_plan9.$O\
	pathdef.$O\
	popupmnu.$O\
	quickfix.$O\
	regexp.$O\
	screen.$O\
	search.$O\
	spell.$O\
	syntax.$O\
	tag.$O\
	term.$O\
	ui.$O\
	undo.$O\
	version.$O\
	window.$O\

</sys/src/cmd/mkmany

CFLAGS=-c -D_POSIX_SOURCE -DPLAN9 -D_PLAN9_SOURCE -Iproto

pathdef.$O:VQ:
	echo creating pathdef.c...
	cat > pathdef.c <<EOF
	/* pathdef.c */
	#include "vim.h"
	char_u *default_vim_dir = (char_u *)"$VIMRCLOC";
	char_u *default_vimruntime_dir = (char_u *)"$VIMRUNTIMEDIR";
	char_u *all_cflags = (char_u *)"$CC $CFLAGS";
	char_u *all_lflags = (char_u *)"$LD -o $O.vim";
	char_u *compiled_user = (char_u *)"$user";
	char_u *compiled_sys = (char_u *)"$sysname";
	EOF
	$CC $CFLAGS pathdef.c

# version.c is compiled each time, so that it sets the build time.
version.$O:V:
	$CC $CFLAGS version.c

# be stricter on our own code.
os_plan9.$O: os_plan9.c
	$CC $CFLAGS -FVw $prereq

$O.vim:V:	$VOFILES
	$LD -o $target $prereq

$O.xxd:	xxd/xxd.c
	$CC -D_POSIX_SOURCE -o $target $prereq
