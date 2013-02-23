#include <iostream>
#include <stdio.h>
#include <assert.h>

#include <stdint.h>
#include <boost/tuple/tuple.hpp>
#include <boost/integer/static_log2.hpp>
#include <boost/bind.hpp>

//static const uint32_t batch_size = 256;
//static const uint32_t max_method_args = 2048;

#define RSXGL_BATCH_SIZE 256
#define RSXGL_MAX_METHOD_ARGS 2047

// 
struct counter {
  mutable uint32_t value;
  mutable uint32_t ncommands, lastgroupn;

  mutable uint32_t * commands, * pcommands;

  counter(uint32_t _value)
    : value(_value), ncommands(0), commands(0), pcommands(0), lastgroupn(0) {
  }

  ~counter() {
    if(commands != 0) delete [] commands;
  }

  inline
  void begin(const uint32_t ninvoc,const uint32_t ninvocremainder,const uint32_t nbatchremainder) const {
    if(commands != 0) delete [] commands;

    const uint32_t n =
      // method + 2048 arguments:
      ((2048 + 1) * ninvoc) +
      // method + ninvocremainder arguments:
      ((ninvocremainder > 0) ? (1 + ninvocremainder) : 0) +
      // method + 1 argument:
      ((nbatchremainder > 0) ? 2 : 0);

    commands = new uint32_t[n];
    pcommands = commands;

    ncommands = 0;
    lastgroupn = 0;
  }

  inline
  void begin_group(const uint32_t n) const {
    //pcommands += (lastgroupn > 0) ? (lastgroupn) : 0;
    pcommands += lastgroupn;
    assert(ncommands == (pcommands - commands));

    pcommands[0] = 0xB00B5000 | n;
    ++pcommands;

    lastgroupn = n;
    ++ncommands;
  }

  inline
  void begin_batch(uint32_t igroup,const uint32_t n) const {
    pcommands[igroup] = ((n - 1) << 24) | value;

    value += n;
    ++ncommands;
  }

  inline
  void end() const {
    pcommands += lastgroupn;
  }
};

#if 0
struct counter {
  uint32_t value;
  uint32_t ncommands, lastgroupn;

  uint32_t * commands, * pcommands;

  counter(uint32_t _value)
    : value(_value), ncommands(0), commands(0), pcommands(0), lastgroupn(0) {
  }

  ~counter() {
    if(commands != 0) delete [] commands;
  }

  static inline void
  begin(counter & c,const uint32_t ninvoc,const uint32_t ninvocremainder,const uint32_t nbatchremainder) {
  }

  static inline void
  begin_group(counter & c,
};

template< uint32_t batch_size, uint32_t max_method_args, typename Data,
	  void (*Begin)(Data &,const uint32_t,const uint32_t,const uint32_t),
	  void (*Group)(Data &,const uint32_t),
	  void (*Batch)(Data &,const uint32_t,const uint32_t),
	  void (*End)(Data &)
	  >
void rsxgl_process_batch(const uint32_t n,Data & data)
{
  static const uint32_t batch_size_bits = boost::static_log2< batch_size >::value;
  const uint32_t nbatch = n >> batch_size_bits;
  const uint32_t nbatchremainder = n & (batch_size - 1);

  uint32_t ninvoc = nbatch / max_method_args;
  const uint32_t ninvocremainder = nbatch % max_method_args;

  std::cerr << ninvoc << " " << ninvocremainder << " " << nbatch << " " << nbatchremainder << std::endl;

  (*Begin)(data,ninvoc,ninvocremainder,nbatchremainder);

  for(;ninvoc > 0;--ninvoc) {
    //object.begin_group(max_method_args);
    (*Group)(data,max_method_args);

    for(size_t i = 0;i < max_method_args;++i) {
      //object.begin_batch(i,batch_size);
      (*Batch)(data,i,batch_size);
    }
  }

  if(ninvocremainder > 0 && nbatchremainder > 0 && ninvocremainder < max_method_args) {
    //object.begin_group(ninvocremainder + 1);
    (*Group)(data,ninvocremainder + 1);

    for(size_t i = 0;i <= ninvocremainder;++i) {
      //object.begin_batch(i,batch_size);
      (*Batch)(data,i,batch_size);
    }
  }
  else {
    if(ninvocremainder > 0) {
      //object.begin_group(ninvocremainder);
      (*Group)(data,ninvocremainder);
      for(size_t i = 0;i < ninvocremainder;++i) {
	//object.begin_batch(i,batch_size);
	(*Batch)(data,i,batch_size);
      }
    }
    
    // one batch of < 256:
    if(nbatchremainder > 0) {
      //object.begin_group(1);
      //object.begin_batch(0,nbatchremainder);
      (*Group)(data,1);
      (*Batch)(data,0,nbatchremainder);
    }
  }

  //object.end();
}
#endif

template< uint32_t batch_size, uint32_t max_method_args, typename Object >
void rsxgl_process_batch(const uint32_t n,const Object & object)
{
  static const uint32_t batch_size_bits = boost::static_log2< batch_size >::value;
  const uint32_t nbatch = n >> batch_size_bits;
  const uint32_t nbatchremainder = n & (batch_size - 1);

  uint32_t ninvoc = nbatch / max_method_args;
  const uint32_t ninvocremainder = nbatch % max_method_args;

  std::cerr << ninvoc << " " << ninvocremainder << " " << nbatch << " " << nbatchremainder << std::endl;

  object.begin(ninvoc,ninvocremainder,nbatchremainder);

  for(;ninvoc > 0;--ninvoc) {
    object.begin_group(max_method_args);
    for(size_t i = 0;i < max_method_args;++i) {
      object.begin_batch(i,batch_size);
    }
  }

  if(ninvocremainder > 0 && nbatchremainder > 0 && ninvocremainder < max_method_args) {
    object.begin_group(ninvocremainder + 1);
    for(size_t i = 0;i <= ninvocremainder;++i) {
      object.begin_batch(i,batch_size);
    }
  }
  else {
    if(ninvocremainder > 0) {
      object.begin_group(ninvocremainder);
      for(size_t i = 0;i < ninvocremainder;++i) {
	object.begin_batch(i,batch_size);
      }
    }
    
    // one batch of < 256:
    if(nbatchremainder > 0) {
      object.begin_group(1);
      object.begin_batch(0,nbatchremainder);
    }
  }

  object.end();
}

uint32_t
do_this_shit(uint32_t n)
{
  counter c(0);
  rsxgl_process_batch< RSXGL_BATCH_SIZE, RSXGL_MAX_METHOD_ARGS >(n,c);

  fprintf(stderr,"%u vertices\n",c.value);
  for(size_t i = 0,n = c.ncommands;i < n;++i) {
    fprintf(stdout,"%x\n",c.commands[i]);
  }

  return c.value;
}

int
main(int argc, char ** argv)
{
  uint32_t n = /*64*/ /*256*/ /*1000*1000*5*/ /*10 * 1000*/ 2047 * 256 /*256*/;

  do_this_shit(n);
}
