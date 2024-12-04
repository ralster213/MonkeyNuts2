/*
#define etc 30
#include <fuse.h>
#define FUSE_USE_VERSION 30
#define <errno.h>
*/

#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static int wfs_getattr(const char *path, struct stat *stbuf) {
    // Implementation of getattr function to retrieve file attributes
    // Fill stbuf structure with the attributes of the file/directory indicated by path
    // ...

    return 0; // Return 0 on success
}

static int wfs_unlink(const char* path) {
    // Remove (delete) the given file, symbolic link, hard link, or special node
    // Note that if you support hard links, unlink only deletes the data when the last hard link is removed. See unlink(2) for details.

}

static int wfs_mknod(const char* path, mode_t mode, dev_t rdev) {
    // Make a special (device) file, FIFO, or socket. See mknod(2) for details. 

}


static int wfs_mkdir(const char* path, mode_t mode) {
    // Create a directory with the given name. 
    // The directory permissions are encoded in mode. 
    // See mkdir(2) for details. This function is needed for any reasonable read/write filesystem.
    return 0; // Success

}

static int wfs_rmdir(const char* path) {
    // Remove the given directory. This should succeed only if the directory is empty (except for "." and ".."). See rmdir(2) for details.
    // You should free all directory data blocks
    // but you do NOT need to free directory data blocks when unlinking files in a directory
    return 0; // Return 0 on success
}

static int wfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    // Read sizebytes from the given file into the buffer buf, beginning offset bytes into the file. See read(2) for full details. 
    // Returns the number of bytes transferred, or 0 if offset was at or beyond the end of the file.
    if (/*offset was at or beyond the end of the file*/1) {
        return 0;
    }
    size_t bytes_transfered = 0;
    return bytes_transfered;
}

static int wfs_write(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    // As for read above, except that it can't return 0.

    size_t bytes_transfered = 0;
    return bytes_transfered;
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
   return 0;
   // struct dirent directory;
   //return directory;

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

/*
int main(int argc, char *argv[]) {
    // Initialize FUSE with specified operations
    // Filter argc and argv here and then pass it to fuse_main
    return fuse_main(argc, argv, &ops, NULL);
}


*/


//use super block to know disk information

// put all operations in here

//how do we find the disks? We could call ls to list disks

int main(int argc, char *argv[]) {

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
