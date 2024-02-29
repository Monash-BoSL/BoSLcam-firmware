%{
// #include <stdio.h>     /* C declarations used in actions */
// #include <stdlib.h>
// #include <ctype.h>
// #include <string.h>
// int isatty(int);
// int _get_osfhandle(int);
//#void *memcpy(void *, const void *, unsigned long)
#include "common.h"
#include "y.tab.h"

void yyerror (char *s);
int yylex();
extern int yylineno;
extern struct master_config_t mcfg;

int yydebug = 1;

%}

%define parse.error verbose

%union {
		int integer; 
		double floating_point; 
		char* string;
		version version;
		enum image_resolution enum_image_resolution;
		}         /* Yacc definitions */


%start line

%token EOL

%token uint32_t_tk
%token enum_tk
%token char_p_tk

%token auto_range_time_tk

%token image_resolution_tk
%token resolution_tk

%token name

%token <integer> integer
%token <floating_point> floating_point
%token <string> string
%token <enum_image_resolution> enum_image_resolution

%%


/* descriptions of expected inputs     corresponding actions (in C) */

line    : entry EOL        {;}
        | line entry EOL   {;}
        ;

entry   : image_config_t_entry      {;}
        | ftp_config_t_entry        {;}
        | sd_config_t_entry         {;}
        | trigger_config_t_entry    {;}
        ;

image_config_t_entry : uint32_t_tk auto_range_time_tk '=' integer                   {mcfg.trig_cfg.auto_range_time = $4}
                     | enum_tk image_resolution_tk resolution_tk '=' enum_image_resolution    {mcfg.trig_cfg.auto_range_time = $5}

ftp_config_t_entry  :                   {;}
                    ;

sd_config_t_entry   :                   {;}
                    ;

trigger_config_t_entry  :                   {;}
                        ;

%%                     /* C code */

void yyerror (char *s) {
}
