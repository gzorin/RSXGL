//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// gl_constants.h - Constants that are mostly related to capabilities and limits
// that OpenGL can report on.

#ifndef rsxgl_gl_constants_H
#define rsxgl_gl_constants_H

#include <boost/mpl/min_max.hpp>

#define RSXGL_MAX_VERTEX_ATTRIBS 16
#define RSXGL_MAX_COMBINED_TEXTURE_IMAGE_UNITS 16
#define RSXGL_MAX_TEXTURE_COORDS 8
#define RSXGL_MAX_TEXTURE_SIZE 4096
#define RSXGL_MAX_CUBE_MAP_TEXTURE_SIZE 4096
#define RSXGL_MAX_3D_TEXTURE_SIZE 512

#define RSXGL__VERTEX__MAX_PROGRAM_INSTRUCTIONS 512
#define RSXGL__FRAGMENT__MAX_PROGRAM_INSTRUCTIONS 4096

#define RSXGL_MAX_PROGRAM_INSTRUCTIONS (boost::mpl::max< boost::mpl::int_< RSXGL__VERTEX__MAX_PROGRAM_INSTRUCTIONS >,boost::mpl::int_< RSXGL__FRAGMENT__MAX_PROGRAM_INSTRUCTIONS > >::type::value)

#define RSXGL__VERTEX__MAX_PROGRAM_UNIFORM_COMPONENTS 2048
#define RSXGL__FRAGMENT__MAX_PROGRAM_UNIFORM_COMPONENTS 1024

#define RSXGL_MAX_PROGRAM_UNIFORM_COMPONENTS (boost::mpl::max< boost::mpl::int_< RSXGL__VERTEX__MAX_PROGRAM_UNIFORM_COMPONENTS >,boost::mpl::int_< RSXGL__FRAGMENT__MAX_PROGRAM_UNIFORM_COMPONENTS > >::type::value)

#define RSXGL_MAX_SAMPLES 0
#define RSXGL_MAX_DRAW_BUFFERS 4
#define RSXGL_MAX_COLOR_ATTACHMENTS 4
#define RSXGL_MAX_RENDERBUFFER_SIZE 4096

#endif
