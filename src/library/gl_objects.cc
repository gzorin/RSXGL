#include "gl_types.h"
#include "object_namespace.h"

#include <string.h>

// textures:
typedef object_namespace< texture_t > textures_namespace;
typedef textures_namespace::type textures_t;

static textures_t textures;

extern "C" uint32_t
textures_gen(uint32_t n,uint32_t * names)
{
  return textures_namespace::gen(textures,n,names);
}

extern "C" struct texture_t *
texture_find(uint32_t n)
{
  return textures_namespace::find(textures,n);
}

extern "C" struct texture_t *
texture_find_or_create(uint32_t n)
{
  texture_t tmp;
  memset(&tmp,0,sizeof(texture_t));
  tmp.object = 1;
  return textures_namespace::find_or_create(textures,n,tmp);
}

extern "C" void
texture_destroy(uint32_t n)
{
  return textures_namespace::destroy(textures,n);
}

// samplers:
typedef object_namespace< sampler_t > samplers_namespace;
typedef samplers_namespace::type samplers_t;

static samplers_t samplers;

extern "C" uint32_t
samplers_gen(uint32_t n,uint32_t * names)
{
  return samplers_namespace::gen(samplers,n,names);
}

extern "C" struct sampler_t *
sampler_find(uint32_t n)
{
  return samplers_namespace::find(samplers,n);
}

extern "C" struct sampler_t *
sampler_find_or_create(uint32_t n)
{
  sampler_t tmp;
  memset(&tmp,0,sizeof(sampler_t));
  return samplers_namespace::find_or_create(samplers,n,tmp);
}

extern "C" void
sampler_destroy(uint32_t n)
{
  return samplers_namespace::destroy(samplers,n);
}
