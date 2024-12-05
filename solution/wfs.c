/*
#define etc 30
#include <fuse.h>
#define FUSE_USE_VERSION 30
#define <errno.h>
*/

#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "wfs.h"
#include <sys/mman.h>
#include <unistd.h>

static int wfs_getattr(const char *path, struct stat *stbuf) {
    // Implementation of getattr function to retrieve file attributes
    // Fill stbuf structure with the attributes of the file/directory indicated by path
    // ...
    printf("DEBUG 1: wfs_getattr called for path: %s\n", path);
    memset(stbuf, 0, sizeof(struct stat));
    
    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        
        return 0;
    }
    // Now also handle regular files
    // DELETE THIS LATER
    if (path[0] == '/') {  // Make sure it's a valid path
        printf("DEBUG 2: wfs_getattr called for path[0]: %d\n", path[0]);
        // For now, pretend any path exists as a regular file
        // Later we'll properly check if the file exists
        stbuf->st_mode = S_IFREG | 0666;  // regular file with 644 permissions
        stbuf->st_nlink = 1;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        stbuf->st_size = 0;
        return 0;
    }
    printf("DEBUG 3: wfs_getattr returning -ENOENT (file not found)\n");
    return -ENOENT;
}

static int wfs_unlink(const char* path) {
    // Remove (delete) the given file, symbolic link, hard link, or special node
    // Note that if you support hard links, unlink only deletes the data when the last hard link is removed. See unlink(2) for details.
    printf("DEBUG: wfs_unlink!\n");
    return 0;
}

static int wfs_mknod(const char* path, mode_t mode, dev_t rdev) {
    // Make a special (device) file, FIFO, or socket. See mknod(2) for details. 
    printf("DEBUG: wfs_mknod!\n");
    return 0;
}


static int wfs_mkdir(const char* path, mode_t mode) {
    // Create a directory with the given name. 
    // The directory permissions are encoded in mode. 
    // See mkdir(2) for details. This function is needed for any reasonable read/write filesystem.
    printf("DEBUG: wfs_mkdir!\n");
    return 0; // Success

}

static int wfs_rmdir(const char* path) {
    // Remove the given directory. This should succeed only if the directory is empty (except for "." and ".."). See rmdir(2) for details.
    // You should free all directory data blocks
    // but you do NOT need to free directory data blocks when unlinking files in a directory

    //check if path is empty
    
    

    //need to look at superblock of parent directory
    //find coresponding inode bitmap and datablock bitmap
    //unallocate both
    
    printf("DEBUG: wfs_rmdir!\n");
    return 0; // Return 0 on success
}

static int wfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    // Read sizebytes from the given file into the buffer buf, beginning offset bytes into the file. See read(2) for full details. 
    // Returns the number of bytes transferred, or 0 if offset was at or beyond the end of the file.
    if (/*offset was at or beyond the end of the file*/1) {
        printf("DEBUG: wfs_read if statement!\n");
        return 0;
    }
    printf("DEBUG: wfs_read outside if statement!\n");
    size_t bytes_transfered = 0;
    return bytes_transfered;
}

static int wfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    printf("DEBUG: wfs_write called for path: %s, size: %zu, offset: %ld\n", path, size, offset);
    // For now, pretend we wrote everything successfully
    //size_t bytes_transfered = 0;
    //return bytes_transfered;
    return size;
}


static int wfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
     /*
    *Return one or more directory entries (struct dirent) to the caller. 
    *This is one of the most complex FUSE functions. 
    *It is related to, but not identical to, the readdir(2) and getdents(2) system calls, and the readdir(3) library function. 
    *Because of its complexity, it is described separately below. 
    *Required for essentially any filesystem, since it's what makes ls and a whole bunch of other things work.
    */
  // return -EBADF if the file handle is invalid, or -ENOENT if you use the path argument and the path doesn't exist
    printf("DEBUG: wfs_readdir called for path: %s\n", path);
    
    if (strcmp(path, "/") != 0)
        return -ENOENT;
    
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    // For now, if we created a file, show it
    // Later we'll properly read directory entries
    filler(buf, "file", NULL, 0);

    return 0;
}

static struct fuse_operations ops = {
  .getattr = wfs_getattr,
  .mknod   = wfs_mknod,
  .mkdir   = wfs_mkdir,
  .unlink  = wfs_unlink,
  .rmdir   = wfs_rmdir,
  .read    = wfs_read,
  .write   = wfs_write,
  .readdir = wfs_readdir,
};


//use super block to know disk information

// put all operations in here

//how do we find the disks? We could call ls to list disks

// Global filesystem state
static struct {
    void **disk_maps;        // Array of mmap'd disk pointers
    int *disk_fds;           // Array of disk file descriptors
    int num_disks;           // Number of disks in array
    size_t disk_size;        // Size of each disk
    struct wfs_sb *sb;       // Pointer to superblock (on first disk)
} fs_state;

// Initialize filesystem
static int init_fs(char *disk_paths[], int num_disks) {
    fs_state.num_disks = num_disks;
    fs_state.disk_maps = malloc(sizeof(void *) * num_disks);
    fs_state.disk_fds = malloc(sizeof(int) * num_disks);
    
    // Open and map all disks
    for (int i = 0; i < num_disks; i++) {
        fs_state.disk_fds[i] = open(disk_paths[i], O_RDWR);
        if (fs_state.disk_fds[i] == -1) {
            return -1;
        }
        
        struct stat st;
        if (fstat(fs_state.disk_fds[i], &st) == -1) { // stores info about files
            return -1;
        }
        fs_state.disk_size = st.st_size;
        
        fs_state.disk_maps[i] = mmap(NULL, fs_state.disk_size, 
                                    PROT_READ | PROT_WRITE, MAP_SHARED, //check piazza for double check
                                    fs_state.disk_fds[i], 0);
        if (fs_state.disk_maps[i] == MAP_FAILED) {
            return -1;
        }
    }
    
    // Use superblock from first disk
    fs_state.sb = (struct wfs_sb *)fs_state.disk_maps[0];
    

    //////////////////////////////////////////

/*
    size_t num_inodes;
    size_t num_data_blocks;
    off_t i_bitmap_ptr;
    off_t d_bitmap_ptr;
    off_t i_blocks_ptr;
    off_t d_blocks_ptr;
    // Extend after this line
    int raid_mode;       // 0 for RAID0, 1 for RAID1
    int disk_count;      // Number of disks in the array
*/

    printf("Printing tester info\n");
    printf("Superblock number of inodes: %ld\n", fs_state.sb->num_inodes);
    printf("Superblock raid mode: %i\n", fs_state.sb->raid_mode);
    
      
    // Verify RAID configuration
    //This isn't verifying the raid configuration, this is verifying that we have the right amount of disks
    if (fs_state.sb->disk_count != num_disks) {
        fprintf(stderr, "Error: filesystem requires %d disks but %d provided\n",
                fs_state.sb->disk_count, num_disks);
        return -1;
    }
    
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 4) { // Need at least: program_name disk1 mountpoint -s
        fprintf(stderr, "Usage: %s disk1 [disk2 ...] [FUSE options] mountpoint\n", argv[0]);
        return 1;
    }

    // Count disks and find mount point
    char *disk_paths[16];  // Assuming reasonable maximum
    int num_disks = 0;
    int mount_idx = argc - 1;  // Mount point is last argument
    int fuse_arg_start = 1;    // Where FUSE args begin

    // Collect disk paths (they come before any FUSE options)
    for (int i = 1; i < mount_idx; i++) {
        if (argv[i][0] == '-') {
            fuse_arg_start = i;
            break;
        }
        disk_paths[num_disks] = argv[i];
	    num_disks++;
    }

    // Initialize filesystem
    if (init_fs(disk_paths, num_disks) != 0) {
        fprintf(stderr, "Failed to initialize filesystem\n");
        return 1;
    }

    // Prepare FUSE arguments
    char *fuse_argv[argc];
    int fuse_argc = 0;

    // First arg is program name
    fuse_argv[fuse_argc++] = argv[0];

    // Add all FUSE options
    for (int i = fuse_arg_start; i < argc; i++) {
        fuse_argv[fuse_argc++] = argv[i];
    }

    // Start FUSE
    return fuse_main(fuse_argc, fuse_argv, &ops, NULL);
}

/*
    int diskCount = 0;

    char mount[] = argv[argc - 1];


    for (int i = 0; i < argc; ++i) {
        //get name of a disk
        if (strcmp(argv[i], 0)) { //replace 0 with the name of a disk
            diskCount++;
        }
        //do check for mount point
        
        //do check for fuse options
    }
    //all disks from mkfs.c need to get passed into here as well?

    return 0;
    }
*/
/*
If the filesystem was created with N drives, it has to be always mounted with N drives. 
    Otherwise you should report an error and exit with a non-zero exit code. 
The order of drives during mount in wfs command does not matter and the 
    mount should always be succeed if correct drives are used. 
Note that filenames representing disks cannot be used as a filesystem identifier. 
Mount still has to work when disk images are renamed.*/


/*
Running this program will mount the filesystem to a mount point, which are specifed by the arguments
You need to pass [FUSE options] along with the mount_point to fuse_main as argv.
You may assume -s is always passed to wfs as a FUSE option to disable multi-threading.
We recommend testing your program using the -f option, which runs FUSE in the foreground.
With FUSE running in the foreground, you will need to open a second terminal to test your filesystem. 
In the terminal running FUSE, printf messages will be printed to the screen. 
You might want to have a printf
     at the beginning of every FUSE callback so you can see which callbacks are being run.*/
