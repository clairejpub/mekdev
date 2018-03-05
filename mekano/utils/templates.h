/*	Stuart Pook, Genoscope 2006 make (Indent on)
 *	$Id: templates.h 724 2007-05-10 10:50:05Z stuart $
 */
#ifndef _import_templates_H
#define _import_templates_H _import_templates_H
#include <sstream>
#include <iterator>
#include <functional>
#include <utility>
#include <iostream>
#include <cassert>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>

template<class In, class Out, class Pred>
inline Out
copy_if(In first, In last, Out res, Pred p)
{
	while (first != last)
	{
		if (p(*first))
			*res++ = *first;
		++first;
	}
	return res;
}

#if 1
template <class Container>
class Counter : public std::iterator<std::output_iterator_tag, void, void, void, void>
{
	unsigned _n;
public:
	explicit Counter() : _n(0) {}
	Counter & operator=(typename Container::value_type const &)
	{
		_n++;
		return *this;
	}
	Counter & operator*() { return *this; }
	Counter & operator++() { return *this; }
	Counter & operator++(int) { return *this; }
	unsigned n() const { return _n; }
};
template <class Container>
inline Counter<Container>
counter(Container c)
{
	return Counter<Container>();
}

#endif

#if 0
template <class I>
inline I
from_string(std::string s)
{
	I i; 
	if (!(std::istringstream(s) >> i))
		return -1;
	return i;
}
#endif

template <class I>
inline std::string to_string(I const & i)
{
	std::stringstream s;
	s << i;
	return s.str();
}

template<class P>
struct pair_less : public std::binary_function<P, P, bool>
{
	bool operator()(P const & o1, P const & o2) const
	{
		if (o1.first < o2.first)
			return true;
		if (o2.first < o1.first)
			return false;
		return o1.second < o2.second;
	}
};

template<class In, class Equal, class Op>
void
on_equal(In first, In last, Equal equal, Op op)
{
	while ((first = std::adjacent_find(first, last, equal)) != last)
	{
		In i = first++;
		op(*i, *first);
	}
}

template<typename ForwardIterator, typename Predicate>
typename std::iterator_traits<ForwardIterator>::difference_type
count_different(ForwardIterator first, ForwardIterator last, Predicate pred)
{
	if (first == last)
		return 0;
	ForwardIterator next = first;
	typename std::iterator_traits<ForwardIterator>::difference_type n = 1;
	while (++next != last)
	{
		if (!pred(*first, *next))
			n++;
		first = next;
	}
	return n;
}

template<class Pair, class Pred>
class UnaryOnFirst : public std::unary_function<Pair, typename Pred::result_type>
{
	Pred op;
public:
	explicit UnaryOnFirst(Pred const & p) : op(p) {}
	typename Pred::result_type operator()(Pair const & x) const { return op(x.first); }
};
template<class Pair, class Pred>
inline UnaryOnFirst<Pair, Pred>
unary_on_first(Pred const & p)
{
	return UnaryOnFirst<Pair, Pred>(p);
}

template<class P, class Op>
struct Pair1 : public std::binary_function<P, P, typename Op::result_type>
{
	Op const op;
	Pair1(Op o) : op(o) {}
	typename Op::result_type operator()(P const & o1, P const & o2) const { return op(o1.first, o2.first); }
};
template<class P, class Op>
inline Pair1<P, Op>
pair1(Op const & op)
{
	return Pair1<P, Op>(op);
}

#if 0
template<class P, class Op>
struct Pair2 : public std::binary_function<P, P, bool>
{
	Pair2(Op o) : op(o) {}
	bool operator()(P const & o1, P const & o2) const { return op(o1.second, o2.second); }
private:
	Op const op;
};
template<class P, class Op>
inline Pair2<P, Op>
pair2(Op const & op)
{
	return Pair2<P, Op>(op);
}
#endif

template <class In, class Test, class BinOp>
class SplitAndRun : public std::binary_function<In, In, void>
{
	Test test;
	BinOp op;
public:
	SplitAndRun(Test t,  BinOp o) : test(t), op(o) {}
	void operator()(In const & first, In const & last) const
	{
		using namespace boost::lambda;
		In other = std::find_if(first, last, !bind<bool>(test, _1, *first));
		for_all_pairs(first, other, other, last, op);
	}
};

template <class In, class Test, class BinOp>
inline SplitAndRun<In, Test, BinOp>
split_and_run(Test test, BinOp op)
{
	return SplitAndRun<In, Test, BinOp>(test, op);
}

template <class V>
class Increase
{
	V v;
	V value;
public:
	Increase(V iv, V initial = 0) : v(iv), value(initial) {}
	V operator()() { V tmp = value; value += v; return tmp; }
};
template <class V>
inline Increase<V>
increase(V v)
{
	return Increase<V>(v);
}

template <class In, class Map, class Op>
Map const & make_simple_mapping(In first, In const & last, Map & map, Op op)
{
	for (; first != last; first++)
		map[*first] = op();
	return map;
}

#if 0
template <class V, class Mapped>
class IncreaseOnChange : public std::unary_function<V, std::pair<typename V::second_type, Mapped> >
{
	Mapped inc;
	Mapped value;
	typename V::first_type current;
	bool first;
public:
	IncreaseOnChange(Mapped iv, Mapped initial) : inc(iv), value(initial), first(true) {}
	std::pair<typename V::second_type, Mapped> operator()(V const & i)
	{
		if (first)
		{
			first = false;
			current = i.first;
		}
		else if (i.first != current)
		{
			value += inc;
			current = i.first;
		}
		return std::make_pair(i.second, value);
	}
};
template <class V, class Mapped>
inline IncreaseOnChange<V, Mapped> increase_on_change(Mapped inc, Mapped initial = 0)
{
	return IncreaseOnChange<V, Mapped>(inc, initial);
}
#endif

#if 0
template <class In, class Map, class Op>
Map const & make_mapping(In first, In const & last, Map & map, Op op)
{
	for (; first != last; first++)
	{
		typename Op::result_type tmp = op(*first);
		map[tmp.first] = tmp.second;
	}
	return map;
}
#endif

template<typename In, typename BinOp>
BinOp
product(In first, In last, BinOp f)
{
	for (; first != last; first++)
		for (In i = first; ++i != last; )
			f(*first, *i);
	return f;
}

template<typename In, typename In2, typename BinOp>
BinOp
for_all_pairs(In first, In last, In2 first2, In2 last2, BinOp f)
{
	for (; first != last; first++)
		for (In2 i = first2; i != last2; i++)
			f(first, i);
	return f;
}

template<typename In, typename In2, typename Out, typename Op>
static Out
transform_all_pairs_if(In main, In last, In2 parallel, Out out, Op f)
{
	unsigned position = 0;
	for (; main != last; main++, parallel++, position++)
	{
		In main2 = main;
		In2 parallel2 = parallel;
		unsigned position2 = position;
		while (++main2 != last)
		{
			typename Op::result_type v = f(
				*main, *parallel, position,
				*main2, *++parallel2, ++position2
			);
			if (v.first)
				*out++ = v.second;
		}
	}
	return out;
}

template<typename In, typename In2, typename Op>
static Op
for_all_pairs(In first, In last, In2 first2, Op f)
{
	for (; first != last; first++, first2++)
	{
		In2 j = first2;
		In i = first;
		while (++i != last)
			f(*first, *first2, *i, *++j);
	}
	return f;
}

template <class In, class In2, class Op>
Op for_each(In first, In last, In2 first2, Op f)
{
	while (first != last)
		f(*first++, *first2++);
	return f;
}

template <class In, class Op>
Op for_eachi(In first, In last, Op f)
{
	while (first != last)
		f(first++);
	return f;
}

template <class In, class Op>
Op for_each_pair_i(In first, In last, Op f)
{
	while (first != last)
	{
		In n = first++;
		if (first == last)
			f(n);
		else
			f(n, first++);
	}
	return f;
}

template<class In, class Out, class Op, class Pred>
Out transform_if(In first, In last, Out out, Op op, Pred p)
{
	while (first != last)
	{
		if (p(*first))
			*out++ = op(*first);
		first++;
	}
	return out;
}

template<class In, class Out, class BinOp>
BinOp transform_op(In first, In last, Out res, BinOp op)
{
	while (first != last)
		*res++ = op(*first++);
	return op;
}

template<class T>
T identity(T const & x) { return x; }

template<typename In1, typename In2, typename Out, typename Op>
static void
double_set_intersection(In1 first1, In1 end1, In2 first2, In2 end2, Out out1, Out out2, Op op)
{
	std::set_intersection(first1, end1, first2, end2, out1, op);
	std::set_intersection(first2, end2, first1, end1, out2, op);
}

template <class Arg1, class Arg2, class Result>
struct construct_from_arguments : public std::binary_function<Arg1, Arg2, Result >
{
	Result operator()(Arg1 const & a1, Arg2 const & a2) const
	{
		return Result(a1, a2);
	}
};
template <class Arg2, class Result, class Arg1>
inline std::binder1st<construct_from_arguments<Arg1, Arg2, Result> >
constructor_from_arguments(Arg1 a1)
{
	return std::bind1st(construct_from_arguments<Arg1, Arg2, Result>(), a1);
}

template<typename T> class Sum : public std::unary_function<T, void>
{
	T res;
public:
	Sum(T i = 0) : res(i) {}
	void operator()(T const & x) { res += x; }
	T result() const { return res; }
};

template<typename U, typename W>
class Sum<std::pair<U, W> > : public std::unary_function<std::pair<U, W>, void>
{
	typedef std::pair<U, W> T;
	T res;
public:
	Sum(T const & i = std::make_pair(0, 0)) : res(i) {}
	void operator()(T const & x) { res.first += x.first;  res.second += x.second; }
	T result() const { return res; }
	T operator()() const { return res; }
};

template <class In, class Op>
typename Op::result_type
sum(In first, In last, Op f)
{
	typename Op::result_type s(0);
	while (first != last)
		s += f(*first++);
	return s;
}

template <class R, class In, class Op>
R
sum(In first, In last, Op f)
{
	R s(0);
	while (first != last)
		s += f(*first++);
	return s;
}

template <class In, class Op, class Pred>
typename Op::result_type
sum_if(In first, In last, Op f, Pred p)
{
	typename Op::result_type s(0);
	for (; first != last; first++)
		if (p(*first))
			s += f(*first);
	return s;
}

template <class Numeric>
inline Numeric
end2length(Numeric const & start, Numeric const & stop)
{
	assert(start <= stop);
	return stop - start + 1;
}

template <class Numeric, class Positive>
inline Numeric
length2end(Numeric const & start, Positive const & length)
{
	assert(length > 0); // if the length is zero, there is no end
	return start + length - 1;
}

template< typename  C, typename  F>
inline F
for_all(C &c, F f)
{
	return std::for_each(c.begin(), c.end(), f);
}
/*
//http://www.awprofessional.com/articles/article.asp?p=345948&seqNum=2&rl=1
template< typename  T, size_t    N, typename  F>
inline F
for_all(T (&ar)[N], F f)
{
  return std::for_each(&ar[0], &ar[N], f);
}*/

#endif
