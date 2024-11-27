#include <stdio.h>

int main(int argc, char *argv[]) {
    

    int raidMode;
    int inode;
    int numberOfBlocks;

    if ((strcmp(argv[1], "-r")) != 0) {
        return -1;
    }

    int raidCheck = argv[2];

    if ((raidCheck != 0) || (raidCheck != 1)) {
        return -1;
    }

    raidMode = argv[2];

    //Do disk stuff here

    int moreDisks = 1;
    int *arr = malloc(sizeof(int) * argc);
    int counter = 0;
    int diskOne = 0; //make sure we have at least one disk
    int lastDisk = 0;

    //Goes through and finds the locations of the disks
    while(counter < argc) {
        if ((strcmp(argv[counter], "-d")) == 0 ) { //we have more disks
            diskOne = 1; //we have at least one disk
            counter++;
            arr[counter] = 1;
            lastDisk = counter;
            counter++;
        }
        else {
            counter += 1;
        }
    }

    counter++;

    if ((strcmp(argv[counter], "-i")) != 0) {
        return -1;
    }

    //Number of inodes can be anything
    counter++;
    inode = argv[counter];


    counter++;
    if ((strcmp(argv[counter], "-b")) != 0) {
        return -1;
    }

    counter++;
    int numberOfBlocks = argv[counter];

    if (numberOfBlocks % 32 != 0) {
        numberOfBlocks += (32 - (numberOfBlocks % 32));
    }

    

    //Go through array and init the disks

    //Heirarchy
    //Superblock
    //Inode Bitmap
    //Data Bitmap
    //Inodes
    //Data blocks

    //Use create_disk.sh to create a file named disk






    free(arr);




}