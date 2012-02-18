#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H

#include <sys/types.h>

#define MAP_FAILED ((void *) -1)

static void *mmap (void *__addr, size_t __len, int __prot,
                   int __flags, int __fd, __off_t __offset) 
{
  return MAP_FAILED;
}

static int munmap (void *__addr, size_t __len) 
{
  return -1;
}

#define __USE_MISC
/* The following definitions basically come from the kernel headers.                                                                                                                           
   But the kernel header is not namespace clean.  */


/* Protections are chosen from these bits, OR'd together.  The                                                                                                                                 
   implementation does not necessarily support PROT_EXEC or PROT_WRITE                                                                                                                         
   without PROT_READ.  The only guarantees are that no writing will be                                                                                                                         
   allowed without PROT_WRITE and no access will be allowed for PROT_NONE. */

#define PROT_READ       0x1             /* Page can be read.  */
#define PROT_WRITE      0x2             /* Page can be written.  */
#define PROT_EXEC       0x4             /* Page can be executed.  */
#define PROT_NONE       0x0             /* Page can not be accessed.  */
#define PROT_GROWSDOWN  0x01000000      /* Extend change to start of                                                                                                                           
                                           growsdown vma (mprotect only).  */
#define PROT_GROWSUP    0x02000000      /* Extend change to start of                                                                                                                           
                                           growsup vma (mprotect only).  */

/* Sharing types (must choose one and only one of these).  */
#define MAP_SHARED      0x01            /* Share changes.  */
#define MAP_PRIVATE     0x02            /* Changes are private.  */
#ifdef __USE_MISC
# define MAP_TYPE       0x0f            /* Mask for type of mapping.  */
#endif

/* Other flags.  */
#define MAP_FIXED       0x10            /* Interpret addr exactly.  */
#ifdef __USE_MISC
# define MAP_FILE       0
# define MAP_ANONYMOUS  0x20            /* Don't use a file.  */
# define MAP_ANON       MAP_ANONYMOUS
# define MAP_32BIT      0x40            /* Only give out 32-bit addresses.  */
#endif

#undef __USE_MISC

#endif
