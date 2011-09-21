//-*-C++-*-

#ifndef rsxgl_striped_array_H
#define rsxgl_striped_array_H

#if !defined(assert)
#include <cassert>
#endif

#include <memory>
#include <cstdlib>
#include <cstring>
#include <boost/mpl/transform.hpp>
#include <boost/fusion/include/at.hpp>
#include <boost/fusion/include/for_each.hpp>
#include <boost/fusion/include/zip.hpp>
#include <boost/type_traits.hpp>

#include <iostream>

#include "cxxutil.h"

#if defined(__APPLE__)
#define RSXGL_MEMALIGN(ALIGN,SIZE) malloc((SIZE))
#else
#include <malloc.h>
#define RSXGL_MEMALIGN(ALIGN,SIZE) memalign((ALIGN),(SIZE))
#endif

// Align must be a power of two.
template< typename Types, typename SizeType, size_t Align >
struct striped_object_array {
  typedef Types value_types;
  typedef SizeType size_type;

  typedef typename boost::mpl::transform< value_types, boost::add_pointer< boost::mpl::_ > >::type pointers_type;
  typedef typename boost::mpl::transform< value_types, typename boost::add_reference< boost::mpl::_ > >::type references_type;

  struct allocate_array {
    size_type size;
    
    allocate_array(size_type _size) : size(_size) {}
    
    template< typename Type >
    void operator()(Type * & array) const {
      const size_t aligned_size = align_pot< size_t, Align >(size * sizeof(Type));
      array = (Type *)RSXGL_MEMALIGN(Align,aligned_size);
    }
  };

  struct default_predicate {
    bool operator()(size_type) const {
      return true;
    }
  };

  template< typename Predicate = default_predicate >
  struct destruct_fn {
    size_type size;
    const Predicate & p;
    
    destruct_fn(size_type _size,const Predicate & _p) : size(_size), p(_p) {}

    template< typename Type >
    void operator()(Type * & array) const {
      if(array != 0) {
	for(size_type i = 0;i < size;++i) {
	  if(p(i)) {
	    (array + i) -> ~Type();
	  }
	}
	free(array);
	array = 0;
      }
    }
  };

  struct resize_array {
    const size_type prev_size, new_size;
    
    resize_array(const size_type _prev_size,const size_type _new_size) : prev_size(_prev_size), new_size(_new_size) {
    }
    
    template< typename Type >
    void operator()(Type * & array) const {
      // See if aligned sizes actually match - then we don't actually allocate anything new:
      const size_t new_aligned_size = align_pot< size_t, Align >(new_size * sizeof(Type));
      if((array != 0) && (align_pot< size_t, Align >(prev_size * sizeof(Type)) == new_aligned_size)) return;
      
      Type * tmp = (Type *)RSXGL_MEMALIGN(Align,new_aligned_size);

      if(array != 0) {
	// Here we do something potentially dangerous, which is to move C++ objects without telling them.
	// Yup, C++0x move constructors could be helpful here, so let's make that a TODO.
	memcpy(tmp,array,std::min(prev_size,new_size) * sizeof(Type));
	free(array);
      }

      array = tmp;
    }
  };

  struct construct_item_fn {
    size_type i;

    construct_item_fn(size_type _i)
      : i(_i) {
    }

    template< typename Type >
    void operator()(Type * & array) const {
      Type * tmp = new (array + i) Type;
    }
  };

  struct destruct_item_fn {
    size_type i;

    destruct_item_fn(size_type _i)
      : i(_i) {
    }

    template< typename Type >
    void operator()(Type * & array) const {
      (array + i) -> ~Type();
    }
  };
    
  struct type {
    pointers_type & values;
    size_type & size;

    type(pointers_type & _values,size_type & _size)
      : values(_values), size(_size) {
    }

    void allocate(size_type _size = 0) {
      boost::fusion::for_each(values,allocate_array(_size));
      size = _size;
    }
    
    void destruct() {
      default_predicate p;
      boost::fusion::for_each(values,destruct_fn< default_predicate >(size,p));
    }
    
    template< typename Predicate >
    void destruct(const Predicate & p) {
      boost::fusion::for_each(values,destruct_fn< Predicate >(size,p));
    }
    
    // 
    void resize(size_type _size) {
      boost::fusion::for_each(values,resize_array(size,_size));
      size = _size;
    }
    
    template< size_t I >
    typename boost::add_reference< typename boost::fusion::result_of::at_c< value_types, I >::type >::type
    at(size_type i) {
      return boost::fusion::at_c< I >(values)[i];
    }

    void construct_item(size_type i) {
      boost::fusion::for_each(values,construct_item_fn(i));
    }
    
    void destruct_item(size_type i) {
      boost::fusion::for_each(values,destruct_item_fn(i));
    }
  };

  struct const_type {
    const pointers_type & values;
    const size_type & size;
    
    const_type(const pointers_type & _values,const size_type & _size)
      : values(_values), size(_size) {
    }

    const_type(type & rhs)
      : values(rhs.values), size(rhs.size) {
    }

    template< size_t I >
    typename boost::add_reference< typename boost::add_const< typename boost::fusion::result_of::at_c< value_types, I >::type >::type >::type
    at(size_type i) const {
      return boost::fusion::at_c< I >(values)[i];
    }
  };

  // Actually moves an object in the same manner as arrays are resized.
  struct move_item_fn {
    size_type lhs_i, rhs_i;

    move_item_fn(const size_type _lhs_i,const size_type _rhs_i)
      : lhs_i(_lhs_i), rhs_i(_rhs_i) {
    }

    template< typename Type >
    static void move_it(Type & lhs,Type const & rhs) {
      memcpy(&lhs,&rhs,sizeof(Type));
    }

    template< typename ArraysPair >
    void operator()(ArraysPair p) const {
      //boost::fusion::at_c< 0 >(p)[lhs_i] = boost::fusion::at_c< 1 >(p)[rhs_i];
      move_it(boost::fusion::at_c< 0 >(p)[lhs_i],boost::fusion::at_c< 1 >(p)[rhs_i]);
    }
  };

  static void move_item(type lhs,const size_type lhs_i,
			type rhs,const size_type rhs_i) {
    assert(lhs_i < lhs.size);
    assert(rhs_i < rhs.size);

    boost::fusion::for_each(boost::fusion::zip(lhs.values,rhs.values),move_item_fn(lhs_i,rhs_i));
  }
};

#endif
