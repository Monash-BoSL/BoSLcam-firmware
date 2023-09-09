
#include <logging/log.h>

#include <errno.h>
#include <fs/fs.h>

#define NDEBUG
#define TJE_IMPLEMENTATION
#include "tjpg.h"

#include "common.h"
#include "sd.h"
#include "jpg.h"

static const char* disk_mount_pt = "/SD:";

LOG_MODULE_REGISTER(jpg);

//ensure that your path beings with a / eg "/im1.bmp" !!
int sdhc_write_jpg(char* sdhc_path, struct capture_t* capture){
	const uint32_t max_path_length = 256;
	char path[max_path_length];
	struct fs_file_t imf;
	
	if(strlen(sdhc_path) > max_path_length + sizeof(disk_mount_pt)-1){
		LOG_ERR("file name too long");
		return -ENAMETOOLONG;
	}
	sprintf(path, "%s%s%08X.jpg", disk_mount_pt,sdhc_path, capture->time);
	
	
	fs_file_t_init(&imf);
	fs_mkdirs(path);
	fs_open(&imf, path, FS_O_WRITE | FS_O_CREATE);

    // typedef void tje_write_func(void* context, void* data, int size);
    tje_encode_with_func(fs_write,
                        &imf,
                        1,//make quality a config parameter
                        IMAGE_WIDTH,
                        IMAGE_HEIGHT,
                        1,//RGB
                        capture->data);

    // fs_write(&imf, bmp_header, BMPIMAGEOFFSET);
	// fs_write(&imf, capture->data, capture->length);
	fs_close(&imf);

	return 0;
}