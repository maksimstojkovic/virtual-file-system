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

// Macro for casting filesystem struct
#define FILESYSTEM ((filesys_t*)(fuse_get_context()->private_data))

char * file_data_file_name = NULL;
char * directory_table_file_name = NULL;
char * hash_data_file_name = NULL;

void filesystem_exists() {
	if (FILESYSTEM == NULL) {
		perror("filesystem_exists: Filesystem does not exist");
		exit(1);
	}
}

int myfuse_getattr(const char * path, struct stat * result) {
	// Check if filesystem exists
	filesystem_exists();

    // Check for valid path
    if (strncmp(path, "/", 1) != 0) {
        return -EINVAL;
    }

	memset(result, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		// Set size as the number of bytes used in the filesystem
        result->st_size = FILESYSTEM->used;

        // Mode is a directory
        result->st_mode = S_IFDIR;
	} else {
        // Set size as the number of bytes used by the file
        char* name = salloc(strlen(path));
        memcpy(name, path + 1, strlen(path));

        ssize_t size = file_size(name, FILESYSTEM);
        free(name);

        // Check if file does not exist
        if (size < 0) {
            return -ENOENT;
        }

        result->st_size = (off_t)size;

        // Mode is a regular file
        result->st_mode = S_IFREG;
	}

	return 0;
}

int myfuse_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info * fi) {
	UNUSED(offset);
	UNUSED(fi);

	// Check if filesystem exists
    filesystem_exists();

	if (strcmp(path, "/") == 0) {
		// List filesystem files in alphabetical order using sorted name array
	    file_t** n_list = FILESYSTEM->n_list->list;
	    int32_t size = FILESYSTEM->n_list->size;

		for (int i = 0; i < size; ++i) {
            filler(buf, n_list[i]->name, NULL, 0);
		}
	} else {
	    // Path is not a directory
	    return -ENOTDIR;
	}

	return 0;
}

int myfuse_unlink(const char * path) {
	// Check if filesystem exists
	filesystem_exists();

	// Check for valid path
    if (path == NULL) {
        return -EINVAL;
    }

	// Check if path is a directory
	if (strcmp(path, "/") == 0) {
		return -EISDIR;
	}

	char* name = salloc(strlen(path));
	memcpy(name, path + 1, strlen(path));

	int ret = delete_file(name, FILESYSTEM);
    free(name);

    // Check if file exists
    if (ret == 1) {
        return -ENOENT;
    }

	return 0;
}

int myfuse_rename(const char * oldname, const char * newname) {
	// Check if filesystem exists
	filesystem_exists();

	// Check for valid paths
    if (oldname == NULL || newname == NULL) {
        return -EINVAL;
    }
    
    // Check if paths are directories
    if (strcmp(oldname, "/") == 0 || strcmp(newname, "/") == 0) {
    	return -EISDIR;
    }
    
    char* old = salloc(strlen(oldname));
	char* new = salloc(strlen(newname));
	memcpy(old, oldname + 1, strlen(oldname));
	memcpy(new, newname + 1, strlen(newname));

	int ret = rename_file(old, new, FILESYSTEM);
	free(old);
	free(new);
	
	// Check if oldname file does not exist or newname file already exists
	if (ret == 1) {
		return -ENOENT;
	}
	
	return 0;
}

int myfuse_truncate(const char * path, off_t length) {
	// Check if filesystem exists
	filesystem_exists();

	// Check for valid path
	if (path == NULL) {
		return -EINVAL;
	}

	// Check if path is a directory
	if (strcmp(path, "/") == 0) {
		return -EISDIR;
	}
	
	char* name = salloc(strlen(path));
	memcpy(name, path + 1, strlen(path));
	
	int ret = resize_file(name, length, FILESYSTEM);
	free(name);
	
	// Check if file exists
	if (ret == 1) {
		return -ENOENT;
	}
	
	// Check if sufficient space in filesystem
	if (ret == 2) {
		return -ENOSPC;
	}
	
	return 0;
}

int myfuse_open(const char * path, struct fuse_file_info * fi) {
	UNUSED(fi);

	// Check if filesystem exists
	filesystem_exists();

	// Check for valid path
	if (path == NULL) {
		return -EINVAL;
	}

	// Check if path is a directory
	if (strcmp(path, "/") == 0) {
		return -EISDIR;
	}

	// For successful open, file should exist in filesystem
	char* name = salloc(strlen(path));
	memcpy(name, path + 1, strlen(path));
	
	ssize_t size = file_size(name, FILESYSTEM);
	free(name);

	// Check if file exists
	if (size < 0) {
		return -ENOENT;
	}
	
	return 0;
}

int myfuse_read(const char * path, char * buf, size_t length, off_t offset, struct fuse_file_info * fi) {
	UNUSED(fi);
	
	// Check if filesystem exists
	filesystem_exists();
	
	// Check for valid arguments
	if (path == NULL || buf == NULL) {
		return -EINVAL;
	}

	// Check if path is a directory
	if (strcmp(path, "/") == 0) {
		return -EISDIR;
	}
	
	// Get current length of file
	char* name = salloc(strlen(path));
	memcpy(name, path + 1, strlen(path));
	
	ssize_t f_size = file_size(name, fuse_get_context()->private_data);
	
	// Check if file exists
	if (f_size < 0) {
		free(name);
		return -ENOENT;
	}
	
	// Check for EOF
	if (offset >= f_size) {
		free(name);
		return 0;
	}
	
	// Update number of bytes to read based on length of file
	size_t read_length;
	if (length > f_size - offset) {
		read_length = f_size - offset;
	} else {
		read_length = length;
	}
	
	int ret = read_file(name, offset, read_length, buf, FILESYSTEM);
	free(name);
	
	// Check for valid filesystem hashing
	if (ret == 3) {
		return -EIO;
	}
	
	return read_length;
}

int myfuse_write(const char * path, const char * buf, size_t length, off_t offset, struct fuse_file_info * fi) {
	UNUSED(fi);

	// Check if filesystem exists
	filesystem_exists();
	
	// Check for valid arguments
	if (path == NULL || buf == NULL) {
		return -EINVAL;
	}

	// Check if path is a directory
	if (strcmp(path, "/") == 0) {
		return -EISDIR;
	}

	// Get current length of file
	char* name = salloc(strlen(path));
	memcpy(name, path + 1, strlen(path));

	ssize_t f_size = file_size(name, FILESYSTEM);

	// Check if file exists
	if (f_size < 0) {
		free(name);
		return -ENOENT;
	}

	// Check for valid offset
	if (offset > f_size) {
		free(name);
		return -EINVAL;
	}

	// Update number of bytes to write based on filesystem space
	size_t write_length;
	if ((ssize_t)offset + length - f_size > FILESYSTEM->len[0] - FILESYSTEM->used) {
		write_length = FILESYSTEM->len[0] - FILESYSTEM->used;
	} else {
		write_length = length;
	}

	char* temp = salloc(write_length);
	memcpy(temp, buf, write_length);
	
	write_file(name, offset, write_length, temp, FILESYSTEM);
	free(name);
	free(temp);
	
	return write_length;
}

int myfuse_release(const char * path, struct fuse_file_info * fi) {
	// Does not alter filesystem
	// No checks requires as release is only called after create or open
	return 0;
}

void * myfuse_init(struct fuse_conn_info * info) {
    // Check for valid filesystem filenames
	if (file_data_file_name == NULL || directory_table_file_name == NULL || hash_data_file_name == NULL) {
	    return NULL;
	}

	return init_fs(file_data_file_name, directory_table_file_name, hash_data_file_name, 1);
}

void myfuse_destroy(void * fs) {
    // Check for valid argument
    if (fs == NULL) {
        return;
    }

	close_fs(fs);
}

int myfuse_create(const char * path, mode_t mode, struct fuse_file_info * fi) {
	UNUSED(mode);
	UNUSED(fi);

	// Check if filesystem exists
	filesystem_exists();

	// Check for valid path
	if (path == NULL) {
		return -EINVAL;
	}

	// Check if path is a directory
	if (strcmp(path, "/") == 0) {
		return -EISDIR;
	}
	
	char* name = salloc(strlen(path));
	memcpy(name, path + 1, strlen(path));
	
	int ret = create_file(name, 0, FILESYSTEM);
	free(name);
	
	// Check if filename already exists
	if (ret == 1) {
		return -EEXIST;
	}

	// Check for sufficient filesystem space (dir_table)
	if (ret == 2) {
		return -ENOSPC;
	}
	
	return 0;
}

struct fuse_operations operations = {
	.getattr = myfuse_getattr,
	.readdir = myfuse_readdir,
	.unlink = myfuse_unlink,
	.rename = myfuse_rename,
    .truncate = myfuse_truncate,
    .open = myfuse_open,
    .read = myfuse_read,
    .write = myfuse_write,
    .release = myfuse_release,
	.init = myfuse_init,
	.destroy = myfuse_destroy,
    .create = myfuse_create
};

int main(int argc, char * argv[]) {
	if (argc >= 5) {
		if (strcmp(argv[argc-4], "--files") == 0) {
			file_data_file_name = argv[argc-3];
			directory_table_file_name = argv[argc-2];
			hash_data_file_name = argv[argc-1];
			argc -= 4;
		}
	}

	// file_data_file_name, directory_table_file_name and hash_data_file_name should be assigned
	int ret = fuse_main(argc, argv, &operations, NULL);
	return ret;
}
