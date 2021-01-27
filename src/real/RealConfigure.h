#ifndef REAL_CONFIGURE_H
#define REAL_CONFIGURE_H
// Port types
#define MPFR_PORT 0
#define DD_PORT 1
#define QD_PORT 2



// Default port is defined here
#define PORT_TYPE DD_PORT


/* 
    A flag that determines whether an original execution should be performed along with the shadow execution.
    This flag should be used for debugging.
*/
#define KEEP_ORIGINAL true


#define TRACK_ERROR true

#define ACTIVE_TRACK_ERROR true

#if ACTIVE_TRACK_ERROR
#ifdef TRACK_ERROR
#undef TRACK_ERROR
#endif
#ifdef KEEP_ORIGINAL
#undef KEEP_ORIGINAL
#endif
#define TRACK_ERROR true
#define KEEP_ORIGINAL true
#endif

/* 
    A flag that determines whether real values are stored in pool.
*/
#define DELEGATE_TO_POOL false

#define DEBUG_INTERNAL true

/* 
    A flag that determines whether the function is inlined.
*/
#define INLINE_FLAGS inline

#define CACHE_SIZE 0x8000

#endif