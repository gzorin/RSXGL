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
  typedef typename base_type::word_type word_type;

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
    for(typename base_type::word_index_type i = 0;i < base_type::num_words;++i) {
      base_type::impl.values[i] = ~0;
    }
    return *this;
  }

  bit_set & set(const typename base_type::index_type i,bool x = true) {
    base_type::set(i,x);
    return *this;
  }

  bit_set & reset() {
    for(typename base_type::word_index_type i = 0;i < base_type::num_words;++i) {
      base_type::impl.values[i] = 0;
    }
    return *this;
  }

  bit_set & reset(const typename base_type::index_type i) {
    base_type::set(i,false);
    return *this;
  }

  bool operator[](const typename base_type::index_type i) const {
    assert(i < N);
    return base_type::get(i);
  }

  bool test(const typename base_type::index_type i) const {
    assert(i < N);
    return base_type::get(i);
  }

  bit_set & flip() {
    for(typename base_type::word_index_type i = 0;i < base_type::num_words;++i) {
      base_type::impl.values[i] = ~base_type::impl.values[i];
    }
    return *this;
  }

  bit_set & flip(const typename base_type::index_type i) const {
    base_type::set(i,!base_type::get(i));
    return *this;
  }

  bool any() const {
    for(typename base_type::word_index_type i = 0;i < base_type::num_words;++i) {
      if(base_type::impl.values[i] != 0) return true;
    }
    return false;
  }

  bool all() const {
    for(typename base_type::word_index_type i = 0;i < base_type::num_words;++i) {
      if(base_type::impl.values[i] != ~0) return false;
    }
    return true;
  }

  bit_set operator ~() const {
    bit_set r;
    for(typename base_type::word_index_type i = 0;i < base_type::num_words;++i) {
      r.base_type::impl.values[i] = ~base_type::impl.values[i];
    }
    return r;
  }

  bit_set operator |(const bit_set & rhs) const {
    bit_set r;
    for(typename base_type::word_index_type i = 0;i < base_type::num_words;++i) {
      r.base_type::impl.values[i] = base_type::impl.values[i] | rhs.base_type::impl.values[i];
    }
    return r;
  }

  bit_set & operator |=(const bit_set & rhs) {
    for(typename base_type::word_index_type i = 0;i < base_type::num_words;++i) {
      base_type::impl.values[i] |= rhs.base_type::impl.values[i];
    }
    return *this;
  }

  bit_set operator &(const bit_set & rhs) const {
    bit_set r;
    for(typename base_type::word_index_type i = 0;i < base_type::num_words;++i) {
      r.base_type::impl.values[i] = base_type::impl.values[i] & rhs.base_type::impl.values[i];
    }
    return r;
  }

  bit_set & operator &=(const bit_set & rhs) {
    for(typename base_type::word_index_type i = 0;i < base_type::num_words;++i) {
      base_type::impl.values[i] &= rhs.base_type::impl.values[i];
    }
    return *this;
  }

  word_type as_integer() const {
    return base_type::impl.values[0];
  }

  template< typename Function >
  static void for_each(bit_set const & bits,Function const & fn) {
    typename base_type::index_type j = 0;
    for(typename base_type::word_index_type i = 0;i < base_type::num_words && j < N;++i) {
      word_type word = bits.impl.values[i];
      for(typename base_type::value_index_type k = 0;k < base_type::word_bits && j < N;++k,++j,word >>= 1) {
	fn(j,(word & 0x1));
      }
    }
  }

  template< typename Function >
  static void for_each_set(bit_set const & bits,Function const & fn) {
    typename base_type::index_type j = 0;
    for(typename base_type::word_index_type i = 0;i < base_type::num_words && j < N;++i) {
      word_type word = bits.impl.values[i];
      for(typename base_type::value_index_type k = 0;k < base_type::word_bits && j < N;++k,++j,word >>= 1) {
	if((word & 0x1)) {
	  fn(j);
	}
      }
    }
  }

  template< typename Function >
  static void for_each_not_set(bit_set const & bits,Function const & fn) {
    typename base_type::index_type j = 0;
    for(typename base_type::word_index_type i = 0;i < base_type::num_words && j < N;++i) {
      word_type word = bits.impl.values[i];
      for(typename base_type::value_index_type k = 0;k < base_type::word_bits && j < N;++k,++j,word >>= 1) {
	if(!(word & 0x1)) {
	  fn(j);
	}
      }
    }
  }

  struct const_iterator {
    typename base_type::word_index_type i;
    typename base_type::index_type j;
    typename base_type::value_index_type k;
    typename base_type::word_type word;

    const_iterator(const typename base_type::word_index_type _i,const typename base_type::index_type _j,const typename base_type::value_index_type _k,const typename base_type::word_type _word)
      : i(_i), j(_j), k(_k), word(_word) {
    }

    typename base_type::index_type index() const {
      return j;
    }

    bool test() const {
      return (typename base_type::value_type)(word & 0x1);
    }

    void next(bit_set const & bits) {
      ++j;
      if(j < N) {
	++k;
	if(k < base_type::values_per_word) {
	  word >>= 1;
	}
	else {
	  word = bits.impl.values[++i];
	  k = 0;
	}
      }
    }

    bool done() const {
      return !(j < N);
    }
  };

  const_iterator begin() const {
    return const_iterator(0,0,0,base_type::impl.values[0]);
  }
};

#endif
