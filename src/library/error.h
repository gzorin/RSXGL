#ifndef rsxgl_error_H
#define rsxgl_error_H

#ifdef __cplusplus
extern "C" {
#endif

// TODO - Compile-time options to affect GL error detection behavior, with the aim
// of creating a library that (optionally) doesn't perform GL error checking at all.
// Making the RSXGL_*ERROR() macros no-op's could induce the compiler to eliminate
// the code paths that check for & set these errors. Additionally, there ought to
// be options to trace or assert when a GL error is detected, to assist in gradually
// eliminating these from a client program so that GL error checking can safely
// be removed. OpenGL also has the ARB_debug_output extension, apparently based
// upon an AMD extension, which produces information about GL errors when they occur;
// this might be nice to support as well.

// Macros for reporting errors & returning from a function:
extern GLenum rsxgl_error;

static inline void
rsxeglSetError(GLenum e)
{
  if(rsxgl_error == GL_NO_ERROR && e != GL_NO_ERROR) {
    rsxgl_error = e;
  }
}

#define RSXGL_NOERROR(RETURN)			\
  {rsxeglSetError(GL_NO_ERROR);			\
    return (RETURN);}

#define RSXGL_ERROR(ERROR,RETURN)		\
  {rsxeglSetError((ERROR));			\
    return (RETURN);}

#define RSXGL_NOERROR_()			\
  {rsxeglSetError(GL_NO_ERROR);			\
    return ;}

#define RSXGL_ERROR_(ERROR)			\
  {rsxeglSetError((ERROR));			\
    return ;}

#define RSXGL_IS_ERROR() (rsxgl_error != GL_NO_ERROR)

#ifdef __cplusplus
}
#endif

#endif
