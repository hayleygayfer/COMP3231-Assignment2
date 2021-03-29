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
#define ERROR            1 // change to appropriate error codes

#define STDIN            0
#define STDOUT           1
#define STDERR           2

/* GLOBAL VARIABLES */

int current_of_index; // init to 0
of_entry open_file_table[OPEN_MAX];

/* FILE RELATED FUNCTIONS */

// create a new open file | return pointer to the open_file struct
of_entry *create_open_file(void) {

    of_entry *new_open_file = kmalloc(sizeof(of_entry));
    
    new_open_file->file_offset = 0;
    new_open_file->v_ptr = NULL;

    return new_open_file;
}

// Add to open file table if the table has not reached capacity
int add_to_of_table(of_entry *ofptr) {

    if (current_of_index >= OPEN_MAX) return ERROR;

    /* ADD SYNCHRONISATION HERE */
    open_file_table[current_of_index] = *ofptr;
    current_of_index++;
    /*--------------------------*/

    return 0;
}

/* SYSCALL INTERFACE FUNCTIONS */

int32_t sys_open(userptr_t filename, int flags, mode_t mode) {

    // Copy filename string into kernel-space 
    char sname[MAX_FILENAME_LEN];
    size_t *string_length = NULL;
    copyinstr(filename, sname, (size_t)MAX_FILENAME_LEN, string_length);

    // Creating open file description, an entry in the system-wide
    // table of open files 
    of_entry *ret = create_open_file();

    // Open file and store virtual node in ret struct
    int result = vfs_open(sname, flags, mode, &(ret->v_ptr));
    if (result) return ERROR;

    // Inserting open file node into the open file table
    int ret_val = add_to_of_table(ret);
    if (ret_val) return ERROR;

    // The file descriptor returned by a successful call will be the
    // lowest-nubered file descriptor no currently open for the process    
    int fd = 3;
    while (curproc->file_table[fd] != NULL) {
        fd++;
    }

    // Inserting open file node into open file table
    curproc->file_table[fd] = ret;

    // If append, set file_offset to the end of the file
    if (flags & O_APPEND) {
        struct stat statbuf;
        result = VOP_STAT(curproc->file_table[fd]->v_ptr, &statbuf); 
        if (result) return ERROR;
        curproc->file_table[fd]->file_offset = statbuf.st_size; 
    }

    return fd;
}
/*
int32_t sys_close(int fd) {
    return 0;
}

ssize_t sys_read(int fd, void *buf, size_t buflen) {
    return (ssize_t)0;
}*/

ssize_t sys_write(int fd, const void *buf, size_t nbytes) { 
    
    // VERY DODGY THIS SHOULD BE SOMEWHERE ELSE     
    if (curproc->file_table[fd] == NULL) {
        // kprintf("\n fd %d | ", fd);
        curproc->file_table[fd] = create_open_file();
        
        /* handling stdin (0), stdout (1), stderr (2) */

        // struct vnode *stdin_vptr;
        struct vnode *stdout_vptr;
        // struct vnode *stderr_vptr;

        char stdio_path[] = "con:";
        
        // vfs_open(stdio_path, O_RDONLY, 0, &stdin_vptr);
        vfs_open(stdio_path, O_WRONLY, 0, &stdout_vptr);
        // vfs_open(stdio_path, O_WRONLY, 0, &stderr_vptr);

        curproc->file_table[fd]->v_ptr = stdout_vptr;
    }

    int result;

    struct uio u;
    struct iovec iov;

    // Copy buf string into kernel-space 
    char *buffer = kmalloc(nbytes);  
    copyin((const_userptr_t)buf, buffer, nbytes);

    // Initialise uio suitable for I/O from a kernal buffer
    uio_kinit(&iov, &u, buffer, nbytes, curproc->file_table[fd]->file_offset, UIO_WRITE);
    result = VOP_WRITE(curproc->file_table[fd]->v_ptr, &u);
    if (result) return ERROR;

    // Update file offset
    curproc->file_table[fd]->file_offset = u.uio_offset;

    size_t bytes_written = nbytes - u.uio_resid;

    kfree(buffer);

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

