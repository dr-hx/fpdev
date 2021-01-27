#ifndef REAL_CONFIGURE_H
#define REAL_CONFIGURE_H


// Port types
#define MPFR_PORT 0
#define DD_PORT 1
#define QD_PORT 2



// Default port is defined here
#ifndef PORT_TYPE
#define PORT_TYPE DD_PORT
#endif

/* 
    A flag that determines whether an original execution should be performed along with the shadow execution.
    This flag should be used for debugging.
*/
#ifndef KEEP_ORIGINAL
#define KEEP_ORIGINAL true
#endif

#define ORACLE_MODE 0
#define DEBUGING_MODE 1
#define RANGE_ACTIVE_MODE 2
#define FULL_ACTIVE_MODE 3


#ifndef TRANCKING_MODE
#define TRANCKING_MODE FULL_ACTIVE_MODE
#endif


#ifndef TRANCKING_MODE
// Manual setting
#define TRACK_ERROR true
#define ACTIVE_TRACK_ERROR true
#define TRACKING_ON true
#else
#if TRANCKING_MODE == ORACLE_MODE
#define TRACK_ERROR false
#define ACTIVE_TRACK_ERROR false
#define TRACKING_ON false
#elif TRANCKING_MODE == DEBUGING_MODE
#define TRACK_ERROR true
#define ACTIVE_TRACK_ERROR false
#define TRACKING_ON false
#elif TRANCKING_MODE == RANGE_ACTIVE_MODE
#define TRACK_ERROR true
#define ACTIVE_TRACK_ERROR true
#define TRACKING_ON false
#elif TRANCKING_MODE == FULL_ACTIVE_MODE
#define TRACK_ERROR true
#define ACTIVE_TRACK_ERROR true
#define TRACKING_ON true
#endif
#endif


// tracking on is valid only when ACTIVE_TRACK_ERROR is true
#if TRACKING_ON
#ifdef ACTIVE_TRACK_ERROR
#undef ACTIVE_TRACK_ERROR
#endif
#define ACTIVE_TRACK_ERROR true
#endif

// default setting of tracking on
#ifndef TRACKING_ON
#define TRACKING_ON true
#endif

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