#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#include "common.h"


enum type_type{
	NULL_TYPE,
	UINT32_T_TYPE,
	CHARP_TYPE,	
	ENUM_TYPE,
};

struct config_variable_t {
	enum type type_type;
	char* name;
	void* vp;
};

config_variable_t auto_range_time_cfg {UINT32_T_TYPE, "auto_range_time", NULL};


int next_token(){
	
}


const config_variable_t null_option = {NULL_TYPE, NULL, NULL};
int parse(){


	while(line){

		config_variable_t line_option = null_option;

		if(!next_tok == type){goto syntax_error;}
		line_option.type_type = next_tok;

		if(!next_tok == name){goto syntax_error;}
		line_option.name = next_tok;

		if(tokmatch(line_option)){goto match_error;}

		if(!next_tok == '='){goto syntax_error;}

		if(!next_tok == value){goto syntax_error;}
		line_option.vp = parse_val_tok(next_tok);

	}

syntax_error:
match_error:


	return ret;

}


enum parse_state{
	NAME = 0,
	COMMENT,
	VALUE,
};

int store_int(char* from_string, uint32_t* to){
	*to = atoi(from_string); 
	return 0;
}
int store_string(char* from_string, char** to){
	char* start = strchr(from_string, '"')+1;
	char* end = strchr(start, '"');
	if(start == NULL || end == NULL){
		LOG_ERR("config string reading error");
		return -EINVAL;
	}
	uint32_t len = end-start;
	
	*to = k_malloc(len+1);
	memcpy(*to, start, len);
	(*to)[len] = 0;//null terminate string
	
	return 0;
}


int store_format_type(char* from_string, enum image_format* to){
	int enum_int;
	store_int(from_string, &enum_int);
	*to = enum_int;
	return 0;
}

int store_cypher_type(char* from_string, enum cypher_type* to){
	int enum_int;
	store_int(from_string, &enum_int);
	*to = enum_int;
	return 0;
}

int store_trigger_type(char* from_string, enum trigger_type* to){
	int enum_int;
	store_int(from_string, &enum_int);
	*to = enum_int;
	return 0;
}




int store_value(char* val, uint32_t* index){
	
	switch (*index){
		case 0://auto_range_time
			store_int(val, &mcfg->im_cfg.auto_range_time);
			break;
		case 1://format
			store_format_type(val, &mcfg->im_cfg.format);
			break;
		case 2://apn
			store_string(val, &mcfg->ftp_cfg.apn);
			break;
		case 3://network_operator
			store_string(val, &mcfg->ftp_cfg.network_operator);
			break;
		case 4://domain
			store_string(val, &mcfg->ftp_cfg.domain);
			break;
		case 5://username
			store_string(val, &mcfg->ftp_cfg.username);
			break;
		case 6://cyphertype
			store_cypher_type(val, &mcfg->ftp_cfg.cyph_type);
			break;
		case 7://password
			store_string(val, &mcfg->ftp_cfg.password);
			const char password[128];
			const char* suffix = PW_SUFFIX;
			size_t pw_len;
			switch(mcfg->ftp_cfg.cyph_type){
				case NONE:
					break;
				case CAESAR:
					decrypt(mcfg->ftp_cfg.password, KEY); 
					break;
				case SUFFIX:
					strcpy(password, mcfg->ftp_cfg.password);
					k_free(mcfg->ftp_cfg.password);
					strcat(password, suffix);
					pw_len = strlen(password);
					mcfg->ftp_cfg.password = k_malloc(pw_len+1);
					memcpy(mcfg->ftp_cfg.password, password, pw_len);
					(mcfg->ftp_cfg.password)[pw_len] = 0;//null terminate string
					break;
			}
			break;
		case 8://image_path
			store_string(val, &mcfg->ftp_cfg.image_path);
			break;
		case 9: //status_path
			store_string(val, &mcfg->ftp_cfg.status_path);
			break;
		case 10://image_path
			store_string(val, &mcfg->sd_cfg.image_path);
			break;
		case 11://status_path
			store_string(val, &mcfg->sd_cfg.status_path);
			break;
		case 12://logging_level
			store_int(val, &mcfg->sd_cfg.logging_level);
			break;
		case 13://trig_type
			store_trigger_type(val, &mcfg->trig_cfg.trig_type);
			break;
		case 14://logging_interval
			store_int(val, &mcfg->trig_cfg.logging_interval);
			break;
		case 15://logging_decimation_ftp
			store_int(val, &mcfg->trig_cfg.logging_decimation_ftp);
			break;
	}
	
	
	(*index)++;
	return 0;
}

int parse_config_file(struct fs_file_t* zfp){
	char next;
	char value[256];
	uint32_t value_indx = 0;
	uint32_t config_index = 0;
	bool pre_comment = 0;
	bool string = 0;
	enum parse_state state = NAME;
	int ret;
	
	do{
		ret = fs_read(zfp, &next, 1);
		if(ret < 0){return ret;}
		
		if(next != '/'){pre_comment = 0;}
		
		switch (state){
		case NAME:
			if (next == '='){state = VALUE; value_indx = 0; string = 0;}
			if (next == '/'){
				if (pre_comment){
					state = COMMENT;
				} else {
					pre_comment = 1;
				}
			}
			break;
		case COMMENT:
			if (next == '\n'){state = NAME;}
			break;
		case VALUE:
			value[value_indx] = next;
			value_indx++;
			if(next == '"'){
				string = !string;
			}
			if (next == '\n'){store_value(value, &config_index); state = NAME;}
			if (next == '/' && !string){
				if (pre_comment){
					store_value(value, &config_index);
					state = COMMENT;
				} else {
					pre_comment = 1;
				}
			}
			break;
		}
	}while(ret > 0);
	
	return 0;
}
