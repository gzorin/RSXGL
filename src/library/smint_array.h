//-*-C++-*-
//
// smint_array.h
//
// Store arrays of integers which can potentially be packed into sub-byte-sized bitfields.

#ifndef smint_array_H
#define smint_array_H

#if !defined(assert)
#include <cassert>
#endif

#include <stdint.h>
#include <limits>

#include <boost/integer.hpp>
#include <boost/integer/integer_mask.hpp>
#include <boost/integer/static_log2.hpp>
#include <boost/mpl/int.hpp>
#include <boost/mpl/comparison.hpp>
#include <boost/mpl/less_equal.hpp>
#include <boost/mpl/if.hpp>

template< boost::static_log2_argument_type M, size_t N, typename MaxType = uintmax_t >
struct smint_array
{
public:

  static const size_t value_bits = boost::static_log2< M >::value + 1;

  // Total number of bits required:
  static const size_t total_bits = value_bits * N;

  // Maximum number of bits stored in an integer:
  static const size_t max_bits = std::numeric_limits< MaxType >::digits;

  typedef typename boost::uint_t< value_bits >::least value_type;

  // Everything fits into a single integer:
  struct scalar_impl {
    typedef typename boost::uint_t< total_bits >::least word_type;
    static const size_t num_words = 1;

    word_type values[1];
  };

  // 
  struct array_impl {
    typedef MaxType word_type;
    static const size_t num_words = (total_bits / max_bits) + ((total_bits % max_bits) ? 1 : 0);

    word_type values[num_words];
  };

  // If total_bits exceeds max_bits, need an array
  typedef typename boost::mpl::if_< boost::mpl::less_equal< boost::mpl::int_< total_bits >, boost::mpl::int_< max_bits > >, scalar_impl, array_impl >::type impl_type;

  impl_type impl;
  typedef typename impl_type::word_type word_type;
  static const word_type value_mask = boost::low_bits_mask_t< value_bits >::sig_bits;
  static const size_t word_bits = std::numeric_limits< typename impl_type::word_type >::digits;
  static const size_t values_per_word = word_bits / value_bits;
  static const size_t num_words = impl_type::num_words;

  smint_array() {
    for(size_t i = 0;i < num_words;++i) {
      impl.values[i] = 0;
    }
  }

  smint_array(const smint_array & rhs) {
    for(size_t i = 0;i < num_words;++i) {
      impl.values[i] = rhs.impl.values[i];
    }
  }

  smint_array & operator =(const smint_array & rhs) {
    for(size_t i = 0;i < num_words;++i) {
      impl.values[i] = rhs.impl.values[i];
    }
    return *this;
  }

  inline
  void set(size_t i,value_type x) {
    assert(i < N);
    const size_t ii = i / values_per_word;
    const size_t jj = i % values_per_word;
    const size_t shift_bits = jj * value_bits;
    impl.values[ii] = (impl.values[ii] & ~(value_mask << shift_bits)) | (((word_type)x & value_mask) << shift_bits);
  }

  inline
  value_type get(size_t i) const {
    assert(i < N);
    const size_t ii = i / values_per_word;
    const size_t jj = i % values_per_word;
    const size_t shift_bits = jj * value_bits;
    return (value_type)((impl.values[ii] >> shift_bits) & value_mask);
  }

  inline
  value_type operator[](size_t i) const {
    assert(i < N);
    const size_t ii = i / values_per_word;
    const size_t jj = i % values_per_word;
    const size_t shift_bits = jj * value_bits;
    return (value_type)((impl.values[ii] >> shift_bits) & value_mask);
  }

  template< typename Function >
  static void for_each(smint_array const & array,Function const & fn) {
    for(size_t i = 0,j = 0;i < num_words && j < N;++i) {
      word_type word = array.impl.values[i];
      for(size_t k = 0;k < values_per_word && j < N;++k,++j,word >>= value_bits) {
	const value_type value = word & value_mask;
	fn(j,value);
      }
    }
  }
};

#endif
