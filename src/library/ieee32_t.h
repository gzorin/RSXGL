//

#ifndef rsxgl_ieee32_t_H
#define rsxgl_ieee32_t_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef union {
  float f;
  uint32_t u;
  struct {
    uint16_t a[2];
  } h;
  struct {
    uint8_t a[4];
  } b;
} ieee32_t;

#if defined(__cplusplus)
}
#endif

#endif
