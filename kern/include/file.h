/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>
#define MAX_PROCESS 10
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
struct open_file {
    FILE *fp;
    struct vnode *v_ptr
} of_entry;

// open file table array
struct open_file_table {
    of_entry open_file_table[OPEN_MAX];
    int cur_index;
} of_table;

// file descriptor entry 
struct fd_entry {
    of_entry *process_fd_array[MAX_PROCESS];
    int cur_index;
} fd_entry;

/* HELPER FUNCTIONS */
fd_entry *initialize_fd_table(void);
of_entry *create_open_file(FILE *fp, struct vnode *v_ptr);
of_table *create_of_table(void);
int add_to_of_table(of_entry *ofptr, of_table of_table[]);
int add_to_fd_table(int process, of_entry *process_fd_table[][], of_entry *ofptr);

#endif /* _FILE_H_ */
