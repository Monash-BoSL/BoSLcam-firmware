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
"image_format"                          {return image_format_tk;}
"format"                                {return format_tk;}
"BMP"                                   {yylval.enum_image_format_value = BMP; return enum_image_format;}
"JPG"                                   {yylval.enum_image_format_value = JPG; return enum_image_format;}


"apn"                                   {return apn_tk;}
"network_operator"                      {return network_operator_tk;}
"domain"                                {return domain_tk;}
"username"                              {return username_tk;}
"cypher_type"                           {return image_resolution_tk;}
"cyph_type"                             {return resolution_tk;}
"NONE"                                  {yylval.enum_cypher_type_value = NONE; return enum_cypher_type;}
"CEASAR"                                {yylval.enum_cypher_type_value = CEASAR; return enum_cypher_type;}
"SUFFIX"                                {yylval.enum_cypher_type_value = SUFFIX; return enum_cypher_type;}
"password"                              {return password_tk;}
"image_path"                            {return image_path_tk;}
"status_path"                           {return status_path_tk;}


"logging_level"                         {return logging_level_tk;}


"trigger_type"                          {return trigger_type_tk;}
"trig_type"                             {return trig_type_tk;}
"TIME_TRIGGER"                          {yylval.enum_trigger_type_value = TIME_TRIGGER; return enum_trigger_type;}
"UART_TRIGGER"                          {yylval.enum_trigger_type_value = UART_TRIGGER; return enum_trigger_type;}


"logging_interval"                      {return logging_interval_tk;}
"logging_decimation_ftp"                {return logging_decimation_ftp_tk;}


[a-zA-Z_$][a-zA-Z_0-9]*                 {/*printf(" %s ", yytext)*/;return name;}
[0-9]+                                  {yylval.integer_value = atoi(yytext); return integer;}
[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?  {yylval.floating_point_value = atof(yytext); return floating_point;}
[ \t\n]                                 {;}
[-+=;^*{},]           	                {return yytext[0];}
.                                       {ECHO; yyerror ("unexpected character");}

%%
int yywrap (void) {return 1;}