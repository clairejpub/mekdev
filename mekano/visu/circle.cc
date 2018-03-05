/*	Stuart Pook, Genoscope 2007 (enscript -E $%) (Indent ON)
 *	$Id: circle.cc 778 2007-06-01 13:55:58Z stuart $
 */
#include "value_input.h"
#include "circle.h"
#include "visu.h"
#include <cmath>
#include <fltk/Choice.h>
#include <fltk/PackedGroup.h>
#include <boost/lambda/bind.hpp>
#include <utils/eyedb.h>
#include <utils/iterators.h>
#include <fltk/Menu.h>
#include <fltk/Output.h>
#include <boost/utility.hpp>
#include <fltk/ValueInput.h>
#include "utils.h"

namespace {

struct ScafInCircleInfo
{
	unsigned count;
	unsigned scaf1;
	unsigned scaf2;
	friend bool operator<(ScafInCircleInfo const & i1, ScafInCircleInfo const & i2) { return i1.count < i2.count; }
	explicit ScafInCircleInfo(unsigned n) : count(n), scaf1(0), scaf2(0) {}
	ScafInCircleInfo(unsigned s1, unsigned s2, unsigned n) :
		count(n),
		scaf1(s1),
		scaf2(s2)
	{
	}
};
typedef std::vector<ScafInCircleInfo> grouped_links_t;
	typedef std::vector<double> positions_t;

typedef std::vector<unsigned> counts_t;

struct CircleWidget : public GLWidget, private boost::noncopyable
{
	typedef ::positions_t positions_t;
	typedef ::counts_t counts_t;
private:
	grouped_links_t _grouped_clone_links;
	counts_t _clone_link_counts;
	grouped_links_t _grouped_matches;
	counts_t _match_counts;
	scaffolds_t _scaffolds;
	BankInfo _bcm;
	double _xmargin;
	double _ymargin;
	void projection();
	void draw_all();
	void make_lists();
	glutil::display_list _circles[2];
	double length;
	fltk::Output * _infos;
	MyValueInput * _min_clone_links;
	MyValueInput * _min_matches;
	fltk::Widget * _min_links_label;
	positions_t _positions; // this calculated twice, too bad
	ObjectHandle<mekano::Parameters> _parameters;
	fltk::Choice _what_to_show;
	void picked(GLint, GLuint const *, int, int);
	bool dragged(short, GLint, GLuint const *, int, int, int, int);
	static void recalculate_cb(Widget *, void * ud) { static_cast<CircleWidget *>(ud)->recalculate(); }
	void recalculate()
	{
		clear();
		redraw();
	}
	bool show_all_links() const { return _what_to_show.value() == 1; }
	bool show_matches() const { return _what_to_show.value() == 2; }
public:
	CircleWidget
	(
		int X, int Y, int W, int H,
		scaffolds_t const & sctgs,
		BankColourMap * bcm,
		ObjectHandle<mekano::Parameters> const & param,
		fltk::PackedGroup * menu
	); // CircleWidget::CircleWidget
};

template <typename C>
class Length
{
	double _length;
public:
	Length() : _length(0) {}
	void operator()(C const & p) { _length += p->length(); }
	double length() { return _length; }
};

template <class I, class M>
class _find_counts_in_sctg : public std::unary_function<mekano::LinkCount const *, void>
{
	M * const _to_do;
	I _inserter;
	typename M::const_iterator _other;
	typename M::const_iterator _end;
public:
	_find_counts_in_sctg(M * m, I i, typename M::const_iterator const & o) :
		_to_do(m),
		_inserter(i),
		_other(o),
		_end(m->end())
	{}
	result_type operator()(argument_type const & clc)
	{
		typename M::const_iterator w = _to_do->find(clc->sctg_oid());
		if (w != _end)
			*_inserter++ = ScafInCircleInfo(_other->second, w->second, clc->n());
	}
};
template <class I, class M>
class _find_counts_in_sctg<I, M>
find_counts_in_sctg(M * m, I i, typename M::const_iterator const & o)
{
	return _find_counts_in_sctg<I, M>(m, i, o);
}

typedef mekano::LinkCounts const * (mekano::Supercontig::*from_sctg_t)(eyedb::Bool *, eyedb::Status *) const;

template <typename I, typename M>
class CountCounts : public std::unary_function<scaffolds_t::value_type, void>
{
	M * const _to_do;
	I _inserter;
	from_sctg_t from_sctg;
public:
	CountCounts(M * to_do, I i, from_sctg_t f) : _to_do(to_do), _inserter(i), from_sctg(f)
	{}
	result_type operator()(argument_type const & sctg)
	{
		typename M::iterator current = _to_do->find(sctg->sctg().oid());
		assert(current != _to_do->end());
		mekano::LinkCounts const * cl = (sctg->sctg()->*from_sctg)(0, 0);
		for_each(begin_link_counts(cl), end_link_counts(cl), find_counts_in_sctg(_to_do, _inserter, current));
		_to_do->erase(current);
	}
};
template <typename I, typename M>
class CountCounts<I, M>
count_counts(M * m, I i, from_sctg_t from_sctg)
{
	return CountCounts<I, M>(m, i, from_sctg);
};

bool
CircleWidget::dragged(short op, GLint hits, GLuint const * selected, int, int, int, int)
{
	if (op == op_info && hits)
	{
		std::string s;
		if (selected[0] == 1)
		{
			mekano::Supercontig * sctg = _scaffolds[selected[3]]->sctg();
			s = sctg->name() + " " + to_string(sctg->length()) + "kb "
				+ stage_type(sctg->created_in()) + to_string(sctg->created_in()->id());
		}
		else if (selected[0] == 3)
		{
			mekano::Supercontig * sctg1 = _scaffolds[selected[3]]->sctg();
			mekano::Supercontig * sctg2 = _scaffolds[selected[4]]->sctg();
			s = sctg1->name() + " | " + sctg2->name() + " = " + to_string(selected[5]);
		}
		_infos->text(s.c_str());
		_infos->position(0);
	}
	return true;
}

void
CircleWidget::projection()
{
	double lw = w() - 2 * _xmargin;
	double lh = h() - 2 * _ymargin;
	double ratio = (lw > lh) ? lw / lh : lh / lw;

	double x = 1;
	double y = 1;
	if (lw < lh)
		y *= ratio;
	else
		x *= ratio;

	double scale = std::min(lw, lh) / 2;
	x += _xmargin / scale;
	y += _ymargin / scale;

	glOrtho(-x, x, -y, y, -10, 10);
}

class map_fill : public std::unary_function <scaffolds_t::value_type, std::pair<eyedb::Oid, unsigned> >
{
	unsigned i;
	scaffolds_t const * const sctgs;
public:
	map_fill(scaffolds_t const * const s) :
		i(0),
		sctgs(s)
	{}
	result_type operator()(argument_type s)
	{
		return std::make_pair(s->sctg().oid(), i++);
	}
};

double const torus_text = 1;
double const torus_inner = 0.05;
double const torus_outer = 1 - torus_inner - 0.01;
double const torus_big_radius = torus_outer;
double const torus_small_radius = torus_inner;
Colour const gap_colour = Colour::gray(100);

class DrawCircular
{
	double const _scaf_gap;
	double _length;
	enum { nhues = 3 };
	static short const _hues[nhues];
	short const * _hue;
	short const * _scaf_hue;
	double _offset;
	static double const _scaf_gap_fraction;
	unsigned _n;
	int const _hole_size;
	Colour colour_from_hue(double hue) const { return Colour::hsv(hue, 0.66, 0.8); }
public:
	typedef double position_t;
	DrawCircular(double length, unsigned count, ObjectHandle<mekano::Parameters> const & param) :
		_scaf_gap(_scaf_gap_fraction / count),
		_length(length / (1.0 - _scaf_gap_fraction)),
		_hue(_hues),
		_scaf_hue(_hues),
		_offset(0),
		_n(count),
		_hole_size(param->hole_size())
	{
		assert(_length > 0);
	}
	double next_scaffold(unsigned length)
	{
		double sctg_end = _offset += length / _length;
		_offset += _scaf_gap;
		_hue = _hues;
		return sctg_end;
	}
	void contig(contig_positions_t::value_type pos, SupercontigData::ConSeqData const & conseq)
	{
		int const isize = conseq.length();
		double size = isize / _length;
		double p = pos / _length + _offset;
		arc(torus_inner, torus_outer, p, size, colour_from_hue(*_hue));
		_hue = &_hues[_hue == _hues];
	}
	void scaffold(unsigned length)
	{
		if (--_n == 0 && _scaf_hue == _hues)
			_scaf_hue++;
		arc(torus_inner, torus_outer, _offset, length / _length, colour_from_hue(*_scaf_hue));
		if (++_scaf_hue == &_hues[nhues])
			_scaf_hue = _hues;
	}
	void gap(contig_positions_t::value_type pos, double size)
	{
		arc(torus_inner, torus_outer, pos / _length + _offset, size / _length,  gap_colour);
	}
	unsigned gap(contig_positions_t::value_type pos)
	{
		arc(torus_inner, torus_outer, pos / _length + _offset, _hole_size / _length, Colour::gray(50));
		return _hole_size;
	}
	double scale() const { return 1.0 / _length; }
	double offset() const { return _offset; }
	static double x() { return 0; }
	static double y() { return 0; }
};
short const DrawCircular::_hues[nhues] = { 240, 0, 120 };
double const DrawCircular::_scaf_gap_fraction = 0.10;

class DrawScafsCircle : public std::unary_function<scaffolds_t::value_type, double>
{
	bool const simple;
	BankInfo * const _bcm;
	unsigned _i;
	DrawCircular * const drawer;
	GLWidget * const _texter;
public:
	DrawScafsCircle(bool s, BankInfo * bcm, DrawCircular * d, GLWidget * t) :
		simple(s),
		_bcm(bcm),
		_i(0),
		drawer(d),
		_texter(t)
	{}
	result_type operator()(argument_type const & scaf)
	{
		glLoadName(_i++);
		std::string const n = scaf->super_contig()->name();
		result_type const offset1 = drawer->offset();
		if (simple)
			drawer->scaffold(scaf->length());
		else
			for_each(scaf->data().contigs().begin(), scaf->data().contigs().end(), draw_con_seq(drawer));
		result_type const offset2 = drawer->next_scaffold(scaf->length());
		if (offset1 > 1) std::clog << "DrawScafsCircle::operator() offset1 " << offset1 << std::endl;
		if (offset1 < 0) std::clog << "DrawScafsCircle::operator() offset1 " << offset1 << std::endl;
		if (offset2 > 1) std::clog << "DrawScafsCircle::operator() offset2 " << offset2 << std::endl;
		if (offset2 < 0) std::clog << "DrawScafsCircle::operator() offset2 " << offset2 << std::endl;
		assert(offset1 <= 1 && offset1 >= 0 && offset2 <= 1 && offset2 >= 0);

		double const theta = (offset1 + offset2) * M_PI;
		double const x = torus_text * cos(theta);
		double const y = torus_text * sin(theta);
		_texter->on_circle(x, y, n);

		return offset1;
	}
};

inline Colour
value_map(double value, double from, double to, double hue, double saturation)
{
	return Colour::hsv(hue, saturation, value * (to - from) + from);
}

inline Colour
value_map(double value, double in_from, double in_to, double out_from, double out_to, double hue, double saturation)
{
	if (in_to == in_from)
		return value_map(0.5, out_from, out_to, hue, saturation);
	return value_map((value - in_from) / (in_to - in_from), out_from, out_to, hue, saturation);
}

template <typename T>
class DrawGroupedLinks : public std::unary_function<T, void>
{
	unsigned const _min;
	unsigned const _max;
	BankInfo * const _bcm;
	std::vector<double> const & _positions;
	double _scale;
	scaffolds_t const & _sctgs;
	double scaled(unsigned p) const {  return _positions[p] + _sctgs[p]->length() / 2.0 * _scale; }
public:
	DrawGroupedLinks(unsigned min, unsigned max, BankInfo * bcm, std::vector<double> const & p,
		double scale,
		scaffolds_t const & sctgs
	) :
		_min(min), _max(max), _bcm(bcm), _positions(p), _scale(scale), _sctgs(sctgs)
	{}
	void operator()(T const & v) const
	{
		glLoadName(v.scaf1);
		glutil::push_name pushed(v.scaf2);
		glutil::push_name pushed2(v.count);
		spline2(scaled(v.scaf1), scaled(v.scaf2), torus_big_radius, torus_small_radius / 2, value_map(v.count, _min, _max, 0.4, 1, 190, 1));
	}
};

class DrawLinkCircle : public std::binary_function<clones_t::value_type, clones_t::value_type, void>
{
	BankInfo * const _bank_map;
	double const _scale;
	double const _position1;
	double const _position2;
public:
	DrawLinkCircle(BankInfo * bcm, double scale, double p1, double p2) :
		_bank_map(bcm),
		_scale(scale),
		_position1(p1),
		_position2(p2) {}
	result_type operator()(first_argument_type const & c1, second_argument_type const & c2)
	{
		double p1 = _scale * (c1.second.first + c1.second.second) / 2 + _position1;
		double p2 = _scale * (c2.second.first + c2.second.second) / 2 + _position2;
		Colour const colour = _bank_map->colour(c1.first, c2.first, false);
		bezier(p1, p2, torus_big_radius - torus_small_radius, torus_small_radius, colour);
	}
};

bool
set_visible(fltk::Widget * w, bool visible)
{
	if (w)
		if (visible)
		{
			w->show();
			return true;
		}
		else
			w->hide();
	return false;
}

void
CircleWidget::make_lists()
{
	bool const show_all = show_all_links();
	if (glutil::display_list::compiler compiling = _circles[show_all].compile())
	{
		_positions.empty();
		glutil::push_name pushed;
		glutil::lighting light;
		DrawCircular drawer(length, _scaffolds.size(), _parameters);
		std::transform(_scaffolds.begin(), _scaffolds.end(), std::back_inserter(_positions), DrawScafsCircle(!show_all, &_bcm, &drawer, this));
		_xmargin = widest();
		_ymargin = ascent() + descent();
		if (show_all)
			_bcm.recalculate();
	}
}

struct DrawLinksCircle
{
	typedef void result_type;
	void operator()
	(
		ScafInCircleInfo const & info,
		BankInfo * bcm,
		double const scale,
		scaffolds_t const & scaffolds,
		CircleWidget::positions_t const & positions
	) const
	{
		clones_t both1;
		clones_t both2;
		common_clones(scaffolds[info.scaf1], scaffolds[info.scaf2], bcm, &both1, &both2);
		for_each(both1.begin(), both1.end(), both2.begin(), DrawLinkCircle(bcm, scale, positions[info.scaf1], positions[info.scaf2]));
	}
};

void
draw_grouped(grouped_links_t const & grouped, BankInfo * bcm, positions_t const & positions, scaffolds_t const & scaffolds, MyValueInput * min_links, double scale)
{
	if (grouped.empty())
		return;
	glutil::lighting light;
	glutil::push_name pushed;
	grouped_links_t::const_iterator start = (min_links) ?
		lower_bound(grouped.begin(), grouped.end(), ScafInCircleInfo(unsigned(min_links->value() + 0.2)))
		:
		grouped.begin();
	std::for_each(start, grouped.end(), DrawGroupedLinks<grouped_links_t::value_type>(start->count, grouped.back().count, bcm, positions, scale, scaffolds));
}

void
CircleWidget::draw_all()
{
	bool const show_all = show_all_links();
	_circles[show_all].call();
	DrawCircular drawer(length, _scaffolds.size(), _parameters);
	if (show_all)
	{
		using namespace boost::lambda;
		for_each
		(
			_grouped_clone_links.begin(),
			_grouped_clone_links.end(),
			bind(DrawLinksCircle(), _1, &_bcm, drawer.scale(), _scaffolds, _positions)
		);
	}
	else if (show_matches())
		draw_grouped(_grouped_matches, &_bcm, _positions, _scaffolds, _min_matches, drawer.scale());
	else
		draw_grouped(_grouped_clone_links, &_bcm, _positions, _scaffolds, _min_clone_links, drawer.scale());
	::set_visible(&_bcm, show_all);
	bool v = ::set_visible(_min_clone_links, !show_all && !show_matches());
	v = ::set_visible(_min_matches, !show_all && show_matches()) || v;
	::set_visible(_min_links_label, v);
}

void
collect_count_info(scaffolds_t const & sctgs, grouped_links_t  * grouped_links, CircleWidget::counts_t * counts,
	from_sctg_t from_sctg)
{
	std::map<eyedb::Oid, unsigned> to_do;
	transform(sctgs.begin(), sctgs.end(), std::inserter(to_do, to_do.begin()), map_fill(&sctgs));
		
	for_each(sctgs.begin(), sctgs.end(), count_counts(&to_do, back_inserter(*grouped_links), from_sctg));
	sort(grouped_links->begin(), grouped_links->end());

	CircleWidget::counts_t all_counts;
	using namespace boost::lambda;
	transform(grouped_links->begin(), grouped_links->end(), back_inserter(all_counts), bind(&ScafInCircleInfo::count, _1));
	unique_copy(all_counts.begin(), all_counts.end(), back_inserter(*counts));
}

MyValueInput *
make_min_count(counts_t const & counts, boost::function<void ()> f, fltk::PackedGroup * menu)
{
	if (counts.size() <= 1)
		return 0;
	MyValueInput * r = new MyValueInput(0, 0, 40, menu->h(), counts, f);
	menu->add(r);
	return r;
}

CircleWidget::CircleWidget
(
	int X, int Y, int W, int H,
	scaffolds_t const & sctgs,
	BankColourMap * bcm,
	ObjectHandle<mekano::Parameters> const & param,
	fltk::PackedGroup * menu
) :
	GLWidget(X, Y, W, H, true),
	_scaffolds(sctgs),
	_bcm(100, 0, 10, menu->h(), bcm, boost::lambda::bind(&CircleWidget::recalculate, this), boost::lambda::bind(std::logical_not<bool>(), true)),
	length(std::for_each(sctgs.begin(), sctgs.end(), Length<scaffolds_t::value_type>()).length()),
	_parameters(param),
	_what_to_show(10, 0, 120, menu->h())
{
	menu->add(_what_to_show);
	_what_to_show.callback(recalculate_cb, this);
	_what_to_show.add("grouped clone links");
	_what_to_show.add("individual clone links");
	_what_to_show.add("grouped matches");

	collect_count_info(sctgs, &_grouped_clone_links, &_clone_link_counts, &mekano::Supercontig::clone_links);
	collect_count_info(sctgs, &_grouped_matches, &_match_counts, &mekano::Supercontig::matches);

	menu->add(_min_links_label = new Label(0, 0, menu->h(), "minimum links:"));

	boost::function<void ()> const recalc = boost::lambda::bind(&CircleWidget::recalculate, this);
	_min_clone_links = make_min_count(_clone_link_counts, recalc, menu);
	_min_matches = make_min_count(_match_counts, recalc, menu);

	menu->add(_bcm);

	_infos = new fltk::Output(200, 0, 400, menu->h()),
	_infos->text("use down gesture on an object for a description");
	_infos->position(0);
	menu->add(_infos);
	menu->resizable(_infos);
}

void
CircleWidget::picked(GLint hits, GLuint const * selected, int, int)
{
	if (hits > 0)
	{
		if (selected[0] == 1)
			open_sctg_window(_scaffolds[selected[3]], _bcm.bcm(), _parameters);

		else if (selected[0] > 1)
			open_sctg_window(_scaffolds[selected[3]], _scaffolds[selected[4]], _bcm.bcm(), _parameters);
	}
}

class CircleWindow : public fltk::Window
{
	fltk::PackedGroup _menu;
	CircleWidget gl;
public:
	CircleWindow
	(
		scaffolds_t const & scaffolds,
		BankColourMap * bcm,
		ObjectHandle<mekano::Parameters> const & param,
		std::string const & title,
		std::string const & text
	) :
		fltk::Window(500, 360, ""),
		_menu(0, 0, w(), button_height),
		gl(0, button_height, w(), h() - button_height, scaffolds, bcm, param, &_menu)
	{
		eyedb::Database const * db = scaffolds.front()->data().sctg().database();
		copy_label(("mekano::" + title + "::" + db->getName() + " " + text).c_str());
		fltk::PackedGroup * const menu = &_menu;
		add(menu);
		add(&gl);

		menu->type(menu->ALL_CHILDREN_VERTICAL);

		resizable(gl);
		show();
	}
	~CircleWindow()
	{
		std::clog << "~CircleWindow" << std::endl;
	}
};

void
open_circle
(
	scaffolds_t const & scaffolds,
	BankColourMap * bcm,
	ObjectHandle<mekano::Parameters> const & parameters,
	std::string const & title,
	std::string const & text
)
{
	new CircleWindow(scaffolds, bcm, parameters, title, text);
}

struct make_sctginfo
{
	typedef scaffolds_t::value_type result_type;
	result_type operator()(ObjectHandle<mekano::Supercontig> const & v, ObjectHandle<mekano::Parameters> const & param) const
	{
		return result_type(new SupercontigInfo(v, param));
	}
};

} // namespace

void
open_circle
(
	scaf_list_t const & slist,
	BankColourMap * bcm,
	ObjectHandle<mekano::Parameters> const & parameters,
	std::string const & title,
	std::string const & text
)
{
	scaffolds_t scaffolds;
	using namespace boost::lambda;
	std::transform(slist.begin(), slist.end(), std::back_inserter(scaffolds), bind(make_sctginfo(), _1, parameters));
	open_circle(scaffolds, bcm, parameters, title, text);
}
