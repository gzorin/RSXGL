void *
dlopen(const char * path,int mode)
{
  return 0;
}

void *
dlsym(void * handle,const char * symbol)
{
  return 0;
}

int
dlclose(void * handle)
{
  return 01;
}

const char *
dlerror(void)
{
  return "lol, no libdl on the PS3!";
}
