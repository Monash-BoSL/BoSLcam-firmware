
#include <zephyr.h>
#include <device.h>

#include <storage/disk_access.h>
#include <fs/fs.h>
#include <ff.h>

#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>
#include <logging/log.h>

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <sys/timeutil.h>

#include "common.h"
#include "sd.h"
#include "util.h"

LOG_MODULE_REGISTER(sd);

static FATFS fat_fs;
/* mounting info */
static struct fs_mount_t mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
};

/*
*  Note the fatfs library is able to mount only strings inside _VOLUME_STRS
*  in ffconf.h
*/
static const char* disk_mount_pt = "/SD:";

static struct master_config_t* mcfg;

int lsdir(const char *path)
{
	int res;
	struct fs_dir_t dirp;
	static struct fs_dirent entry;

	fs_dir_t_init(&dirp);

	/* Verify fs_opendir() */
	res = fs_opendir(&dirp, path);
	if (res) {
		LOG_ERR("Error opening dir %s [%d]\n", path, res);
		return res;
	}

	LOG_INF("\nListing dir %s ...\n", path);
	for (;;) {
		/* Verify fs_readdir() */
		res = fs_readdir(&dirp, &entry);

		/* entry.name[0] == 0 means end-of-dir */
		if (res || entry.name[0] == 0) {
			break;
		}

		if (entry.type == FS_DIR_ENTRY_DIR) {
			printk("[DIR ] %s\n", entry.name);
		} else {
			printk("[FILE] %s (size = %zu)\n",
				entry.name, entry.size);
		}
	}

	/* Verify fs_closedir() */
	fs_closedir(&dirp);

	return res;
}

int fs_mkdirs(const char* path) {
	int res;
	size_t pathlen = strlen(path)+1;
	if(pathlen > 256){return -ENAMETOOLONG;}//magic number of max path length
	
	char* current = k_malloc(pathlen);
	memset(current, '\0', pathlen);//null terminate
	
	char* pos = path;
	char* end = strchr(pos+1, '/');
	while(NULL != (end = strchr(pos+1, '/'))){
		strncpy(current+(pos-path), pos, end-pos);
		// printk("mkdir %s\n", current);
		
		struct fs_dirent dirstat;
		int res = fs_stat(current, &dirstat);
		if(res == -ENOENT && strcmp(current, disk_mount_pt)){
				fs_mkdir(current);
		}else{
			
		}
		pos = end;
	}
 
    k_free(current);
	return res;//make sure that we return a nice error code here. 
}

void sdhc_info(void){
		/* raw disk i/o */
	do {
		static const char *disk_pdrv = "SD";
		uint64_t memory_size_mb;
		uint32_t block_count;
		uint32_t block_size;

		if (disk_access_init(disk_pdrv) != 0) {
			LOG_ERR("Storage init ERROR!");
			break;
		}

		if (disk_access_ioctl(disk_pdrv,
				DISK_IOCTL_GET_SECTOR_COUNT, &block_count)) {
			LOG_ERR("Unable to get sector count");
			break;
		}
		LOG_INF("Block count %u", block_count);

		if (disk_access_ioctl(disk_pdrv,
				DISK_IOCTL_GET_SECTOR_SIZE, &block_size)) {
			LOG_ERR("Unable to get sector size");
			break;
		}
		LOG_INF("Sector size %u\n", block_size);

		memory_size_mb = (uint64_t)block_count * block_size;
		LOG_INF("Memory Size(MB) %u\n", (uint32_t)memory_size_mb>>20);
	} while (0);
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

int sdhc_mount(void){
	int res;
	mp.mnt_point = disk_mount_pt;

	res = fs_mount(&mp);

	if (res == FR_OK) {
		LOG_INF("Disk mounted.\n");
		return 0;
	} else {
		LOG_ERR("Error mounting disk.\n");
		return -1;
	}
}

int sdhc_load_config(char* sdhc_path, struct master_config_t* master_cfg){
	const uint32_t max_path_length = 256;
	char path[max_path_length];
	struct fs_file_t imf;
	mcfg = master_cfg;
	
	if(strlen(sdhc_path) > max_path_length + sizeof(disk_mount_pt)-1){
		LOG_ERR("file name too long");
		return -ENAMETOOLONG;
	}
	strcpy(path, disk_mount_pt);
	strcpy(path+sizeof(disk_mount_pt), sdhc_path);
	
	fs_file_t_init(&imf);
	fs_open(&imf, path, FS_O_READ);

	parse_config_file(&imf);

	fs_close(&imf);

	return 0;
}


int sdhc_load_last_status_time(char* sdhc_path, struct tm* cal){
	const uint32_t max_path_length = 256;
	char path[max_path_length];
	struct fs_file_t imf;
	char strtime[80];
	int ret;
	
	if(strlen(sdhc_path) > max_path_length + sizeof(disk_mount_pt)-1){
		LOG_ERR("file name too long");
		return -ENAMETOOLONG;
	}
	strcpy(path, disk_mount_pt);
	strcat(path, sdhc_path);
	
	fs_file_t_init(&imf);
	ret = fs_open(&imf, path, FS_O_READ);
	if(ret < 0){return ret;}
	{//get date string from file
		char next;
		bool date = 1;
		uint32_t i = 0;
		
		do{
			ret = fs_read(&imf, &next, 1);
			if(ret < 0){return ret;}
		
			if(next == '\n'){date = 1; i = 0; continue;}
			if(next == ','){date = 0; strtime[i] = '\0'; continue;}
			if(date == 1){
				strtime[i] = next;
				i++;
				continue;
			}
		
		}while(ret > 0);
	}
	fs_close(&imf);
	
	
	ret = sscanf(strtime, "%d/%d/%d-%d:%d:%d", 	
												&cal->tm_year,
												&cal->tm_mon, 
												&cal->tm_mday, 
												&cal->tm_hour, 
												&cal->tm_min, 
												&cal->tm_sec);
	cal->tm_year -= 1900;
	cal->tm_mon -= 1;
	cal->tm_wday = 0;
	cal->tm_yday = 0;
	cal->tm_isdst = 0;
	
	// *ct = timeutil_timegm64(&cal);

	return 0;
}


//ensure that your path beings with a / eg "/im1.bmp" !!
int sdhc_write_image(char* sdhc_path, struct capture_t* capture){
	const uint32_t max_path_length = 256;
	char path[max_path_length];
	struct fs_file_t imf;


	if(strlen(sdhc_path) > max_path_length + sizeof(disk_mount_pt)-1){
		LOG_ERR("file name too long");
		return -ENAMETOOLONG;
	}
	sprintf(path, "%s%s%08X.bmp", disk_mount_pt,sdhc_path, capture->time);

	
	fs_file_t_init(&imf);
	fs_mkdirs(path);

	fs_open(&imf, path, FS_O_WRITE | FS_O_CREATE);
	fs_write(&imf, bmp_header, BMPIMAGEOFFSET);
	fs_write(&imf, capture->data, capture->length);
	fs_close(&imf);


	return 0;
}

//ensure that your path beings with a / eg "/im1.bmp" !!
int sdhc_write_status(char* sdhc_path, struct status_t* status){
	const uint32_t max_path_length = 256;
	char path[max_path_length];
	struct fs_file_t imf;
	struct tm cal;
	
	
	if(strlen(sdhc_path) > max_path_length + sizeof(disk_mount_pt)-1){
		LOG_ERR("file name too long");
		return -ENAMETOOLONG;
	}
	strcpy(path, disk_mount_pt);
	strcat(path, sdhc_path);
	
	fs_file_t_init(&imf);
    fs_mkdirs(path);
	fs_open(&imf, path, FS_O_WRITE | FS_O_CREATE | FS_O_APPEND);
	//here is where we write what we want to log to file
	unix_date(&cal, status->system_time);
	strftime(path, max_path_length, "%Y/%m/%d-%H:%M:%S UTC" , &cal);
	sprintf(path+strlen(path), ",%s,%d,%d\n", 
								time_source_str[status->time_src],
								status->captures, 
								status->battery_voltage);
	
	fs_write(&imf, path, strlen(path));
	fs_close(&imf);

	return 0;
}
