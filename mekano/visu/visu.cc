/*	Stuart Pook, Genoscope 2007 (enscript -E $%) (Indent ON)
 *	$Id: visu.cc 804 2008-05-06 16:18:50Z cjubin $
 *	Requires fltk-2 (or greater?)
 */
#include <fltk/Window.h>
#include <fltk/GlWindow.h>
#include <fltk/Widget.h>
#include <fltk/run.h>
#include <fltk/events.h>
#include <fltk/Button.h>
#include <fltk/CycleButton.h>
#include <fltk/CheckButton.h>
#include <fltk/ValueInput.h>
#include <fltk/PopupMenu.h>
#include <fltk/PackedGroup.h>
#include <fltk/Choice.h>
#include <fltk/gl.h>
#include <fltk/glut.h>
#include <GL/glu.h>
#include <fltk/visual.h>
#include <fltk/Output.h>
#include <fltk/ValueSlider.h>
#include <fltk/FloatInput.h>

#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <limits>
#include <set>
#include <list>
#include <functional>
#include <cassert>
#include <algorithm>
#include <errno.h>

#include "mekano.h"
#include <utils/templates.h>
#include <utils/iterators.h>
#include <utils/utilities.h>
#include "graphics.h"
#include "utils.h"
#include "visu.h"
#include "bcm.h"
#include "circle.h"
#include <boost/lambda/bind.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/any.hpp>

#include <utils/sctg_data.h>
#include <utils/eyedb.h>

namespace
{
	boost::lambda::placeholder1_type l1;
	boost::lambda::placeholder2_type l2;
}

typedef clones_t read_positions_t;

static char const * myname;

namespace
{
	double const torus_text = 1;
	double const torus_inner = 0.05;
	double const torus_outer = 1 - torus_inner - 0.01;
	double const torus_big_radius = torus_outer;
	double const torus_small_radius = torus_inner;
	Colour const gap_colour = Colour::gray(100);
}

struct lookup
{
	typedef eyedb::Oid result_type;
	result_type operator()(eyedb::Database * const db, std::string const & type, std::string const & name, bool force = true)
	{
		return this->operator()(db, "select " + type + ".name == \"" + name + '"', force);
	}
	result_type operator()(eyedb::Database * const db, std::string const & type, int id, bool force = true)
	{
		return this->operator()(db, "select " + type + ".id==" + to_string(id), force);
	}
	result_type operator()(eyedb::Database * const db, std::string const & query, bool force = true)
	{
		eyedb::OidArray oids;
		eyedb::OQL(db, string2eyedb(query)).execute(oids);
		if (oids.getCount() > 1)
			throw MyError("multiple objects from: " + query);
		if (oids.getCount() == 0)
			if (force)
				throw MyError("zero objects from: " + query);
			else
				return eyedb::Oid::nullOid;
		return oids[0];
	}
};

inline eyedb::Object *
load(eyedb::Database * const db, eyedb::Oid const & oid)
{
	if (oid == eyedb::Oid::nullOid)
		return 0;
	eyedb::Object * o;
	db->loadObject(oid, o);
	return o;
}

inline eyedb::Object *
load(eyedb::Database * const db, std::string const & type, std::string const & name, bool force = true)
{
 	eyedb::Oid oid = lookup()(db, type, name, force);
	return load(db, oid);
}

inline eyedb::Object *
load(eyedb::Database * const db, std::string const & type, int id, bool force = true)
{
 	eyedb::Oid oid = lookup()(db, type, id, force);
	if (oid == eyedb::Oid::nullOid)
		return 0;
	return load(db, oid);
}

enum
{
	main_radius = 7,
	link_radius = 2,
	link_gap = 1,
};

struct ReadEqual : public std::binary_function<SupercontigData::PositionnedRead const *, SupercontigData::PositionnedRead const *, bool>
{
	bool operator()(first_argument_type const & p1, second_argument_type const & p2) const
	{
		return p1->read_position()->read_oid() == p2->read_position()->read_oid();
	}
};

struct CloneEqual : public std::binary_function<SupercontigData::PositionnedRead const *,SupercontigData::PositionnedRead const *, bool>
{
	bool operator()(first_argument_type const & p1, second_argument_type const & p2) const
	{
		return p1->read_position()->read()->clone_oid() == p2->read_position()->read()->clone_oid();
	}
};

template <typename P>
struct LessClone : public std::binary_function<P, P, bool>
{
	bool operator()(P const & p1, P const & p2) const
	{
		return p1.first->read_position()->read()->clone_oid() < p2.first->read_position()->read()->clone_oid();
	}
};

typedef Overlap<boost::any> overlap_t;

namespace
{
	bool forward(SupercontigData::PositionnedRead const * pr, SupercontigData const * scaf)
	{
	  // std::clog << "forward: read " << pr->read_position()->read()->name() << " is " << pr->read_position()->forward();
	  // std::clog << ", SCdata is " << scaf->contigs().at(pr->ctg_number()).forward();
	  // std::clog << ", XOR is " << (pr->read_position()->forward() ^ scaf->contigs().at(pr->ctg_number()).forward());
	  // std::clog << ", result is " << !(pr->read_position()->forward() ^ scaf->contigs().at(pr->ctg_number()).forward()) << std::endl;

		return !(pr->read_position()->forward() ^ scaf->contigs().at(pr->ctg_number()).forward());
	}
}

struct DrawLien
{
	static int y(unsigned where) { return where * (link_radius * 2 + link_gap); }
	typedef void result_type;
	int x(int x, int pos) const { return x - (_reverse ? pos : 0); }
	void operator()(SupercontigInfo::clone_link_t const & i, unsigned where)
	{
		int const yp = _y + y(where) * _direction;
		bool /* const */ reversed = !(forward(i.positionned_read1(), _scaf) ^ forward(i.positionned_read2(), _scaf));
		enum { start_in_db = SupercontigData::start_in_db, };
	      
		/* debug. See below */
#if 0
		std::clog << "Read1 : " << i.positionned_read1()->read_position()->read()->name() << " @ " << i.positionned_read1()->read_position()->start_on_contig() << " is " << forward(i.positionned_read1(), _scaf);
		std::clog << ", on contig " << i.positionned_read1()->ctg_number() << " (" << _scaf->contigs().at(i.positionned_read1()->ctg_number()).name();
		std::clog << " " << _scaf->contigs().at(i.positionned_read1()->ctg_number()).length() << ")";
		std::clog << std::endl;

		std::clog << "read_info : " << i.clone1()->second.first + start_in_db << "-" << i.clone1()->second.second + start_in_db << std::endl;

		std::clog << "Read2 : " << i.positionned_read2()->read_position()->read()->name() << " @ " << i.positionned_read2()->read_position()->start_on_contig() << " is " << forward(i.positionned_read2(), _scaf);

		std::clog << ", on contig " << i.positionned_read2()->ctg_number() << " (" << _scaf->contigs().at(i.positionned_read2()->ctg_number()).name();
		std::clog << " " << _scaf->contigs().at(i.positionned_read2()->ctg_number()).length() << ")";
		std::clog << std::endl;

		std::clog << "read_info : " << i.clone2()->second.first + start_in_db << "-" << i.clone2()->second.second + start_in_db << std::endl;
#endif

		/*
		 * claude@genoscope.cns.fr -- mai 2008
		 *
		 * bugfix: direction errors were not reported when read ends were 
		 * opposite direction but misplaced
		 *
		 */
		if ((!reversed) && (!forward(i.positionned_read1(), _scaf))){
		  if (i.clone1()->second.first + start_in_db  < i.clone2()->second.first + start_in_db){
		    reversed = true;
		    
		    // std::clog << "BUG" << std::endl;
		  }
		}

		Colour c = (bank_colours->colour(i.bank(), reversed));
		int const x = _x + (_reverse ? -(i.start() + i.length()) : i.start());
// 40s real time to draw SC2 when slow, ~2s when fast
// 3m16 to draw SC1 when slow, 9.6s to draw SC1 when fast
		_overlap->add(x, yp - link_radius, i.length(), 2 * link_radius, i);
		cylinder(x, yp, i.length(), link_radius, c);
	}
	DrawLien(int x, int y, BankInfo * bcm, int d, bool reverse, SupercontigData const * scaf,
		overlap_t * const overlap) :
		_x(x),
		_y(y),
		bank_colours(bcm),
		_direction(d),
		_reverse(reverse),
		_scaf(scaf),
		_overlap(overlap)
		 {}
private:
	int const _x;
	int const _y;
	BankInfo * const bank_colours;
	int const _direction;
	bool const _reverse;
	SupercontigData const * const _scaf;
	overlap_t * const _overlap;
};

template <class Iterator, class Inserter>
struct BuildCloneLink : public std::binary_function<Iterator, Iterator, void>
{
	void operator()(Iterator i, Iterator j)
	{
		SupercontigInfo::clone_link_t r(&*i, &*j);
		*_biggest_error = std::max(r.length_error(), *_biggest_error);
		*insert++ = r;
	}
	BuildCloneLink(Inserter const & i, double * biggest_error) :
		insert(i),
		_biggest_error(biggest_error)
	{
	}
private:
	Inserter insert;
	double * const _biggest_error;
};

template<class Iterator, class Inserter>
inline BuildCloneLink<Iterator, Inserter>
build_clone_link(Inserter inserter, double * biggest_error)
{
	return BuildCloneLink<Iterator, Inserter>(inserter, biggest_error);
}

template <class Draw>
class OrderLinks
{
	unsigned * _maxsize;
	Draw _draw;
	BankInfo * _bcm;
	double _max_error;
	typedef std::vector<std::map<int, int> > map_t;
	map_t _first_available;
	struct fits
	{
		typedef bool result_type;
		result_type operator()(map_t::value_type const & m, int start, int end) const
		{
			map_t::value_type::const_iterator p = m.lower_bound(start);
			if (p == m.end())
				return (--p)->second < start;
			else if (p == m.begin())
				return end < p->first;
			else
				return end < p->first && (--p)->second < start;
		}
	};
public:
	typedef void result_type;
	OrderLinks(unsigned * maxsize, BankInfo * const bcm, double max_error, Draw const & draw) : _maxsize(maxsize), _draw(draw),
		_bcm(bcm),
		_max_error(max_error)
	{
		*_maxsize = 0;
	}
	void operator()(SupercontigInfo::clone_link_t const & v)
	{
		if (!_bcm->visible(v.bank()) || v.length_error() > _max_error)
			return;
		enum { separator = link_radius * 4 };
		int const end = v.start() + v.length() + separator;
		using namespace boost::lambda;
		map_t::iterator p = find_if(_first_available.begin(), _first_available.end(), bind(fits(), l1, v.start(), end));

		unsigned where;
		if (p != _first_available.end())
			where = p - _first_available.begin();
		else
		{
			*_maxsize = where = _first_available.size();
			_first_available.resize(where + 1);
		}
		_draw(v, where);
		_first_available.at(where).insert(std::make_pair(v.start(), end));
	}
};
template <class Draw>
inline OrderLinks<Draw>
order_links(unsigned * maxsize, BankInfo * bcm, double max_error, Draw const & d) { return OrderLinks<Draw>(maxsize, bcm, max_error, d); }

template<class In, class Equal, class Op>
bool
on_all_equal_sequences(In where, In last, Equal equal, Op op)
{
	while ((where = std::adjacent_find(where, last, equal)) != last)
	{
		In i = where;
		using namespace boost::lambda;
		where = std::find_if(where + 1, last, !bind<bool>(equal, boost::lambda::_1, *i));
		op(i, where);
	}
	return true;
}

void
calculate_clone_links
(
	std::string const & sctg,
	clones_t const & clones,
	double * const biggest_error,
	SupercontigInfo::clone_links_t * clone_links
)
{
	clone_links->clear();
	*biggest_error = -1;
	Perfs p1("calculate_clone_links: " + sctg);
	Perfs perf_on_all_equal_sequences("calculate_clone_links: on_all_equal_sequences " + sctg);
	on_all_equal_sequences
	(
		clones.begin(),
		clones.end(),
		boost::bind
		(
			CloneEqual(),
			boost::bind(&read_positions_t::value_type::first, _1),
			boost::bind(&read_positions_t::value_type::first, _2)
		),
		split_and_run<read_positions_t::const_iterator>
		(
			boost::bind
			(
				ReadEqual(),
				boost::bind(&read_positions_t::value_type::first, _1),
				boost::bind(&read_positions_t::value_type::first, _2)
			),
			build_clone_link<clones_t::const_iterator>(std::back_inserter(*clone_links), biggest_error)
		)
	);
	perf_on_all_equal_sequences.done();

	Perfs perf_sort("calculate_clone_links: sort " + sctg);
	using namespace boost::lambda;
	std::sort
	(
		clone_links->begin(),
		clone_links->end(),
		bind(&SupercontigInfo::clone_link_t::length, l1) > bind(&SupercontigInfo::clone_link_t::length, l2)
	);
}

static int
draw_liens
(
	int const x,
	int const y,
	BankInfo * const bcm,
	int const direction,
	bool const reverse,
	SupercontigInfo const * const scaf,
	overlap_t * const overlap,
	double const error_limit
)
{
	glutil::begin poly(GL_QUADS);
	unsigned rows;
	Perfs perf_order_links("draw_liens: " + scaf->sctg()->name());
	std::for_each(scaf->clone_links().begin(), scaf->clone_links().end(), order_links(&rows, bcm, error_limit, DrawLien(x, y, bcm, direction, reverse, &scaf->data(), overlap)));
	return y + (DrawLien::y(rows) + link_radius) * direction;
}

inline std::string
name(mekano::Sequence const * s)
{
	return s->name();
}

template<class Drawer>
class DrawSctg : public std::unary_function<SupercontigData::SupercontigRange, void>
{
	Drawer * _drawer;
	int _hole_size;
	ObjectHandle<mekano::Parameters> _parameters;
	unsigned pos;
public:
	DrawSctg(Drawer * d, ObjectHandle<mekano::Parameters> const & p) :
		_drawer(d),
		_hole_size(p->hole_size()),
		pos(0)
	{}
	result_type operator()(argument_type const & sctg)
	{
		unsigned const len = sctg.length(_hole_size);
		if (mekano::Supercontig * s = sctg.sctg())
		{
//			std::clog << s->name() << " bases " << sctg.bases() << " holes " << sctg.holes() << " length " << len << std::endl;
			_drawer->sctg(pos, len, sctg.forward(), name(s));
		}
		pos += len;
	}
};
template<class Drawer>
class DrawSctg<Drawer>
draw_sctg(Drawer * drawer, ObjectHandle<mekano::Parameters> const & parameters)
{
	return DrawSctg<Drawer>(drawer, parameters);
}

class Positionner : public std::unary_function<SupercontigData::Range, contig_positions_t::value_type>
{
	contig_positions_t::value_type * const _pos;
	int const _hole_size;
public:
	Positionner(contig_positions_t::value_type * pos, ObjectHandle<mekano::Parameters> const & param) :
		_pos(pos),
		_hole_size(param->hole_size())
	{
		*_pos = 0;
	}
	result_type operator()(argument_type const & datum)
	{
		result_type r = *_pos;
		if (!datum.is_gap())
			*_pos += datum.length();
		else
			*_pos += (datum.gap_unknown()) ? _hole_size : datum.gap_size();
		return r;
	}
};

struct link_position
{
	typedef clones_t::value_type result_type;
	result_type operator()
	(
		SupercontigData::PositionnedRead const & pr,
		contig_positions_t * ctg_positions,
		SupercontigData::ContigContainer const * cc
	)
	{
		unsigned const pos = (*ctg_positions)[pr.ctg_number()];
		result_type::second_type r;
		SupercontigData::ContigContainer::value_type const * ctg = &(*cc)[pr.ctg_number()];
		if (ctg->cs_forward())
			r = std::make_pair(pos + pr.start_on_contig(), pos + pr.stop_on_contig());
		else
		{
			result_type::second_type::first_type a = pos + ctg->length() - 1;
			r = std::make_pair(a - pr.stop_on_contig(), a - pr.start_on_contig());
//			std::clog << "link_position: rev ctg " << ctg->name() << " " << pr.read_position()->read()->name();
//			std::clog << " " << pr.start_on_contig() << " " << pr.stop_on_contig() << std::endl;
//			std::clog << " " << r.first << " " << r.second << std::endl;
		}
		return result_type(&pr, r);
	}
};

static contig_positions_t::value_type
calculate_supercontig_infos
(
	SupercontigData const & data,
	contig_positions_t * contig_positions,
	contig_positions_t * conseq_positions,
	ObjectHandle<mekano::Parameters> const & param,
	clones_t * clones
)
{
	contig_positions->clear();
	conseq_positions->clear();
	clones->clear();

	contig_positions->reserve(data.contigs().size());
	conseq_positions->reserve(data.continuous_sequences().size());

	contig_positions_t::value_type length;
	transform(data.contigs().begin(), data.contigs().end(), std::back_inserter(*contig_positions), Positionner(&length, param));
	contig_positions_t::value_type length1;
	transform(data.continuous_sequences().begin(), data.continuous_sequences().end(), std::back_inserter(*conseq_positions), Positionner(&length1, param));
	assert(length == length1);

	clones->reserve(data.reads().size());
	transform
	(
		data.reads().begin(), data.reads().end(), std::back_inserter(*clones),
		boost::bind(link_position(), _1, contig_positions, &data.contigs())
	);

	return length;
}
	
void SupercontigInfo::do_validate() const
{
	calculate_supercontig_infos(_data, &_contig_positions, &_continuous_sequence_positions, _parameters, &_clones);
	calculate_clone_links(_data.sctg()->name(), _clones, &_biggest_clone_link_error, &_clone_links);
	_valid = true;
}

class ConseqOverlap
{
	contig_positions_t::value_type _position;
	SupercontigData::ConSeqData const * _conseq;
public:
	ConseqOverlap(contig_positions_t::value_type p, SupercontigData::ConSeqData const * c) :
		_position(p),
		_conseq(c)
	{}
	contig_positions_t::value_type position() const { return _position; }
	SupercontigData::ConSeqData const * conseq() const { return _conseq; }
};

struct SequenceOverlap
{
	std::string name;
	contig_positions_t::value_type position;
	int length;
	SequenceOverlap(std::string const & n, contig_positions_t::value_type p, int l) :
		name(n),
		position(p),
		length(l)
	{}
};

struct DrawLinear
{
	typedef int position_t;
private:
	ScaledText * const _texter;
	int const _x0;
	int const _y;
	static short const _hues[2];
	short const *_hue;
	bool const _reverse;
	GLuint const _forward_arrow;
	overlap_t * const overlaps;
	int const _hole_size;
	int x(int pos, int size) const
	{
		return _x0 + ((_reverse) ?  -(pos + size) : pos);
	}
	void sequence(contig_positions_t::value_type pos, int const isize, bool forward, std::string const & name, boost::any const & a)
	{
		int const x = this->x(pos, isize);
		_texter->scaled(x, _y, isize, main_radius * 2, name);
		forward ^= _reverse;
		texture_box(x, _y, isize, main_radius, Colour::hsv(*_hue, 0.6, 1), _forward_arrow, forward);
		overlaps->add(x, _y - main_radius, isize, main_radius * 2, a);
		_hue = &_hues[_hue == _hues];
	}
public:
	DrawLinear(ScaledText * const t, int x, int y, bool reverse, GLuint fa, overlap_t * o,
		ObjectHandle<mekano::Parameters> const & parameters
	) : _texter(t), _x0(x), _y(y), _hue(_hues),
		_reverse(reverse),
		_forward_arrow(fa),
		overlaps(o),
		_hole_size(parameters->hole_size())
		{}
	void contig(contig_positions_t::value_type pos, SupercontigData::ConSeqData const & conseq)
	{
		sequence(pos, conseq.length(), conseq.forward(), conseq.conseq()->name(), ConseqOverlap(pos, &conseq));
#if 0
		int const isize = conseq.length();
		bool forward = conseq.forward();
		std::string const & name = conseq.conseq()->name();
		int const x = this->x(pos, isize);
		_texter->scaled(x, _y, isize, main_radius * 2, name);
		forward ^= _reverse;
		texture_box(x, _y, isize, main_radius, Colour::hsv(*_hue, 0.6, 1), _forward_arrow, forward);
		overlaps->add(x, _y - main_radius, isize, main_radius * 2, ConseqOverlap(pos, &conseq));
		_hue = &_hues[_hue == _hues];
#endif
	}
	void sctg(contig_positions_t::value_type pos, int const isize, bool forward, std::string const & name)
	{
		sequence(pos, isize, forward, name, SequenceOverlap(name, pos, isize));
#if 0
		int const x = this->x(pos, isize);
		_texter->scaled(x, _y, isize, main_radius * 2, name);
		forward ^= _reverse;
		texture_box(x, _y, isize, main_radius, Colour::hsv(*_hue, 0.6, 1), _forward_arrow, forward);
		overlaps->add(x, _y - main_radius, isize, main_radius * 2, SequenceOverlap(name, pos, isize));
		_hue = &_hues[_hue == _hues];
#endif
	}
	void gap(contig_positions_t::value_type pos, int size)
	{
		int const x = this->x(pos, size);
		_texter->scaled(x, _y, size, main_radius * 2, to_string(size));
		cylinder(x, _y, size, main_radius, gap_colour);
	}
	unsigned gap(contig_positions_t::value_type pos)
	{
		cylinder(x(pos, _hole_size), _y, _hole_size, main_radius, Colour::i(0, 0, 0));
		return _hole_size;
	}
};
short const DrawLinear::_hues[2] = { 275, 333 };

static int
draw1scaf(
	ScaledText * texter,
	int x,
	int y,
	scaffolds_t::value_type scaf,
	int const direction,
	BankInfo * const bcm,
	ObjectHandle<mekano::Parameters> const & parameters,
	bool draw_continuous_sequences,
	bool draw_top_sctg,
	GLuint forward_arrow,
	overlap_t * const overlap,
	double const error_limit,
	bool const reverse = false
)
{
	{
		Perfs perfs("draw1scaf contis/sctgs: " + scaf->sctg()->name());
		glutil::enable do_texture(GL_TEXTURE_2D);
		glutil::begin poly(GL_QUADS); 
		DrawLinear drawer(texter, x, y, reverse, forward_arrow, overlap, parameters);
		if (draw_continuous_sequences)
			for_each(scaf->data().continuous_sequences().begin(), scaf->data().continuous_sequences().end(), draw_con_seq(&drawer));
		else
			for_each(scaf->data().contigs().begin(), scaf->data().contigs().end(), draw_con_seq(&drawer));

		y += main_radius * 3 * direction;
		DrawLinear drawer2(texter, x, y, reverse, forward_arrow, overlap, parameters);
		if (draw_top_sctg)
			for_each(scaf->data().top_sctgs().begin(), scaf->data().top_sctgs().end(), draw_sctg(&drawer2, parameters));
		else
			for_each(scaf->data().bottom_sctgs().begin(), scaf->data().bottom_sctgs().end(), draw_sctg(&drawer2, parameters));
		y += main_radius * 2 * direction;
	}
	y = draw_liens(x, y, bcm, direction, reverse, scaf.get(), overlap, error_limit);
	return y;
}

void common_clones
(
	scaffolds_t::value_type const & scaf1,
	scaffolds_t::value_type const & scaf2,
	BankInfo * const bcm,
	clones_t * both1,
	clones_t * both2
)
{
	clones_t filtered1;
	using namespace boost::lambda;
	copy_if
	(
		scaf1->clone_begin(),
		scaf1->clone_end(),
		back_inserter(filtered1),
		bind
		(
			&BankInfo::visible,
			bcm,
			bind(&SupercontigData::PositionnedRead::bank, bind(&clones_t::value_type::first, l1))
		)
	);
	double_set_intersection(filtered1.begin(), filtered1.end(), scaf2->clone_begin(), scaf2->clone_end(), std::back_inserter(*both1), std::back_inserter(*both2), LessClone<clones_t::value_type>());
}

template <class T>
struct CloneLengthCompare : public std::binary_function <T, T, bool>
{
	bool operator()(T const & a, T const & b) const
	{
		return a.first->read_position()->read()->clone()->bank()->length() < b.first->read_position()->read()->clone()->bank()->length();
	}
};

template <class Value>
struct DrawLinkBetweenScafs : public std::binary_function<Value, Value, void>
{
	void operator()(Value const & i, Value const & j)
	{
		line
		(
			_x1 + (i.second.first + i.second.second) / 2.0,
			_y1,
			_x2 + (j.second.first + j.second.second) / (_reverse_sctg2 ? -2.0 : 2.0),
			_y2,
			bank_colours->colour(i.first, j.first, _reverse_sctg2)
		);
	}
	DrawLinkBetweenScafs(double x1, double y1, double x2, double y2, BankInfo * bcm, bool reverse_sctg2) :
		_x1(x1), _y1(y1),
		_x2(x2), _y2(y2),
		bank_colours(bcm),
		_reverse_sctg2(reverse_sctg2) {}
private:
	double const _x1;
	double const _y1;
	double const _x2;
	double const _y2;
	BankInfo * const bank_colours;
	bool const _reverse_sctg2;
};

struct BoundingBox
{
	double xmin;
	double ymin;
	double xmax;
	double ymax;
	BoundingBox() :  xmin(0), ymin(0) {}
	void check() const {  assert(xmin <= xmax); assert(ymin <= ymax); }
	BoundingBox(double x1, double y1, double x2, double y2) :  xmin(x1), ymin(y1), xmax(x2), ymax(y2) {}
};

static BoundingBox
draw2scafs(
	ScaledText * texter,
	scaffolds_t::value_type scaf1,
	scaffolds_t::value_type scaf2,
	bool draw_continuous_sequences,
	bool draw_top_sctgs,
	bool reverse_sctg2,
	BankInfo * const bcm,
	ObjectHandle<mekano::Parameters> const & parameters,
	GLuint forward_arrow,
	overlap_t * overlap,
	double const error_limit,
	bool const draw_matches
)
{
	clones_t both1;
	clones_t both2;
	Perfs perfs("draw2scafs(" +  scaf1->sctg()->name() + ", " + scaf2->sctg()->name() + ")");
	Perfs for_set_intersection(perfs, "set_intersection");
	common_clones(scaf1, scaf2, bcm, &both1, &both2);
	for_set_intersection.done();
	
	int x1 = 0;
	int x2 = 0;
	if (both1.size())
	{
		clones_t::const_iterator smallest1 = std::min_element(both1.begin(), both1.end(), CloneLengthCompare<clones_t::value_type>());
		clones_t::const_iterator smallest2 = std::min_element(both2.begin(), both2.end(), CloneLengthCompare<clones_t::value_type>());
		x1 -= (smallest1->second.first + smallest1->second.second) / 2; // FIX rounding
		x2 -= (smallest2->second.first + smallest2->second.second) / (reverse_sctg2 ? -2 : 2);
	}
		
	BoundingBox bb;
	int const ygap = main_radius * 4;
	int const y = ygap + main_radius;
	Perfs for_draw1scaf1(perfs, "1st draw1scaf");
	glutil::push_name pushed(0);
	bb.ymax = draw1scaf(texter, x1, y, scaf1, 1, bcm, parameters, draw_continuous_sequences, draw_top_sctgs, forward_arrow, overlap, error_limit);
	for_draw1scaf1.done();
	
	pushed.change(1);
	Perfs for_draw1scaf2(perfs, "2nd draw1scaf");
	bb.ymin = draw1scaf(texter, x2, -y, scaf2, -1, bcm, parameters, draw_continuous_sequences, draw_top_sctgs, forward_arrow, overlap, error_limit, reverse_sctg2);
	for_draw1scaf2.done();

	if (draw_matches)
		matches(scaf1, x1, ygap, scaf2, x2, -ygap, reverse_sctg2);
	{
		glutil::begin begin(GL_LINES);
		for_each(both1.begin(), both1.end(), both2.begin(), DrawLinkBetweenScafs<clones_t::value_type>(x1, ygap, x2, -ygap, bcm, reverse_sctg2));
	}
	bb.xmin = std::min(x2, x1);
	bb.xmax = std::max(x2 + scaf2->length(), x1 + scaf1->length());
	return bb;
}

#if 1
// ugly hack: http://gcc.gnu.org/ml/gcc-help/2003-02/msg00145.html
namespace std {
template <typename V, typename W>
std::ostream & operator<<(std::ostream & o, std::pair<V, W> const & v)
{
	return o << '(' << v.first << ',' << v.second << ')';
}
}
#else
// to be tested
//http://www.codeguru.com/cpp/tic/tic0231.shtml
// A generic global operator<< 
// for printing any STL pair<>:
template<typename Pair> std::ostream&
operator<<(std::ostream& os, const Pair& p) {
  return os << p.first << "\t" 
    << p.second << std::endl;
}
#endif

class SctgWidget : public GLWidget
{
	typedef GLWidget superclass_t;
	void draw_all(); // SctgWidget::draw_all
	void projection(); // SctgWidget::projection
	static double const xscale_change;
	bool dragged(short, GLint, GLuint const *, int, int, int, int);
	int handle(int);
	virtual BoundingBox draw_sctgs(ScaledText *, ObjectHandle<mekano::Parameters> const &, double) = 0;
	double _xoffset;
 	double _yoffset;
	glutil::display_list graphics_list;
	double _xscale;
	static double const yscale;
	ScaledText _texts;
	double ymin;
	double ymax;
	void make_lists();
	fltk::FloatInput * _size_error_limit;
	fltk::Output * const _output;
	std::string _default_info;
	virtual void picked(GLint, GLuint const *, int x, int y);
	static void size_error_limit_cb(Widget *, void * ud)
	{
		static_cast<SctgWidget *>(ud)->recalculate();
	}
	static void banks_by_bank_cb(Widget *, void * ud)
	{
		static_cast<SctgWidget *>(ud)->banks_by_bank();
	}
	void banks_by_bank()
	{
		recalculate();
		_legend->reset_hidden();
	}
	fltk::Button * const _group_banks;
	boost::function<void(std::string const &)> const _update_title;
	ObjectHandle<mekano::Parameters> const & _parameters;
protected:
	Legend * const _legend;
	overlap_t _overlaps;
	fltk::Menu * const _metacontigs;
	fltk::Menu * const _top_sctgs;
	void click_text(std::string s = "")
	{
		if (!s.size())
			s = _default_info;
		_output->text(s.c_str());
		_output->position(0);
		_update_title(s);
	}
public:
//	void finish(void (*)(fltk::Widget*, void*), fltk::Window *, fltk::PackedGroup *, std::string const &);
	void recalculate()
	{
		clear();
		graphics_list.clear();
		redraw();
	}
	SctgWidget(int, int, BankColourMap *, fltk::PackedGroup *, boost::function<void(std::string const &)>, ObjectHandle<mekano::Parameters> const &,
		void (*)(fltk::Widget*, void*),
		fltk::Window *,
		std::string const &,
		double,
		fltk::Widget * = 0,
		fltk::Widget * = 0
	);
};

SctgWidget::SctgWidget(int w, int h, BankColourMap * bcm, fltk::PackedGroup * menu, boost::function<void(std::string const &)> update_title, ObjectHandle<mekano::Parameters> const & param,
	void (*f)(fltk::Widget*, void*),
	fltk::Window * window,
	std::string const & s,
	double clone_links_error,
	fltk::Widget * extra1,
	fltk::Widget * extra2
) :
	superclass_t(0, button_height, w, h, true),
	_xoffset(0),
	_yoffset(0),
	_xscale(0),
	_output(new fltk::Output(0, 0, 100, menu->h())),
	_group_banks(new fltk::CheckButton(0, 0, 10, menu->h())),
	_update_title(update_title),
	_parameters(param),
	_legend
	(
		new Legend
		(
			0, 0, 200, menu->h(), bcm, boost::lambda::bind(&SctgWidget::recalculate, this),
			boost::bind(std::logical_not<bool>(), boost::bind(&fltk::Button::value, _group_banks))
		)
	),
	_metacontigs(new fltk::Choice(0, 0, 60, menu->h())),
	_top_sctgs(new fltk::Choice(0, 0, 100, menu->h()))
{
	_metacontigs->add("ctgs");
	_metacontigs->add("mctgs");
	_top_sctgs->add("lowest sctgs");
	_top_sctgs->add("highest sctgs");
	
	menu->add(new Label(0, 0, menu->h(), "show"));
	menu->add(_metacontigs);
	menu->add(new Label(0, 0, menu->h(), "show"));
	menu->add(_top_sctgs);

	if (extra1)
		menu->add(extra1);
	if (extra2)
		menu->add(extra2);
	
	menu->type(menu->ALL_CHILDREN_VERTICAL);
	_group_banks->callback(banks_by_bank_cb, this);

	_metacontigs->callback(f, window);
	_top_sctgs->callback(f, window);

	if (clone_links_error <= 0)
		_size_error_limit = 0;
	else
	{
		menu->add(new Label(0, 0, menu->h(), "bank error limit:"));
		_size_error_limit = new fltk::FloatInput(0, 0, 50, menu->h());
		_size_error_limit->when(fltk::WHEN_ENTER_KEY);
		_size_error_limit->callback(size_error_limit_cb, this);
		_size_error_limit->value(clone_links_error);
		if (_size_error_limit->fvalue() < clone_links_error)
			_size_error_limit->value(clone_links_error * 2);
		menu->add(_size_error_limit);
	}
	
	menu->add(new Label(0, 0, menu->h(), "group banks:"));
	menu->add(_group_banks);
	_group_banks->value(true);
	
	menu->add(new Label(0, 0, menu->h(), "bank colours:"));
	menu->add(_legend);
	
	menu->add(_output);
	_default_info = s;
	click_text(_default_info);
	menu->resizable(_output);

}
double const SctgWidget::xscale_change = 1.1;
double const SctgWidget::yscale = 1;

namespace
{
	std::string
	read_info(clones_t::value_type const * clone)
	{
		SupercontigData::PositionnedRead const * pr = clone->first;
		mekano::ReadPosition * rp = pr->read_position();
		enum { start_in_db = SupercontigData::start_in_db, };
		return pr->read_position()->read()->name()
			+ " " + to_string(rp->start_on_contig())
			+ "-" + to_string(length2end(rp->start_on_contig(), rp->length())) + "/ctg"
			+ " " + to_string(clone->second.first + start_in_db)
			+ "-" + to_string(clone->second.second + start_in_db) + "/sctg"
			;
	}
}

void
SctgWidget::picked(GLint, GLuint const *, int x, int y)
{
	enum { start_in_db = SupercontigData::start_in_db, };
	if (unproject(&x, &y))
		if (boost::any const * a = _overlaps.overlap(x, y))
			if (SupercontigInfo::clone_link_t const * const o = boost::any_cast<SupercontigInfo::clone_link_t const>(a))
			{
				click_text(read_info(o->clone1()) + " & " + read_info(o->clone2())
					+ " (" + to_string(o->start() + start_in_db) + "-"
					+ to_string(length2end(o->start(), o->length()) + start_in_db) + ")"
				);
				return;
			}
			else if (SequenceOverlap const * o = boost::any_cast<SequenceOverlap>(a))
			{
				click_text
				(
					o->name + " " + to_string(o->position + start_in_db) +
					+ "-" + to_string(length2end(o->position + start_in_db, o->length)) +
					+ "=" + to_string(o->length)
				);
				return;
			}
			else if (ConseqOverlap const * o = boost::any_cast<ConseqOverlap>(a))
			{
				click_text
				(
					o->conseq()->name()
					+ " " + to_string(o->conseq()->start() + start_in_db)
					+ "-" + to_string(o->conseq()->stop() + start_in_db) + "/ctg"
					+ " " + to_string(o->position() + start_in_db)
					+ "-" + to_string(length2end(o->position(), o->conseq()->length()) + start_in_db) + "/sctg"
				);
				return;
			}

	click_text();
}

class OneSctgWidget : public SctgWidget
{
	typedef SctgWidget superclass_t;
	scaffolds_t::value_type _sctg;
	BoundingBox draw_sctgs(ScaledText *, ObjectHandle<mekano::Parameters> const & parameters, double);
public:
	OneSctgWidget
	(
		int w, int h, scaffolds_t::value_type sctg,
		BankColourMap * bcm,
		ObjectHandle<mekano::Parameters> const & parameters, 
		fltk::PackedGroup * menu,
		boost::function<void(std::string const &)> update_label,
		void (*f)(fltk::Widget*, void*),
		fltk::Window * window,
		std::string const & s,
		double clone_links_error,
		fltk::Widget * extra1,
		fltk::Widget * extra2
	) :
		SctgWidget(w, h, bcm, menu, update_label, parameters, f, window, s, clone_links_error, extra1, extra2),
		_sctg(sctg)
	{}
};

namespace
{
	std::string click_info(SupercontigData const & data, GLuint const * selected)
	{
		GLuint rp1 = selected[0];
		assert(rp1 < data.reads().size());
		GLuint rp2 = selected[1];
		assert(rp2 < data.reads().size());
		return data.reads()[rp1].read_position()->read()->name() + " @ " + to_string(selected[2])
			+ " & "
			+ data.reads()[rp2].read_position()->read()->name() + " @ " + to_string(selected[4])
			+ " = " + to_string(selected[3]);
	}
}

class TwoSctgWidget : public SctgWidget
{
	typedef SctgWidget superclass_t;
	scaffolds_t::value_type _sctg1;
	scaffolds_t::value_type _sctg2;
	boost::function<bool()> const _reverse;
	boost::function<bool()> const _matches;
	BoundingBox draw_sctgs(ScaledText *, ObjectHandle<mekano::Parameters> const & parameters, double);
public:
	TwoSctgWidget
	(
		int w, int h, 
		scaffolds_t::value_type sctg1,
		scaffolds_t::value_type sctg2,
		BankColourMap * bcm,
		ObjectHandle<mekano::Parameters> const & parameters, 
		boost::function<bool ()> reverse,
		boost::function<bool ()> matches,
		fltk::PackedGroup * menu,
		boost::function<void(std::string const &)> update_label,
		void (*f)(fltk::Widget*, void*),
		fltk::Window * window,
		std::string const & s,
		double clone_links_error,
		fltk::Widget * extra1,
		fltk::Widget * extra2
	) :
		SctgWidget(w, h, bcm, menu, update_label, parameters, f, window, s, clone_links_error, extra1, extra2),
		_sctg1(sctg1),
		_sctg2(sctg2),
		_reverse(reverse),
		_matches(matches)
	{
	}
};

bool
SctgWidget::dragged(short op, GLint hits, GLuint const * selected, int from_x, int from_y, int to_x, int to_y)
{
	if (op == op_info)
	{
		picked(hits, selected, from_x, from_y);
		return true;
	}
	else if (op == op_pan_vertical)
	{
		if (int const ydiff = from_y - to_y)
		{
			_yoffset -= yscale * ydiff;
			redraw();
		}
		return false;
	}
	else if (int const xdiff = from_x - to_x)
	{
		_xoffset += _xscale * xdiff;
		redraw();
	}
	return false;
}

int
SctgWidget::handle(int event)
{
	switch (event)
	{
	case fltk::MOUSEWHEEL:
		{
			double const oscale = _xscale;
			_xscale *= (fltk::event_dy() < 0) ? xscale_change : 1.0 / xscale_change;
			_xoffset += fltk::event_x() * (oscale - _xscale);
		}
		clear();
		redraw();
		return 1;

	default:
		return superclass_t::handle(event);
	}
}

BoundingBox
OneSctgWidget::draw_sctgs(ScaledText * texter, ObjectHandle<mekano::Parameters> const & parameters, double error_limit)
{
	return BoundingBox(0, 0, _sctg->length(), draw1scaf(texter, 0, main_radius, _sctg, 1, _legend, parameters, _metacontigs->value(), _top_sctgs->value(), arrow_texture(), &_overlaps, error_limit));
}

BoundingBox
TwoSctgWidget::draw_sctgs(ScaledText * texter, ObjectHandle<mekano::Parameters> const & parameters, double error_limit)
{
	return draw2scafs(texter, _sctg1, _sctg2, _metacontigs->value(), _top_sctgs->value(), _reverse(), _legend, parameters, arrow_texture(), &_overlaps, error_limit, _matches());
}

void
SctgWidget::make_lists()
{
	if (glutil::display_list::compiler compiling = graphics_list.compile())
	{
//		std::clog << "SctgWidget::make_lists() compiling into " << graphics_list << std::endl;
		_overlaps.clear();
		_texts.clear();
		double error_limit = _size_error_limit ? _size_error_limit->fvalue() : std::numeric_limits<double>::max();
		BoundingBox bb(draw_sctgs(&_texts, _parameters, error_limit));
		bb.check();
		if (_xscale == 0)
		{
			_xscale = (bb.xmax - bb.xmin) / w();
			_xoffset = bb.xmin;
		}
		ymin = bb.ymin;
		ymax = bb.ymax;
		_legend->sctg_drawn();
	}
}

void
SctgWidget::draw_all()
{
	graphics_list.call();
	_texts.draw(_xscale, yscale, _xoffset, 0, this);
}

void
SctgWidget::projection()
{
	double y = h() * yscale / (ymax - ymin);
	glOrtho(_xoffset, w() * _xscale + _xoffset, ymin * y + _yoffset, y * ymax + _yoffset, -10, 10);
}

namespace
{
	struct id_compare : std::binary_function<ObjectHandle<mekano::Stage>, ObjectHandle<mekano::Stage>, bool>
	{
		result_type operator()(first_argument_type const & a, second_argument_type const & b) const
		{
			return a->id() < b->id();
		}
	};
	void
	find_stages(mekano::Supercontig * sctg, std::vector<ObjectHandle<mekano::Stage> > * stages)
	{
		std::string q = "function f(s, i)\n"
			"{\n"
			" if (i >= s.groups[!])\n"
			"  return false;\n"
			" if (" + to_string(sctg->getOid()) + " in s.groups[i].sctgs)\n"
			"  return true;\n"
			" return f(s, i + 1);\n"
			"};\n"
			"select s from Stage s where f(s, 0)";
		eyedb::OidArray list;
		eyedb::OQL(sctg->getDatabase(), q.c_str()).execute(list);
		unsigned const n = list.getCount();
		stages->reserve(n);
		for (unsigned i = 0; i < n; i ++)
			stages->push_back(ObjectHandle<mekano::Stage>(sctg->getDatabase(), list[i]));
		std::sort(stages->begin(), stages->end(), id_compare());
	}
	void
	add_to_menu(fltk::PopupMenu * m, ObjectHandle<mekano::Stage> s)
	{
		m->add((stage_type(s) + to_string(s->id())).c_str());
	}
}

class SctgWindow : public fltk::Window
{
	std::string const _title_base;
protected:
	fltk::PackedGroup _menu;
	SctgWindow(
		std::string const window_name,
		eyedb::Database const * db,
		std::string const title
	) :
		fltk::Window(fltk::USEDEFAULT, fltk::USEDEFAULT, 1200, 600, ""),
		_title_base("mekano::" + window_name + "::" + db->getName() + ": " + title),
		_menu(0, 0, w(), button_height)
	{
		add(_menu);
		resizable(_menu);
		copy_label(_title_base.c_str());
		show();
		fltk::flush();
	}
public:
	void update_label(std::string const & l)
	{
		copy_label((_title_base + ": " + l).c_str());
	}
};

class OneSctgWindow : public SctgWindow
{
	scaffolds_t::value_type _sctg;
	BankColourMap * const _bcm;
	fltk::ValueInput * _min_links;
	fltk::PopupMenu * _related;
	ObjectHandle<mekano::Parameters> _parameters;
	OneSctgWidget * gl;

	std::vector<ObjectHandle<mekano::Stage> > stages;
	
	static void recalculate(Widget *, void * ud)
	{
		static_cast<OneSctgWindow *>(ud)->gl->recalculate();
	}
	static void open_related_sctgs(Widget *, void * ud)
	{
		static_cast<OneSctgWindow *>(ud)->open_related_sctgs();
	}
	void open_related_sctgs();
public:
	OneSctgWindow(scaffolds_t::value_type const & sctg, BankColourMap * bcm, ObjectHandle<mekano::Parameters> const & param) :
		SctgWindow("1", sctg->sctg().database(), sctg->sctg()->name()),
		_sctg(sctg),
		_bcm(bcm),
		_parameters(param)
	{
		_related = new fltk::PopupMenu(0, 0, 70, button_height);
		_related->callback(open_related_sctgs, this);
		std::vector<std::string> stage_names;
		find_stages(sctg->sctg(), &stages);
		for_each(stages.begin(), stages.end(), std::bind1st(std::ptr_fun(add_to_menu), _related));
		
		_min_links = new fltk::ValueInput(0, 0, 70, button_height);
		_min_links->range(1, 9999);
		_min_links->step(1);
		_min_links->value(10);
		gl = new OneSctgWidget(w(), h() - button_height, sctg, bcm, param, &_menu,
			boost::lambda::bind(&SctgWindow::update_label, this, boost::lambda::_1),
			recalculate,
			this,
			sctg->sctg()->name() + " " + to_string(sctg->data().length(_parameters->hole_size())),
			sctg->biggest_clone_length_error(),
			_related,
			_min_links
		);
		add(gl);
		resizable(gl);
	}
};

void OneSctgWindow::open_related_sctgs()
{
	ObjectHandle<mekano::Stage> const stage = stages[_related->value()];
	int const min = int(_min_links->value() + 0.4);
	std::ostringstream query;
	query << "l := list();\n";
	query << "sctg := " << _sctg->sctg()->getOid() << ";\n";
	query << "stage := " << stage.oid() << ";\n";
	query << "for (i := sctg.clone_links[!]; --i >= 0; )\n";
	query << "{\n";
	query << " cl := sctg.clone_links[i];\n";
	query << " if (cl.n >= " << min << ")\n";
	query << "  for (j := stage.groups[!]; --j >= 0; )\n";
	query << "   if (cl.sctg in stage.groups[j].sctgs)\n";
	query << "   {\n";
	query << "    l += cl.sctg;\n";
	query << "    break;\n";
	query << "   }\n";
	query << "}\n";
	query << "l";
	eyedb::OidArray sctg_oids;
	eyedb::Database * const db = _sctg->sctg()->getDatabase();
	std::string const q = query.str();
//	std::cout << "open_related_sctgs " << related.value() << " " << q << std::endl;
	eyedb::OQL(db, string2eyedb(q.c_str())).execute(sctg_oids);
//	std::clog << "open_related_sctgs " << sctg_oids.getCount() << std::endl;
	unsigned const n = sctg_oids.getCount();
	scaf_list_t sctgs;
	sctgs.push_back(_sctg->sctg());
	for (unsigned i = 0; i < n; i++)
		sctgs.push_back(ObjectHandle<mekano::Supercontig>(db, sctg_oids[i]));
	open_circle(sctgs, _bcm, _parameters,
		"single", "supercontigs in stage " + to_string(stage->id()) + " with " + to_string(min) + " links with " + _sctg->sctg()->name()
	);
}

class TwoSctgWindow : public SctgWindow
{
	fltk::CheckButton _reverse;
	fltk::CheckButton _matches;
	TwoSctgWidget gl;
	static void recalculate(Widget *, void * ud)
	{
		static_cast<TwoSctgWindow *>(ud)->gl.recalculate();
	}
public:
	TwoSctgWindow
	(
		scaffolds_t::value_type const & sctg1,
		scaffolds_t::value_type const & sctg2, BankColourMap * bcm,
		ObjectHandle<mekano::Parameters> const & param
	) :
		SctgWindow("2", sctg1->sctg().database(), sctg1->sctg()->name() + " & " + sctg2->sctg()->name()),
		_reverse(0, 0, 10, button_height, "reverse"),
		_matches(0, 0, 10, button_height, "matches"),
		gl
		(
			w(), h() - button_height, sctg1, sctg2, bcm, param,
			boost::bind(&fltk::Button::value, &_reverse),
			boost::bind(&fltk::Button::value, &_matches),
			&_menu,
			boost::lambda::bind(&SctgWindow::update_label, this, boost::lambda::_1),
			recalculate,
			this,
			sctg1->sctg()->name() + " " + to_string(sctg1->data().length(param->hole_size())) + " : " +
			sctg2->sctg()->name() + " " + to_string(sctg2->data().length(param->hole_size())),
			std::max(sctg1->biggest_clone_length_error(), sctg2->biggest_clone_length_error()),
			&_reverse,
			&_matches
		)
	{
		_reverse.w(label_width(&_reverse) + 25);
		_matches.w(label_width(&_matches) + 25);
		_reverse.callback(recalculate, this);
		_matches.callback(recalculate, this);
		_matches.value(true);
		add(gl),
		resizable(gl);
	}
};

void
open_sctg_window(scaffolds_t::value_type const & s, BankColourMap * bcm, ObjectHandle<mekano::Parameters> const & p)
{
	new OneSctgWindow(s, bcm, p);
}

void
open_sctg_window(scaffolds_t::value_type const & s1, scaffolds_t::value_type const & s2, BankColourMap * bcm, ObjectHandle<mekano::Parameters> const & p)
{
	new TwoSctgWindow(s1, s2, bcm, p);
}

void open_sctg_window(scaffolds_t::value_type const &, scaffolds_t::value_type const &, BankColourMap *, ObjectHandle<mekano::Parameters> const &);

void
Legend::picked(GLint, GLuint const *, int x, int y)
{
	if (unproject(&x, &y))
		if (int const * p = _overlaps.overlap(x, y))
		{
			std::set<int>::iterator q = _hidden.find(*p);
			if (q == _hidden.end())
				_hidden.insert(*p);
			else
				_hidden.erase(q);
			recalculate();
			_hidden_changed();
		}
}

void
Legend::label_bank(used_t::value_type const & v, int * const x, int *)
{
	enum { gap = 2, };
	bool const hidden = _hidden.find(v.first) != _hidden.end();
	bool const by_bank = _banks_by_bank();
	std::string text(by_bank ? v.second->name() : to_string(v.second->length()));
	int w = width(text) + gap * 2;
	{
		glutil::begin poly(GL_QUADS);
		_overlaps.add(*x, - (h() + 1) / 2, w, h(), v.first);
		if (!hidden)
			cylinder(double(*x), 0.0, w, h() / 3.0, _bcm->colour(v.first, by_bank, false));
		double const rs = h() / 12.0;
		double const rp = 5 * h() / 12.0;
		Colour const rc(_bcm->colour(v.first, by_bank, !hidden));
		cylinder(double(*x), -rp, w, rs, rc);
		cylinder(double(*x), +rp, w, rs, rc);
	}
	draw(*x + gap, 0, text, hidden ? Colour(1, 1, 1) : Colour(0, 0, 0));
	*x += w + space;
}

void
Legend::draw_all()
{
#if 1
	bool const by_bank = _banks_by_bank();
	if (!_used[by_bank].empty())
	{
		int x = space;
		int i = 0;
		_overlaps.clear();
		using namespace boost::lambda;
		for_each(_used[by_bank].begin(), _used[by_bank].end(), bind(&Legend::label_bank, this, l1, &x, &i));
		resize(x, h());
	}
#endif
}

static int
fail(std::ostream & o)
{
	o << std::endl;
	exit(1);
	return 1;
}

static int
fail(std::ostream & o, eyedb::Exception const & e)
{
	return fail(o << ": " << e.getString());
}

int
main(int argc, char * argv[])
{
	std::string version("$Revision: 804 $");
	if ((myname = strrchr(argv[0], '/')) == 0 || *++myname == 0)
		myname = argv[0];

	eyedb::init(argc, argv);
	eyedb::Database::OpenFlag mode = eyedb::Database::DBReadLocal;
	char const * db_name("oikopleura");

	char const options[] = "+ld:";
	while (int c = getopt(argc, argv, options))
		if (c == -1)
			break;
		else if (c == 'd')
			db_name = optarg;
		else if (c == 'r')
			mode = eyedb::Database::DBRead;
		else if (c == 'l')
			mode = eyedb::Database::DBReadLocal;
		else
			return fail(std::clog << myname << ": bad option: " << char(optopt) << ", options are: " << (options + 1));	
	argv += optind;
	argc -= optind;

	std::cout << myname << ": " << version << std::endl;

	mekano::mekano::init();
	eyedb::Exception::setMode(eyedb::Exception::ExceptionMode);
	eyedb::Connection conn;
	mekano::mekanoDatabase db(db_name);
	try
	{
		conn.open();
		db.open(&conn, mode);
	}
	catch (eyedb::Exception & e)
	{
		return fail(std::clog << myname << ": EyeDB exception opening " << db_name, e);
	}

	if (setgid(getgid()) < 0)
		return fail(std::clog << myname << ": setgid failed: " << strerror(errno));

	try
	{
		TransactionControl trans(&db);
		BankColourMap banks(&db);
		ObjectHandle<mekano::Parameters> parameters(&db, select_one(&db, parameters.class_name()));
		if (!parameters)
			throw MyError("no " + std::string(parameters.class_name()) + " in db");

		std::list<std::string> names;
		std::copy(&argv[0], &argv[argc], std::back_inserter(names));
		names.sort();
		names.unique();
//		std::copy(names.begin(), names.end(), std::ostream_iterator<std::string>(std::clog, " ")); std::clog << std::endl;

		std::set<eyedb::Oid> command_line;
		std::transform(names.begin(), names.end(), std::inserter(command_line, command_line.begin()),
			boost::bind(lookup(), &db, mekano::Supercontig::_classname(), _1)
		);
//		scaffolds_t scaffolds;
//		std::transform(scaf_list.begin(), scaf_list.end(), std::back_inserter(scaffolds), make_sctginfo());

		draw_selector(&db, &banks, parameters, command_line);
		fltk::run();
	}
	catch (eyedb::Exception const & e)
	{
		return fail(std::clog << myname << ": eyedb::Exception&", e);
	}
	catch (MyError & e)
	{
		return fail(std::clog << myname << ": error: " << e.toString());
	}
	return 0;
}
