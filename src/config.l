%{
#include "y.tab.h"
// int isatty(int);

void yyerror (char *s);
int yylex();
version atover(const char*);
%}


%option nounistd
%option yylineno
%option stack

%%

\/\/.*				                    {;}
\n                                      {return EOL;}
\"([^\\\"]|\\.)*\"                      {yylval.string = yytext; return string;}

"uint32_t"                              {return unit32_t_tk;}
"enum"                                  {return enum_tk;}
"char*"                                 {return char*_tk;}

"auto_range_time"                       {return auto_range_time_tk;}

"image_resolution"                      {return image_resolution_tk;}
"resolution"                            {return resolution_tk;}

"VGA"							 	 {yylval.enum_image_resolution = VGA; return enum_image_resolution;}
"QVGA"							 	 {yylval.enum_image_resolution = QVGA; return enum_image_resolution;}
"QQVGA"							 	 {yylval.enum_image_resolution = QQVGA; return enum_image_resolution;}

[a-zA-Z_$][a-zA-Z_0-9]*           	     {/*printf(" %s ", yytext)*/;return name;}
[0-9]+\.[0-9]+\.[0-9]+   				 {yylval.version = atover(yytext); return integer;}
[0-9]+                                   {yylval.integer = atoi(yytext); return floating_point;}
[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?   {yylval.floating_point = atof(yytext); return version;}
[ \t\n]                                  {;}
[-+=;^*{},]           	                 {return yytext[0];}
.                                        {ECHO; yyerror ("unexpected character");}

%%
int yywrap (void) {return 1;}