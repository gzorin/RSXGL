//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// migrate.h - Migrate client data to RSX memory.

#ifndef rsxgl_migrate_H
#define rsxgl_migrate_H

#include "mem.h"
#include "gcm.h"

#include <stdint.h>

rsx_ptr_t rsxgl_ringbuffer_migrate_memalign(gcmContextData *,const rsx_size_t,const rsx_size_t);
void rsxgl_ringbuffer_migrate_free(gcmContextData *,const rsx_ptr_t,const rsx_size_t);
void rsxgl_ringbuffer_migrate_reset(gcmContextData *);

rsx_ptr_t rsxgl_dumb_migrate_memalign(gcmContextData *,const rsx_size_t,const rsx_size_t);
void rsxgl_dumb_migrate_free(gcmContextData *,const rsx_ptr_t,const rsx_size_t);
void rsxgl_dumb_migrate_reset(gcmContextData *);

//#define rsxgl_migrate_memalign rsxgl_dumb_migrate_memalign
//#define rsxgl_migrate_free rsxgl_dumb_migrate_free
//#define rsxgl_migrate_reset rsxgl_dumb_migrate_reset

#define rsxgl_migrate_memalign rsxgl_ringbuffer_migrate_memalign
#define rsxgl_migrate_free rsxgl_ringbuffer_migrate_free
#define rsxgl_migrate_reset rsxgl_ringbuffer_migrate_reset

#endif
