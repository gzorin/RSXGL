//-*-C++-*-
//
// Adaptation of the C++ standard library's set functions. The main difference
// between the two is that when members of each iteration compare equally with
// each other, /both/ are sent passed to the function's output. For these functions,
// the "output iterator" is actually more like a visitor functor that has first(),
// second(), and both() functions. The compare functor returns a signed integer
// in the manner of strcmp() - less than is indicated by a value < 0, equality
// represented by a value of 0, and greater than is indicated by a value > 0.

#ifndef set_algorithm2_H
#define set_algorithm2_H

#include <algorithm>
#include <functional>
#include <boost/bind/bind.hpp>

template<typename _InputIterator1, typename _InputIterator2,
	 typename _Visitor, typename _Compare>
_Visitor
set_union2(_InputIterator1 __first1, _InputIterator1 __last1,
	   _InputIterator2 __first2, _InputIterator2 __last2,
	   _Visitor __visitor, _Compare __comp)
{
  typedef typename std::iterator_traits<_InputIterator1>::value_type
    _ValueType1;
  typedef typename std::iterator_traits<_InputIterator2>::value_type
    _ValueType2;
  
  while (__first1 != __last1 && __first2 != __last2)
    {
      int c = __comp(*__first1,*__first2);

      if (c < 0)
	{
	  __visitor.first(*__first1);
	  ++__first1;
	}
      else if (c > 0)
	{
	  __visitor.second(*__first2);
	  ++__first2;
	}
      else
	{
	  __visitor.both(*__first1,*__first2);
	  ++__first1;
	  ++__first2;
	}
    }

  std::for_each(__first1,__last1,boost::bind(&_Visitor::first,&__visitor,_1));
  std::for_each(__first2,__last2,boost::bind(&_Visitor::second,&__visitor,_1));
}

template<typename _InputIterator1, typename _InputIterator2,
	 typename _Visitor, typename _Compare>
_Visitor
set_intersection2(_InputIterator1 __first1, _InputIterator1 __last1,
		  _InputIterator2 __first2, _InputIterator2 __last2,
		  _Visitor __visitor, _Compare __comp)
{
  typedef typename std::iterator_traits<_InputIterator1>::value_type
    _ValueType1;
  typedef typename std::iterator_traits<_InputIterator2>::value_type
    _ValueType2;
  
  while (__first1 != __last1 && __first2 != __last2) {
    int c = __comp(*__first1,*__first2);
    
    if (c < 0)
      ++__first1;
    else if (c > 0)
      ++__first2;
    else
      {
	__visitor.both(*__first1,*__first2);
	++__first1;
	++__first2;
      }
  }

  return __visitor;
}

template<typename _InputIterator1, typename _InputIterator2,
	 typename _Visitor, typename _Compare>
_Visitor
set_difference2(_InputIterator1 __first1, _InputIterator1 __last1,
		_InputIterator2 __first2, _InputIterator2 __last2,
		_Visitor __visitor, _Compare __comp)
{
  typedef typename std::iterator_traits<_InputIterator1>::value_type
    _ValueType1;
  typedef typename std::iterator_traits<_InputIterator2>::value_type
    _ValueType2;
  
  while (__first1 != __last1 && __first2 != __last2) {
    int c = __comp(*__first1,*__first2);
    
    if (c < 0)
      {
	__visitor.first(*__first1);
	++__first1;
      }
    else if (c > 0)
      ++__first2;
    else
      {
	++__first1;
	++__first2;
      }
  }
    
  std::for_each(__first1,__last1);
}

template<typename _InputIterator1, typename _InputIterator2,
	 typename _Visitor, typename _Compare>
_Visitor
set_symmetric_difference2(_InputIterator1 __first1, _InputIterator1 __last1,
			  _InputIterator2 __first2, _InputIterator2 __last2,
			  _Visitor __visitor,
			  _Compare __comp)
{
  typedef typename std::iterator_traits<_InputIterator1>::value_type
    _ValueType1;
  typedef typename std::iterator_traits<_InputIterator2>::value_type
    _ValueType2;
  
  while (__first1 != __last1 && __first2 != __last2) {
    int c = __comp(*__first1,*__first2);
    
    if (c < 0)
      {
	*__visitor.first(*__first1);
	++__first1;
      }
    else if (c > 0)
      {
	*__visitor.second(*__first2);
	++__first2;
    }
    else
      {
	++__first1;
	++__first2;
      }
  }
  
  std::for_each(__first1,__last1,boost::bind(&_Visitor::first,&__visitor,_1));
  std::for_each(__first2,__last2,boost::bind(&_Visitor::second,&__visitor,_1));
}

#endif
