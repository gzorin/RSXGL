//-*-C++-*-
//
// array.h
//
// Bog-simple resizable container.

#ifndef rsxgl_array_H
#define rsxgl_array_H

#if !defined(assert)
#include <cassert>
#endif

#include <memory>
#include <algorithm>

template< typename Type, typename SizeType, typename Alloc = std::allocator< void > >
struct array {
  typedef Type value_type;
  typedef SizeType size_type;
  typedef value_type * pointer_type;
  typedef const value_type * const_pointer_type;

  typedef typename Alloc::template rebind< value_type >::other allocator;

  struct type {
    pointer_type & values;
    size_type & size;

    type(pointer_type & _values,size_type & _size)
      : values(_values), size(_size) {
    }
    //type(const type & rhs) = delete;
    //type(type && rhs) = default;
    //type & operator =(const type & rhs) = delete;

    void construct(size_type _size = 0,const value_type & value = value_type()) {
      if(_size > 0) {
	values = allocator().allocate(_size);
	size = _size;
	std::uninitialized_fill(values,values + size,value);
      }
      else {
	values = 0;
	size = 0;
      }
    }

    void destruct() {
      if(values != 0) {
	allocator alloc;
	for(size_type i = 0;i < size;++i) {
	  alloc.destroy(values + i);
	}
	alloc.deallocate(values,size);
	values = 0;
	size = 0;
      }
    }

    void resize(size_type _size,const value_type & value = value_type()) {
      allocator alloc;
      pointer_type tmp = (_size > 0) ? alloc.allocate(_size) : 0;
      
      if(values != 0) {
	std::uninitialized_copy(values,values + std::min(size,_size),tmp);
	alloc.deallocate(values,size);
      }
      if(tmp != 0 && size < _size) {
	std::uninitialized_fill(tmp + size,tmp + _size,value);
      }
      
      values = tmp;
      size = _size;
    }
  
    value_type & operator[](size_type i) {
      assert(i < size);
      return values[i];
    }

    template< typename OtherSizeType >
    void set(const void * data,OtherSizeType _n) {
      const value_type * tmp = static_cast< const value_type * >(data);
      const size_type n = std::min((size_type)_n,size);
      if(n > 0) {
	std::copy(tmp,tmp + n,values);
      }
    }

    template< typename OtherSizeType >
    void resize_and_set(const void * data,OtherSizeType _n) {
      const value_type * tmp = static_cast< const value_type * >(data);
      const size_type n = _n;
      resize(n);
      if(n > 0) {
	std::copy(tmp,tmp + n,values);
      }
    }

    template< typename OtherSizeType >
    void get(void * data,OtherSizeType _n) {
      value_type * tmp = static_cast< value_type * >(data);
      const OtherSizeType n = std::min(_n,(OtherSizeType)size);
      if(n > 0) {
	std::copy(values,values + n,tmp);
      }
    }
  };

  struct const_type {
    const const_pointer_type & values;
    const size_type & size;

    const_type(const const_pointer_type & _values,const size_type & _size)
      : values(_values), size(_size) {
    }
    //const_type(const const_type & rhs) = delete;
    //const_type(const_type && rhs) = default;
    //const_type(type && rhs) : values(std::move(rhs.values)), size(std::move(rhs.size)) {}
    //const_type & operator =(const const_type & rhs) = delete;

    const value_type & operator[](size_type i) const {
      assert(i < size);
      return values[i];
    }

    template< typename OtherSizeType >
    void get(void * data,OtherSizeType _n) {
      value_type * tmp = static_cast< value_type * >(data);
      const OtherSizeType n = std::min(_n,(OtherSizeType)size);
      if(n > 0) {
	std::copy(values,values + n,tmp);
      }
    }
  };

  static void assign(type lhs,const_type rhs) {
    lhs.destruct();
    lhs.construct(rhs.size);
    std::copy(rhs.values,rhs.values + rhs.size,lhs.values);
  }
};

#endif
