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

int filesystem_exists() {
	if (fuse_get_context()->private_data == NULL) {
		perror("filesystem_exists: Filesystem does not exist");
		exit(1);
	}
	return 0;
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
	    // TODO: REMOVE
        printf("%s directory\n", path);

		// Set size as the number of bytes used in the filesystem
        result->st_size = ((filesys_t*)(fuse_get_context()->private_data))->used;

        // Mode is a directory
        result->st_mode = S_IFDIR;
	} else {
        // TODO: REMOVE
	    printf("%s!!!!!!!!\n", path);

        // Set size as the number of bytes used by the file
        char* name = salloc(strlen(path));
        memcpy(name, path + 1, strlen(path));

        ssize_t size = file_size(name, fuse_get_context()->private_data);
        free(name);

        // Check if file exists
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
    // Exit if filesystem does not exist
    if (fuse_get_context()->private_data == NULL) {
        perror("myfuse_readdir: Filesystem does not exist");
        exit(1);
    }

	if (strcmp(path, "/") == 0) {
	    filesys_t* fs = (filesys_t*)(fuse_get_context()->private_data);
	    file_t** n_list = fs->n_list->list;
	    int32_t size = fs->n_list->size;

	    // Iterate over filesystem files in alphabetical order
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
    if (strncmp(path, "/", 1) != 0 || strcmp(path, "/") == 0) {
        return -EINVAL;
    }

	char* name = salloc(strlen(path));
	memcpy(name, path + 1, strlen(path));

	int ret = delete_file(name, fuse_get_context()->private_data);
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
    
    // Check for valid arguments
    if (oldname == NULL || newname == NULL) {
        return -EINVAL;
    }
    
    // Check if names are directories
    if (strcmp(oldname, "/") == 0 || strcmp(newname, "/") == 0) {
    	return -EISDIR;
    }
    
    char* old = salloc(strlen(oldname));
	char* new = salloc(strlen(newname));
	memcpy(old, oldname + 1, strlen(oldname));
	memcpy(new, newname + 1, strlen(newname));

	int ret = rename_file(old, new, fuse_get_context()->private_data);
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
	
	// Check for valid arguments
	if (path == NULL) {
		return -EINVAL;
	}
	
	// Check if path is a directory
	if (strcmp(path, "/") == 0) {
		return -EISDIR;
	}
	
	char* name = salloc(strlen(path));
	memcpy(name, path + 1, strlen(path));
	
	int ret = resize_file(name, length, fuse_get_context()->private_data);
	free(name);
	
	// Check if file exists
	if (ret == 1) {
		return -ENOENT;
	}
	
	// Check if sufficient space in filesystem
	if (ret == 2) {
		return -EINVAL;
	}
	
	return 0;
}

// TODO: READ ED THREADS
int myfuse_open(const char *, struct fuse_file_info *);

int myfuse_read(const char * path, char * buf, size_t length, off_t offset, struct fuse_file_info * fi) {
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
	
	// Check current length of file
	char* name = salloc(strlen(path));
	memcpy(name, path + 1, strlen(path));
	
	ssize_t f_size = file_size(name, fuse_get_context()->private_data);
	
	// Check if file exists
	if (f_size < 0) {
		return -ENOENT;
	}
	
	// Check for EOF
	if (offset >= f_size) {
		return 0;
	}
	
	// Update number of bytes to read based on length of file
	size_t read_length;
	if (length > f_size - offset) {
		read_length = f_size - offset;
	} else {
		read_length = length;
	}
	
	// Relevant error checks for read_file have been performed by this point
	read_file(name, offset, read_length, buf,fuse_get_context()->private_data);
	free(name);
	
	return read_length;
}

int myfuse_write(const char * path, const char * buf, size_t length, off_t offset, struct fuse_file_info * fi) {

}

// TODO: READ ED THREADS
int myfuse_release(const char *, struct fuse_file_info *);
	// FILL OUT

void * myfuse_init(struct fuse_conn_info * info) {
    // Check for valid arguments
	if (file_data_file_name == NULL || directory_table_file_name == NULL || hash_data_file_name == NULL) {
		// TODO: REMOVE
		printf("FILESYSTEM NOT INPUT CORRECTLY\n");
        printf("%s %s %s\n", file_data_file_name, directory_table_file_name, hash_data_file_name);
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

int myfuse_create(const char * path, mode_t mode, struct fuse_file_info * fi);

struct fuse_operations operations = {
	.getattr = myfuse_getattr,
	.readdir = myfuse_readdir,
	.unlink = myfuse_unlink,
	.rename = myfuse_rename,
    .truncate = myfuse_truncate,
//    .open =
//    .read =
//    .write =
//    .release =
	.init = myfuse_init,
	.destroy = myfuse_destroy // TODO: COMMA HERE
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
