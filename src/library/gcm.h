/***
 * These are functions that are provided by libgcm_sys.sprx, which is the only
 * part of libgcm that we are leagally allowed to use.
 *
 * TODO: Fill out the rest of theses based on psl1ght/sprx/libgcm_sys/exports.h
 */

#ifndef gcm_H
#define gcm_H

#include <rsx/gcm_sys.h>
#include <stdint.h>

#define ATTRIBUTE_PRXPTR __attribute__((mode(SI)))

//typedef void * rsx_ptr_t ATTRIBUTE_PRXPTR;
typedef void * rsx_ptr_t;

typedef uint32_t rsx_size_t;

#define RSX_PTR_CAST(TYPE,PTR) ((TYPE)(uint64_t)PTR)

#endif
