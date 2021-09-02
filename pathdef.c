/* pathdef.c */
#include "vim.h"
char_u *default_vim_dir = (char_u *)"./lib";
char_u *default_vimruntime_dir = (char_u *)"./lib/vimfiles";
char_u *all_cflags = (char_u *)"pcc -c -D_POSIX_SOURCE -DPLAN9 -D_PLAN9_SOURCE -Iproto";
char_u *all_lflags = (char_u *)"pcc -o 6.vim";
char_u *compiled_user = (char_u *)"glenda";
char_u *compiled_sys = (char_u *)"tenshi";
