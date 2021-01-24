#ifndef REAL_CONFIGURE_H
#define REAL_CONFIGURE_H
// Port types
#define MPFR_PORT 0
#define DD_PORT 1
#define QD_PORT 2



// Default port is defined here
#define PORT_TYPE MPFR_PORT

/* 
    A flag that determines whether an original execution should be performed along with the shadow execution.
    This flag should be used for debugging.
*/
#define KEEP_ORIGINAL false

/* 
    A flag that determines whether real values are stored in pool.
*/
#define DELEGATE_TO_POOL false


/* 
    A flag that determines whether the function is inlined.
*/
#define INLINE_FLAGS 

#define CACHE_SIZE 0x8000

#endif