/*
#define etc 30
#include <fuse.h>
#define FUSE_USE_VERSION 30
#define <errno.h>
*/

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "wfs.h"
#include <sys/mman.h>
#include <unistd.h>

// Global filesystem state
static struct {
    void **disk_maps;        // Array of mmap'd disk pointers
    int *disk_fds;           // Array of disk file descriptors
    int num_disks;           // Number of disks in array
    size_t disk_size;        // Size of each disk
    struct wfs_sb *sb;       // Pointer to superblock (on first disk)
} fs_state;

// Helper function to get root inode
static struct wfs_inode *get_root_inode() {
    return (struct wfs_inode *)((char *)fs_state.disk_maps[0] + fs_state.sb->i_blocks_ptr);
}

// Helper function to get block pointer
static void *get_block_ptr(int block_num) {
    if (block_num < 0 || block_num >= fs_state.sb->num_data_blocks) {
        return NULL;
    }
    return (void *)((char *)fs_state.disk_maps[0] + fs_state.sb->d_blocks_ptr + block_num * BLOCK_SIZE);
}

// Helper function to sync changes in RAID-1 mode
static void sync_raid1_changes() {
    if (fs_state.sb->raid_mode == 1) {
        for (int i = 1; i < fs_state.num_disks; i++) {
            memcpy(fs_state.disk_maps[i], fs_state.disk_maps[0], fs_state.disk_size);
        }
    }
}


static struct wfs_inode* find_inode_by_path(const char* path) {
    if (strcmp(path, "/") == 0) {
        // Root inode is always the first inode
        return (struct wfs_inode*)((char*)fs_state.disk_maps[0] + fs_state.sb->i_blocks_ptr);
    }

    // Skip leading slash
    if (path[0] == '/') path++;

    struct wfs_inode* current = (struct wfs_inode*)((char*)fs_state.disk_maps[0] + fs_state.sb->i_blocks_ptr);
    char* path_copy = strdup(path);
    char* component = strtok(path_copy, "/");

    while (component) {
        int found = 0;
        // Search through current directory's blocks
        for (int i = 0; i < D_BLOCK && current->blocks[i]; i++) {
            struct wfs_dentry* entries = (struct wfs_dentry*)((char*)fs_state.disk_maps[0] + 
                                        fs_state.sb->d_blocks_ptr + current->blocks[i] * BLOCK_SIZE);
            int num_entries = BLOCK_SIZE / sizeof(struct wfs_dentry);
            
            for (int j = 0; j < num_entries; j++) {
                if (entries[j].num != 0 && strcmp(entries[j].name, component) == 0) {
                    current = (struct wfs_inode*)((char*)fs_state.disk_maps[0] + 
                             fs_state.sb->i_blocks_ptr + entries[j].num * sizeof(struct wfs_inode));
                    found = 1;
                    break;
                }
            }
            if (found) break;
        }
        
        if (!found) {
            free(path_copy);
            return NULL;
        }
        
        component = strtok(NULL, "/");
    }
    
    free(path_copy);
    return current;
}

static int wfs_getattr(const char *path, struct stat *stbuf) {
    printf("DEBUG: wfs_getattr called for path: %s\n", path);
    memset(stbuf, 0, sizeof(struct stat));

    struct wfs_inode* inode = find_inode_by_path(path);
    if (!inode) {
        return -ENOENT;
    }

    stbuf->st_mode = inode->mode;
    stbuf->st_nlink = inode->nlinks;
    stbuf->st_uid = inode->uid;
    stbuf->st_gid = inode->gid;
    stbuf->st_size = inode->size;
    stbuf->st_atime = inode->atim;
    stbuf->st_mtime = inode->mtim;
    stbuf->st_ctime = inode->ctim;

    return 0;
}
// static int wfs_getattr(const char *path, struct stat *stbuf) {
//     // Implementation of getattr function to retrieve file attributes
//     // Fill stbuf structure with the attributes of the file/directory indicated by path
//     // ...
//     printf("DEBUG 1: wfs_getattr called for path: %s\n", path);
//     memset(stbuf, 0, sizeof(struct stat));
    
//     if (strcmp(path, "/") == 0) {
//         stbuf->st_mode = S_IFDIR | 0755;
//         stbuf->st_nlink = 2;
//         stbuf->st_uid = getuid();
//         stbuf->st_gid = getgid();
        
//         return 0;
//     }
//     // Now also handle regular files
//     // DELETE THIS LATER
//     if (path[0] == '/') {  // Make sure it's a valid path
//         printf("DEBUG 2: wfs_getattr called for path[0]: %d\n", path[0]);
//         // For now, pretend any path exists as a regular file
//         // Later we'll properly check if the file exists
//         stbuf->st_mode = S_IFREG | 0666;  // regular file with 644 permissions
//         stbuf->st_nlink = 1;
//         stbuf->st_uid = getuid();
//         stbuf->st_gid = getgid();
//         stbuf->st_size = 0;
//         return 0;
//     }
//     printf("DEBUG 3: wfs_getattr returning -ENOENT (file not found)\n");
//     return -ENOENT;
// }

static int wfs_unlink(const char* path) {
    // Remove (delete) the given file, symbolic link, hard link, or special node
    // Note that if you support hard links, unlink only deletes the data when the last hard link is removed. See unlink(2) for details.
    printf("DEBUG: wfs_unlink!\n");
    return 0;
}

static int wfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi) {
    printf("DEBUG: wfs_readdir called for path: %s\n", path);

    struct wfs_inode *inode = find_inode_by_path(path);
    if (!inode) {
        return -ENOENT;
    }

    // Check if it's actually a directory
    if (!S_ISDIR(inode->mode)) {
        return -ENOTDIR;
    }

    // Add . and .. entries
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    // Read through all direct blocks
    for (int i = 0; i < D_BLOCK && inode->blocks[i]; i++) {
        struct wfs_dentry *entries = (struct wfs_dentry *)((char *)fs_state.disk_maps[0] + 
                                    fs_state.sb->d_blocks_ptr + inode->blocks[i] * BLOCK_SIZE);
        int num_entries = BLOCK_SIZE / sizeof(struct wfs_dentry);

        for (int j = 0; j < num_entries; j++) {
            if (entries[j].num != 0) {  // Valid entry
                if (filler(buf, entries[j].name, NULL, 0)) {
                    return 0;  // Buffer full
                }
            }
        }
    }

    return 0;
}

// Helper function to find a free inode number
static int find_free_inode() {
    char *inode_bitmap = (char *)fs_state.disk_maps[0] + fs_state.sb->i_bitmap_ptr;
    for (size_t i = 0; i < fs_state.sb->num_inodes; i++) {
        if (!(inode_bitmap[i / 8] & (1 << (i % 8)))) {
            return i;
        }
    }
    return -1;
}

// Helper function to find a free data block
static int find_free_block() {
    char *block_bitmap = (char *)fs_state.disk_maps[0] + fs_state.sb->d_bitmap_ptr;
    for (size_t i = 0; i < fs_state.sb->num_data_blocks; i++) {
        if (!(block_bitmap[i / 8] & (1 << (i % 8)))) {
            return i;
        }
    }
    return -1;
}

// Helper function to add directory entry
static int add_dir_entry(struct wfs_inode *dir, const char *name, int inode_num) {
    // Only use existing blocks - we should never need to allocate new ones
    // since the root directory is pre-allocated with enough space
    for (int i = 0; i < D_BLOCK && dir->blocks[i]; i++) {
        struct wfs_dentry *entries = get_block_ptr(dir->blocks[i]);
        if (!entries) return -EIO;

        int num_entries = BLOCK_SIZE / sizeof(struct wfs_dentry);
        for (int j = 0; j < num_entries; j++) {
            if (entries[j].num == 0) {  // Found empty entry
                strncpy(entries[j].name, name, MAX_NAME - 1);
                entries[j].name[MAX_NAME - 1] = '\0';
                entries[j].num = inode_num;
                
                // Update directory size if this entry extends it
                size_t entry_offset = j * sizeof(struct wfs_dentry);
                if (entry_offset + sizeof(struct wfs_dentry) > dir->size) {
                    dir->size = entry_offset + sizeof(struct wfs_dentry);
                }
                
                sync_raid1_changes();
                return 0;
            }
        }
    }

    // If we get here, the existing blocks are full
    return -ENOSPC;
}


static int wfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    printf("DEBUG: wfs_mknod called for path: %s\n", path);

    // Don't handle special files
    if (!S_ISREG(mode)) {
        return -EINVAL;
    }

    // Get parent directory path and file name
    char *path_copy = strdup(path);
    char *last_slash = strrchr(path_copy, '/');
    if (!last_slash) {
        free(path_copy);
        return -EINVAL;
    }

    char *file_name = last_slash + 1;
    if (strlen(file_name) >= MAX_NAME) {
        free(path_copy);
        return -ENAMETOOLONG;
    }

    *last_slash = '\0';  // Split path
    const char *dir_path = (*path_copy == '\0') ? "/" : path_copy;

    // Find parent directory inode
    struct wfs_inode *dir_inode = find_inode_by_path(dir_path);
    if (!dir_inode) {
        free(path_copy);
        return -ENOENT;
    }

    // Check if file already exists
    struct wfs_inode *existing = find_inode_by_path(path);
    if (existing) {
        free(path_copy);
        return -EEXIST;
    }

    // Find free inode
    int inode_num = find_free_inode();
    if (inode_num == -1) {
        free(path_copy);
        return -ENOSPC;
    }

    // Initialize new inode
    struct wfs_inode *new_inode = (struct wfs_inode *)((char *)fs_state.disk_maps[0] + 
                                 fs_state.sb->i_blocks_ptr + inode_num * sizeof(struct wfs_inode));
    new_inode->num = inode_num;
    new_inode->mode = mode;
    new_inode->uid = getuid();
    new_inode->gid = getgid();
    new_inode->size = 0; // TODO CHANGE TO 0
    new_inode->nlinks = 1;
    time_t curr_time = time(NULL);
    new_inode->atim = curr_time;
    new_inode->mtim = curr_time;
    new_inode->ctim = curr_time;
    memset(new_inode->blocks, 0, sizeof(new_inode->blocks));

    // Mark inode as used
    char *inode_bitmap = (char *)fs_state.disk_maps[0] + fs_state.sb->i_bitmap_ptr;
    inode_bitmap[inode_num / 8] |= (1 << (inode_num % 8));

    // Add directory entry
    int ret = add_dir_entry(dir_inode, file_name, inode_num);
    if (ret < 0) {
        // Cleanup on failure
        inode_bitmap[inode_num / 8] &= ~(1 << (inode_num % 8));
        memset(new_inode, 0, sizeof(struct wfs_inode));
        free(path_copy);
        return ret;
    }

    // Update directory timestamps
    dir_inode->mtim = curr_time;
    dir_inode->ctim = curr_time;

    free(path_copy);
    return 0;
}


// static int wfs_mkdir(const char* path, mode_t mode) {
//     // Create a directory with the given name. 
//     // The directory permissions are encoded in mode. 
//     // See mkdir(2) for details. This function is needed for any reasonable read/write filesystem.
//     printf("DEBUG: wfs_mkdir!\n");
//     return 0; // Success

// }
static int wfs_mkdir(const char* path, mode_t mode) {
    printf("DEBUG: wfs_mkdir called for path: %s, mode: %o\n", path, mode);

    // Get parent directory path and directory name
    char *path_copy = strdup(path);
    char *last_slash = strrchr(path_copy, '/');
    if (!last_slash) {
        free(path_copy);
        return -EINVAL;
    }

    char *dir_name = last_slash + 1;
    if (strlen(dir_name) >= MAX_NAME) {
        free(path_copy);
        return -ENAMETOOLONG;
    }

    *last_slash = '\0';  // Split path
    const char *parent_path = (*path_copy == '\0') ? "/" : path_copy;

    struct wfs_inode *parent_inode;
    if (strcmp(parent_path, "/") == 0) {
        parent_inode = get_root_inode();
    } else {
        parent_inode = find_inode_by_path(parent_path);
    }

    if (!parent_inode) {
        free(path_copy);
        return -ENOENT;
    }

    int inode_num = find_free_inode();
    if (inode_num == -1) {
        free(path_copy);
        return -ENOSPC;
    }

    int block_num = find_free_block();
    if (block_num == -1) {
        char *inode_bitmap = (char *)fs_state.disk_maps[0] + fs_state.sb->i_bitmap_ptr;
        inode_bitmap[inode_num / 8] &= ~(1 << (inode_num % 8));
        free(path_copy);
        return -ENOSPC;
    }

    // Initialize new directory inode
    struct wfs_inode *new_inode = (struct wfs_inode *)((char *)fs_state.disk_maps[0] + 
                                 fs_state.sb->i_blocks_ptr + inode_num * sizeof(struct wfs_inode));
    new_inode->num = inode_num;
    new_inode->mode = S_IFDIR | (mode & 0777);
    new_inode->uid = getuid();
    new_inode->gid = getgid();
    new_inode->size = BLOCK_SIZE;
    new_inode->nlinks = 2;
    time_t curr_time = time(NULL);
    new_inode->atim = curr_time;
    new_inode->mtim = curr_time;
    new_inode->ctim = curr_time;
    memset(new_inode->blocks, 0, sizeof(new_inode->blocks));
    new_inode->blocks[0] = block_num;

    // Mark inode and block as used
    char *inode_bitmap = (char *)fs_state.disk_maps[0] + fs_state.sb->i_bitmap_ptr;
    char *block_bitmap = (char *)fs_state.disk_maps[0] + fs_state.sb->d_bitmap_ptr;
    inode_bitmap[inode_num / 8] |= (1 << (inode_num % 8));
    block_bitmap[block_num / 8] |= (1 << (block_num % 8));

    // Initialize directory entries
    struct wfs_dentry *entries = get_block_ptr(block_num);
    if (!entries) {
        // Cleanup
        inode_bitmap[inode_num / 8] &= ~(1 << (inode_num % 8));
        block_bitmap[block_num / 8] &= ~(1 << (block_num % 8));
        free(path_copy);
        return -EIO;
    }

    memset(entries, 0, BLOCK_SIZE);
    
    // Set up . and .. entries
    strncpy(entries[0].name, ".", MAX_NAME - 1);
    entries[0].num = inode_num;
    strncpy(entries[1].name, "..", MAX_NAME - 1);
    entries[1].num = parent_inode->num;

    // Add entry to parent directory
    int ret = add_dir_entry(parent_inode, dir_name, inode_num);
    if (ret < 0) {
        // Cleanup
        inode_bitmap[inode_num / 8] &= ~(1 << (inode_num % 8));
        block_bitmap[block_num / 8] &= ~(1 << (block_num % 8));
        memset(new_inode, 0, sizeof(struct wfs_inode));
        free(path_copy);
        return ret;
    }

    // Update parent directory
    parent_inode->nlinks++;
    parent_inode->mtim = curr_time;
    parent_inode->ctim = curr_time;

    sync_raid1_changes();
    free(path_copy);
    return 0;
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

static ssize_t wfs_read_block(int block_num, char *buf, size_t size, off_t offset) {
    if (fs_state.sb->raid_mode == 0) {  // RAID 0
        // Calculate which disk and block
        int stripe_size = BLOCK_SIZE;
        int disk_idx = (block_num * stripe_size + offset) / stripe_size % fs_state.num_disks;
        int disk_block = block_num / fs_state.num_disks;
        
        char *block_ptr = (char *)fs_state.disk_maps[disk_idx] + 
                         fs_state.sb->d_blocks_ptr + disk_block * BLOCK_SIZE;
        memcpy(buf, block_ptr + (offset % BLOCK_SIZE), size);
    } else {  // RAID 1
        // Read from first disk by default
        char *block_ptr = (char *)fs_state.disk_maps[0] + 
                         fs_state.sb->d_blocks_ptr + block_num * BLOCK_SIZE;
        memcpy(buf, block_ptr + offset, size);
    }
    return size;
}

static ssize_t wfs_write_block(int block_num, const char *buf, size_t size, off_t offset) {
    if (fs_state.sb->raid_mode == 0) {  // RAID 0
        // Calculate which disk and block
        int stripe_size = BLOCK_SIZE;
        int disk_idx = (block_num * stripe_size + offset) / stripe_size % fs_state.num_disks;
        int disk_block = block_num / fs_state.num_disks;
        
        char *block_ptr = (char *)fs_state.disk_maps[disk_idx] + 
                         fs_state.sb->d_blocks_ptr + disk_block * BLOCK_SIZE;
        memcpy(block_ptr + (offset % BLOCK_SIZE), buf, size);
    } else {  // RAID 1
        // Write to all disks
        for (int i = 0; i < fs_state.num_disks; i++) {
            char *block_ptr = (char *)fs_state.disk_maps[i] + 
                            fs_state.sb->d_blocks_ptr + block_num * BLOCK_SIZE;
            memcpy(block_ptr + offset, buf, size);
        }
    }
    return size;
}

static int wfs_read(const char* path, char *buf, size_t size, off_t offset,
                   struct fuse_file_info* fi) {
    printf("DEBUG: wfs_read called for path: %s, size: %zu, offset: %ld\n", 
           path, size, offset);

    struct wfs_inode *inode = find_inode_by_path(path);
    if (!inode) {
        return -ENOENT;
    }

    if (!S_ISREG(inode->mode)) {
        return -EISDIR;
    }

    // Check if offset is beyond file size
    if (offset >= inode->size) {
        return 0;
    }

    // Adjust size if it would read past end of file
    if (offset + size > inode->size) {
        size = inode->size - offset;
    }

    size_t bytes_read = 0;
    while (bytes_read < size) {
        // Calculate which block contains this part of the file
        int block_index = offset / BLOCK_SIZE;
        int block_offset = offset % BLOCK_SIZE;
        
        // Handle direct blocks
        if (block_index < D_BLOCK) {
            if (inode->blocks[block_index] == 0) {
                break;  // Hit a hole in the file
            }
            
            size_t block_bytes = MIN(BLOCK_SIZE - block_offset, 
                                   size - bytes_read);
            
            ssize_t read_bytes = wfs_read_block(inode->blocks[block_index],
                                              buf + bytes_read,
                                              block_bytes,
                                              block_offset);
            if (read_bytes < 0) {
                return read_bytes;
            }
            
            bytes_read += read_bytes;
            offset += read_bytes;
        }
        // Handle indirect block
        else if (block_index < D_BLOCK + BLOCK_SIZE/sizeof(int)) {
            if (inode->blocks[IND_BLOCK] == 0) {
                break;  // No indirect block allocated
            }
            
            int *indirect_block = get_block_ptr(inode->blocks[IND_BLOCK]);
            int indirect_index = block_index - D_BLOCK;
            
            if (indirect_block[indirect_index] == 0) {
                break;  // Hit a hole in the file
            }
            
            size_t block_bytes = MIN(BLOCK_SIZE - block_offset,
                                   size - bytes_read);
            
            ssize_t read_bytes = wfs_read_block(indirect_block[indirect_index],
                                              buf + bytes_read,
                                              block_bytes,
                                              block_offset);
            if (read_bytes < 0) {
                return read_bytes;
            }
            
            bytes_read += read_bytes;
            offset += read_bytes;
        }
        else {
            break;  // Beyond maximum file size
        }
    }

    // Update access time
    inode->atim = time(NULL);
    if (fs_state.sb->raid_mode == 1) {
        sync_raid1_changes();
    }

    return bytes_read;
}

static int wfs_write(const char* path, const char *buf, size_t size, off_t offset,
                    struct fuse_file_info* fi) {
    printf("DEBUG: wfs_write called for path: %s, size: %zu, offset: %ld\n",
           path, size, offset);

    struct wfs_inode *inode = find_inode_by_path(path);
    if (!inode) {
        return -ENOENT;
    }

    if (!S_ISREG(inode->mode)) {
        return -EISDIR;
    }

    size_t bytes_written = 0;
    while (bytes_written < size) {
        // Calculate which block contains this part of the file
        int block_index = offset / BLOCK_SIZE;
        int block_offset = offset % BLOCK_SIZE;
        
        // Handle direct blocks
        if (block_index < D_BLOCK) {
            if (inode->blocks[block_index] == 0) {
                // Allocate new block
                int new_block = find_free_block();
                if (new_block == -1) {
                    return bytes_written > 0 ? bytes_written : -ENOSPC;
                }
                
                char *block_bitmap = (char *)fs_state.disk_maps[0] + fs_state.sb->d_bitmap_ptr;
                block_bitmap[new_block / 8] |= (1 << (new_block % 8));
                inode->blocks[block_index] = new_block;
            }
            
            size_t block_bytes = MIN(BLOCK_SIZE - block_offset,
                                   size - bytes_written);
            
            ssize_t written_bytes = wfs_write_block(inode->blocks[block_index],
                                                  buf + bytes_written,
                                                  block_bytes,
                                                  block_offset);
            if (written_bytes < 0) {
                return written_bytes;
            }
            
            bytes_written += written_bytes;
            offset += written_bytes;
        }
        // Handle indirect block
        else if (block_index < D_BLOCK + BLOCK_SIZE/sizeof(int)) {
            if (inode->blocks[IND_BLOCK] == 0) {
                // Allocate indirect block
                int new_block = find_free_block();
                if (new_block == -1) {
                    return bytes_written > 0 ? bytes_written : -ENOSPC;
                }
                
                char *block_bitmap = (char *)fs_state.disk_maps[0] + fs_state.sb->d_bitmap_ptr;
                block_bitmap[new_block / 8] |= (1 << (new_block % 8));
                inode->blocks[IND_BLOCK] = new_block;
                
                // Initialize indirect block
                int *indirect_block = get_block_ptr(new_block);
                memset(indirect_block, 0, BLOCK_SIZE);
            }
            
            int *indirect_block = get_block_ptr(inode->blocks[IND_BLOCK]);
            int indirect_index = block_index - D_BLOCK;
            
            if (indirect_block[indirect_index] == 0) {
                // Allocate new data block
                int new_block = find_free_block();
                if (new_block == -1) {
                    return bytes_written > 0 ? bytes_written : -ENOSPC;
                }
                
                char *block_bitmap = (char *)fs_state.disk_maps[0] + fs_state.sb->d_bitmap_ptr;
                block_bitmap[new_block / 8] |= (1 << (new_block % 8));
                indirect_block[indirect_index] = new_block;
            }
            
            size_t block_bytes = MIN(BLOCK_SIZE - block_offset,
                                   size - bytes_written);
            
            ssize_t written_bytes = wfs_write_block(indirect_block[indirect_index],
                                                  buf + bytes_written,
                                                  block_bytes,
                                                  block_offset);
            if (written_bytes < 0) {
                return written_bytes;
            }
            
            bytes_written += written_bytes;
            offset += written_bytes;
        }
        else {
            return -EFBIG;  // File too big
        }
    }

    // Update file size if necessary
    if (offset > inode->size) {
        inode->size = offset;
    }

    // Update modification time
    time_t curr_time = time(NULL);
    inode->mtim = curr_time;
    inode->ctim = curr_time;

    if (fs_state.sb->raid_mode == 1) {
        sync_raid1_changes();
    }

    return bytes_written;
}

// static int wfs_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
//     // Read sizebytes from the given file into the buffer buf, beginning offset bytes into the file. See read(2) for full details. 
//     // Returns the number of bytes transferred, or 0 if offset was at or beyond the end of the file.
//     if (/*offset was at or beyond the end of the file*/1) {
//         printf("DEBUG: wfs_read if statement!\n");
//         return 0;
//     }
//     printf("DEBUG: wfs_read outside if statement!\n");
//     size_t bytes_transfered = 0;
//     return bytes_transfered;
// }

// static int wfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi) {
//     printf("DEBUG: wfs_write called for path: %s, size: %zu, offset: %ld\n", path, size, offset);
//     // For now, pretend we wrote everything successfully
//     //size_t bytes_transfered = 0;
//     //return bytes_transfered;
//     return size;
// }



// static int wfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
//      /*
//     *Return one or more directory entries (struct dirent) to the caller. 
//     *This is one of the most complex FUSE functions. 
//     *It is related to, but not identical to, the readdir(2) and getdents(2) system calls, and the readdir(3) library function. 
//     *Because of its complexity, it is described separately below. 
//     *Required for essentially any filesystem, since it's what makes ls and a whole bunch of other things work.
//     */
//   // return -EBADF if the file handle is invalid, or -ENOENT if you use the path argument and the path doesn't exist
//     printf("DEBUG: wfs_readdir called for path: %s\n", path);
    
//     if (strcmp(path, "/") != 0)
//         return -ENOENT;
    
//     filler(buf, ".", NULL, 0);
//     filler(buf, "..", NULL, 0);

//     // For now, if we created a file, show it
//     // Later we'll properly read directory entries
//     //filler(buf, "file", NULL, 0);

//     return 0;
// }

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
// Initialize filesystem
static int init_fs(char *disk_paths[], int num_disks) {
    fs_state.num_disks = num_disks;
    fs_state.disk_maps = malloc(sizeof(void *) * num_disks);
    fs_state.disk_fds = malloc(sizeof(int) * num_disks);
    
    // Open and map all disks
    for (int i = 0; i < num_disks; i++) {
        fs_state.disk_fds[i] = open(disk_paths[i], O_RDWR);
        if (fs_state.disk_fds[i] == -1) {
            perror("Failed to open disk");
            return -1;
        }
        
        struct stat st;
        if (fstat(fs_state.disk_fds[i], &st) == -1) { // stores info about files
            perror("Failed to stat disk");
            return -1;
        }
        fs_state.disk_size = st.st_size;
        
        fs_state.disk_maps[i] = mmap(NULL, fs_state.disk_size, 
                                    PROT_READ | PROT_WRITE, MAP_SHARED, // check piazza for double check
                                    fs_state.disk_fds[i], 0);
        if (fs_state.disk_maps[i] == MAP_FAILED) {
            perror("Failed to mmap disk");
            return -1;
        }
    }
    
    // Use superblock from first disk
    fs_state.sb = (struct wfs_sb *)fs_state.disk_maps[0];
    
    // Verify RAID configuration
    if (fs_state.sb->disk_count != num_disks) {
        fprintf(stderr, "Error: filesystem requires %d disks but %d provided\n",
                fs_state.sb->disk_count, num_disks);
        return -1;
    }

//     // Get pointer to root inode
//     struct wfs_inode *root_inode = (struct wfs_inode *)((char *)fs_state.disk_maps[0] + fs_state.sb->i_blocks_ptr);
//     // printf("fs_state.disk_maps[0] = %p\n", fs_state.disk_maps[0] );
//     // printf("fs_state.sb->i_blocks_ptr = %lu\n", fs_state.sb->i_blocks_ptr);
    
    
//     // Verify root inode setup
//     if (root_inode->num != 0 || !S_ISDIR(root_inode->mode)) {
//         fprintf(stderr, "Error: invalid root inode\n");
//         return -1;
//     }

//     //Verify root directory has at least one data block
//     if (root_inode->blocks[0] == 0) {
//         fprintf(stderr, "Root directory data block missing \n");
//         return -1;
// }


//     // Get root directory entries
//     struct wfs_dentry *root_entries = (struct wfs_dentry *)((char *)fs_state.disk_maps[0] + 
//                                      fs_state.sb->d_blocks_ptr + root_inode->blocks[0] * BLOCK_SIZE);

//     // Verify "." and ".." entries
//     if (strcmp(root_entries[0].name, ".") != 0 || root_entries[0].num != 0 ||
//         strcmp(root_entries[1].name, "..") != 0 || root_entries[1].num != 0) {
//         fprintf(stderr, "Error: invalid root directory entries\n");
//         return -1; // TODO UNCOMMENT THIS
//     }

    printf("Filesystem initialized successfully:\n");
    printf("- Number of inodes: %zu\n", fs_state.sb->num_inodes);
    printf("- Number of data blocks: %zu\n", fs_state.sb->num_data_blocks);
    printf("- RAID mode: %d\n", fs_state.sb->raid_mode);
    printf("- Number of disks: %d\n", fs_state.sb->disk_count);
    printf(" disk size %li \n", fs_state.disk_size);

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
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

    if (num_disks < 2) {
        fprintf(stderr, "Error: at least 2 disks are required\n");
        return 1;
    }

    // Initialize filesystem
    if (init_fs(disk_paths, num_disks) != 0) {
        // Error message already printed by init_fs
        printf("ERROR: init_fs didn't return 0\n");
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
    //int ret = fuse_main(fuse_argc, fuse_argv, &ops, NULL);

    // // Cleanup
    // for (int i = 0; i < fs_state.num_disks; i++) {
    //     if (fs_state.disk_maps[i] != MAP_FAILED) {
    //         munmap(fs_state.disk_maps[i], fs_state.disk_size);
    //     }
    //     if (fs_state.disk_fds[i] != -1) {
    //         close(fs_state.disk_fds[i]);
    //     }
    // }
    // free(fs_state.disk_maps);
    // free(fs_state.disk_fds);

    //return ret;
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
