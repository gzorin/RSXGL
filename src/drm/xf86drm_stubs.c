#include "xf86drm.h"

// libdrm stubs:
int           drmAvailable(void) {}
int           drmOpen(const char *name, const char *busid) {}
int drmOpenControl(int minor) {}
int           drmClose(int fd) {}
drmVersionPtr drmGetVersion(int fd) {}
drmVersionPtr drmGetLibVersion(int fd) {}
int           drmGetCap(int fd, uint64_t capability, uint64_t *value) {}
void          drmFreeVersion(drmVersionPtr ptr) {}
int           drmGetMagic(int fd, drm_magic_t * magic) {}
char          *drmGetBusid(int fd) {}
int           drmGetInterruptFromBusID(int fd, int busnum, int devnum,
					      int funcnum) {}
int           drmGetMap(int fd, int idx, drm_handle_t *offset,
			       drmSize *size, drmMapType *type,
			       drmMapFlags *flags, drm_handle_t *handle,
			       int *mtrr) {}
int           drmGetClient(int fd, int idx, int *auth, int *pid,
				  int *uid, unsigned long *magic,
				  unsigned long *iocs) {}
int           drmGetStats(int fd, drmStatsT *stats) {}
int           drmSetInterfaceVersion(int fd, drmSetVersion *version) {}
int           drmCommandNone(int fd, unsigned long drmCommandIndex) {}
int           drmCommandRead(int fd, unsigned long drmCommandIndex,
                                    void *data, unsigned long size) {}
int           drmCommandWrite(int fd, unsigned long drmCommandIndex,
                                     void *data, unsigned long size) {}
int           drmCommandWriteRead(int fd, unsigned long drmCommandIndex,
				  void *data, unsigned long size) {}

int           drmCreateContext(int fd, drm_context_t * handle) {}
int           drmDestroyContext(int fd, drm_context_t handle) {}
int drmIoctl(int fd, unsigned long request, void *arg) {}
