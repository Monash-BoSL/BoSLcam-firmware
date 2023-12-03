
#include <logging/log.h>

#include <errno.h>
#include <fs/fs.h>

#define NDEBUG
#define TJE_IMPLEMENTATION
#include "tjpg.h"

#include "common.h"
#include "sd.h"
#include "jpg.h"


LOG_MODULE_REGISTER(jpg);

//ensure that your path beings with a / eg "/im1.bmp" !!
//overwrites the image buffer in ram with the jpg	  !!
int sdhc_write_jpg(char* sdhc_path, struct capture_t* capture){
	int ret;
	char path[MAX_PATH];
	struct fs_file_t imf;
	
	if(strlen(sdhc_path) > MAX_PATH + STRLEN(DISK_MOUNT_PT)){
		LOG_ERR("file name too long");
		return -ENAMETOOLONG;
	}
	sprintf(path, "%s%s%08X.jpg", DISK_MOUNT_PT,sdhc_path, capture->time);
	
	
	fs_file_t_init(&imf);
	fs_mkdirs(path);
	fs_open(&imf, path, FS_O_WRITE | FS_O_CREATE);

    // typedef void tje_write_func(void* context, void* data, int size);
    ret = tje_encode_with_func(fs_write,
                        &imf,
                        2,//make quality a config parameter
                        image_resolutions[capture->resolution].width,
                        image_resolutions[capture->resolution].height,
                        TJE_RGB565,
                        capture->data);


	fs_close(&imf);

	if(ret == 0){
		fs_unlink(path);
		return EINVAL;
	}

	//overwrite capture with jpg data
	ret = fs_open(&imf, path, FS_O_READ);
	ret = fs_read(&imf, capture->data, capture->size);
	if(ret > 0){
		capture->size = ret;//store the new file size the capture length
	}
	fs_close(&imf);


	return (0 > ret ? ret : 0);
}