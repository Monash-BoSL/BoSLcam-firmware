
#include <zephyr.h>
#include <device.h>

#include <storage/disk_access.h>
#include <fs/fs.h>
#include <ff.h>

#include <sys/util.h>
#include <sys/printk.h>
#include <inttypes.h>
#include <logging/log.h>

#include "config.h"
#include "sd.h"

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


static int lsdir(const char *path)
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
			LOG_INF("[DIR ] %s\n", entry.name);
		} else {
			LOG_INF("[FILE] %s (size = %zu)\n",
				entry.name, entry.size);
		}
	}

	/* Verify fs_closedir() */
	fs_closedir(&dirp);

	return res;
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

//ensure that your path beings with a / eg "/im1.bmp" !!
int sdhc_write_image(char* sdhc_path, char* data, uint32_t length){
	const uint32_t max_path_length = 256;
	char path[max_path_length];
	int res;
	struct fs_file_t imf;
	
	if(strlen(sdhc_path) > max_path_length + sizeof(disk_mount_pt)-1){
		LOG_ERR("file name too long");
		return -ENAMETOOLONG;
	}
	
	strcpy(path, disk_mount_pt);
	strcpy(path+sizeof(disk_mount_pt), sdhc_path);
	
	mp.mnt_point = disk_mount_pt;

	res = fs_mount(&mp);

	if (res == FR_OK) {
		LOG_INF("Disk mounted.\n");
		// lsdir(disk_mount_pt);
	} else {
		LOG_ERR("Error mounting disk.\n");
	}

	
	fs_file_t_init(&imf);
	fs_open(&imf, path, FS_O_WRITE | FS_O_CREATE);
	fs_write(&imf, bmp_header, BMPIMAGEOFFSET);
	fs_write(&imf, data, length);
	fs_close(&imf);

	return 0;
}

