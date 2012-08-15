//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// texture_migrate.h - Establish memory to migrate texture data with

#ifndef rsxgl_texture_migrate_H
#define rsxgl_texture_migrate_H

#include <stdint.h>

#include "mem.h"


void *rsxgl_texture_migrate_buffer_new(const rsx_size_t align,const rsx_size_t size, uint32_t *offset);
void rsxgl_texture_migrate_buffer_free(void * ptr);
void * rsxgl_texture_migrate_memalign(const rsx_size_t,const rsx_size_t);
void rsxgl_texture_migrate_free(void *);
void rsxgl_texture_migrate_reset();
void * rsxgl_texture_migrate_address(const uint32_t);
uint32_t rsxgl_texture_migrate_offset(const void *);
void * rsxgl_texture_migrate_base();

#endif
