/*	Stuart Pook, Genoscope 2007 (Indent ON)
 *	$Id: graph.cc 615 2007-03-30 10:23:04Z stuart $
 */

#include "graphics.h"
#include <list>
#include <algorithm>

#include "graph.h"
#include <boost/bind.hpp>
#include <cmath>

namespace
{
	Colour const non_selected(0, 149 / 255.0, 255 / 255.0);

	void draw_point
	(
		Graph * widget,
		index_t::value_type const & v,
		index_t::value_type::first_type const offset,
		double const width,
		double const max,
		index_t::value_type::first_type start,
		index_t::value_type::first_type stop
	)
	{
		double const x = (v.first - offset) * width;
		if (v.first == start)
			widget->set_colour(Colour(1, 1, 0));
		double height = ((v.second == 1) ? log(2) / 2 : log(v.second)) * max;
		glVertex2d(x, 0);
		glVertex2d(x, height);
		if (v.first == stop)
		{
	//		std::cout << "draw_point v.first " << v.first << " offset " << offset << " x " << x << std::endl;
	//		std::cout << "draw_point v.second " << v.second << " max " << max << " height " << height << std::endl;
			widget->set_colour(non_selected);
		}
	}
}

void
Graph::draw_all()
{
	if (!_table->empty())
	{
		index_t::value_type::first_type const offset = _table->front().first;
		double const width = (_table->back().first == offset) ? 0 : 1.0 / (_table->back().first - offset);
		index_t::value_type::second_type max = std::max_element
		(
			_table->begin(),
			_table->end(),
			boost::bind
			(
				std::less<index_t::value_type::second_type>(),
				boost::bind(&index_t::value_type::second, _1),
				boost::bind(&index_t::value_type::second, _2)
			)
		)->second;
		glutil::begin poly(GL_LINES);
		set_colour(non_selected);
		std::for_each(_table->begin(), _table->end(), boost::bind(draw_point, this, _1, offset, width, 1 / log(max), _start, _stop));
	}
}

void
Graph::initialize()
{
	glClearColor(0.3, 0.3, 0.4, 0);
//	glShadeModel(GL_SMOOTH);
}

void
Graph::make_lists()
{
}

void
Graph::projection()
{
	double const d = 0.004;
	glOrtho(-d, 1 + d, 0, 1, -10, 10);
}
