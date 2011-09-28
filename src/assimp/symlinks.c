// PSL1GHT SDK doesn't implement lstat() or readlink() (perhaps GameOS doesn't support
// symbolic links - I don't know). So so functions are implemented here.

#include <sys/stat.h>
#include <errno.h>

int
lstat(const char *restrict path, struct stat *restrict buf)
{
  errno = ENOENT;
  return -1;
}

ssize_t
readlink(const char *path, char *buf, size_t bufsiz)
{
  errno = ENOENT;
  return -1;
}
