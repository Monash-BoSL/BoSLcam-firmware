%{
#include <string.h>
#include <zephyr.h>
#include "common.h"
#include "y.tab.h"
#include <errno.h>

void yyerror (const char *s);
int yylex();
extern int yylineno;
struct master_config_t* parser_config_handle;

int string_malloc(char** dst, char* src);

int yydebug = 1;

%}

%define parse.error verbose

%union {
		int integer_value; 
		double floating_point_value; 
		char* string_value;
		enum image_resolution enum_image_resolution_value;
		enum image_format enum_image_format_value;
		enum flash_t enum_flash_t_value;
		enum cypher_t enum_cypher_t_value;
                enum trigger_t enum_trigger_t_value;
		}         /* Yacc definitions */


%start config

%token EOL

%token uint32_t_tk
%token enum_tk
%token char_p_tk

%token image_config_t_tk
%token auto_range_time_tk
%token image_resolution_tk
%token resolution_tk
%token image_format_tk
%token format_tk
%token flash_t_tk
%token flash_tk
%token use_flash_tk

%token ftp_config_t_tk
%token apn_tk
%token mccmnc_tk
%token domain_tk
%token username_tk
%token cypher_t_tk
%token cypher_tk
%token password_tk
%token image_path_tk
%token status_path_tk

%token sd_config_t_tk
%token logging_level_tk

%token trigger_config_t_tk
%token trigger_t_tk
%token trigger_tk
%token logging_interval_tk
%token logging_decimation_ftp_tk

%token name

%token <integer_value> integer
%token <floating_point_value> floating_point
%token <string_value> string
%token <enum_image_resolution_value> enum_image_resolution
%token <enum_image_format_value> enum_image_format
%token <enum_flash_t_value> enum_flash_t
%token <enum_cypher_t_value> enum_cypher_t
%token <enum_trigger_t_value> enum_trigger_t

%%


/* descriptions of expected inputs     corresponding actions (in C) */

config  : struct         {;}
        | config struct    {;}
        ;

struct  : image_config_t            {;}
        | ftp_config_t              {;}
        | sd_config_t               {;}
        | trigger_config_t          {;}
        ;

image_config_t  : image_config_t_tk image_config_t_members      {;}

image_config_t_members  : image_config_t_entry                          {;}
                        | image_config_t_members image_config_t_entry   {;}
                        ;

image_config_t_entry    : uint32_t_tk auto_range_time_tk                   '=' integer                     {parser_config_handle->im_cfg.auto_range_time = $4;}
                        | enum_tk image_resolution_tk resolution_tk        '=' enum_image_resolution       {parser_config_handle->im_cfg.resolution = $5;}
                        | enum_tk image_format_tk format_tk                '=' enum_image_format           {parser_config_handle->im_cfg.format = $5;}
                        | enum_tk flash_t_tk flash_tk                      '=' enum_flash_t                {parser_config_handle->im_cfg.flash = $5;}
                        | uint32_t_tk use_flash_tk                         '=' integer                     {parser_config_handle->im_cfg.use_flash = $4;}
                        ;


ftp_config_t  : ftp_config_t_tk ftp_config_t_members      {;}

ftp_config_t_members    : ftp_config_t_entry                          {;}
                        | ftp_config_t_members ftp_config_t_entry   {;}
                        ;

ftp_config_t_entry      : char_p_tk apn_tk                                 '=' string                      {string_malloc(&parser_config_handle->ftp_cfg.apn,$4);}
                        | char_p_tk mccmnc_tk                              '=' string                      {string_malloc(&parser_config_handle->ftp_cfg.mccmnc,$4);}
                        | char_p_tk domain_tk                              '=' string                      {string_malloc(&parser_config_handle->ftp_cfg.domain,$4);}
                        | char_p_tk username_tk                            '=' string                      {string_malloc(&parser_config_handle->ftp_cfg.username,$4);}
                        | enum_tk cypher_t_tk cypher_tk                    '=' enum_cypher_t               {parser_config_handle->ftp_cfg.cypher = $5;}
                        | char_p_tk password_tk                            '=' string                      {string_malloc(&parser_config_handle->ftp_cfg.password,$4);}
                        | char_p_tk image_path_tk                          '=' string                      {string_malloc(&parser_config_handle->ftp_cfg.image_path,$4);}
                        | char_p_tk status_path_tk                         '=' string                      {string_malloc(&parser_config_handle->ftp_cfg.status_path,$4);}
                        ;


sd_config_t  : sd_config_t_tk sd_config_t_members      {;}

sd_config_t_members     : sd_config_t_entry                          {;}
                        | sd_config_t_members sd_config_t_entry   {;}
                        ;

sd_config_t_entry       : char_p_tk image_path_tk                          '=' string                      {string_malloc(&parser_config_handle->sd_cfg.image_path,$4);}
                        | char_p_tk status_path_tk                         '=' string                      {string_malloc(&parser_config_handle->sd_cfg.status_path,$4);}
                        | uint32_t_tk logging_level_tk                     '=' integer                     {parser_config_handle->sd_cfg.logging_level = $4;}
                        ;


trigger_config_t          : trigger_config_t_tk trigger_config_t_members      {;}

trigger_config_t_members  : trigger_config_t_entry                            {;}
                          | trigger_config_t_members trigger_config_t_entry   {;}
                          ;

trigger_config_t_entry  : enum_tk trigger_t_tk trigger_tk                  '=' enum_trigger_t              {parser_config_handle->trig_cfg.trigger = $5;}
                        | uint32_t_tk logging_interval_tk                  '=' integer                     {parser_config_handle->trig_cfg.logging_interval = $4;}
                        | uint32_t_tk logging_decimation_ftp_tk            '=' integer                     {parser_config_handle->trig_cfg.logging_decimation_ftp = $4;}
                        ;

%%                     /* C code */

void yyerror (const char *s) {
        printk("\033[1;31m YYERROR: " CONFIG_FILE ":%d: %s\033[0m\n", yylineno, s); 
        k_msleep(100);
}

//this string is "" enclosed so we need to remove the first and last characters
int string_malloc(char** dst, char* src){
        size_t len = strlen(src) - 2;//remove first and last characters

        *dst = k_malloc(len+1);
        if(*dst == NULL){return -ENOMEM;}
        
        memcpy(*dst, src+1, len);//+1 to remove first character
        (*dst)[len] = '\0';//null terminate string

        return 0;
}


