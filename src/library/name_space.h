//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// name_space.h - Manage names (integers) that can be allocated from a set.

#ifndef rsxgl_name_space_H
#define rsxgl_name_space_H

#include "array.h"

#include <memory>
#include <limits>

#include <boost/tuple/tuple.hpp>
#include <boost/integer.hpp>
#include <boost/integer/integer_mask.hpp>

#if !defined(assert)
#include <cassert>
#endif

#define BOOST_CB_DISABLE_DEBUG
#include <boost/circular_buffer.hpp>

template<
  // Maximum number of names, not the maximum name value:
  size_t MaxNames = std::numeric_limits< uint32_t >::max() + 1,

  // Initial capacity:
  size_t InitialCapacity = 0,

  // Integer type used to store each element of the bitfield array:
  typename NameBitfieldT = boost::uintmax_t,
  
  typename Alloc = std::allocator< void >
  >
struct name_space
{
  typedef typename boost::uint_value_t< MaxNames - 1 >::least name_type;
  typedef typename boost::uint_value_t< MaxNames >::least size_type;

  typedef NameBitfieldT name_bitfield_type;
  static const size_t name_bitfield_type_bits = std::numeric_limits< name_bitfield_type >::digits;
  static const size_t name_bitfield_type_positions = name_bitfield_type_bits / 2;

  static const name_bitfield_type name_bitfield_mask = boost::low_bits_mask_t< 2 >::sig_bits_fast;
  static const name_bitfield_type name_bitfield_named_mask = boost::high_bit_mask_t< 1 >::bit_position;
  static const name_bitfield_type name_bitfield_init_mask = boost::high_bit_mask_t< 2 >::bit_position;

  typedef array< name_bitfield_type, name_type, Alloc > name_bitfield_array_type;

  name_type m_name_bitfield_size;
  typename name_bitfield_array_type::pointer_type m_name_bitfield;

  typename name_bitfield_array_type::type name_bitfield() {
    return typename name_bitfield_array_type::type(m_name_bitfield,m_name_bitfield_size);
  }

  typename name_bitfield_array_type::const_type name_bitfield() const {
    return typename name_bitfield_array_type::const_type(m_name_bitfield,m_name_bitfield_size);
  }

  //
  static inline std::pair< size_t, size_t > 
  name_bitfield_location(const name_type name) {
    const size_t name_position2 = name << 1;
    return std::pair< size_t, size_t >(name_position2 / name_bitfield_type_bits,name_position2 % name_bitfield_type_bits);
  }

  typedef boost::circular_buffer< name_type > name_queue_type;
  name_queue_type m_name_queue;

#if !defined(NDEBUG)
  size_type m_num_names;
#endif

  name_space() {
    static const typename name_bitfield_array_type::size_type initial_name_bitfield_array_size = (InitialCapacity / name_bitfield_type_positions) + ((InitialCapacity % name_bitfield_type_positions) > 0 ? 1 : 0);
    name_bitfield().construct(std::max((typename name_bitfield_array_type::size_type)1,initial_name_bitfield_array_size),0);
  }

  ~name_space() {
    name_bitfield().destruct();
  }

  // Number of names that can be accommodated without growing the name array:
  name_type capacity() const {
    return m_name_bitfield_size * name_bitfield_type_positions;
  }

  name_type create_name() {
    name_type name = 0;

    // No names to reclaim, create a new one:
    if(m_name_queue.empty()) {
      assert(name_bitfield().size > 0);

      const size_t name_bitfield_index = name_bitfield().size - 1;
      const name_bitfield_type name_bitfield_value = name_bitfield()[name_bitfield_index];

      size_t name_bitfield_position = 0;

      // If either created or init bits are set, skip it:
      name_bitfield_type mask = (name_bitfield_named_mask | name_bitfield_init_mask) << (name_bitfield_position << 1);

      while(((name_bitfield_value & mask) != 0) && (name_bitfield_position < name_bitfield_type_positions)) {
	++name_bitfield_position;
	mask <<= 2;
      }

      name = (name_bitfield_index * name_bitfield_type_positions) + name_bitfield_position;
    }
    // Reclaim the name from the queue:
    else {
      name = m_name_queue.front();
      m_name_queue.pop_front();
    }

    // Expand the bitfield that keeps track of generated & created names:
    size_t name_bitfield_index, name_bitfield_position2;
    boost::tie(name_bitfield_index,name_bitfield_position2) = name_bitfield_location(name);
    const size_t name_bitfield_size = name_bitfield_index + 1;
    
    if(name_bitfield_size > name_bitfield().size) {
      name_bitfield().resize(name_bitfield_size);
    }

    name_bitfield()[name_bitfield_index] &= ~(name_bitfield_type)(name_bitfield_mask << name_bitfield_position2);
    name_bitfield()[name_bitfield_index] |= (name_bitfield_type)(name_bitfield_named_mask << name_bitfield_position2);

#if !defined(NDEBUG)
    ++m_num_names;
#endif
    return name;
  }

  template< typename OtherNameType >
  size_t create_names(const size_t n,OtherNameType * names) {
    size_t i;
    for(i = 0;i < n;++i,++names) {
      *names = create_name();
    }
    return i;
  }

  bool is_name(const name_type name) const {
    size_t name_bitfield_index, name_bitfield_position2;
    boost::tie(name_bitfield_index,name_bitfield_position2) = name_bitfield_location(name);
    
    if(name_bitfield_index < name_bitfield().size) {
      return (name_bitfield()[name_bitfield_index] & (name_bitfield_named_mask << name_bitfield_position2)) != 0;
    }
    else {
      return false;
    }
  }

  void destroy_name(const name_type name,const size_t name_bitfield_index,const size_t name_bitfield_position2) {
    assert(name < capacity());
    assert(name != 0);

    // Destroy the name:
    if((name_bitfield()[name_bitfield_index] & (name_bitfield_named_mask << name_bitfield_position2)) != 0) {
      if(m_name_queue.full()) {
	const name_type name_queue_grow = std::max((name_type)(m_name_queue.size() / 2),(name_type)1);
	const name_type name_queue_size = m_name_queue.size() + name_queue_grow;

	name_queue_type new_name_queue(name_queue_size);
	std::copy(m_name_queue.begin(),m_name_queue.end(),std::back_inserter(new_name_queue));
	m_name_queue = new_name_queue;
      }

      m_name_queue.push_back(name);
      
      name_bitfield()[name_bitfield_index] &= ~(name_bitfield_type)(name_bitfield_named_mask << name_bitfield_position2);

#if !defined(NDEBUG)
      --m_num_names;
#endif
    }
  }

  void destroy_name(const name_type name) {
    size_t name_bitfield_index, name_bitfield_position2;
    boost::tie(name_bitfield_index,name_bitfield_position2) = name_bitfield_location(name);
    destroy_name(name,name_bitfield_index,name_bitfield_position2);
  }
};

#endif
