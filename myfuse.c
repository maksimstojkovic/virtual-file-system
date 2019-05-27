/* Do not change! */
#define FUSE_USE_VERSION 29
#define _FILE_OFFSET_BITS 64
/******************/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fuse.h>
#include <errno.h>

#include "structs.h"
#include "helper.h"
#include "myfilesystem.h"

char * file_data_file_name = NULL;
char * directory_table_file_name = NULL;
char * hash_data_file_name = NULL;

int myfuse_getattr(const char * path, struct stat * result) {
	// Exit if filesystem does not exist
	if (fuse_get_context()->private_data == NULL) {
	    perror("myfuse_getattr: Filesystem does not exist");
	    exit(1);
	}

    // Check for valid path
    if (strncmp(path, "/", 1) != 0) {
        return -ENOTDIR;
    }

	memset(result, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
        // Mode is a directory
	    result->st_mode = S_IFDIR;

        printf("%s directory\n", path);

		// Set size as the number of bytes used in the filesystem, or 0 if filesystem is NULL
		if ((fuse_get_context()->private_data) == NULL) {
            result->st_size = 0;
		} else {
            result->st_size = ((filesys_t*)(fuse_get_context()->private_data))->used;
		}
	} else {
		// Mode is a regular file
	    result->st_mode = S_IFREG;

        printf("%s!!!!!!!!\n", path);

        // Set size as the number of bytes used by the file, or 0 if filesystem is NULL
        if ((fuse_get_context()->private_data) == NULL) {
            result->st_size = 0;
        } else {
            char* name = salloc(strlen(path));
            memcpy(name, path + 1, strlen(path));

            ssize_t size = file_size(name, fuse_get_context()->private_data);
            free(name);

            // Check if file exists
            if (size < 0) {
                return -EBADF;
            }

            result->st_size = (off_t)size;
        }


	}

	return 0;
}

int myfuse_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info * fi) {
	// MODIFY THIS FUNCTION
	if (strcmp(path, "/") == 0) {
		filler(buf, "test_file", NULL, 0);
	}
	return 0;
}

int myfuse_unlink(const char * path) {
	char* name = salloc(strlen(path) + 1);
	memcpy(name, path, strlen(path) + 1);

	int ret = delete_file(name, fuse_get_context()->private_data);

	free(name);
	return ret;
}

int myfuse_rename(const char * oldname, const char * newname) {
	char* old = salloc(strlen(oldname) + 1);
	char* new = salloc(strlen(newname) + 1);
	memcpy(old, oldname, strlen(oldname) + 1);
	memcpy(new, newname, strlen(newname) + 1);

	int ret = rename_file(old, new, fuse_get_context()->private_data);

	free(old);
	free(new);
	return ret;
}

int myfuse_truncate(const char *, off_t);
	// FILL OUT

int myfuse_open(const char *, struct fuse_file_info *);
	// FILL OUT

int myfuse_read(const char *, char *, size_t, off_t, struct fuse_file_info *);
	// FILL OUT

int myfuse_write(const char *, const char *, size_t, off_t, struct fuse_file_info *);
	// FILL OUT

int myfuse_release(const char *, struct fuse_file_info *);
	// FILL OUT

void * myfuse_init(struct fuse_conn_info * info) {
	// Check for valid arguments
	if (file_data_file_name == NULL || directory_table_file_name == NULL || hash_data_file_name) {
		return NULL;
	}

	return init_fs(file_data_file_name, directory_table_file_name, hash_data_file_name, 1);
}

void myfuse_destroy(void * fs) {
	close_fs(fs);
}

int myfuse_create(const char *, mode_t, struct fuse_file_info *);

struct fuse_operations operations = {
	.getattr = myfuse_getattr,
	.readdir = myfuse_readdir,
	.unlink = myfuse_unlink,
	.rename = myfuse_rename,
//    .truncate =
//    .open =
//    .read =
//    .write =
//    .release =
	.init = myfuse_init,
	.destroy = close_fs,
//    .create =
};

int main(int argc, char * argv[]) {
	// MODIFY (OPTIONAL)
	if (argc >= 5) {
		if (strcmp(argv[argc-4], "--files") == 0) {
			file_data_file_name = argv[argc-3];
			directory_table_file_name = argv[argc-2];
			hash_data_file_name = argv[argc-1];
			argc -= 4;
		}
	}

	// After this point, you have access to file_data_file_name, directory_table_file_name and hash_data_file_name
	int ret = fuse_main(argc, argv, &operations, NULL);
	return ret;
}
