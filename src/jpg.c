
#include <errno.h>
#include "common.h"

#define NDEBUG
#define TJE_IMPLEMENTATION
#include "tjpg.h"

#include "sd.h"
#include "jpg.h"
#include "tmalloc.h"


LOG_MODULE_REGISTER(jpg);


int buffer_closure_from_file(struct buffer_closure* bc, void* data){
    int read_lines = 8*(bc->buffer_size_lines/8);
    int read_bytes = read_lines*bc->line_size_bytes;
    int ret = fs_read(bc->ibfp, data, read_bytes); 
    return ret/bc->line_size_bytes;
};

void tje_write(void *zfp, void *ptr, int size) {
    fs_write(zfp, ptr, size);
}

//ensure that your path beings with a / eg "/im1.bmp" !!
//overwrites the image buffer in ram with the jpg     !!
int sdhc_write_jpg(const char* const sdhc_path, struct capture_t* const capture){
    int ret = 0;
    char path[MAX_PATH];
    struct fs_file_t jpgf;
    struct fs_file_t ibf;

    if(STRLEN(DISK_MOUNT_PT) + strlen(sdhc_path) + 8 + 4 + 1 > MAX_PATH ){// 8 for timestamp; 4 for .jpg
        LOG_ERR("file name too long");
        return -ENAMETOOLONG;
    }
    sprintf(path, "%s%s%08X.jpg", DISK_MOUNT_PT,sdhc_path, capture->time_wall);


    fs_file_t_init(&jpgf);
    fs_mkdirs(path);
    fs_open(&jpgf, path, FS_O_WRITE | FS_O_CREATE);

    fs_file_t_init(&ibf);
    ret = fs_open(&ibf, capture->fp, FS_O_READ);
    ret = fs_seek(&ibf, BMPIMAGEOFFSET, FS_SEEK_SET);

    /* jpg encoding uses a lot of ram, we can only afford to use half of the buffer */
    struct buffer_closure bc = {
        .lines = image_resolutions[capture->resolution].height,
        .line_size_bytes = (RBG565_PIXEL_SIZE_BYTES*image_resolutions[capture->resolution].width),
        .buffer_size_lines = (capture->capacity/2)/(RBG565_PIXEL_SIZE_BYTES*image_resolutions[capture->resolution].width), /* /2 because we free the other half of the buffer for a jpg stack */
        .src_data = capture->data,
        .ibfp = &ibf,
        .fill = buffer_closure_from_file,
    };
    /* the high half of the capture buffer we use for jpg encoding scratch space
     * variables created in this scratch space with the TNEW macro exist for the
     * lifetime of the tmalloc variable declared hear. Note however that no
     * compiler warning will be thrown if you try to access these variables
     * after tmalloc is deleted. We handle this here by only making all the 
     * variables with TNEW local within the tje_encode_with_func function
     * and its subfunctions therefore none exist after this function returns 
     * which is shorter than the lifetime of tmalloc here.
     * Likewise we are not checking for NULLPTR return of TNEW in this code and 
     * so no error will be thrown if tmalloc runs out of space. However for this
     * jpeg encoding this is unlikely as it only requires ~ 10 kB of memory and 
     * the workspace length is 76 kB.
     */
    tmalloc_t tmalloc;
    char* const workspace = &(capture->data[capture->capacity/2]);
    const size_t workspace_len = capture->capacity/2;
    t_init(&tmalloc, workspace, workspace_len);

    ret = tje_encode_with_func(tje_write,
                        &jpgf,
                        2,//make quality a config parameter
                        image_resolutions[capture->resolution].width,
                        image_resolutions[capture->resolution].height,
                        TJE_RGB565,
                        (struct buffer_closure*)&bc,
                        &tmalloc
                        );


    fs_close(&jpgf);
    fs_close(&ibf);

    if(ret == 0){
        fs_unlink(path);
        return EINVAL;
    }

    strcpy(capture->fp,path);
    capture->format=JPG;
    capture->where=DATA_LOCATION_DISK;


    return (0 > ret ? ret : 0);
}