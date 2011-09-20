//-*-C++-*-

#ifndef rsxgl_object_namespace_H
#define rsxgl_object_namespace_H

#include <vector>
#include <deque>
#include <unordered_map>
#include <algorithm>
#include <cassert>

// For this namespace class, names are optionally managed by the application. Use this for OpenGL objects
// that have a glGen*() associated with them - textures, buffers, etc. 
template< typename _Tp >
struct object_namespace {
  typedef std::unordered_map< uint32_t, _Tp > type;
  
  static uint32_t gen(type & ns,uint32_t n,uint32_t * names) {
    uint32_t count = 0,name = 1;
    uint32_t * pnames = names;
    for(uint32_t i = 0;i < n;++i,++pnames) {
      while(name != 0 && ns.find(name) != ns.end()) {
	++name;
      }
      if(name != 0) {
	*pnames = name++;
	++count;
      }
      else {
	// out of names:
	return count;
      }
    }
    return count;
  }

  static _Tp * create(type & ns,uint32_t name,const _Tp & value = _Tp()) {
    std::pair< typename type::iterator, bool > r = ns.insert(typename type::value_type(name,value));
    if(r.second) {
      return &r.first -> second;
    }
    else {
      return 0;
    }
  }

  static _Tp * find(type & ns,uint32_t name) {
    typename type::iterator it = ns.find(name);

    if(it != ns.end()) {
      return &it -> second;
    }
    else {
      return 0;
    }
  }

  static _Tp * find_or_create(type & ns,uint32_t name,const _Tp & value = _Tp()) {
    std::pair< typename type::iterator, bool > r = ns.insert(typename type::value_type(name,value));
    return &r.first -> second;
  }

  static void destroy(type & ns,uint32_t name) {
    typename type::iterator it = ns.find(name);

    if(it != ns.end()) {
      ns.erase(it);
    }
  }
};

// Using this namespace class, the library manages names exclusively. This is for OpenGL objects
// that have a glCreate*() function, like shaders and programs.
template< typename _Tp >
struct managed_object_namespace {
  struct type {
    std::deque< _Tp > objects;
    std::deque< uint32_t > available_names;
  };

  static std::pair< uint32_t,bool > create(type & ns,const _Tp & value = _Tp()) {
    uint32_t name = 0;

    // no names to reclaim, expand the objects list:
    if(ns.available_names.empty()) {
      name = ns.objects.size() + 1;
      ns.objects.push_back(value);
      return std::make_pair(name,true);
    }
    // there is a name, re-use it:
    else {
      name = ns.available_names.front();
      ns.available_names.pop_front();
      return std::make_pair(name,false);
    }
  }

  static uint32_t create2(type & ns,const _Tp & value = _Tp()) {
    uint32_t name = 0;

    // no names to reclaim, expand the objects list:
    if(ns.available_names.empty()) {
      name = ns.objects.size() + 1;
      ns.objects.push_back(value);
    }
    // there is a name, re-use it:
    else {
      name = ns.available_names.front();
      ns.available_names.pop_front();
      ns.objects.at(name) = value;
    }

    return name;
  }

  static _Tp * get(type & ns,uint32_t name) {
    assert(name != 0);
    return &(ns.objects.at(name - 1));
  }

  static void destroy(type & ns,uint32_t name) {
    assert(name != 0);
    ns.available_names.push_back(name);
  }
};

// This is a combination of both of the above. Names are managed by the library, but generating
// a name and actually allocating space for an object are separate operations. This is meant to
// support the object gen followed-by create pattern of OpenGL 3.1.
template< typename _Tp >
struct managed_object_namespace2 {
  struct type {
    type() {
      // name 0 should never get used:
      names.push_back(false);
      created.push_back(false);
    }

    std::vector< _Tp > objects;
    std::vector< bool > names, created;
    std::deque< uint32_t > available_names;
  };

  static uint32_t gen(type & ns,uint32_t n,uint32_t * names) {
    uint32_t i = 0;
    for(;i < n;++i,++names) {
      uint32_t name = 0;

      // no names to reclaim, expand the names list:
      if(ns.available_names.empty()) {
	name = ns.names.size();
	ns.names.push_back(true);
	ns.created.push_back(false);
      }
      // there is a name, re-use it:
      else {
	name = ns.available_names.front();
	ns.available_names.pop_front();
	ns.names.at(name) = true;
	ns.created.at(name) = false;
      }

      *names = name;
    }
    return i;
  }

  static bool is_name(type & ns,uint32_t name) {
    if(name == 0) {
      return false;
    }
    else if(name < ns.names.size() && (bool)ns.names.at(name)) {
      return true;
    }
    else {
      return false;
    }
  }

  static _Tp & create(type & ns,uint32_t name,const _Tp & value = _Tp()) {
    // library should have checked to see if the name was generated or not:
    assert(is_name(ns,name));

    // it's likely that the library would check for this, too, otherwise something will probably leak:
    assert(!ns.created.at(name));

    if(ns.objects.size() < name) {
      ns.objects.resize(name);
    }

    ns.objects[name - 1] = value;
    ns.created.at(name) = true;
    return ns.objects[name - 1];
  }

  static bool is_created(type & ns,uint32_t name) {
    if(name == 0) {
      return false;
    }
    else if(name < ns.created.size() && ns.created.at(name)) {
      return true;
    }
    else {
      return false;
    }
  }

  static _Tp & get(type & ns,uint32_t name) {
    assert(is_created(ns,name));

    return ns.objects.at(name - 1);
  }

  static void destroy(type & ns,uint32_t name) {
    if(name > 0) {
      ns.names.at(name) = false;
      ns.created.at(name) = false;
      ns.available_names.push_back(name);
    }
  }
};

#endif
