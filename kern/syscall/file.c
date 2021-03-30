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
#define ERROR            -1 // change to appropriate error codes

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
    new_open_file->flags = 0;
    new_open_file->ref_count = 0;

    return new_open_file;
}

// Free memory associated with of | return 1 if success or 0 if file non-existent
int free_open_file(of_entry *open_file) {
    
    if (open_file == NULL) return 0;

    kfree(open_file);
    
    return 1;
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

    if (filename == NULL) return EFAULT; // invalid filename ptr

    // Creating open file description, an entry in the system-wide
    // table of open files 
    of_entry *ret = create_open_file();
    if (ret == NULL) return ENOMEM; // no memory

    // Open file and store virtual node in ret struct
    int result = vfs_open(sname, flags, mode, &(ret->v_ptr));
    if (result) return result; // return error if error exists

    // Inserting open file node into the open file table
    int ret_val = add_to_of_table(ret);
    if (ret_val) return ERROR;

    // The file descriptor returned by a successful call will be the
    // lowest-nubered file descriptor no currently open for the process    
    int fd = 3;
    while (curproc->file_table[fd] != NULL && fd < OPEN_MAX) {
        fd++;
    }

    if (fd == OPEN_MAX) return EMFILE; // too many open files

    // Inserting open file node into open file table
    curproc->file_table[fd] = ret;
    curproc->file_table[fd]->flags = flags & O_ACCMODE; // set access permissions (read or write)
    curproc->file_table[fd]->ref_count = 1;

    // If append, set file_offset to the end of the file
    if (flags & O_APPEND) {
        struct stat statbuf;
        result = VOP_STAT(curproc->file_table[fd]->v_ptr, &statbuf); 
        if (result) return ERROR;
        curproc->file_table[fd]->file_offset = statbuf.st_size; 
    }

    return fd;
}

int32_t sys_close(int fd) {
    // ERROR CHECKING

    if (fd < 0 || fd >= OPEN_MAX) return EBADF; // invalid fd

    if (curproc->file_table[fd] == NULL) return EBADF; // invalid fd

    of_entry *ret = curproc->file_table[fd];

    ret->ref_count--;

    // File not found in the file table
    if (ret == NULL) return ERROR;

    if (ret->ref_count == 0) {
        vfs_close(ret->v_ptr);

        /* If fd is the last file descriptor referring to the underlying
        open file description, the resources associated with the ofd are freed */

        free_open_file(curproc->file_table[fd]);
        curproc->file_table[fd] = NULL; // update fd process table
        return 0;
    }

    return 0;
}


ssize_t sys_read(int fd, void *buf, size_t buflen) {

    // ERROR CHECKING 
    if (fd < 0 || fd >= OPEN_MAX) return EBADF; // invalid fd
    if (curproc->file_table[fd] == NULL) return EBADF; // invalid fd
    if (buf == NULL) return EFAULT; // address space invalid
    if (curproc->file_table[fd]->flags == O_WRONLY) return EACCES; // file is write only
    if (buflen <= 0) return EINVAL; // buflen cannot be 0 or negative

    of_entry *of_entry = curproc->file_table[fd];

    if (of_entry == NULL) return ERROR;

    struct iovec iov;
    struct uio u;

    // char *buffer = kmalloc(buflen);  

    uio_kinit(&iov, &u, buf, buflen, curproc->file_table[fd]->file_offset, UIO_READ);

    size_t bytes_remaining = u.uio_resid;

    int result = VOP_READ(of_entry->v_ptr, &u);
    if (result) return ERROR;

    bytes_remaining = buflen - u.uio_resid;

    curproc->file_table[fd]->file_offset = u.uio_offset;

    // kfree(buffer);

    return bytes_remaining;
}

ssize_t sys_write(int fd, const void *buf, size_t nbytes) { 
    // ERROR CHECKING 
    if (fd < 0 || fd >= OPEN_MAX) return EBADF; // invalid fd
    if (curproc->file_table[fd] == NULL) return EBADF; // invalid fd
    if (buf == NULL) return EFAULT; // address space invalid
    if (curproc->file_table[fd]->flags == O_RDONLY) return EACCES; // file is read only
    if (nbytes <= 0) return EINVAL; // buflen cannot be 0 or negative

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

off_t sys_lseek(int fd, off_t pos, int whence) {
    if (fd < 0 || fd >= OPEN_MAX) return EBADF; // invalid fd
    if (curproc->file_table[fd] == NULL) return EBADF; // invalid fd

    int result;

    struct stat file_stat; // find stat struct in kern/stat.h

    if (whence == SEEK_SET) {
        if (pos < 0) return EINVAL; // invalid offset
        result = curproc->file_table[fd]->file_offset = pos; // return and set given offset from start of file
    } else if (whence == SEEK_CUR) {
        result = curproc->file_table[fd]->file_offset += pos; // return and set given offset added to current offset of file
        if (result < 0) return EINVAL; // result of offset + cur position cannot be 0
    } else if (whence == SEEK_END) {
        result = VOP_STAT(curproc->file_table[fd]->v_ptr, &file_stat);
        if (result) return result; // return if error
        result = curproc->file_table[fd]->file_offset = pos + file_stat.st_size; // return end of file using size field of file stat
    } else {
        return EINVAL;
    }

    if (!VOP_ISSEEKABLE(curproc->file_table[fd]->v_ptr)) {
        return ESPIPE; // cannot seek
    }

    return result;
}

int32_t sys_dup2(int oldfd, int newfd) {
    if (oldfd < 0 || oldfd >= OPEN_MAX) return EBADF; // invalid oldfd
    if (newfd < 0 || newfd >= OPEN_MAX) return EBADF; // invalid oldfd
    if (curproc->file_table[oldfd] == NULL) return EBADF; // invalid fd

    int result;

    if (curproc->file_table[newfd] != NULL) {
        result = sys_close(newfd); // attempt to close file if new fd is occupied
        if (result) return result; // return error if failed
    }

    curproc->file_table[newfd] = curproc->file_table[oldfd]; // put a copy of of_entry in fd into newfd
    curproc->file_table[newfd]->ref_count++;

    return newfd;
}

int run_stdio() {
    int result;
    /* handling stdin (0), stdout (1), stderr (2) */

    /*------------------STDIN---------------------*/
    of_entry *stdin = create_open_file();

    curproc->file_table[0] = stdin;
    if (curproc->file_table[0] == NULL) return ENOMEM;

    curproc->file_table[0]->ref_count = 1;
    curproc->file_table[0]->flags = O_RDONLY;

    // struct vnode *stdin_vptr;
    struct vnode *stdin_vptr;
    // struct vnode *stderr_vptr;

    char c0[] = "con:";
    
    // vfs_open(stdio_path, O_RDONLY, 0, &stdin_vptr);
    result = vfs_open(c0, O_RDONLY, 0, &stdin_vptr);
    // vfs_open(stdio_path, O_WRONLY, 0, &stderr_vptr);

    if (result) return result; // error handling

    curproc->file_table[0]->v_ptr = stdin_vptr;

    sys_close(0); // fd 0 can start closed

    /*------------------STOUT---------------------*/

    char c1[] = "con:";

    of_entry *stdout = create_open_file();

    curproc->file_table[1] = stdout;
    if (curproc->file_table[1] == NULL) return ENOMEM;

    curproc->file_table[1]->ref_count = 1;
    curproc->file_table[1]->flags = O_WRONLY;

    struct vnode *stdout_vptr;

    result = vfs_open(c1, O_WRONLY, 0, &stdout_vptr);

    curproc->file_table[1]->v_ptr = stdout_vptr;

	if (result) return result; // error handling

    /*------------------STERR---------------------*/

    char c2[] = "con:";

    of_entry *stderr = create_open_file();

    curproc->file_table[2] = stderr;
    if (curproc->file_table[2] == NULL) return ENOMEM;

    curproc->file_table[2]->ref_count = 1;
    curproc->file_table[2]->flags = O_WRONLY;

    struct vnode *stderr_vptr;

    result = vfs_open(c2, O_WRONLY, 0, &stderr_vptr);

    curproc->file_table[2]->v_ptr = stderr_vptr;

	if (result) return result; // error handling

    return 0;
}


