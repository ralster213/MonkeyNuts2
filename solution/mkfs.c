#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MIN_DISKS 2
#define BLOCK_ALIGNMENT 32

typedef struct {
    int raid_mode;
    int inode_count;
    int block_count;
    char **disk_paths;
    int disk_count;
} Config;

static int align_block_count(int blocks) {
    if (blocks % BLOCK_ALIGNMENT == 0) return blocks;
    return blocks + (BLOCK_ALIGNMENT - (blocks % BLOCK_ALIGNMENT));
}

static int parse_args(int argc, char *argv[], Config *config) {
    if (argc < 11) return -1;  // Minimum required arguments
    
    if (strcmp(argv[1], "-r") != 0) return -1;
    config->raid_mode = atoi(argv[2]);
    if (config->raid_mode != 0 && config->raid_mode != 1) return -1;

    config->disk_paths = malloc(sizeof(char*) * argc);
    if (!config->disk_paths) return -1;
    
    int i;
    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 >= argc) return -1;
            config->disk_paths[config->disk_count++] = argv[++i];
        } else if (strcmp(argv[i], "-i") == 0) {
            if (i + 1 >= argc) return -1;
            config->inode_count = atoi(argv[++i]);
            break;
        }
    }

    if (config->disk_count < MIN_DISKS) {
        fprintf(stderr, "Error: only provided %d disks\n", config->disk_count);
        return -1;
    }

    if (i >= argc - 2 || strcmp(argv[i + 1], "-b") != 0) return -1;
    config->block_count = align_block_count(atoi(argv[i + 2]));

    return 0;
}

int main(int argc, char *argv[]) {
    Config config = {0};
    
    if (parse_args(argc, argv, &config) != 0) {
        fprintf(stderr, "Usage: %s -r mode -d disk1 [-d disk2 ...] -i inodes -b blocks\n", argv[0]);
        return -1;
    }

    // TODO: Initialize disks and create file system structure
    /*
    File system hierarchy:
    - Superblock
    - Inode Bitmap
    - Data Bitmap
    - Inodes
    - Data blocks
    */

    free(config.disk_paths);
    return 0;
}