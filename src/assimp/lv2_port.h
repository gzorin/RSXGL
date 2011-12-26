#define ASSIMP_BUILD_NO_COB_IMPORTER 1
#define ASSIMP_BUILD_NO_MD3_IMPORTER 1
#define ASSIMP_BUILD_NO_MDL_IMPORTER 1
#define ASSIMP_BUILD_NO_Q3D_IMPORTER 1
#define ASSIMP_BUILD_NO_Q3BSP_IMPORTER 1
#define ASSIMP_BUILD_NO_IFC_IMPORTER 1
#define ASSIMP_BUILD_NO_X_IMPORTER 1
#define ASSIMP_BUILD_NO_HMP_IMPORTER 1
#define ASSIMP_BUILD_NO_BLEND_IMPORTER 1

#include <string.h>
#include <unistd.h>

static char *realpath(const char *path, char *resolved)
{
  if(resolved != 0) {
    strncpy(resolved,path,MAXPATHLEN);
    return resolved;
  }
  else {
    return strdup(path);
  }
}

