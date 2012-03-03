//-*-C++-*-
//
// set.h
//
// Simple set, based upon the array class.

#ifndef rsxgl_set_H
#define rsxgl_set_H

#include "array.h"

#include <memory>
#include <functional>

#include <iostream>

template< typename Type, typename SizeType, typename Compare = std::less< Type >, typename Alloc = std::allocator< void > >
struct set : public array< Type, SizeType, Alloc > {
  typedef array< Type, SizeType, Alloc > array_type;
  
  typedef typename array_type::value_type value_type;
  typedef typename array_type::size_type size_type;
  typedef typename array_type::pointer_type pointer_type;

  typedef typename array_type::allocator allocator;
  
  struct type : protected array_type::type {
    type(pointer_type & _values,size_type & _size) : array_type::type(_values,_size) {
    }

    type(const type & rhs) : array_type::type(rhs) {
    }

    void construct() {
      array_type::type::construct(0);
    }

    void destruct() {
      array_type::type::destruct();
    }

    bool insert(const value_type & x) const {
      pointer_type it = std::lower_bound(array_type::type::values,array_type::type::values + array_type::type::size,x,Compare());

      if(it != array_type::type::values + array_type::type::size && !Compare()(x,*it)) {
	return false;
      }
      else {
	const size_type i = it - array_type::type::values;
	const size_type _size = array_type::type::size + 1;

	allocator alloc;
	pointer_type tmp = (_size > 0) ? alloc.allocate(_size) : 0;
      
	if(array_type::type::values != 0) {
	  std::uninitialized_copy(array_type::type::values,it,tmp);
	  
	  std::uninitialized_copy(it,array_type::type::values + array_type::type::size,tmp + i + 1);
	  alloc.deallocate(array_type::type::values,array_type::type::size);
	}

	std::uninitialized_copy(&x,&x + 1,tmp + i);

	array_type::type::values = tmp;
	array_type::type::size = _size;
	
	return true;
      }
    }

    bool erase(const value_type & x) const {
      pointer_type it = std::lower_bound(array_type::type::values,array_type::type::values + array_type::type::size,x,Compare());

      if(it != array_type::type::values + array_type::type::size && !Compare()(x,*it)) {
	const size_type i = it - array_type::type::values;
	const size_type _size = array_type::type::size - 1;

	allocator alloc;
	pointer_type tmp = (_size > 0) ? alloc.allocate(_size) : 0;
      
	if(array_type::type::values != 0) {
	  std::uninitialized_copy(array_type::type::values,it,tmp);
	  std::uninitialized_copy(it + 1,array_type::type::values + array_type::type::size,tmp + i);
	  alloc.deallocate(array_type::type::values,array_type::type::size);
	}

	array_type::type::values = tmp;
	array_type::type::size = _size;
	
	return true;
      }
      else {
	return false;
      }
    }

    bool has(const value_type & x) const {
      return binary_search(array_type::type::values,array_type::type::values + array_type::type::size,x,Compare());
    }
  };

  struct const_type : protected array_type::type {
    const_type(const pointer_type & _values,const size_type & _size) : array_type::const_type(_values,_size) {
    }

    const_type(const const_type & rhs) : array_type::const_type(rhs) {
    }

    bool has(const value_type & x) const {
      return binary_search(array_type::type::values,array_type::type::values + array_type::type::size,x,Compare());
    }
  };

  static void assign(type lhs,const_type rhs) {
  }
};

#endif
