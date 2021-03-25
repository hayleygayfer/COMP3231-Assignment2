/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */

#include <limits.h>

#define MAX_PROCESS 128
#define NUM_SYSCALLS 7
#define OPEN 0
#define READ 1
#define WRITE 2
#define LSEEK 3
#define CLOSE 4
#define DUP2 5
#define FORK 6

/*
 * Put your function declarations and data types here ...
 */

// open file table entry
typedef struct open_file {
    struct vnode *v_ptr;
    off_t file_offset;
} of_entry;

// file descriptor entry 
typedef struct fd_entry {
    of_entry *process_fd_array[MAX_PROCESS];
    int cur_index;
} fd_entry;

/* HELPER FUNCTIONS */
fd_entry *initialize_fd_table(void);
of_entry *create_open_file(void);
// of_table *create_of_table(void);
int add_to_of_table(of_entry *ofptr);
int add_to_fd_table(int process, fd_entry *process_fd_table, of_entry *ofptr);

#endif /* _FILE_H_ */
