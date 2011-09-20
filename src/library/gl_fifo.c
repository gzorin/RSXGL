#include "gl_fifo.h"

int32_t __attribute__((noinline))
gcm_reserve_callback(gcmContextData *context,uint32_t count)
{
  register int32_t result asm("3");
  asm volatile (
		"stdu	1,-128(1)\n"
		"mr		31,2\n"
		"lwz	0,0(%0)\n"
		"lwz	2,4(%0)\n"
		"mtctr	0\n"
		"bctrl\n"
		"mr		2,31\n"
		"addi	1,1,128\n"
		: : "b"(context->callback)
		: "r31", "r0", "r1", "r2", "lr"
		);
  return result;
}
