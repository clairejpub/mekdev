/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: selector.cc 781 2007-06-01 15:18:30Z stuart $
 */
#include "value_input.h"
#include <utils/templates.h>
#include <utils/iterators.h>
#include "graphics.h"
#include "mekano.h"
#include "utils.h"
#include "circle.h"
#include <cmath>
#include <iostream>
#include <fstream>
#include <errno.h>
#include <set>
#include <iomanip>
#include <fltk/Choice.h>
#include <fltk/PackedGroup.h>
#include <fltk/TiledGroup.h>
#include <fltk/Output.h>
#include <fltk/Menu.h>
#include <fltk/Item.h>
#include <fltk/ValueInput.h>
#include <fltk/ValueOutput.h>
#include <fltk/Button.h>
#include <fltk/draw.h>
#include <fltk/run.h>
#include <fltk/CheckButton.h>
#include <fltk/Cursor.h>

#include <boost/bind.hpp>
#include <boost/function.hpp>

#include "graph.h"
#include <utils/utilities.h>

namespace
{

typedef std::insert_iterator<std::set<eyedb::Oid> > (*operator_t)
(
	std::set<eyedb::Oid>::const_iterator,
	std::set<eyedb::Oid>::const_iterator,
	std::vector<eyedb::Oid>::const_iterator,
	std::vector<eyedb::Oid>::const_iterator,
	std::insert_iterator<std::set<eyedb::Oid> >
);
	class SymbolFont
	{
		fltk::Font * const f;
	public:
		SymbolFont() : f(fltk::font("Standard Symbols L")) {}
		void operator()(fltk::Button * b) const
		{
			if (f)
				b->labelfont(f);
		}
		static char const * Union() { return "\u222A"; } // union
		static char const * intersection() { return "\u2229"; }
		static char const * difference() { return "\u2212"; }
	};
	
	void
	read_array_of_collections
	(
		eyedb::Database * const db,
		eyedb::Oid const & stage,
		int const group,
		std::string const & array_name,
		index_t * const table
	)
	{
		std::ostringstream q;
		q <<
			"r := \"\"; sp := \" \";\n"
  			"for (x in (contents(" + to_string(stage) + ".groups[" << group << "]." + array_name + ")))\n"
			"{\n"
			" v := x.value;\n"
			" count := (contents(v.s))[!];\n"
			" if (count > 0)\n"
			"  r += sp + string(x.index) + sp + string(count);\n"
			"}\n"
			"r";
		std::string query(q.str());
		eyedb::Value results;
		eyedb::OQL(db, string2eyedb(query)).execute(results);
		if (results.getType() == eyedb::Value::tString)
		{
			std::istringstream input(results.str);
			index_t::value_type::first_type index;
			index_t::value_type::second_type count;
			while (input >> index >> count)
				table->push_back(std::make_pair(index, count));
		}
		else
			throw MyError("read_array_of_collections: bad result type from " + query + ": " + to_string(results.getType()));
	}

template <typename V>
void inline
set_input(fltk::ValueInput * txt, V v)
{
	txt->value(v);
}

enum { button_width = 36, };

class Selector;
class GroupSelector;
struct Plot : public fltk::Group
{
	typedef boost::function<void (operator_t, index_t::const_iterator, index_t::const_iterator, unsigned)> callback_t;
private:
	enum { n_buttons = 3, n_fields = 3, label_space = 60, inset = 2, };
	fltk::ValueInput lower;
	fltk::ValueInput upper;
	fltk::ValueOutput count;
	fltk::Button union_button;
	fltk::Button intersection_button;
	fltk::Button difference_button;
	Graph _graph;
	std::string const _group_name;
	callback_t const _operator_cb;
	index_t table;
	std::vector<index_t> _tables;
	std::vector<int> _read;

	index_t::const_iterator begin;
	index_t::const_iterator stop;
	index_t::const_iterator _selection_start;
	index_t::const_iterator _selection_stop;
	unsigned _selected;

	int labelheight() { int x, y; measure_label(x, y); return y; }
	int labelwidth() { int x, y; measure_label(x, y); return x; }
	void set_start() { lower.value(_selection_start->first); _graph.start(_selection_start->first); }
	void set_stop() { upper.value(_selection_stop->first); _graph.stop(_selection_stop->first); }
	void set_count() { count.value(_selected); }
	void lower_cb();
	void upper_cb();
	void callback(operator_t f)
	{
		index_t::const_iterator i = _selection_stop;
		_operator_cb(f, _selection_start,  ++i, _selected);
	}
	static void lower_cb(fltk::Widget *, void * ud) { static_cast<Plot *>(ud)->lower_cb(); }
	static void upper_cb(fltk::Widget *, void * ud) { static_cast<Plot *>(ud)->upper_cb(); }
	static void union_cb(fltk::Widget *, void * ud)
	{
		static_cast<Plot *>(ud)->callback(std::set_union);
	}
	static void intersection_cb(fltk::Widget *, void * ud)
	{
		static_cast<Plot *>(ud)->callback(std::set_intersection);
	}
	static void difference_cb(fltk::Widget *, void * ud)
	{
		static_cast<Plot *>(ud)->callback(std::set_difference);
	}
	void set_all();
	void switch_off();
	void switch_on();
public:
	Plot(int, int, int, char const *, callback_t, SymbolFont const &, std::string const &);
	void refill(ObjectHandle<mekano::Stage> const &, GroupSelector const *);
	void fill(ObjectHandle<mekano::Stage> const &, GroupSelector const *);
	void set_lower(index_t::value_type::first_type);
	void adjust_start(bool);
	void adjust_end(bool);
	index_t::const_iterator selection_start() const { return _selection_start; }
	index_t::const_iterator selection_stop() const { return _selection_stop; }
	void selection_start(index_t::const_iterator);
	void selection_stop(index_t::const_iterator);
	void check() const {}
};

class NameAdder : public fltk::Group
{
	typedef boost::function<void (eyedb::Oid const &, operator_t)> callback_t;
	eyedb::Database * const _db;
	callback_t _callback;
	fltk::Input sctg_name;
	fltk::Button difference_button;
	fltk::Button intersection_button;
	fltk::Button union_button;
	fltk::Output message;
	eyedb::Oid _sctg;
	enum { height = 25, my_inset = 2 };
	void name_callback()
	{
		ObjectHandle<mekano::Supercontig> sctg(_db, sctg_name.value());
		activate(sctg);
		if (sctg)
		{
			std::string m = sctg->name() + " " + stage_type(sctg->created_in()) + to_string(sctg->created_in()->id()) + " " + to_string(sctg->length()) + " bases";
			message.value(m.c_str());
			_sctg = sctg.oid();
		}
		else
			message.value((sctg_name.value() + std::string(" not found")).c_str());

	}
	static void name_callback(Widget *, void * ud) { static_cast<NameAdder *>(ud)->name_callback(); }
	void activate(bool b)
	{
		difference_button.activate(b);
		intersection_button.activate(b);
		union_button.activate(b);
	}
	static void union_cb(Widget *, void * ud) { static_cast<NameAdder *>(ud)->operator_cb(std::set_union); }
	static void intersection_cb(Widget *, void * ud) { static_cast<NameAdder *>(ud)->operator_cb(std::set_intersection); }
	static void difference_cb(Widget *, void * ud) { static_cast<NameAdder *>(ud)->operator_cb(std::set_difference); }
	void operator_cb(operator_t f) { _callback(_sctg, f); }
public:
	NameAdder(int y, int width, SymbolFont const & symbols, eyedb::Database * const db, callback_t f) :
		fltk::Group(0, y, width, button_height + 2 * my_inset, "by name", true),
		_db(db),
		_callback(f),
		sctg_name(label_width(this) + 15, my_inset, 120, button_height),
		difference_button(r() - 2 * my_inset - button_width, sctg_name.y(), button_width, sctg_name.h(), symbols.difference()),
		intersection_button(difference_button.x() - button_width, sctg_name.y(), button_width, sctg_name.h(), symbols.intersection()),
		union_button(intersection_button.x() - button_width, sctg_name.y(), button_width, sctg_name.h(), symbols.Union()),
		message(sctg_name.r(), sctg_name.y(), union_button.x() - sctg_name.r(), sctg_name.h())
	{
		end();
		box(fltk::BORDER_FRAME);
		flags(fltk::ALIGN_INSIDE_LEFT);
		symbols(&union_button);
		symbols(&intersection_button);
		symbols(&difference_button);
		sctg_name.callback(name_callback, this);
		sctg_name.when(fltk::WHEN_ENTER_KEY);
		union_button.callback(union_cb, this);
		intersection_button.callback(intersection_cb, this);
		difference_button.callback(difference_cb, this);
		sctg_name.value("sctg");
		activate(false);
	}
};

inline int
label_height(fltk::Widget const * widget)
{
	int w;
	int h;
	widget->measure_label(w, h);
	return h;
}

inline void
labelwidth2width(fltk::Widget * widget)
{
	widget->w(label_width(widget) + 10);
}

void
adjust4label(fltk::Widget * widget)
{
	int w = label_width(widget) + 10;
	widget->resize(widget->x() + w, widget->y(), widget->w() - w, widget->h());
}

typedef std::vector<unsigned> filter_index_t;
class Filter : public fltk::Group
{
	typedef boost::function<void (filter_index_t::const_iterator, filter_index_t::const_iterator)> callback_t;
	filter_index_t const _link_indexes;
	Label _title;
	Label _min_label;
	MyValueInput min_value;
	Label _max_label;
	MyValueInput max_value;
	fltk::Button go_button;
	callback_t _callback;
	void go()
	{
		_callback(min_value.current(), max_value.current() + 1);
	}
	static void go_callback(Widget *, void * ud) { static_cast<Filter *>(ud)->go(); }
	void min_changed()
	{
		if (*min_value.current() > *max_value.current())
			max_value.current(min_value.current());
	}
	void max_changed()
	{
		if (*min_value.current() > *max_value.current())
			min_value.current(max_value.current());
	}
public:
	Filter(int, eyedb::CollArray const *, std::string const &, callback_t); // Filter::Filter
};

class GroupSelector : public fltk::PackedGroup
{
	Label _label;
	typedef boost::function<void ()> modified_cb_t;
	modified_cb_t const _modified_cb;
	typedef std::vector<std::pair<eyedb::Oid, fltk::CheckButton *> > buttons_t;
	buttons_t _buttons;
	fltk::CheckButton * find(eyedb::Oid const & o) const
	{
		buttons_t::const_iterator i = std::find_if
		(
			_buttons.begin(),
			_buttons.end(),
			boost::bind(std::equal_to<eyedb::Oid>(), boost::bind(&buttons_t::value_type::first, _1), o)
		);
		return (i == _buttons.end()) ? 0 : i->second;
	}
	void make_checkbutton(ObjectHandle<mekano::GroupName> const &);
	static void modified_cb(Widget *, void * gs)
	{
		static_cast<GroupSelector *>(gs)->_modified_cb();
	}
public:
	GroupSelector(int x, int y, int w, eyedb::Database *, modified_cb_t);
	bool selected(eyedb::Oid const & o) const
	{
		if (fltk::CheckButton * b = find(o))
			return b->value() != 0;
		return false;
	}
};
struct Selector : public fltk::Window
{
	typedef mekano::SupercontigSet const * (mekano::Group::*get_sctg_set_t)(unsigned, eyedb::Bool *, eyedb::Status *) const;
	typedef eyedb::CollBag const * (mekano::Calculated::*from_calculated_t)(unsigned, eyedb::Bool *, eyedb::Status *) const;
	typedef mekano::LinkCounts const * (mekano::Supercontig::*from_sctg_t)(eyedb::Bool *, eyedb::Status *) const;
private:
	int _current_stage;
	NameAdder by_name;
	fltk::Choice stage_choice;
	GroupSelector group_selector;
	fltk::ValueOutput * count;
	std::vector<Plot *> _plots;
	BankColourMap * const _bcm;
	ObjectHandle<mekano::Parameters> _parameters;
	ObjectHandle<mekano::Calculated> _calculated;
	std::set<eyedb::Oid> oids;
	std::vector<ObjectHandle<mekano::Stage> > stage_list;
	int fill_menu(eyedb::Database * db, fltk::Choice * stages);
	static void item_callback(Widget * w, void * ud)
	{
		int const pos = w->parent()->find(w);
		if (pos != w->parent()->children())
			static_cast<Selector *>(ud)->fill_plots(pos);
	}
	static void clear_cb(Widget *, void * ud) { static_cast<Selector *>(ud)->clear_cb(); }
	static void show_cb(Widget *, void * ud) { static_cast<Selector *>(ud)->show_cb(); }
	void clear_cb() { oids.clear(); update_count(); }
	void show_cb();
	void fill_plots(int);
	void update_count() { count->value(oids.size()); }
	void operator_cb(get_sctg_set_t, operator_t, index_t::const_iterator, index_t::const_iterator, unsigned);
	ObjectHandle<mekano::Stage> stage() const { return stage_list[_current_stage]; }
	void filter(from_calculated_t, from_sctg_t, filter_index_t::const_iterator, filter_index_t::const_iterator);
	void group_selected();
	void oid_cb(eyedb::Oid const &, operator_t);
	void apply(operator_t, std::vector<eyedb::Oid>::const_iterator, std::vector<eyedb::Oid>::const_iterator);
public:
	Selector(eyedb::Database * db, BankColourMap * bcm, ObjectHandle<mekano::Parameters> const & param, SymbolFont const &, std::set<eyedb::Oid> const &);
};

filter_index_t
make_index_vector_from_array(eyedb::CollArray const * array)
{
	filter_index_t index;
	eyedb::ValueArray elements;
	array->getElements(elements, eyedb::True);
	index.reserve(elements.getCount() / 2);
	transform_if
	(
		elements.getValues(),
		elements.getValues() + elements.getCount(),
		back_inserter(index),
		boost::bind(&eyedb::Value::i, _1),
		boost::bind(std::equal_to<eyedb::Value::Type>(), boost::bind(&eyedb::Value::type, _1), eyedb::Value::tInt)
	);
	return index;
}

Filter::Filter(int w, eyedb::CollArray const * collection, std::string const & what, callback_t cb) :
	fltk::Group(0, 0, w, 25, "", true),
	_link_indexes(make_index_vector_from_array(collection)),
	_title(0, 0, h(), "filter by number of " + what),
	_min_label(_title.r(), 0, h(), "minimum", label_width(this)),
	min_value(_min_label.r(), 0, 50, h(), _link_indexes, boost::lambda::bind(&Filter::min_changed, this)),
	_max_label(min_value.r(), 0, h(), "maximum"),
	max_value(_max_label.r(), 0, min_value.w(), h(), _link_indexes, true, boost::lambda::bind(&Filter::max_changed, this)),
	go_button(max_value.r() + 5, 0, 5, h(), "filter"),
	_callback(cb)
{
	end();
	flags(fltk::ALIGN_INSIDE_LEFT);

	labelwidth2width(&go_button);
	resizable(_min_label);
	
	if (_link_indexes.empty())
		go_button.deactivate();
	else
		go_button.callback(go_callback, this);
}

void
Selector::oid_cb(eyedb::Oid const & sctg, operator_t f)
{
	std::vector<eyedb::Oid> selected(1, sctg);
	apply(f, selected.begin(), selected.end());
}

void
Selector::group_selected()
{
	ShowWaitCursor w(this, "Selector::group_selected");
	for_all(_plots, boost::bind(&Plot::refill, _1, stage(), &group_selector));
}

void
GroupSelector::make_checkbutton(ObjectHandle<mekano::GroupName> const & gn)
{
	fltk::CheckButton * b = new fltk::CheckButton(0, 0, 40, h(), "");
	b->copy_label(gn->name().c_str());
	b->w(label_width(b) + 20);
	add(b);
	b->callback(GroupSelector::modified_cb, this);
	_buttons.push_back(std::make_pair(gn.oid(), b));
}

GroupSelector::GroupSelector(int x, int y, int w, eyedb::Database * db, modified_cb_t f) :
	fltk::PackedGroup(x, y, w, 30),
	_label(0, 0, h(), "Groups"),
	_modified_cb(f)
{
	add(&_label);
	this->x(x + label_width(this) + 10);
	this->w(w - (this->x() - x));
	type(PackedGroup::ALL_CHILDREN_VERTICAL);
//	flags(fltk::ALIGN_LEFT);
	typedef ObjectHandle<mekano::GroupName> db_class_t;
	std::vector<db_class_t> group_names;
	query(db, std::back_inserter(group_names), "");

	for_each(group_names.begin(), group_names.end(), boost::bind(&GroupSelector::make_checkbutton, this, _1));
}


Plot::Plot(int y, int iw, int height, char const * n,
	callback_t cb,
 	SymbolFont const & symbols, std::string const & group_name
) :
	fltk::Group
	(
		0,
		y,
		iw,
		height,
		n,
		true
	),
	lower(inset, label_height(this) + inset + 3, (w() - n_buttons * button_width - 2 * inset) / n_fields, button_height, "lower"),
	upper(lower.r(), lower.y(), lower.w(), lower.h(), "upper"),
	count(upper.r(), lower.y(), lower.w(), lower.h(), "count"),
	union_button(count.r(), lower.y(), button_width, lower.h(), symbols.Union()),
	intersection_button(union_button.r(), lower.y(), button_width, lower.h(), symbols.intersection()),
	difference_button(intersection_button.r(), lower.y(), button_width, lower.h(), symbols.difference()),
	_graph(inset, lower.y() + lower.h(), w() - 2 * inset, h() - lower.h() - 2 * inset - lower.y(), &table),
	_group_name(group_name),
	_operator_cb(cb)
{
	end();
	adjust4label(&lower);
	adjust4label(&count);
	adjust4label(&upper);
	resizable(&_graph);
	symbols(&union_button);
	symbols(&intersection_button);
	symbols(&difference_button);
	box(fltk::BORDER_FRAME);
	flags(fltk::ALIGN_INSIDE_TOPLEFT);
	_graph.text("to come");
	lower.step(1);
	upper.step(1);
	lower.when(fltk::WHEN_RELEASE | fltk::WHEN_ENTER_KEY);
	upper.when(fltk::WHEN_RELEASE | fltk::WHEN_ENTER_KEY);
	lower.callback(lower_cb, this);
	upper.callback(upper_cb, this);
	union_button.callback(union_cb, this);
	intersection_button.callback(intersection_cb, this);
	difference_button.callback(difference_cb, this);
}

void
Plot::set_all()
{
	set_count();
	set_start();
	set_stop();
	check();
}

void
Plot::lower_cb()
{
	index_t::value_type::first_type const v(index_t::value_type::first_type(lower.value() + 0.5));
	if (v < _selection_start->first)
		while (_selection_start != begin && v < _selection_start->first)
			adjust_start(false);
	else
		while (_selection_start != stop && v > _selection_start->first)
			adjust_start(true);
	set_all();
}

template <class Insertor>
struct AddSctgs : public std::unary_function<index_t::value_type, void>
{
	typedef Selector::get_sctg_set_t getter_t;
private:
	ObjectHandle<mekano::Stage> stage;
	unsigned const group;
	getter_t get_sctg_set;
	Insertor insert;
public:
	AddSctgs(ObjectHandle<mekano::Stage> const & s, unsigned gr, getter_t g, Insertor const & i) :
		stage(s),
		group(gr),
		get_sctg_set(g),
		insert(i)
	{}
	result_type operator()(argument_type const & index)
	{
		if (mekano::SupercontigSet const * ss = (stage->groups(group)->*get_sctg_set)(index.first, 0, 0))
		{
			eyedb::Value value;
			for (eyedb::CollectionIterator iter(ss->s()); iter.next(value); *insert++ = *value.oid )
				assert(value.getType() == eyedb::Value::tOid);
		}
	}
};
template <class Insertor>
AddSctgs<Insertor>
add_sctgs(ObjectHandle<mekano::Stage> const & stage, unsigned group, Selector::get_sctg_set_t g, Insertor const & i)
{
	return AddSctgs<Insertor>(stage, group, g, i);
}

void
Plot::upper_cb()
{
	index_t::value_type::first_type const v(index_t::value_type::first_type(upper.value() + 0.5));
	if (v > _selection_stop->first)
		while (_selection_stop != stop && v > _selection_stop->first)
			adjust_end(true);
	else
		while (_selection_stop != begin && v < _selection_stop->first)
			adjust_end(false);
	set_all();
}

void
Plot::switch_on()
{
	lower.activate();
	upper.activate();
	union_button.activate();
	intersection_button.activate();
	difference_button.activate();
}

void
Plot::switch_off()
{
	lower.deactivate();
	upper.deactivate();
	union_button.deactivate();
	intersection_button.deactivate();
	difference_button.deactivate();
	lower.value(0);
	upper.value(0);
	count.value(0);
}

	void
	combine_into(index_t::const_iterator from, index_t::const_iterator const & from_end, index_t * const into)
	{
		index_t::iterator to = into->begin();
		index_t::iterator const into_end = into->end();
		for (; from != from_end; ++from)
		{
			std::pair <index_t::iterator, index_t::iterator> p = equal_range
			(
				to,
				into_end,
				*from,
				boost::bind
				(
					std::less<index_t::value_type::first_type>(),
					boost::bind(&index_t::value_type::first, _1),
					boost::bind(&index_t::value_type::first, _2)
				)
			);
			if (p.first == p.second)
				into->insert(p.first, *from);
			else
			{
				p.first->second += from->second;
				assert(p.first->first == from->first && ++p.first == p.second);
			}
			to = p.second;
		}
	}

void
Plot::refill(ObjectHandle<mekano::Stage> const & stage, GroupSelector const * group_selector)
{
	table.clear();

	for (unsigned i = stage->groups_cnt(); i-- > 0; )
		if (group_selector->selected(stage->groups(i)->name_oid()))
		{
			if (!_read[i]++)
				read_array_of_collections(stage.database(), stage.oid(), i, _group_name, &_tables[i]);
			combine_into(_tables[i].begin(), _tables[i].end(), &table);
		}

	if (table.empty())
		switch_off();
	else
	{
		switch_on();
		begin = table.begin();
		stop = table.end();
		--stop;
		_selection_start = begin;
		_selection_stop = stop;

		_selected = for_each(table.begin(), table.end(), Sum<index_t::value_type>())().second;
		lower.range(_selection_start->first, _selection_stop->first);
		upper.range(_selection_start->first, _selection_stop->first);
		set_all();
	}
	_graph.refill();
}

void
Plot::fill(ObjectHandle<mekano::Stage> const & stage, GroupSelector const * group_selector)
{
	_tables.assign(stage->groups_cnt(), index_t());
	_read.assign(_tables.size(), 0);

	refill(stage, group_selector);
}

void
Plot::set_lower(index_t::value_type::first_type v)
{
	while (_selection_start != stop && v > _selection_start->first)
		adjust_start(true);
	index_t::const_iterator i;
	while (_selection_start != begin && v <= (i = _selection_start, --i)->first)
		adjust_start(false);
	std::clog << "PlotInfo::set_lower " << v << " -> " << _selection_start->first << std::endl;
	assert(_selection_start == stop || v <= _selection_start->first);
	assert(_selection_start == begin || v > (i = _selection_start, --i)->first);
	set_start();
}

#if 0
void
Plot::selection_start(index_t::const_iterator i)
{
//	assert(i <= stop);
	for (; _selection_start < i; _selection_start++)
		_selected -= _selection_start->second;
	for (; _selection_start > i; )
		_selected += (--_selection_start)->second;
	set_count();
	set_start();
}

void
Plot::selection_stop(index_t::const_iterator i)
{
//	assert(i <= stop);
	while (_selection_stop < i)
		_selected += (++_selection_stop)->second;
	while (_selection_stop > i)
		_selected -= (_selection_stop--)->second;
	set_count();
	set_stop();
}
#endif

void
Plot::adjust_start(bool up)
{
//	std::clog << "PlotInfo::adjust_start: " << up << std::endl;
	index_t::const_iterator const o = _selection_start;
	if (up)
	{
		if (_selection_start != stop)
		{
			if (_selection_start == _selection_stop)
				adjust_end(true);
			_selected -= _selection_start++->second;
		}
	}
	else if (_selection_start != begin)
		_selected += (--_selection_start)->second;
}

void
Plot::adjust_end(bool up)
{
//	std::clog << "PlotInfo::adjust_end: " << up << std::endl;
	check();
	index_t::const_iterator const o = _selection_stop;
	if (up)
	{
		if (_selection_stop != stop)
			_selected += (++_selection_stop)->second;
	}
	else if (_selection_stop != begin)
	{
		if (_selection_start == _selection_stop)
			adjust_start(false);
		_selected -= (_selection_stop--)->second;
	}
}

static unsigned
sctgs_count(mekano::Group const * g)
{
	return g->sctgs_cnt();
}

void
Selector::fill_plots(int const i)
{
	_current_stage = i;
	if (i >= 0 && unsigned(i) < stage_list.size())
	{
		ShowWaitCursor w(this, "Selector::fill_plots");
//		std::clog << " stage_list[" << i << "].id=" << stage->id() << std::endl;
		for_all(_plots, boost::bind(&Plot::fill, _1, stage(), &group_selector));
		if (i)
			stage_choice.value(i); // only necessary if do fill_plots(i) where i != 0
	}
}

class filter_sctgs : public std::unary_function<eyedb::Oid, void>
{
	std::set<eyedb::Oid> const * const _all;
	std::set<eyedb::Oid> * const _ok;
	int const _min;
	int const _max;
	eyedb::Database * const _db;
	typedef mekano::LinkCounts const * (mekano::Supercontig::*link_counts_t)(eyedb::Bool *, eyedb::Status *) const;
	link_counts_t const _link_counts;
public:
	filter_sctgs(std::set<eyedb::Oid> const * all, std::set<eyedb::Oid> * ok, int v, int max, eyedb::Database * db,
		link_counts_t lc) :
		_all(all), _ok(ok), _min(v), _max(max), _db(db), _link_counts(lc)
	{
	}
	result_type operator()(argument_type const & sctg_oid)
	{
		if (_ok->find(sctg_oid) == _ok->end())
		{
			ObjectHandle<mekano::Supercontig> sctg(_db, sctg_oid);
			mekano::LinkCounts const * cls = (sctg->*_link_counts)(0, 0);
			for (int i = cls->link_counts_cnt(0); --i >= 0; )
			{
				mekano::LinkCount const * clc = cls->link_counts(i);
				if (clc->n() >= _min && clc->n() <= _max && _all->find(clc->sctg_oid()) != _all->end())
				{
					_ok->insert(clc->sctg_oid());
					_ok->insert(sctg_oid);
					return;
				}
			}
		}
	}
};

void
sctg_pair_filter
(
	std::set<eyedb::Oid> const & actual,
	std::set<eyedb::Oid> * const sctgs,
	mekano::Calculated const * const clc,
	filter_index_t::const_iterator const ibegin,
	filter_index_t::const_iterator const iend,
	eyedb::CollBag const * (mekano::Calculated::* at)(unsigned, eyedb::Bool *, eyedb::Status *) const
)
{
	Perfs perf("sctg_pair_filter" + to_string(iend - ibegin) + " " + to_string(actual.size()));
	
	std::set<eyedb::Oid>::iterator const end = actual.end();
	unsigned const n = actual.size();
	
	static int eyedb_collection_iterator_prints_debug = 0;
	if (!eyedb_collection_iterator_prints_debug)
	{
		freopen("/dev/null", "w", stdout);
		++eyedb_collection_iterator_prints_debug;
	}
	eyedb::Value v;
	for (filter_index_t::const_iterator fi = ibegin; fi != iend; ++fi)
		for (eyedb::CollectionIterator ci((clc->*at)(*fi, 0, 0)); ci.next(v); )
			if (v.type == v.tObject)
				if (mekano::SupercontigPair const * scp = mekano::SupercontigPair::_convert(v.o))
					if (actual.find(scp->sctg0_oid()) != end && actual.find(scp->sctg1_oid()) != end)
					{
						sctgs->insert(scp->sctg0_oid());
						sctgs->insert(scp->sctg1_oid());
						if (n == sctgs->size())
							return;
					}
}

void
Selector::filter
(
	from_calculated_t ccounts,
	from_sctg_t scounts,
	filter_index_t::const_iterator const ibegin,
	filter_index_t::const_iterator iend
)
{
	ShowWaitCursor wait(this, "Selector::filter");
	std::set<eyedb::Oid> sctgs;
	oids.swap(sctgs);
	if (unsigned(iend - ibegin) < sctgs.size())
		sctg_pair_filter(sctgs, &oids, _calculated, ibegin, iend, ccounts);
	else
		for_each
		(
			sctgs.begin(),
			sctgs.end(),
			filter_sctgs
			(
				&sctgs,
				&oids,
				*ibegin,
				iend[-1],
				stage_list[0].database(),
				scounts
			)
		);
	update_count();
}

void
Selector::apply(operator_t f, std::vector<eyedb::Oid>::const_iterator b, std::vector<eyedb::Oid>::const_iterator e)
{
	std::set<eyedb::Oid> tmp;
	f(oids.begin(), oids.end(), b, e, std::inserter(tmp, tmp.begin()));
	oids.swap(tmp);
	update_count();
}

void
Selector::operator_cb(get_sctg_set_t get, operator_t f, index_t::const_iterator b, index_t::const_iterator e, unsigned n)
{
	ShowWaitCursor wait(this, "Selector::operator_cb");
	std::vector<eyedb::Oid> selected;
	selected.reserve(n);
	for (unsigned i = stage()->groups_cnt(); i-- > 0; )
		if (group_selector.selected(stage()->groups(i)->name_oid()))
			for_each(b, e, add_sctgs(stage(), i, get, std::back_inserter(selected)));
	if (selected.size() != n)
		throw MyError("make_set_from_selection: expected " + to_string(n)  + " sctgs but found " + to_string(selected.size()) + " sctgs");
	sort(selected.begin(), selected.end());
	if (unique(selected.begin(), selected.end()) != selected.end())
		throw MyError("Plot::callback: found duplicates!");
	apply(f, selected.begin(), selected.end());
}

Selector::Selector(eyedb::Database * db, BankColourMap * bcm, ObjectHandle<mekano::Parameters> const & param, SymbolFont const & symbols, std::set<eyedb::Oid> const & scafs) :
	fltk::Window(fltk::USEDEFAULT, fltk::USEDEFAULT, 500, 500, "see below", true),
	_current_stage(-1),
	by_name(0, w(), symbols, db, boost::bind(&Selector::oid_cb, this, _1, _2)),
	stage_choice(0, by_name.b(), 100, 30, "stage "),
	group_selector(stage_choice.r(), stage_choice.y(), w() - stage_choice.w(), db, boost::bind(&Selector::group_selected, this)),
	_bcm(bcm),
	_parameters(param),
	_calculated(param->calculated()),
	oids(scafs)
{
	end();
	copy_label((std::string("mekano::selection ") + db->getName()).c_str());
	
	stage_choice.x(stage_choice.x() + label_width(&stage_choice) + 10);
	stage_choice.w(fill_menu(db, &stage_choice) + 40);
	group_selector.x(stage_choice.r());
	group_selector.w(w() - stage_choice.r());
	if (stage_choice.h() > group_selector.h())
		group_selector.h(stage_choice.h());
	else
		stage_choice.h(group_selector.h());

	Filter * clone_links_filter = new Filter(w(), _calculated->clone_links(), "clone links", boost::bind(&Selector::filter, this, from_calculated_t(&mekano::Calculated::clone_links_at), from_sctg_t(&mekano::Supercontig::clone_links), _1, _2));
	add(clone_links_filter);
	
	Filter * matches_filter = new Filter(w(), _calculated->matches(), "matches", boost::bind(&Selector::filter, this, from_calculated_t(&mekano::Calculated::matches_at), from_sctg_t(&mekano::Supercontig::matches), _1, _2));
	add(matches_filter);
	matches_filter->y(h() - matches_filter->h());
	
	clone_links_filter->y(matches_filter->y() - clone_links_filter->h());
	
	Label * count_label = new Label(0, 0, 25, "number of supercontigs selected");
	add(count_label);
	
	count = new fltk::ValueOutput(count_label->r(), 0, 50, count_label->h());
	count->y(clone_links_filter->y() - count->h());
	count_label->y(count->y());
	count->value(oids.size());
	add(count);
	
	fltk::Button * clear_button = new fltk::Button(count->r(), count->y(), 20, count->h(), "clear");
	labelwidth2width(clear_button);
	add(clear_button);
	clear_button->callback(clear_cb, this);
	
	fltk::Button * show_button = new fltk::Button(clear_button->r(), clear_button->y(), 20, clear_button->h(), "show");
	labelwidth2width(show_button);
	add(show_button);
	show_button->callback(show_cb, this);
	
	int const plot_height = (count->y() - group_selector.b()) / 3;

	_plots.push_back
	(
		new Plot
		(
			group_selector.b(),
			w(),
			plot_height,
			"length",
			boost::bind(&Selector::operator_cb, this, get_sctg_set_t(&mekano::Group::length_counts_at), _1, _2, _3, _4),
			symbols,
			"length_counts"
		)
	);
	_plots.push_back
	(
		new Plot
		(
			_plots.back()->b(),
			w(),
			plot_height,
			"number of continuous sequences",
			boost::bind(&Selector::operator_cb, this, get_sctg_set_t(&mekano::Group::cseq_counts_at), _1, _2, _3, _4),
			symbols,
			"cseq_counts"
		)
	);
	_plots.push_back
	(
		new Plot
		(
			_plots.back()->b(),
			w(),
			plot_height,
			"number of contigs",
			boost::bind(&Selector::operator_cb, this, get_sctg_set_t(&mekano::Group::ctg_counts_at), _1, _2, _3, _4),
			symbols,
			"ctg_counts"
		)
	);
	typedef void (fltk::Group::* which_add)(fltk::Widget *);
	for_all(_plots, boost::bind(which_add(&fltk::Group::add), this, _1));

	add_resizable(*new fltk::InvisibleBox(0, _plots.front()->y(), w(), _plots.back()->b() - _plots.front()->y()));
	show();
	fill_plots(0);
}

template <class C>
struct ReadObjectOid : std::binary_function<eyedb::Database *, eyedb::Oid, ObjectHandle<C> >
{
	ObjectHandle<C> operator()(eyedb::Database * db, eyedb::Oid const & oid) const
	{
		return ObjectHandle<C>(db, oid);
	}
};

void
Selector::show_cb()
{
	scaf_list_t scaf_list;
	std::transform(oids.begin(), oids.end(), std::back_inserter(scaf_list), std::bind1st(ReadObjectOid<mekano::Supercontig>(), stage_list[0].database()));
	if (!scaf_list.empty())
		open_circle(scaf_list, _bcm, _parameters, "list",  "selected supercontigs");
}

int
Selector::fill_menu(eyedb::Database * db, fltk::Choice * stages)
{
	int max_w = 0;
	eyedb::OidArray oids;
	eyedb::OQL(db, string2eyedb("select list(s, s.id) from Stage s order by s.id")).execute(oids);
	for (int i = 0; i < oids.getCount(); i++)
	{
		ObjectHandle<mekano::Stage> stage(db, oids[i]);
		unsigned n_sctgs = sum<unsigned>(begin_groups(stage), end_groups(stage), sctgs_count);
		{
			stage_list.push_back(stage);
			std::string const txt = std::string(stage_type(stage)) + to_string(stage->id()) + " " + to_string(n_sctgs);
			max_w = std::max(max_w, label_width(stages->add(txt.c_str(), 0, item_callback, this)));
//			std::cout << "Selector::fill txt=" << txt << " max_w= " << max_w << std::endl;
		}
	}
	return max_w;
}

} // namespace

void
draw_selector(eyedb::Database * db, BankColourMap * bcm, ObjectHandle<mekano::Parameters> const & param, std::set<eyedb::Oid> const & scafs)
{
	static SymbolFont symbols;
	new Selector(db, bcm, param, symbols, scafs);
}
