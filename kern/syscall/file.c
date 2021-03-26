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
    new_open_file->file_offset = 0;
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
/*
int32_t sys_close(int fd) {
    return 0;
}

ssize_t sys_read(int fd, void *buf, size_t buflen) {
    return (ssize_t)0;
}*/

ssize_t sys_write(int fd, const void *buf, size_t nbytes) { 

    int result;

    struct uio *u = kmalloc(sizeof(struct uio));
    struct iovec *iov = kmalloc(sizeof(struct iovec));
    
    if (curproc->file_table[fd] == NULL) {
        curproc->file_table[fd] = create_open_file();
    }

    uio_kinit(iov, u, (void *)buf, nbytes, curproc->file_table[fd]->file_offset, UIO_WRITE);

    result = VOP_WRITE(curproc->file_table[fd]->v_ptr, u);

    if (result) {
        return result;
    }

    // set file offset to updated offset after write
    curproc->file_table[fd]->file_offset = u->uio_offset;

    size_t bytes_written = nbytes - u->uio_resid;

    return bytes_written;    
}

/*
off_t sys_lseek(int fd, off_t pos, int whence) {
    return (ssize_t)0;
}

int32_t sys_dup2(int oldfd, int newfd) {
    return 0;
}
*/

