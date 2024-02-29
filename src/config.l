%{
#include "common.h"
#include "y.tab.h"
// int isatty(int);

void yyerror (char *s);
int yylex();
%}


%option nounistd
%option yylineno
%option stack

%%

\/\/.*                                  {;}
\n                                      {return EOL;}
\"([^\\\"]|\\.)*\"                      {yylval.string_value = yytext; return string;}

"uint32_t"                              {return uint32_t_tk;}
"enum"                                  {return enum_tk;}
"char*"                                 {return char_p_tk;}

"auto_range_time"                       {return auto_range_time_tk;}

"image_resolution"                      {return image_resolution_tk;}
"resolution"                            {return resolution_tk;}

"VGA"                                   {yylval.enum_image_resolution_value = VGA; return enum_image_resolution;}
"QVGA"                                  {yylval.enum_image_resolution_value = QVGA; return enum_image_resolution;}
"QQVGA"                                 {yylval.enum_image_resolution_value = QQVGA; return enum_image_resolution;}

[a-zA-Z_$][a-zA-Z_0-9]*                 {/*printf(" %s ", yytext)*/;return name;}
[0-9]+                                  {yylval.integer_value = atoi(yytext); return integer;}
[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?  {yylval.floating_point_value = atof(yytext); return floating_point;}
[ \t\n]                                 {;}
[-+=;^*{},]           	                {return yytext[0];}
.                                       {ECHO; yyerror ("unexpected character");}

%%
int yywrap (void) {return 1;}