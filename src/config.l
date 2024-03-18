%{
#include "common.h"
#include "y.tab.h"

#define YY_BUF_SIZE (512)

int isatty(int);
int fileno(FILE* );

void yyerror (char *s);
int yylex();
%}


%option nounistd
%option yylineno
%option stack

/* \n                                      {return EOL;} */
%%

\/\/.*                                  {;}
\"([^\\\"]|\\.)*\"                      {yylval.string_value = yytext; return string;}

"uint32_t"                              {return uint32_t_tk;}
"enum"                                  {return enum_tk;}
"char*"                                 {return char_p_tk;}


"image_config_t"                        {return image_config_t_tk;}
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
"flash_t"                               {return flash_t_tk;}
"flash"                                 {return flash_tk;}
"NO_FLASH"                              {yylval.enum_flash_t_value = NO_FLASH; return enum_flash_t;}
"LED_INBUILT_FLASH"                     {yylval.enum_flash_t_value = LED_INBUILT_FLASH; return enum_flash_t;}
"LED_EXTERNAL_FLASH"                    {yylval.enum_flash_t_value = LED_EXTERNAL_FLASH; return enum_flash_t;}


"ftp_config_t"                          {return ftp_config_t_tk;}
"apn"                                   {return apn_tk;}
"mccmnc"                                {return mccmnc_tk;}
"domain"                                {return domain_tk;}
"username"                              {return username_tk;}
"cyphter_t"                             {return cypher_t_tk;}
"cypher"                                {return cypher_tk;}
"NONE"                                  {yylval.enum_cypher_t_value = NONE; return enum_cypher_t;}
"CAESAR"                                {yylval.enum_cypher_t_value = CAESAR; return enum_cypher_t;}
"SUFFIX"                                {yylval.enum_cypher_t_value = SUFFIX; return enum_cypher_t;}
"password"                              {return password_tk;}
"image_path"                            {return image_path_tk;}
"status_path"                           {return status_path_tk;}


"sd_config_t"                           {return sd_config_t_tk;}
"logging_level"                         {return logging_level_tk;}


"trigger_config_t"                      {return trigger_config_t_tk;}
"trigger_t"                             {return trigger_t_tk;}
"trigger"                               {return trigger_tk;}
"TIME_TRIGGER"                          {yylval.enum_trigger_t_value = TIME_TRIGGER; return enum_trigger_t;}
"UART_TRIGGER"                          {yylval.enum_trigger_t_value = UART_TRIGGER; return enum_trigger_t;}
"logging_interval"                      {return logging_interval_tk;}
"logging_decimation_ftp"                {return logging_decimation_ftp_tk;}


[a-zA-Z_$][a-zA-Z_0-9]*                 {/*printf(" %s ", yytext)*/;return name;}
[0-9]+                                  {yylval.integer_value = atoi(yytext); return integer;}
[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?  {yylval.floating_point_value = atof(yytext); return floating_point;}
[ \t\n\r]                               {;}
[-+=;^*{},]           	                {return yytext[0];}
.                                       {ECHO; yyerror ("unexpected character");}

%%
int yywrap (void) {return 1;}