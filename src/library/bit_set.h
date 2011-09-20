//-*-C++-*-
//
// bit_set.h
//
// 

#ifndef bit_set_H
#define bit_set_H

#if !defined(assert)
#include <cassert>
#endif

#include <smint_array.h>

template< size_t N, typename MaxType = uintmax_t >
struct bit_set : public smint_array< 1, N, MaxType >
{
public:

  typedef smint_array< 1, N, MaxType > base_type;
  typedef typename base_type::impl_type::storage_type storage_type;

  bit_set() {
  }

  bit_set(const bit_set & rhs) 
    : base_type(rhs) {
  }

  bit_set & operator =(const bit_set & rhs) {
    base_type::operator =(rhs);
    return *this;
  }

  bit_set & set() {
    for(size_t i = 0;i < base_type::word_size;++i) {
      base_type::impl.values[i] = ~0;
    }
    return *this;
  }

  bit_set & set(size_t i,bool x = true) {
    base_type::set(i,x);
    return *this;
  }

  bit_set & reset() {
    for(size_t i = 0;i < base_type::word_size;++i) {
      base_type::impl.values[i] = 0;
    }
    return *this;
  }

  bit_set & reset(size_t i) {
    base_type::set(i,false);
    return *this;
  }

  bool operator[](size_t i) const {
    assert(i < N);
    return base_type::get(i);
  }

  bool test(size_t i) const {
    assert(i < N);
    return base_type::get(i);
  }

  bit_set & flip() {
    for(size_t i = 0;i < base_type::word_size;++i) {
      base_type::impl.values[i] = ~base_type::impl.values[i];
    }
    return *this;
  }

  bit_set & flip(size_t i) const {
    base_type::set(i,!base_type::get(i));
    return *this;
  }

  bool any() const {
    for(size_t i = 0;i < base_type::word_size;++i) {
      if(base_type::impl.values[i] != 0) return true;
    }
    return false;
  }

  bool all() const {
    for(size_t i = 0;i < base_type::word_size;++i) {
      if(base_type::impl.values[i] != ~0) return false;
    }
    return true;
  }

  bit_set operator ~() const {
    bit_set r;
    for(size_t i = 0;i < base_type::word_size;++i) {
      r.base_type::impl.values[i] = ~base_type::impl.values[i];
    }
    return r;
  }

  bit_set operator |(const bit_set & rhs) const {
    bit_set r;
    for(size_t i = 0;i < base_type::word_size;++i) {
      r.base_type::impl.values[i] = base_type::impl.values[i] | rhs.base_type::impl.values[i];
    }
    return r;
  }

  bit_set & operator |=(const bit_set & rhs) {
    for(size_t i = 0;i < base_type::word_size;++i) {
      base_type::impl.values[i] |= rhs.base_type::impl.values[i];
    }
    return *this;
  }

  bit_set operator &(const bit_set & rhs) const {
    bit_set r;
    for(size_t i = 0;i < base_type::word_size;++i) {
      r.base_type::impl.values[i] = base_type::impl.values[i] & rhs.base_type::impl.values[i];
    }
    return r;
  }

  bit_set & operator &=(const bit_set & rhs) {
    for(size_t i = 0;i < base_type::word_size;++i) {
      base_type::impl.values[i] &= rhs.base_type::impl.values[i];
    }
    return *this;
  }

  storage_type as_integer() const {
    return base_type::impl.values[0];
  }
};

#endif
