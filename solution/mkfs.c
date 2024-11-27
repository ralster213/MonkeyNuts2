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

    //Goes through and finds the locations of the disks
    while(counter < argc) {
        if ((strcmp(argv[counter], "-d")) == 0 ) { //we have more disks
            diskOne = 1; //we have at least one disk
            counter++;
            arr[counter] = 1;
            counter++;
        }
        else {
            moreDisks = 0;
        }
    }

}