// "Unit testing" for the implementation of gl_object.
//
// Test different object types:
// - normal object (there are none in OpenGL)
// - normal objects with a valid default object (VAO, textures)
// - reference counted objects (shader objects, programs, buffers)
// - bindable or not
// - test preallocating too

#include <iostream>
#include <string>
#include <stdexcept>
#include <cstdlib>
#include <vector>

struct assertion : public std::runtime_error {
  assertion(const std::string & info)
    : std::runtime_error(info) {
  }
};

#define cxx_assert(__e) ((__e) ? (void)0 : throw assertion(std::string(#__e)));
#define assert cxx_assert

#include "gl_object.h"

//
struct normal_object {
  static const size_t max_buffers = (1 << 16);
  static const size_t max_targets = 16;

  typedef bindable_gl_object< normal_object, max_buffers, max_targets > gl_object_type;
  typedef gl_object_type::name_type name_type;
  typedef gl_object_type::storage_type storage_type;
  typedef gl_object_type::binding_bitfield_type binding_bitfield_type;
  typedef gl_object_type::binding_type binding_type;

  static storage_type & storage();

  binding_bitfield_type binding_bitfield;

  uint32_t deleted;
  uint32_t ref_count;

  normal_object() : deleted(0), ref_count(0), contents(current_contents++) {
    std::cout << __PRETTY_FUNCTION__ << ":" << contents << std::endl;
  }

  ~normal_object() {
    std::cout << __PRETTY_FUNCTION__ << ":" << contents << std::endl;
  }

  void access(std::ostream & s) const {
    s << contents;
  }

  static uint32_t current_contents;
  uint32_t contents;
};

uint32_t normal_object::current_contents = 42;

normal_object::storage_type &
normal_object::storage()
{
  static normal_object::storage_type _storage(16);
  return _storage;
}

//
struct default_object {
  static const size_t max_buffers = (1 << 16);
  static const size_t max_targets = 16;

  typedef bindable_gl_object< default_object, max_buffers, max_targets, 1 > gl_object_type;
  typedef gl_object_type::name_type name_type;
  typedef gl_object_type::storage_type storage_type;
  typedef gl_object_type::binding_bitfield_type binding_bitfield_type;
  typedef gl_object_type::binding_type binding_type;

  static storage_type & storage();

  binding_bitfield_type binding_bitfield;

  default_object() : contents(current_contents++) {
    std::cout << __PRETTY_FUNCTION__ << ":" << contents << std::endl;
  }

  ~default_object() {
    std::cout << __PRETTY_FUNCTION__ << ":" << contents << std::endl;
  }

  void access(std::ostream & s) const {
    s << contents;
  }

  static uint32_t current_contents;
  uint32_t contents;
};

uint32_t default_object::current_contents = 42;

default_object::storage_type &
default_object::storage()
{
  static default_object::storage_type _storage;
  return _storage;
}

template< typename Object, size_t Size >
struct container_object : public object_container_type< Object, Size > {
  static const size_t max_objects = (1 << 16);
  static const size_t max_targets = 1;

  typedef container_object< Object, Size > this_type;
  typedef bindable_gl_object< this_type, max_objects, max_targets, 1 > gl_object_type;
  typedef typename gl_object_type::name_type name_type;
  typedef typename gl_object_type::storage_type storage_type;
  typedef typename gl_object_type::binding_bitfield_type binding_bitfield_type;
  typedef typename gl_object_type::binding_type binding_type;

  typedef object_container_type< Object, Size > container_type;

  static storage_type & storage();

  binding_bitfield_type binding_bitfield;

  uint32_t deleted;
  uint32_t ref_count;

#if 0
private:

  typename Object::name_type names[Size];
#endif

public:

  container_object() : deleted(0), ref_count(0), contents(current_contents++) {
    std::cout << __PRETTY_FUNCTION__ << ":" << contents << std::endl;

#if 0
    for(size_t i = 0;i < Size;++i) {
      names[i] = 0;
    }
#endif
  }

  ~container_object() {
    std::cout << __PRETTY_FUNCTION__ << ":" << contents << std::endl;

#if 0
    for(size_t i = 0;i < Size;++i) {
      if(names[i] == 0) continue;
      Object::gl_object_type::unref_and_maybe_delete(names[i]);
    }
#endif
  }

  void access(std::ostream & s) const {
    s << contents << " contains the following objects: ";
    for(size_t i = 0;i < Size;++i) {
      if(container_type::names[i] != 0) s << container_type::names[i] << " ";
    }
  }

  static uint32_t current_contents;
  uint32_t contents;

#if 0
  void bind(size_t i,const typename Object::name_type name) {
    assert(i < Size);

    const typename Object::name_type prev_name = names[i];
    names[i] = name;

    if(name != 0) Object::gl_object_type::ref(name);
    if(prev_name != 0) Object::gl_object_type::unref_and_maybe_delete(prev_name);
  }
#endif
};

template< typename Object, size_t Size >
uint32_t container_object< Object, Size >::current_contents = 13;

template< typename Object, size_t Size >
typename container_object< Object, Size >::storage_type &
container_object< Object, Size >::storage()
{
  static typename container_object< Object, Size >::storage_type _storage;
  return _storage;
}

//
template< typename Object >
void summarize()
{
#if !defined(NDEBUG)
  std::cout << Object::storage().current_potential_size() << " potential names (" << Object::storage().m_num_names << " actual) "
	    << Object::storage().contents_size() << " potential objects (" << Object::storage().m_num_objects << " actual) "
	    << Object::storage().orphans_size() << " potential orphans (" << Object::storage().m_num_orphans << " actual) "
	    << std::endl;
#else
  std::cout << Object::storage().current_potential_size() << " potential names "
	    << Object::storage().contents_size() << " potential objects "
	    << Object::storage().orphans_size() << " potential orphans (" << Object::storage().m_num_orphans << " actual) "
	    << std::endl;
#endif

  for(size_t i = 0;i < Object::storage().current_potential_size();++i) {
    std::cout << " name i: " << i
	      << " is_name: " << (int)Object::storage().is_name(i)
	      << " is_constructed: " << (int)Object::storage().is_constructed(i)
	      << " is_object: " << (int)Object::storage().is_object(i)
	      << std::endl;
  }
}

// Try accessing all the potentially available objects; some will fail:
template< typename Object >
void access()
{
  for(size_t i = 0;i < Object::storage().current_potential_size();++i) {
    std::cout << i << ": ";
    try {
      if(Object::storage().is_constructed(i) && !Object::storage().is_name(i)) std::cout << " ! ";
      Object::storage().at(i).access(std::cout);
    }
    catch (const assertion a) {
      std::cout << "possibly expected assertion: " << a.what();
    }
    std::cout << std::endl;
  }
}

template< typename Object >
void basic_tests()
{
  try {
    // Create some names:
    // - Should return 1 through 10
    // - Should not see any constructors get called in the process
    // - For each name, is_name(), is_constructed(), is_object() will return 1, 0, 0

    size_t num_created_objects = 0;

    // Summarize an empty set of objects:
    summarize< Object >();
    access< Object >();

    // Access object 0; should fail:
    {
      try {
	Object & object = Object::storage().at(0);
	assert(0);
      }
      catch (const assertion a) {
	std::cout << "expected assertion: " << a.what() << std::endl;
      }
    }

    // Detach object 0; should also fail:
    {
      try {
	Object::storage().detach(0);
	assert(0);
      }
      catch (const assertion a) {
	std::cout << "expected assertion: " << a.what() << std::endl;
      }
    }

    // Summarize an empty set of objects:
    summarize< Object >();
    access< Object >();

    const size_t n = 10;
    typename Object::name_type names[n];
    
    for(size_t i = 0;i < n;++i) {
      names[i] = Object::storage().create_name();
    }
    
    summarize< Object >();
    access< Object >();
    
    {
      // turn a sampling of names into actual objects:
      const size_t m = 5;
      typename Object::name_type objects[m];
      for(size_t i = 0;i < m;++i) {
	objects[m] = names[rand() % n];
	Object::storage().create_object(objects[m]);
	++num_created_objects;
      }
    }

    summarize< Object >();
    access< Object >();

    // detach a sampling of the objects:
    // this should not fail, but, afterward,
    {
      size_t count = 0;
      for(size_t i = 0;i < n;++i) {
	if(rand() % 2) {
	  if(Object::storage().is_object(names[i])) {
	    Object::storage().detach(names[i]);
	    ++count;
	  }
	}
      }
      std::cout << "detached " << count << " objects" << std::endl;
    }
    
    summarize< Object >();
    access< Object >();
    
    // get more names - these should all be unique names, despite some having been detached:
    {
      std::cout << "new names" << std::endl;
      
      const size_t m = 30;
      typename Object::name_type new_names[m];
      Object::storage().create_names(m,new_names);
      
      for(size_t i = 0;i < m;++i) {
	std::cout << new_names[i] << std::endl;
	
	for(size_t j = 0;j < n;++j) {
	  if(new_names[i] == names[j]) assert(0);
	}
      }
    }
    
    summarize< Object >();
    access< Object >();

    // create yet more objects, via create_name_and_object; this should cause the contents list to grow
    {
      const size_t m = 64;
      typename Object::name_type new_objects[m];

      for(size_t i = 0;i < m;++i) {
	new_objects[i] = Object::storage().create_name_and_object();
	++num_created_objects;
	std::cout << i << ":" << new_objects[i] << std::endl;
      }
    }

    summarize< Object >();
    access< Object >();

    std::cout << "tried to create " << num_created_objects << " objects" << std::endl;
    assert(num_created_objects != Object::current_contents);

    //
    // detach more objects:
    {
      size_t count = 0;
      for(size_t i = 0;i < num_created_objects;++i) {
	if(rand() % 2) {
	  if(i != 0 && Object::storage().is_object(i)) {
	    Object::storage().detach(i);
	    ++count;
	  }
	}
      }
      std::cout << "detached " << count << " objects" << std::endl;
    }

    summarize< Object >();
    access< Object >();

    // simulate object deletion
    //
    {
      // Just delete a bunch of objects:
      const size_t m = (Object::storage().m_num_objects < 20) ? Object::storage().m_num_objects : 20;

      std::cout << "try to delete the first " << m << " objects" << std::endl;

      for(size_t i = 0;i < (num_created_objects < m ? num_created_objects : m);++i) {
	std::cout << "want to destroy " << i << ": " 
		  << " is_name: " << (int)Object::storage().is_name(i)
		  << " is_constructed: " << (int)Object::storage().is_constructed(i)
		  << " is_object: " << (int)Object::storage().is_object(i)
		  << std::endl;
	try {
	  Object::storage().destroy(i);
	}
	catch (const assertion a) {
	  std::cout << "possibly expected assertion: " << a.what() << std::endl;
	}
      }
    }

    summarize< Object >();
    access< Object >();

    {
      // Delete the first actually created objects:
      const size_t m = (Object::storage().m_num_objects < 20) ? Object::storage().m_num_objects : 20;

      std::cout << "try to delete the first " << m << " actual objects" << std::endl;

      for(size_t i = 0,count = 0;i < Object::storage().current_potential_size() && count < m;++i) {
	if(!Object::storage().is_object(i)) continue;

	std::cout << "want to destroy " << i << ": " 
		  << " is_name: " << (int)Object::storage().is_name(i)
		  << " is_constructed: " << (int)Object::storage().is_constructed(i)
		  << " is_object: " << (int)Object::storage().is_object(i);
	try {
	  Object::storage().destroy(i);
	  std::cout << std::endl;
	}
	catch (const assertion a) {
	  std::cout << "possibly expected assertion: " << a.what() << std::endl;
	}

	++count;
      }
    }

    summarize< Object >();
    access< Object >();

    // create yet more objects, via create_name_and_object - this should re-use some names:
    {
      const size_t m = 64;
      typename Object::name_type new_objects[m];

      for(size_t i = 0;i < m;++i) {
	new_objects[i] = Object::storage().create_name_and_object();
	++num_created_objects;
	std::cout << i << ":" << new_objects[i] << std::endl;
      }
    }

    summarize< Object >();
    access< Object >();

    // delete all possible object objects - we should see that destructors are called, but storage arrays are not free'd
    {
      std::cout << "try to delete everything" << std::endl;

      for(size_t i = 0;i < Object::storage().current_potential_size();++i) {
	std::cout << "want to destroy " << i << ": " 
		  << " is_name: " << (int)Object::storage().is_name(i)
		  << " is_constructed: " << (int)Object::storage().is_constructed(i)
		  << " is_object: " << (int)Object::storage().is_object(i);
	try {
	  Object::storage().destroy(i);
	  std::cout << std::endl;
	}
	catch (const assertion a) {
	  std::cout << "possibly expected assertion: " << a.what() << std::endl;
	}
      }
    }

    summarize< Object >();
    access< Object >();

    {
      // Start over:
      typename Object::name_type name = Object::storage().create_name_and_object();
      std::cout << "started over with: " << name << std::endl;
    }

    summarize< Object >();
    access< Object >();
  }
  catch(const assertion a) {
    std::cout << "unexpected assertion: " << a.what() << std::endl;
  }
}

template< typename Object >
void refcount_tests()
{
  typedef container_object< Object, 16 > container_type;
  typedef typename container_type::name_type container_name_type;
  typedef typename Object::name_type object_name_type;

  // create the container itself:
  container_name_type c = container_type::storage().create_name_and_object();
  std::cout << "c: " << c << std::endl;

  // create some objects:
  const size_t n = 10;
  object_name_type names[n];
  for(size_t i = 0;i < n;++i) {
    names[i] = Object::storage().create_name_and_object();
  }

  std::cout << "object names: " << std::endl;
  for(size_t i = 0;i < n;++i) {
    std::cout << "\t" << names[i] << std::endl;
  }

  // associate some of those objects with the container:
  for(size_t i = 0;i < n;++i) {
    if(rand() % 2) {
      container_type::storage().at(c).bind(rand() % 16,names[i]);
    }
  }

  summarize< container_type >();
  access< container_type >();

  // /try/ to delete all of the objects; the ones that are contained will not be deleted:
  for(size_t i = 0;i < n;++i) {
    Object::gl_object_type::maybe_delete(names[i]);
  }

  summarize< Object >();
  access< Object >();

  // construct a bunch of new objects - this should result in completely new set of names
  {
    object_name_type names[n];
    for(size_t i = 0;i < n;++i) {
      names[i] = Object::storage().create_name_and_object();
    }
    
    std::cout << "object names: " << std::endl;
    for(size_t i = 0;i < n;++i) {
      std::cout << "\t" << names[i] << std::endl;
    }
    container_type::storage().at(c).access(std::cout);
    std::cout << std::endl;
  }

  // delete c - we should see destructors called for Object
  container_type::storage().destroy(c);
}

int
main(int argc, char ** argv)
{
  {
    // Create initial storage. Even if a certain number of objects are pre-allocated, you should not see any constructors get called:
    normal_object::storage();
    basic_tests< normal_object >();
    std::cout << "normal_object done" << std::endl;
  }

  {
    default_object::storage();
    basic_tests< default_object >();
    std::cout << "default_object done" << std::endl;
  }

  {
    refcount_tests< normal_object >();
    std::cout << "refcount tests done" << std::endl;
  }

  return 0;
}
