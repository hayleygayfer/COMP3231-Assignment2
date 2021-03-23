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

/*
 * Add your file-related functions here ...
 */

/* INITIALISE THE TABLES */

// file descriptor array | created per process | return the array (pointer to the array)
fd_entry *initialize_fd_table(void) {
    fd_entry per_process_fd_table[NUM_SYSCALLS];

    for (i = 0; i < NUM_SYSCALLS; i++) {
        per_process_fd_table[i].cur_index = 3;
    }

    return per_process_fd_table;
}

// create a new open file | return pointer to the open_file struct
of_entry *create_open_file(FILE *fp, struct vnode *v_ptr) {
    of_entry *new_open_file = malloc(sizeof(of_entry));
    new_open_file->fp = fp;
    new_open_file->v_ptr = v_ptr;

    return new_open_file;
}

of_table *create_of_table(void) {
    of_table *new_of_table = malloc(sizeof(of_table));
    new_of_table->cur_index = 0;

    return new_of_table;
}

// add to the open file table / array | return success 0 or failure 1
int add_to_of_table(of_entry *ofptr, of_table of_table[]) {
    if (of_table->cur_index >= OPEN_MAX) return 1;

    /* ADD SYNCHRONISATION HERE */
    of_table[of_table->cur_index] = of_entry;
    of_table->cur_index++;
    /*--------------------------*/

    return 0;
}

// add to a descriptor array | return success 0 or failure 1
int add_to_fd_table(int process, fd_entry *process_fd_table[], of_entry *ofptr) {
    if (process_fd_array[process].cur_index >= MAX_PROCESS) return 1;

    process_fd_table[process].process_fd_array[process_fd_array[process].cur_index] = ofptr;
    process_fd_array[process].cur_index++;
    return 0;
}

// SYSCALL INTERFACE FUNCTIONS



