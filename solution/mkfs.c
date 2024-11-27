#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "wfs.h"

static void init_superblock(struct wfs_sb *sb, int num_inodes, int num_blocks) {
    sb->num_inodes = num_inodes;
    sb->num_data_blocks = num_blocks;
    sb->i_bitmap_ptr = sizeof(struct wfs_sb);
    sb->d_bitmap_ptr = sb->i_bitmap_ptr + (num_inodes + 7) / 8;
    sb->i_blocks_ptr = sb->d_bitmap_ptr + (num_blocks + 7) / 8;
    sb->d_blocks_ptr = sb->i_blocks_ptr + num_inodes * sizeof(struct wfs_inode);
}

static void init_root_inode(struct wfs_inode *root) {
    time_t curr_time = time(NULL);
    root->num = 0;
    root->mode = S_IFDIR | 0755;
    root->uid = getuid();
    root->gid = getgid();
    root->size = 0;
    root->nlinks = 2;
    root->atim = curr_time;
    root->mtim = curr_time;
    root->ctim = curr_time;
    memset(root->blocks, 0, sizeof(root->blocks));
}

int main(int argc, char *argv[]) {
    if (argc < 11) return -1;

    int raid_mode = -1;
    int inode_count = 0;
    int block_count = 0;
    char *disk_paths[10];
    int disk_count = 0;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-r") == 0 && i + 1 < argc) {
            raid_mode = atoi(argv[++i]);
            if(raid_mode != 0 && raid_mode != 1) return -1;
        }
        else if(strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            disk_paths[disk_count++] = argv[++i];
        }
        else if(strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
            inode_count = atoi(argv[++i]);
        }
        else if(strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            block_count = atoi(argv[++i]);
        }
    }

    if(raid_mode == -1 || disk_count < 2 || inode_count <= 0 || block_count <= 0) return -1;

    // Align block count to 32
    if(block_count % 32 != 0) {
        block_count += (32 - (block_count % 32));
    }

    // Calculate total size needed
    size_t fs_size = sizeof(struct wfs_sb) +                     // superblock
                     (inode_count + 7) / 8 +                     // inode bitmap
                     (block_count + 7) / 8 +                     // data bitmap
                     inode_count * sizeof(struct wfs_inode) +    // inodes
                     block_count * BLOCK_SIZE;                   // data blocks

    // Calculate size in MB needed (rounded up)
    size_t size_in_mb = (fs_size + (1024*1024) - 1) / (1024*1024); //50 is a buffer

    // Create each disk with calculated size
    char cmd[1024];
    for(int i = 0; i < disk_count; i++) {
        snprintf(cmd, sizeof(cmd), "../solution/create_disk.sh %s %luM 1", disk_paths[i], size_in_mb);
        if(system(cmd) != 0) return -1;
    }

    // Initialize each disk
    for(int i = 0; i < disk_count; i++) {
        int fd = open(disk_paths[i], O_RDWR);
        if(fd == -1) return -1;

        struct stat st;
        if(fstat(fd, &st) == -1 || st.st_size < fs_size) {
            close(fd);
            return -1;
        }

        void *addr = mmap(NULL, fs_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if(addr == MAP_FAILED) {
            close(fd);
            return -1;
        }

        struct wfs_sb *sb = (struct wfs_sb *)addr;
        init_superblock(sb, inode_count, block_count);

        memset((char *)addr + sb->i_bitmap_ptr, 0, (inode_count + 7) / 8);
        memset((char *)addr + sb->d_bitmap_ptr, 0, (block_count + 7) / 8);

        struct wfs_inode *root_inode = (struct wfs_inode *)((char *)addr + sb->i_blocks_ptr);
        init_root_inode(root_inode);

        char *inode_bitmap = (char *)addr + sb->i_bitmap_ptr;
        inode_bitmap[0] |= 1;

        msync(addr, fs_size, MS_SYNC);
        munmap(addr, fs_size);
        close(fd);
    }

    return 0;
}
