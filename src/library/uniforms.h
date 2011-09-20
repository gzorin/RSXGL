//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// uniforms.h - Private constants, types and functions related to setting program uniform variables.

#ifndef rsxgl_uniforms_H
#define rsxgl_uniforms_H

#include "gl_constants.h"
#include "rsxgl_limits.h"
#include "program.h"

struct rsxgl_context_t;

void rsxgl_uniforms_validate(rsxgl_context_t *,program_t &);

#endif
