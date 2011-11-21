//-*-C++-*-
// RSXGL - Graphics library for the PS3 GPU.
//
// Copyright (c) 2011 Alexander Betts (alex.betts@gmail.com)
//
// name_space.h - Manage names that can be allocated from a contiguous set of integers.
// Used to allocate names for GL objects (e.g., buffers, programs, etc) as well as to
// allocate finite system resources (e.g., semaphores).
//
// A name is marked as allocated by setting a bit in a bitfield that grows as new names
// are required. Released names get pushed onto a circular buffer to be reclaimed later.

#ifndef rsxgl_name_space_H
#define rsxgl_name_space_H

#include "array.h"

#include <memory>
#include <limits>

#include <boost/tuple/tuple.hpp>
#include <boost/integer.hpp>
#include <boost/integer/integer_mask.hpp>
#include <boost/mpl/if.hpp>
#include <boost/static_assert.hpp>

#define BOOST_CB_DISABLE_DEBUG
#include <boost/circular_buffer.hpp>

template<
  // Maximum number of names, not the maximum name value:
  size_t MaxNames = std::numeric_limits< uint32_t >::max() + 1
  >
struct name_traits
{
  typedef typename boost::uint_value_t< MaxNames - 1 >::least name_type;
  typedef typename boost::uint_value_t< MaxNames >::least size_type;
};

template<
  // Maximum number of names, not the maximum name value:
  size_t MaxNames = std::numeric_limits< uint32_t >::max() + 1,

  // Is the namespace finite - does it keep track of a count & check to see if it's reached MaxNames?
  // If it's not, then don't waste space keeping track of the count.
  bool Finite = false,

  // Set a number of "user bits" associated with the name - mainly this is for GL objects,
  // which need to store a bit indicating that the object associated with the name has,
  // in fact, been created
  size_t UserBits = 0,

  // Integer type used to store each element of the bitfield array:
  typename NameBitfieldT = boost::uintmax_t,
  
  typename Alloc = std::allocator< void >
  >
struct name_space
{
  //typedef typename boost::uint_value_t< MaxNames - 1 >::least name_type;
  //typedef typename boost::uint_value_t< MaxNames >::least size_type;
  typedef typename name_traits< MaxNames >::name_type name_type;
  typedef typename name_traits< MaxNames >::size_type size_type;

  typedef NameBitfieldT bitfield_type;
  static const size_t bitfield_bits_per_name = UserBits + 1;

  static const size_t bitfield_type_bits = std::numeric_limits< bitfield_type >::digits;

  static const size_t bitfield_type_positions = bitfield_type_bits / bitfield_bits_per_name;
  static const bitfield_type bitfield_mask = boost::low_bits_mask_t< bitfield_bits_per_name >::sig_bits_fast;
  
  static const bitfield_type bitfield_named_mask = boost::high_bit_mask_t< 0 >::high_bit_fast;

  typedef array< bitfield_type, name_type, Alloc > bitfield_array_type;

  typename bitfield_array_type::size_type m_bitfield_size;
  typename bitfield_array_type::pointer_type m_bitfield;

  typename bitfield_array_type::type bitfield() {
    return typename bitfield_array_type::type(m_bitfield,m_bitfield_size);
  }

  typename bitfield_array_type::const_type bitfield() const {
    return typename bitfield_array_type::const_type(m_bitfield,m_bitfield_size);
  }

  // return index into bitfield array, and bit position in that element
  struct bitfield_location_type {
    size_t index, position;

    bitfield_location_type(const size_t _index,const size_t _position)
      : index(_index), position(_position) {
    }
  };

  static inline bitfield_location_type
  bitfield_location(const name_type name) {
    const size_t name_position = name * bitfield_bits_per_name;
    return bitfield_location_type(name_position / bitfield_type_bits,name_position % bitfield_type_bits);
  }

  typedef boost::circular_buffer< name_type > name_queue_type;
  name_queue_type m_name_queue;

  // Name counter:
  //
  // Finite counter:
  struct finite_count_type {
    size_type m_count;

    finite_count_type()
      : m_count(0) {
    }

    bool is_full() const {
      return (m_count == MaxNames);
    }

    void increment() {
      rsxgl_assert(m_count < MaxNames);
      ++m_count;
    }

    void decrement() {
      --m_count;
    }
  };

  // Infinite counter - doesn't do much of anything:
  struct infinite_count_type {
    infinite_count_type() {}

    bool is_full() const {
      return false;
    }

    void increment() {}
    void decrement() {}
  };

  typename boost::mpl::if_c< Finite, finite_count_type, infinite_count_type >::type m_count;

public:

  //
  name_space() {
    bitfield().construct(1,0);
  }

  ~name_space() {
    bitfield().destruct();
  }

  // Number of names that can be accommodated without growing the name array:
  size_type capacity() const {
    return m_bitfield_size * bitfield_type_positions;
  }

  std::pair< name_type, bool >
  create_name() {
    if(!m_count.is_full()) {
      name_type name = 0;
      
      // No names to reclaim, create a new one:
      if(m_name_queue.empty()) {
	rsxgl_assert(bitfield().size > 0);
	
	const size_t bitfield_index = bitfield().size - 1;
	const bitfield_type bitfield_value = bitfield()[bitfield_index];
	
	size_t bitfield_position = 0;
	
	// If either created or init bits are set, skip it:
	bitfield_type mask = bitfield_mask;
	
	while(((bitfield_value & mask) != 0) && (bitfield_position < bitfield_type_positions)) {
	  ++bitfield_position;
	  mask <<= bitfield_bits_per_name;
	}
	
	name = (bitfield_index * bitfield_type_positions) + bitfield_position;
      }
      // Reclaim the name from the queue:
      else {
	name = m_name_queue.front();
	m_name_queue.pop_front();
      }
      
      // Expand the bitfield that keeps track of generated & created names:
      const bitfield_location_type location = bitfield_location(name);
      const size_t bitfield_size = location.index + 1;
      
      if(bitfield_size > bitfield().size) {
	std::cout << "resizing bitfield array: " << bitfield_size << std::endl;
	bitfield().resize(bitfield_size);
      }
      
      bitfield()[location.index] &= ~(bitfield_type)(bitfield_mask << location.position);
      bitfield()[location.index] |= (bitfield_type)(bitfield_named_mask << location.position);

      m_count.increment();
      return std::make_pair(name,true);
    }
    else {
      return std::make_pair((name_type)0,false);
    }
  }

  bool is_name(const name_type name) const {
    const bitfield_location_type location = bitfield_location(name);
    
    if(location.index < bitfield().size) {
      return (bitfield()[location.index] & (bitfield_named_mask << location.position)) != 0;
    }
    else {
      return false;
    }
  }

  void destroy_name(const name_type name) {
    rsxgl_assert(name < capacity());

    const bitfield_location_type location = bitfield_location(name);

    if((bitfield()[location.index] & (bitfield_mask << location.position)) != 0) {
      if(m_name_queue.full()) {
	const name_type name_queue_grow = std::max((name_type)(m_name_queue.size() / 2),(name_type)1);
	const name_type name_queue_size = m_name_queue.size() + name_queue_grow;

	name_queue_type new_name_queue(name_queue_size);
	std::copy(m_name_queue.begin(),m_name_queue.end(),std::back_inserter(new_name_queue));
	m_name_queue = new_name_queue;
      }

      m_name_queue.push_back(name);
      
      bitfield()[location.index] &= ~(bitfield_type)(bitfield_mask << location.position);

      m_count.decrement();
    }
  }

  void detach_name(const name_type name) {
    rsxgl_assert(name < capacity());

    const bitfield_location_type location = bitfield_location(name);

    bitfield()[location.index] &= ~(bitfield_type)(bitfield_named_mask << location.position);
  }

  // Set, test, and clear the user bits:
  template< size_t Bit >
  struct user_bit_mask {
    BOOST_STATIC_ASSERT(Bit < UserBits);
    static const bitfield_type value = boost::high_bit_mask_t< Bit + 1 >::high_bit_fast;
  };

  template< size_t Bit >
  void set_user_bit(const name_type name) {
    const bitfield_location_type location = bitfield_location(name);
    bitfield()[location.index] |= (user_bit_mask< Bit >::value << location.position);
  }

  template< size_t Bit >
  bool test_user_bit(const name_type name) const {
    const bitfield_location_type location = bitfield_location(name);
    return bitfield()[location.index] & (user_bit_mask< Bit >::value << location.position);
  }

  template< size_t Bit >
  void clear_user_bit(const name_type name) {
    const bitfield_location_type location = bitfield_location(name);
    bitfield()[location.index] &= ~(user_bit_mask< Bit >::value << location.position);
  }
};

#endif
