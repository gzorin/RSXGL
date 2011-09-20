// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// limits.h - Set various limits particular to this implementation. Changing
// these will affect the amount of resources consumed by the library.

#ifndef rsxgl_limits_H
#define rsxgl_limits_H

// Limits of the hardware (Cell & RSX) go here. These shouldn't be changed:
#define RSXGL_CACHE_LINE_SIZE 128
#define RSXGL_CACHE_LINE_BITS 7

#define RSXGL_MAX_DRAW_BATCH_SIZE 256
#define RSXGL_MAX_FIFO_METHOD_ARGS 2047

#define RSXGL_MAX_SYNC_OBJECTS 192
// End hardware limits

// These limits are arbitrary, but they ought to correspond to the maximum value of various
// integral types + 1, as they are used to define the types that represent the "names" of
// various types of objects.
#define RSXGL_MAX_ARENAS 256

#define RSXGL_MAX_BUFFERS 65536

#define RSXGL_MAX_VERTEX_ARRAYS 65536

#define RSXGL_MAX_SHADERS 512
#define RSXGL_MAX_PROGRAMS 512

#define RSXGL_MAX_SAMPLERS 65536
#define RSXGL_MAX_TEXTURES 65536

#define RSXGL_MAX_RENDERBUFFERS 65536
#define RSXGL_MAX_FRAMEBUFFERS 65536

#define RSXGL_MAX_QUERIES 65536

// For glFinish, number of iterations to wait before giving up on the GPU.
#define RSXGL_FINISH_SLEEP_ITERATIONS 100000

// Time interval, in microseconds, to sleep while waiting to sync with the RSX
#define RSXGL_SYNC_SLEEP_INTERVAL 30

#define RSXGL_MIGRATE_BUFFER_ALIGN 16
#define RSXGL_MIGRATE_BUFFER_LOCATION 0

// Maximum value for a drawing timestamp. It's set this way so that GL objects
// can have 1 bit for a deleted flag, and the remaining 31 bits for a timestamp.
#define RSXGL_MAX_TIMESTAMP (((uint32_t)1 << 31) - 1)

// End software limits.

#endif
