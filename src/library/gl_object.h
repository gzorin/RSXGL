//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// gl_object.h - Template for GL objects.

#ifndef rsxgl_gl_object_H
#define rsxgl_gl_object_H

#if !defined(assert)
//#include <cassert>
#include "rsxgl_assert.h"
#endif

#include <boost/integer.hpp>

#include "gl_object_storage.h"
#include "bit_set.h"

template< typename ObjectT,
	  size_t Max,
	  int DefaultObject = 0 >
struct gl_object {
  typedef ObjectT object_type;
  typedef typename boost::uint_value_t< Max - 1 >::least name_type;
  static const bool has_default_object = DefaultObject;
  typedef gl_object_storage< ObjectT, name_type, has_default_object > storage_type;

  // Provision for objects that have reference counts - objects that can be contained by other
  // objects. Examples include buffer objects, which maybe contained by vertex array objects,
  // and GPU program objects, which can be contained by GPU programs.
  static size_t ref(const name_type name) {
    return ++object_type::storage().at(name).ref_count;
  }
  
  static size_t unref(const name_type name) {
    return --object_type::storage().at(name).ref_count;
  }

  // Implement glDelete*() functionality - if the object isn't being used anywhere,
  // it gets deleted immediately. Otherwise, it's marked for deletion & destroyed
  // when something else unref's it.
  static void maybe_delete(const name_type name) {
    assert(name != 0);
    object_type & object = object_type::storage().at(name);
    assert(!object.deleted);

    object.deleted = true;

    if(object.ref_count == 0) {
      object_type::storage().destroy(name);
    }
    else {
      object_type::storage().detach(name);
    }
  }

  static void unref_and_maybe_delete(const name_type name) {
    assert(name != 0);
    object_type & object = object_type::storage().at(name);

    const size_t ref_count = --object.ref_count;

    if(ref_count == 0 && object.deleted) {
      object_type::storage().destroy(name);
    }
  }

  // Delete, detach, or orphan an object. in_use is a parameter that tells the function if
  // the object is in use by the GPU or not. It's a parameter so that this header doesn't
  // need to concern itself with checking timestamps. Return code indicates what was actually
  // done with the object - 0 == deleted, 1 == detached, 2 == orphaned
  static const size_t was_destroyed = 0, was_detached = 1, was_orphaned = 2, was_unrefed = 3;

  static size_t maybe_delete_or_orphan(const bool in_use, const name_type name) {
    assert(name != 0);
    object_type & object = object_type::storage().at(name);
    assert(!object.deleted);

    object.deleted = true;

    // still in use by the GPU:
    if(in_use) {
      // nobody holds onto it - it gets orphaned:
      if(object.ref_count == 0) {
	object_type::storage().orphan(name);
	return was_orphaned;
      }
      // name is still valid - it is detached:
      else {
	object_type::storage().detach(name);
	return was_detached;
      }
    }
    // no longer used:
    else {
      // nobody holds onto it - it gets deleted:
      if(object.ref_count == 0) {
	object_type::storage().destroy(name);
	return was_destroyed;
      }
      // name is still valid - it is detached:
      else {
	object_type::storage().detach(name);
	return was_detached;
      }
    }
  }

  static size_t unref_and_maybe_delete_or_orphan(const bool in_use,const name_type name) {
    assert(name != 0);
    object_type & object = object_type::storage().at(name);

    assert(object.ref_count > 0);
    const size_t ref_count = --object.ref_count;

    if(ref_count == 0 && object.deleted) {
      // it is in use - orphan it:
      if(in_use) {
	object_type::storage().orphan(name);
	return was_orphaned;
      }
      // it is not in use - it can be deleted:
      else {
	object_type::storage().destroy(name);
	return was_destroyed;
      }
    }
    else {
      return was_unrefed;
    }
  }
};

template< typename ObjectType, size_t Targets >
struct object_binding_type {
  typedef ObjectType object_type;
  typedef typename object_type::gl_object_type::name_type name_type;
  static const bool has_default_object = object_type::gl_object_type::has_default_object;

  name_type names[Targets];
  typedef typename boost::uint_value_t< Targets - 1 >::least size_type;

  typedef bit_set< Targets > bitfield_type;
  
  object_binding_type() {
    for(size_type i = 0;i < Targets;++i) {
      names[i] = 0;
    }
  }
  
  void bind(const size_type target,const name_type name) {
    assert(target < Targets);

    if(names[target] != name) {    
      if(has_default_object) {
	object_type::storage().at(names[target]).binding_bitfield.reset(target);
	names[target] = name;
	object_type::storage().at(names[target]).binding_bitfield.set(target);
      }
      else {
	if(names[target] != 0) object_type::storage().at(names[target]).binding_bitfield.reset(target);
	names[target] = name;
	if(names[target] != 0) object_type::storage().at(names[target]).binding_bitfield.set(target);
      }
    }
  }

  bool is_bound(const size_type target,const name_type name) const {
    assert(target < Targets);
    return names[target] == name;
  }

  bool is_bound(const name_type name) const {
    for(size_type i = 0;i < Targets;++i) {
      if(names[i] == name) return true;
    }
    return false;
  }

  bool is_anything_bound(const size_type target) const {
    assert(target < Targets);
    return names[target] != 0;
  }

  void unbind_from_all(const name_type name) {
    assert(name != 0);

    object_type & object = object_type::storage().at(name);

    if(object.binding_bitfield.any()) {
      for(size_type target = 0;target < Targets;++target) {
	if(names[target] == name) {
	  names[target] = 0;
	}
      }

      object.binding_bitfield.reset();
    }
  }
  
  object_type & operator[](const size_type target) {
    assert(target < Targets);
    assert((has_default_object == 0) ? (names[target] != 0) : true);
    return object_type::storage().at(names[target]);
  }
  
  const object_type & operator[](const size_type target) const {
    assert(target < Targets);
    assert((has_default_object == 0) ? (names[target] != 0) : true);
    return object_type::storage().at(names[target]);
  }
};

// Contain referenfe counted ObjectType's.
template< typename ObjectType, size_t Size >
struct object_container_type {
  typedef ObjectType object_type;
  typedef typename object_type::gl_object_type::name_type name_type;
  static const bool has_default_object = object_type::gl_object_type::has_default_object;

  name_type names[Size];
  typedef typename boost::uint_value_t< Size - 1 >::least size_type;

  object_container_type() {
    for(size_type i = 0;i < Size;++i) {
      names[i] = 0;
    }
  }

  ~object_container_type() {
    for(size_type i = 0;i < Size;++i) {
      if(names[i] == 0) continue;
      object_type::gl_object_type::unref_and_maybe_delete(names[i]);
    }
  }
  
  void bind(const size_type target,const name_type name) {
    assert(target < Size);

    const name_type prev_name = names[target];
    names[target] = name;

    if(name != 0) object_type::gl_object_type::ref(name);
    if(prev_name != 0) object_type::gl_object_type::unref_and_maybe_delete(prev_name);
  }

  bool is_bound(const size_type target,const name_type name) const {
    assert(target < Size);
    return names[target] == name;
  }

  bool is_bound(const name_type name) const {
    for(size_type i = 0;i < Size;++i) {
      if(names[i] == name) return true;
    }
    return false;
  }

  bool is_anything_bound(const size_type target) const {
    assert(target < Size);
    return names[target] != 0;
  }

  void unbind_from_all(const name_type name) {
    assert(name != 0);

    for(size_type target = 0;target < Size;++target) {
      if(names[target] == name) {
	typename object_type::gl_object_type::unref_and_maybe_delete(name);
      }
    }
  }
  
  object_type & operator[](const size_type target) {
    assert(target < Size);
    assert((has_default_object == 0) ? (names[target] != 0) : true);
    return object_type::storage().at(names[target]);
  }
  
  const object_type & operator[](const size_type target) const {
    assert(target < Size);
    assert((has_default_object == 0) ? (names[target] != 0) : true);
    return object_type::storage().at(names[target]);
  }
};

template< typename ObjectT,
	  size_t Max,
	  size_t Targets,
	  int DefaultObject = 0 >
struct bindable_gl_object : public gl_object< ObjectT, Max, DefaultObject > {
  typedef gl_object< ObjectT, Max, DefaultObject > base_type;
  typedef typename base_type::object_type object_type;
  typedef typename base_type::name_type name_type;
  typedef typename base_type::storage_type storage_type;

  typedef object_binding_type< object_type, Targets > binding_type;
  typedef typename binding_type::bitfield_type binding_bitfield_type;
};

#endif
