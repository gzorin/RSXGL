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

  static const size_t element_bits = boost::static_log2< M >::value + 1;

  // Total number of bits required:
  static const size_t total_bits = element_bits * N;

  // Maximum number of bits stored in an integer:
  static const size_t max_bits = std::numeric_limits< MaxType >::digits;

  typedef typename boost::uint_t< element_bits >::least value_type;

  // Everything fits into a single integer:
  struct scalar_impl {
    typedef typename boost::uint_t< total_bits >::least storage_type;
    static const size_t size = 1;

    storage_type values[1];
  };

  // 
  struct array_impl {
    typedef MaxType storage_type;
    static const size_t size = (total_bits / max_bits) + ((total_bits % max_bits) ? 1 : 0);

    storage_type values[size];
  };

  // If total_bits exceeds max_bits, need an array
  typedef typename boost::mpl::if_< boost::mpl::less_equal< boost::mpl::int_< total_bits >, boost::mpl::int_< max_bits > >, scalar_impl, array_impl >::type impl_type;

  impl_type impl;
  typedef typename impl_type::storage_type impl_storage_type;
  static const impl_storage_type storage_mask = boost::low_bits_mask_t< element_bits >::sig_bits;
  static const size_t width = std::numeric_limits< typename impl_type::storage_type >::digits / element_bits;
  static const size_t word_size = impl_type::size;

  smint_array() {
    for(size_t i = 0;i < impl_type::size;++i) {
      impl.values[i] = 0;
    }
  }

  smint_array(const smint_array & rhs) {
    for(size_t i = 0;i < impl_type::size;++i) {
      impl.values[i] = rhs.impl.values[i];
    }
  }

  smint_array & operator =(const smint_array & rhs) {
    for(size_t i = 0;i < impl_type::size;++i) {
      impl.values[i] = rhs.impl.values[i];
    }
    return *this;
  }

  inline
  void set(size_t i,value_type x) {
    assert(i < N);
    const size_t ii = i / width;
    const size_t jj = i % width;
    const size_t shift_bits = jj * element_bits;
    impl.values[ii] = (impl.values[ii] & ~(storage_mask << shift_bits)) | (((impl_storage_type)x & storage_mask) << shift_bits);
  }

  inline
  value_type get(size_t i) const {
    assert(i < N);
    const size_t ii = i / width;
    const size_t jj = i % width;
    const size_t shift_bits = jj * element_bits;
    return (value_type)((impl.values[ii] >> shift_bits) & storage_mask);
  }

  inline
  value_type operator[](size_t i) const {
    assert(i < N);
    const size_t ii = i / width;
    const size_t jj = i % width;
    const size_t shift_bits = jj * element_bits;
    return (value_type)((impl.values[ii] >> shift_bits) & storage_mask);
  }
};

#endif
