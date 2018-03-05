/*	Stuart Pook, Genoscope 2007 (enscript -E $%) (Indent ON)
 *	$Id: value_input.h 781 2007-06-01 15:18:30Z stuart $
 */
#include <fltk/ValueInput.h>
#include <vector>
#include <boost/function.hpp>

class MyValueInput : public fltk::ValueInput
{
	typedef std::vector<unsigned> counts_t;
	counts_t::const_iterator const _begin;
	counts_t::const_iterator const _end;
	counts_t::const_iterator _current_count;
	counts_t::const_iterator _prev;
	counts_t::value_type _back;
	boost::function<void ()> const cb;
	void value_damage()
	{
		limit_value();
		fltk::ValueInput::value_damage();
	}
	static void call_arg(Widget *, void * ud);
	void limit_value();
	void init(counts_t::const_iterator current);
public:
	MyValueInput(int x, int y, int w, int h, counts_t const & c, boost::function<void ()> f = 0) :
		fltk::ValueInput(x, y, w, h),
		_begin(c.begin()),
		_end(c.end()),
		cb(f)
	{
		init(_begin);
		assert(counts_t::const_reverse_iterator(_begin) == c.rend());
	}
	MyValueInput(int x, int y, int w, int h, counts_t const & c, bool, boost::function<void ()> f = 0) :
		fltk::ValueInput(x, y, w, h),
		_begin(c.begin()),
		_end(c.end()),
		cb(f)
	{
		init(_begin == _end ? _begin : _end - 1);
//		assert(counts_t::const_reverse_iterator(_begin) == c.rend());
	}
	MyValueInput(int x, int y, int w, int h, counts_t::const_iterator & b, counts_t::const_iterator & e, boost::function<void ()> f = 0) :
		fltk::ValueInput(x, y, w, h),
		_begin(b),
		_end(e),
		cb(f)
	{
		init(_begin);
	}
	counts_t::const_iterator current() const { return _current_count; }
	void current(counts_t::const_iterator);
};
