
#include <errno.h>
#include "common.h"

#define NDEBUG
#define TJE_IMPLEMENTATION
#include "tjpg.h"

#include "sd.h"
#include "jpg.h"


LOG_MODULE_REGISTER(jpg);



int buffer_closure_from_sram(struct buffer_closure* bc, void* data){
    return bc->lines;
};

int buffer_closure_from_file(struct buffer_closure* bc, void* data){
    int read_lines = 8*(bc->buffer_size_lines/8);
    int read_bytes = read_lines*bc->line_size_bytes;
    int ret = fs_read(bc->ibfp, data, read_bytes); 
    return ret/bc->line_size_bytes;
};

void tje_write(void *zfp, void *ptr, int size)
{
    fs_write(zfp, ptr, size);
}

//ensure that your path beings with a / eg "/im1.bmp" !!
//overwrites the image buffer in ram with the jpg	  !!
int sdhc_write_jpg(char* sdhc_path, struct capture_t* capture){
    int ret = 0;
    char path[MAX_PATH];
    struct fs_file_t jpgf;
    struct fs_file_t ibf;

    if(STRLEN(DISK_MOUNT_PT) + strlen(sdhc_path) + 8 + 4 + 1 > MAX_PATH ){// 8 for timestamp; 4 for .jpg
        LOG_ERR("file name too long");
        return -ENAMETOOLONG;
    }
    sprintf(path, "%s%s%08X.jpg", DISK_MOUNT_PT,sdhc_path, capture->time);


    fs_file_t_init(&jpgf);
    fs_mkdirs(path);
    fs_open(&jpgf, path, FS_O_WRITE | FS_O_CREATE);

    fs_file_t_init(&ibf);
    ret = fs_open(&ibf, capture->fp, FS_O_READ);
    ret = fs_seek(&ibf, BMPIMAGEOFFSET, FS_SEEK_SET);

    const struct buffer_closure bc = {
        .lines = image_resolutions[capture->resolution].height,
        .line_size_bytes = (RBG565_PIXEL_SIZE_BYTES*image_resolutions[capture->resolution].width),
        .buffer_size_lines = capture->capacity/(RBG565_PIXEL_SIZE_BYTES*image_resolutions[capture->resolution].width),
        .src_data = capture->data,
        .ibfp = &ibf,
        .fill = buffer_closure_from_file,
    };

    ret = tje_encode_with_func(tje_write,
                        &jpgf,
                        2,//make quality a config parameter
                        image_resolutions[capture->resolution].width,
                        image_resolutions[capture->resolution].height,
                        TJE_RGB565,
                        (struct buffer_closure*)&bc
                        );


    fs_close(&jpgf);
    fs_close(&ibf);

    if(ret == 0){
        fs_unlink(path);
        return EINVAL;
    }

    // //overwrite capture with jpg data
    // ret = fs_open(&jpgf, path, FS_O_READ);
    // ret = fs_read(&jpgf, capture->data, capture->capacity);
    // if(ret > 0){
    //     capture->size = ret;//store the new file size the capture length
    // }
    // fs_close(&jpgf);
    strcpy(capture->fp,path);
    capture->format=JPG;
    capture->where=DATA_LOCATION_DISK;


    return (0 > ret ? ret : 0);
}