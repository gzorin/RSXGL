//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// migrate.h - Migrate client data to RSX memory.

#ifndef rsxgl_vertex_migrate_H
#define rsxgl_vertex_migrate_H

#include <stdint.h>

#include "mem.h"

typedef struct _gcmCtxData gcmContextData;

void * rsxgl_ringbuffer_migrate_memalign(gcmContextData *,const rsx_size_t,const rsx_size_t);
void rsxgl_ringbuffer_migrate_free(gcmContextData *,const void *,const rsx_size_t);
void rsxgl_ringbuffer_migrate_reset(gcmContextData *);

void * rsxgl_dumb_migrate_memalign(gcmContextData *,const rsx_size_t,const rsx_size_t);
void rsxgl_dumb_migrate_free(gcmContextData *,const void *,const rsx_size_t);
void rsxgl_dumb_migrate_reset(gcmContextData *);

//#define rsxgl_vertex_migrate_memalign rsxgl_dumb_migrate_memalign
//#define rsxgl_vertex_migrate_free rsxgl_dumb_migrate_free
//#define rsxgl_vertex_migrate_reset rsxgl_dumb_migrate_reset

#define rsxgl_vertex_migrate_memalign rsxgl_ringbuffer_migrate_memalign
#define rsxgl_vertex_migrate_free rsxgl_ringbuffer_migrate_free
#define rsxgl_vertex_migrate_reset rsxgl_ringbuffer_migrate_reset

#endif
