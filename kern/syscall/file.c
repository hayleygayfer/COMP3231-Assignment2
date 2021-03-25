#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>

#define MAX_FILENAME_LEN 128

int current_of_index; // assume global variables initialised to 0
of_entry open_file_table[OPEN_MAX];

/*
 * Add your file-related functions here ...
 */

/* INITIALISE THE TABLES */

// create a new open file | return pointer to the open_file struct
of_entry *create_open_file(void) {
    of_entry *new_open_file = kmalloc(sizeof(of_entry));
    new_open_file->fp = NULL;
    new_open_file->v_ptr = NULL;

    return new_open_file;
}

// add to the open file table / array | return success 0 or failure 1
int add_to_of_table(of_entry *ofptr) {

    if (current_of_index >= OPEN_MAX) return 1;

    /* ADD SYNCHRONISATION HERE */
    
    open_file_table[current_of_index] = *ofptr;
    current_of_index++;

    /*--------------------------*/

    return 0;
}

/*
// add to a descriptor array | return success 0 or failure 1
int add_to_fd_table(int process, fd_entry *process_fd_table, of_entry *ofptr) {

    int current_index = process_fd_table[process].cur_index;

    if (current_index >= MAX_PROCESS) return 1;

    process_fd_table[process].process_fd_array[current_index] = ofptr;
    process_fd_table[process].cur_index++;

    return 0;
}
*/

// SYSCALL INTERFACE FUNCTIONS
int32_t sys_open(userptr_t filename, int flags, mode_t mode) {
    
    char sname[MAX_FILENAME_LEN];
    size_t *got = NULL;

    // Converting the filename into a string
    copyinstr(filename, sname, (size_t)MAX_FILENAME_LEN, got);

    // Creating open file node 
    of_entry *ret = create_open_file();
    int result = vfs_open(sname, flags, mode, &ret->v_ptr);
    if (result != 0) return 1;

    // Inserting open file node into the open file table
    int ret_val = add_to_of_table(ret);
    if (ret_val == 1) return 1;

    // changing the current proc to point to the open file node
    int i = 3;
    while (curproc->file_table[i] != NULL) {
        i++;
    }

    curproc->file_table[i] = ret;

    return 0;
}

