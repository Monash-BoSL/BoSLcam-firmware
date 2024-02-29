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
		int integer_value; 
		double floating_point_value; 
		char* string_value;
		enum image_resolution enum_image_resolution_value;
		enum image_format enum_image_format_value;
		enum cypher_type enum_cypher_type_value;
                enum trigger_type enum_trigger_type_value;
		}         /* Yacc definitions */


%start line

%token EOL

%token uint32_t_tk
%token enum_tk
%token char_p_tk

%token auto_range_time_tk
%token image_resolution_tk
%token resolution_tk
%token image_format_tk
%token format_tk

%token apn_tk
%token network_operator_tk
%token domain_tk
%token username_tk
%token cypher_type_tk
%token cyph_type_tk
%token password_tk
%token image_path_tk
%token status_path_tk

%token logging_level_tk

%token trigger_type_tk
%token trig_type_tk
%token logging_interval_tk
%token logging_decimation_ftp_tk

%token name

%token <integer_value> integer
%token <floating_point_value> floating_point
%token <string_value> string
%token <enum_image_resolution_value> enum_image_resolution
%token <enum_image_format_value> enum_image_format
%token <enum_cypher_type_value> enum_cypher_type
%token <enum_trigger_type_value> enum_trigger_type

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

image_config_t_entry    : uint32_t_tk auto_range_time_tk                   '=' integer                     {mcfg.im_cfg.auto_range_time = $4;}
                        | enum_tk image_resolution_tk resolution_tk        '=' enum_image_resolution       {mcfg.im_cfg.resolution = $5;}
                        | enum_tk image_format_tk format_tk                '=' enum_image_format           {mcfg.im_cfg.format = $5;}
                        ;

ftp_config_t_entry      : char_p_tk apn_tk                                  '=' string                      {string_malloc(mcfg.ftp_cfg.apn,$4);}
                        | char_p_tk network_operator_tk                    '=' string                      {string_malloc(mcfg.ftp_cfg.network_operator,$4);}
                        | char_p_tk domain_tk                              '=' string                      {string_malloc(mcfg.ftp_cfg.domain,$4);}
                        | char_p_tk username_tk                            '=' string                      {string_malloc(mcfg.ftp_cfg.username,$4);}
                        | enum_tk cypher_type_tk cyph_type_tk              '=' enum_cypher_type            {mcfg.ftp_cfg.cyph_type = $5;}
                        | char_p_tk password_tk                            '=' string                      {string_malloc(mcfg.ftp_cfg.password,$4);}
                        | char_p_tk image_path_tk                          '=' string                      {string_malloc(mcfg.ftp_cfg.image_path,$4);}
                        | char_p_tk status_path_tk                         '=' string                      {string_malloc(mcfg.ftp_cfg.status_path,$4);}
                        ;

sd_config_t_entry       : char_p_tk image_path_tk                          '=' string                      {string_malloc(mcfg.sd_cfg.image_path,$4);}
                        | char_p_tk status_path_tk                         '=' string                      {string_malloc(mcfg.sd_cfg.status_path,$4);}
                        | uint32_t_tk logging_level_tk                     '=' integer                     {mcfg.sd_cfg.logging_level = $4;}
                        ;

trigger_config_t_entry  : enum_tk trigger_type_tk trig_type_tk             '=' enum_trigger_type           {mcfg.trig_cfg.trig_type = $5;}
                        | uint32_t_tk logging_interval_tk                  '=' integer                     {mcfg.trig_cfg.logging_interval = $4;}
                        | uint32_t_tk logging_decimation_ftp_tk            '=' integer                     {mcfg.trig_cfg.logging_decimation_ftp = $4;}
                        ;

%%                     /* C code */

void yyerror (char *s) {

}

int string_malloc(char* dst, char* src){
        size_t len = strlen(src);

        dst = k_malloc(len+1);
        if(dst == NULL){return -ENOMEM;}
        
        memcpy(*dst, src, len);
        (*dst)[len] = '\0';//null terminate string

        return 0;
}