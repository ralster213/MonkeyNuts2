/*
#define etc 30
#include <fuse.h>
#define FUSE_USE_VERSION 30
#define <errno.h>
*/


#define etc 30
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

static int my_getattr(const char *path, struct stat *stbuf) {
    // Implementation of getattr function to retrieve file attributes
    // Fill stbuf structure with the attributes of the file/directory indicated by path
    // ...

    return 0; // Return 0 on success
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
