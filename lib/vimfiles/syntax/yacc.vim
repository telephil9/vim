" Vim syntax file
" Language:	Yacc
" Maintainer:	Dr. Charles E. Campbell, Jr. <NdrOchipS@PcampbellAfamily.Mbiz>
" Last Change:	Feb 22, 2006
" Version:	4
" URL:	http://mysite.verizon.net/astronaut/vim/index.html#vimlinks_syntax
"
" Option:
"   g:yacc_uses_cpp : if this variable exists, then C++ is loaded rather than C

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

" Read the C syntax to start with
if version >= 600
  if exists("g:yacc_uses_cpp")
    runtime! syntax/cpp.vim
  else
    runtime! syntax/c.vim
  endif
elseif exists("g:yacc_uses_cpp")
  so <sfile>:p:h/cpp.vim
else
  so <sfile>:p:h/c.vim
endif

" Clusters
syn cluster	yaccActionGroup	contains=yaccDelim,cInParen,cTodo,cIncluded,yaccDelim,yaccCurlyError,yaccUnionCurly,yaccUnion,cUserLabel,cOctalZero,cCppOut2,cCppSkip,cErrInBracket,cErrInParen,cOctalError,cCommentStartError,cParenError
syn cluster	yaccUnionGroup	contains=yaccKey,cComment,yaccCurly,cType,cStructure,cStorageClass,yaccUnionCurly

" Yacc stuff
syn match	yaccDelim	"^\s*[:|;]"
syn match	yaccOper	"@\d\+"

syn match	yaccKey	"^\s*%\(token\|type\|left\|right\|start\|ident\|nonassoc\)\>"
syn match	yaccKey	"\s%\(prec\|expect\)\>"
syn match	yaccKey	"\$\(<[a-zA-Z_][a-zA-Z_0-9]*>\)\=[\$0-9]\+"
syn keyword	yaccKeyActn	yyerrok yyclearin

syn match	yaccUnionStart	"^%union"	skipwhite skipnl nextgroup=yaccUnion
syn region	yaccUnion	contained matchgroup=yaccCurly start="{" matchgroup=yaccCurly end="}"	contains=@yaccUnionGroup
syn region	yaccUnionCurly	contained matchgroup=yaccCurly start="{" matchgroup=yaccCurly end="}" contains=@yaccUnionGroup
syn match	yaccBrkt	contained "[<>]"
syn match	yaccType	"<[a-zA-Z_][a-zA-Z0-9_]*>"	contains=yaccBrkt
syn match	yaccDefinition	"^[A-Za-z][A-Za-z0-9_]*\_s*:"

" special Yacc separators
syn match	yaccSectionSep	"^[ \t]*%%"
syn match	yaccSep	"^[ \t]*%{"
syn match	yaccSep	"^[ \t]*%}"

" I'd really like to highlight just the outer {}.  Any suggestions???
syn match	yaccCurlyError	"[{}]"
syn region	yaccAction	matchgroup=yaccCurly start="{" end="}" contains=ALLBUT,@yaccActionGroup


" Define the default highlighting.
" For version 5.7 and earlier: only when not done already
" For version 5.8 and later: only when an item doesn't have highlighting yet
if version >= 508 || !exists("did_yacc_syn_inits")
  if version < 508
    let did_yacchdl_syn_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif

  " Internal yacc highlighting links
  HiLink yaccBrkt	yaccStmt
  HiLink yaccKey	yaccStmt
  HiLink yaccOper	yaccStmt
  HiLink yaccUnionStart	yaccKey

  " External yacc highlighting links
  HiLink yaccCurly	Delimiter
  HiLink yaccCurlyError	Error
  HiLink yaccDefinition	Function
  HiLink yaccDelim	Function
  HiLink yaccKeyActn	Special
  HiLink yaccSectionSep	Todo
  HiLink yaccSep	Delimiter
  HiLink yaccStmt	Statement
  HiLink yaccType	Type

  " since Bram doesn't like my Delimiter :|
  HiLink Delimiter	Type

  delcommand HiLink
endif

let b:current_syntax = "yacc"

" vim: ts=15
