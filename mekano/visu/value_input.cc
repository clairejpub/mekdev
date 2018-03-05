/*	Stuart Pook, Genoscope 2007 (enscript -E $%) (Indent ON)
 *	$Id: value_input.cc 781 2007-06-01 15:18:30Z stuart $
 */
#include "value_input.h"

void
MyValueInput::limit_value()
{
	if (_begin == _end)
		return;
	counts_t::const_iterator const orig = _current_count;
	int const v = int(value() + 0.2);
	if (v < 0 || unsigned(v) <= *_begin)
		_current_count = _begin;
	else if (unsigned(v) >= _back)
		_current_count = _end - 1;
	else if (unsigned(v) > *_current_count)
		_current_count = lower_bound(_current_count, _end, unsigned(v));
	else if (unsigned(v) < *_current_count)
	{
		counts_t::const_reverse_iterator r(_current_count + 1);
		counts_t::const_reverse_iterator rend(_begin);
		counts_t::const_reverse_iterator r2 = lower_bound(r, rend, unsigned(v), std::greater<unsigned>());
		_current_count = (++r2).base();
	}
//	std::cout << "@ v = " << v << " *orig " << *orig << " *_current_count " << *_current_count << std::endl;
	if (*_current_count != value())
		value(*_current_count);
}

void
MyValueInput::init(counts_t::const_iterator current)
{
	_current_count = current;
	if (_begin == _end)
		deactivate();
	else
	{
		_prev = _current_count;
		_back = *(_end - 1);
		callback(call_arg, this);
		linesize(1);
		value(*_current_count);
		range(*_begin, _back);
		when(fltk::WHEN_RELEASE | fltk::WHEN_ENTER_KEY);
	}
}

void
MyValueInput::call_arg(Widget *, void * ud)
{
	MyValueInput * w = static_cast<MyValueInput *>(ud);
	w->limit_value();
	if (w->_current_count != w->_prev)
	{
		if (w->cb)
			w->cb();
		w->_prev = w->_current_count;
	}
}

void
MyValueInput::current(counts_t::const_iterator current)
{
	if (current != _current_count)
	{
		_current_count = current;
		value(*_current_count);
	}
}
