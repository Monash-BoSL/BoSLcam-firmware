
#include <ff.h>

#include <inttypes.h>


#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "common.h"
#include "sd.h"
#include "util.h"

#ifdef CONFIG_DBG_SEND_IMAGE_RTT
    #include <SEGGER_RTT.h>
#endif

LOG_MODULE_REGISTER(sdhc);

static FATFS fat_fs;
/* mounting info */
static struct fs_mount_t mp = {
    .type = FS_FATFS,
    .mnt_point = DISK_MOUNT_PT,
    .fs_data = &fat_fs,
};

/*
*  Note the fatfs library is able to mount only strings inside _VOLUME_STRS
*  in ffconf.h
*/

int lsdir(const char *path)
{
    int ret = 0;
    struct fs_dir_t dirp;
    static struct fs_dirent entry;

    fs_dir_t_init(&dirp);

    /* Verify fs_opendir() */
    ret = fs_opendir(&dirp, path);
    if (ret) {
        LOG_ERR("Error opening dir %s [%d]\n", path, ret);
        return ret;
    }

    LOG_INF("\nListing dir %s ...\n", path);
    for (;;) {
        /* Verify fs_readdir() */
        ret = fs_readdir(&dirp, &entry);

        /* entry.name[0] == 0 means end-of-dir */
        if (ret || entry.name[0] == 0) {
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

    return ret;
}

int fs_mkdirs(const char* path) {
    int ret = 0;
    size_t pathlen = strlen(path)+1;
    if(pathlen > 256){return -ENAMETOOLONG;}//magic number of max path length

    char* current = k_malloc(pathlen);
    memset(current, '\0', pathlen);//null terminate

    char* pos = (char*) path;
    char* end = strchr(pos+1, '/');
    while(NULL != (end = strchr(pos+1, '/'))){
        strncpy(current+(pos-path), pos, end-pos);
        // printk("mkdir %s\n", current);

        struct fs_dirent dirstat;
        int res = fs_stat(current, &dirstat);
        if(res == -ENOENT && strcmp(current, DISK_MOUNT_PT)){
                fs_mkdir(current);
        }else{

        }
        pos = end;
    }

    k_free(current);
    return ret;//make sure that we return a nice error code here.
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

int sdhc_mount(void){
    int ret = fs_mount(&mp);
    if(ret < 0){LOG_ERR("Error mounting disk.\n"); return ret;}
    return ret;
}

int caesar_encrypt(char* msg, int key){
  char* chr_p = msg;
  while(*chr_p != '\0'){
    int chr = *chr_p;
    if(32 < chr && chr < 127){
      chr = (chr - 33 + key + (127-33))%(127-33) + 33;//the extra plus ensures that we do not get c weird modulo of negative nubmers
      *chr_p = (char)chr;
    }
    chr_p++;
  }
  return 1;
}
int caesar_decrypt(char* msg, int key){
  return caesar_encrypt(msg, -key);
}

int suffix_decrypt(char** password_p, const char* suffix){
    const size_t password_len = strlen(*password_p);
    const size_t suffix_len = strlen(suffix);
    const size_t decrypted_password_size = password_len + suffix_len + 1;

    char* decrypted_password = k_malloc(decrypted_password_size);
    if(decrypted_password == NULL){return -ENOMEM;}
    
    strcpy((char*) decrypted_password, *password_p);
    strcat((char*) decrypted_password, suffix);

    k_free(*password_p);
    *password_p = decrypted_password;

    return 0;
};

int decrypt_password(char** password_p, const enum cypher_t cypher){
    switch(cypher){
        case CYPHER_NONE:
            return 0;
            break;
        case CYPHER_CAESAR:
            return caesar_decrypt(*password_p, CAESAR_KEY);
            break;
        case CYPHER_SUFFIX:
            return suffix_decrypt(password_p, SUFFIX_KEY);
            break;
    }

    return -1;
} 

typedef struct yy_buffer_state * YY_BUFFER_STATE;
extern int yyparse(void);
extern YY_BUFFER_STATE yy_scan_bytes(char* str, size_t len);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
extern struct master_config_t* parser_config_handle;
int sdhc_load_config(const char* const sdhc_path, struct master_config_t* const master_cfg){
    int ret = -1;
    char path[MAX_PATH];
    struct fs_file_t imf;

    if(strlen(sdhc_path) > MAX_PATH + STRLEN(DISK_MOUNT_PT)){
        LOG_ERR("file name too long");
        return -ENAMETOOLONG; 
    }
    strcpy(path, DISK_MOUNT_PT);
    strcpy(path+STRLEN(DISK_MOUNT_PT), sdhc_path);

    fs_file_t_init(&imf);
    ret = fs_open(&imf, path, FS_O_READ);
    if(ret < 0){return ret;}

    char* config_str = k_malloc(YY_PARSE_BUFFER_SIZE);
    if(config_str == NULL){
        LOG_ERR("out of memory. cannot allocate config buffer");
        return -ENOMEM;
    }
    size_t config_str_len = fs_read(&imf, config_str, YY_PARSE_BUFFER_SIZE);
    if(config_str_len < 0){
        LOG_ERR("Cannot read config file");
        return config_str_len;
    }else if (config_str_len == YY_PARSE_BUFFER_SIZE) {
        LOG_ERR("Config file too large (> %dB)", YY_PARSE_BUFFER_SIZE);
        return -EFBIG;
    }

    fs_close(&imf);

    parser_config_handle = master_cfg;

    YY_BUFFER_STATE buffer = yy_scan_bytes(config_str, config_str_len);
    ret = yyparse();
    yy_delete_buffer(buffer);

    parser_config_handle = NULL;

    k_free(config_str);
    
    LOG_INF("config parsed with result %d", ret);

    decrypt_password(&master_cfg->ftp_cfg.password, master_cfg->ftp_cfg.cypher);

    return ret;
}


int sdhc_load_last_status_time(const char* const sdhc_path, struct tm* const cal){
    char path[MAX_PATH];
    struct fs_file_t imf;
    char strtime[80];
    int ret = 0;

    if(strlen(sdhc_path) > MAX_PATH + STRLEN(DISK_MOUNT_PT)){
        LOG_ERR("file name too long");
        return -ENAMETOOLONG;
    }
    strcpy(path, DISK_MOUNT_PT);
    strcat(path, sdhc_path);

    fs_file_t_init(&imf);
    ret = fs_open(&imf, path, FS_O_READ);
    if(ret < 0){goto cleanup;}
    {//get date string from file
        char next;
        bool date = 1;
        uint32_t i = 0;

        do{
            ret = fs_read(&imf, &next, 1);
            if(ret < 0){goto cleanup;}

            if(next == '\n'){date = 1; i = 0; continue;}
            if(next == ','){date = 0; strtime[i] = '\0'; continue;}
            if(date == 1){
                strtime[i] = next;
                i++;
                continue;
            }

        } while(ret > 0);
    }



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
cleanup:
    fs_close(&imf);

    return ret;
}


//ensure that your path beings with a / eg "/im1.bmp" !!
int sdhc_move_image(const char* const sdhc_path, struct capture_t* const capture){
    int ret = 0;
    char path[MAX_PATH];

    if(strlen(DISK_MOUNT_PT) + strlen(sdhc_path) + 12 + 1 > MAX_PATH){// 12 chars for timestamp and .bmp
        LOG_ERR("file name too long");
        return -ENAMETOOLONG;
    }
    sprintf(path, "%s%s%08X.bmp", DISK_MOUNT_PT,sdhc_path, capture->time_wall);

    fs_mkdirs(path);
    // ret = fs_rename(capture->fp, path);
    // if(ret < 0){return ret;}
    fs_rename(capture->fp, path); // i think the ret is broken!
    strcpy(capture->fp, path);

    return ret;
}

//ensure that your path beings with a / eg "/im1.bmp" !!
int sdhc_write_image(const char* const sdhc_path, const struct capture_t* const capture){
    char path[MAX_PATH];
    struct fs_file_t imf;


    if(strlen(DISK_MOUNT_PT) + strlen(sdhc_path) + 12 + 1 > MAX_PATH){// 12 chars for timestamp and .bmp
        LOG_ERR("file name too long");
        return -ENAMETOOLONG;
    }
    sprintf(path, "%s%s%08X.bmp", DISK_MOUNT_PT,sdhc_path, capture->time_wall);


    fs_file_t_init(&imf);
    fs_mkdirs(path);

    fs_open(&imf, path, FS_O_WRITE | FS_O_CREATE);
    fs_write(&imf, image_resolutions[capture->resolution].bmp_header, BMPIMAGEOFFSET);
    fs_write(&imf, capture->data, capture->size);
    fs_close(&imf);


    return 0;
}

//ensure that your path beings with a / eg "/im1.bmp" !!
int sdhc_write_status(const char* const sdhc_path, const struct status_t* const status){
    int ret = 0;
    char path[MAX_PATH];
    struct fs_file_t imf;
    struct tm cal;

    if(strlen(sdhc_path) > MAX_PATH + STRLEN(DISK_MOUNT_PT)){
        LOG_ERR("file name too long");
        return -ENAMETOOLONG;
    }
    strcpy(path, DISK_MOUNT_PT);
    strcat(path, sdhc_path);

    fs_file_t_init(&imf);

    fs_mkdirs(path);

    ret = fs_open(&imf, path, FS_O_WRITE | FS_O_CREATE | FS_O_APPEND);
    if(ret < 0){return ret;}

    //here is where we write what we want to log to file
    unix_date(&cal, status->time_wall);
    strftime(path, MAX_PATH, "%Y/%m/%d-%H:%M:%S UTC" , &cal);
    sprintf(path+strlen(path), ",%s,%d,%d,%s,%d,%d\n",
                                get_time_source_str(status->time_src),
                                status->captures,
                                status->battery_voltage,
                                status->mccmnc,
                                status->rsrq,
                                status->rsrp
                                );

    ret = fs_write(&imf, path, strlen(path));
    if(ret < 0){goto cleanup;}

cleanup:
    fs_close(&imf);

    return ret;
}



#ifdef CONFIG_DBG_SEND_IMAGE_RTT

#define RTT_BUFFER_UP_SIZE (0x1000)
#define RTT_BUFFER_DOWN_SIZE (0x08)
int _rtt_image_upbuf = -1;
int _rtt_image_downbuf = -1;

int get_rtt_up_image(void){
    if(_rtt_image_upbuf < 0){
        void* rtt_image_up_buffer = k_malloc(RTT_BUFFER_UP_SIZE);
        _rtt_image_upbuf = SEGGER_RTT_AllocUpBuffer("image_data", rtt_image_up_buffer, RTT_BUFFER_UP_SIZE, SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
    }
    return _rtt_image_upbuf;
}

int get_rtt_down_image(void){
    if(_rtt_image_downbuf < 0){
        void* rtt_image_down_buffer = k_malloc(RTT_BUFFER_DOWN_SIZE);
        _rtt_image_downbuf = SEGGER_RTT_AllocDownBuffer("image_data", rtt_image_down_buffer, RTT_BUFFER_DOWN_SIZE, SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
    }
    return _rtt_image_downbuf;
}

int clear_rtt_image(void){
    LOG_ERR("cannot dealloc rtt_image");
    k_oops();
    // if(_rtt_image_upbuf < 0){
    // 	return 0;
    // }else{
    // 	k_malloc(RTT_BUFFER_UP_SIZE)
    // }
    // return _rtt_image_upbuf;
}


int sdhc_file_to_rtt(const char* const sdhc_path){
    int ret = 0;
    char path[MAX_PATH];
    struct fs_file_t imf;

    void* rtt_image_buffer = k_malloc(RTT_BUFFER_UP_SIZE);
    void* rtt_image_ack_buffer = k_malloc(RTT_BUFFER_DOWN_SIZE);

    int rtt_up_image = get_rtt_up_image();
    int rtt_down_image = get_rtt_down_image();

    LOG_INF("sending over rtt: %s", sdhc_path);
    LOG_INF("wating for rtt connection...");
    while(log_process(false));


    if(strlen(sdhc_path) > MAX_PATH + STRLEN(DISK_MOUNT_PT)){
        LOG_ERR("file name too long");
        ret = -ENAMETOOLONG;
        goto cleanup;
    }
    strcpy(path, DISK_MOUNT_PT);
    strcat(path, sdhc_path);


    fs_file_t_init(&imf);

    ret = fs_open(&imf, path, FS_O_READ);
    if(ret < 0){goto cleanup;}


    ssize_t bytes_read;
    while((bytes_read = fs_read(&imf, rtt_image_buffer, RTT_BUFFER_UP_SIZE)) > 0){
        memset(rtt_image_ack_buffer, 0, RTT_BUFFER_DOWN_SIZE);
        while(!SEGGER_RTT_HasData(rtt_down_image));
        SEGGER_RTT_Read(rtt_down_image, rtt_image_ack_buffer, RTT_BUFFER_DOWN_SIZE); //read out data


        SEGGER_RTT_WriteNoLock(rtt_up_image, &bytes_read, sizeof(bytes_read));
        SEGGER_RTT_WriteNoLock(rtt_up_image, rtt_image_buffer, bytes_read);
    }
    if(bytes_read < 0){goto cleanup;}
    LOG_INF("file sent!");



cleanup:
    k_free(rtt_image_buffer);
    k_free(rtt_image_ack_buffer);
    fs_close(&imf);


    return ret;
}
#endif
