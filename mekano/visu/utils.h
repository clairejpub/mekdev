/*	Stuart Pook, Genoscope 2006 (make) (Indent ON)
 *	$Id: utils.h 771 2007-05-31 09:42:06Z stuart $
 */
#ifndef MEKANO_VISU_UTILS_H
#define MEKANO_VISU_UTILS_H MEKANO_VISU_UTILS_H
#include <list>
#include "mekano.h"
#include <utils/templates.h>
#include <utils/eyedb.h>
#include <map>
#include <set>
#include <utils/sctg_data.h>
#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>

class BankColourMap;

class MyError
{
	std::string _message;
public:
	MyError(std::string const & m) : _message(m) {}
	std::string toString() const { return _message; }
};

inline std::string stage_type(mekano::Stage const * s)
{
	if (!s) 
		return "no Stage";
	if (mekano::StageType const * st = s->stage_type())
		return st->name();
	return "no StageType";
}

typedef std::list<ObjectHandle<mekano::Supercontig> > scaf_list_t;

void draw_selector(eyedb::Database *, BankColourMap *, ObjectHandle<mekano::Parameters> const &, std::set<eyedb::Oid> const &);

template <typename C>
class Overlap
{
	typedef std::map<int, std::pair<int, C> > x_t;
	typedef std::map<int, std::pair<int, x_t> > y_t;
	y_t elements;
public:
	void add(int x, int y, int w, int h, C const & a)
	{
		int const y1 = y + h - 1;
		typename y_t::iterator p = elements.insert(std::make_pair(y1, std::make_pair(y, x_t()))).first;
		p->second.second.insert(std::make_pair(x + w - 1, std::make_pair(x, a)));
		assert(p->second.first == y);
	}
	C const * overlap(int x, int y) const
	{
		typename y_t::const_iterator p = elements.lower_bound(y);
		if (p != elements.end() && y <= p->first && y >= p->second.first)
		{
			typename x_t::const_iterator q = p->second.second.lower_bound(x);
			if (q != p->second.second.end() && x <= q->first && x >= q->second.first)
				return &q->second.second;
		}
		return 0;
	}
	void clear() { elements.clear(); }
};
typedef std::vector<std::pair<SupercontigData::PositionnedRead const *, std::pair<int, int> > > clones_t;
typedef std::vector<unsigned> contig_positions_t;

struct SupercontigInfo : private boost::noncopyable
{
	class clone_link_t
	{
		clones_t::value_type const * _clone1;
		clones_t::value_type const * _clone2;
	public:
		mekano::Bank * bank() const { return _clone1->first->read_position()->read()->clone()->bank(); }
		double length_error() const { return double(length()) / bank()->length(); }
		int start() const { return _clone1->second.first; }
		int length() const { return end2length(start(), std::max(_clone1->second.second, _clone2->second.second)); }
		clone_link_t(clones_t::value_type const * c1, clones_t::value_type const * c2) :
			_clone1(c1->second.first < c2->second.first ? c1 : c2),
			_clone2(_clone1 == c1 ? c2 : c1)
			{}
		clones_t::value_type const * clone1() const { return _clone1; }
		clones_t::value_type const * clone2() const { return _clone2; }
		SupercontigData::PositionnedRead const * positionned_read1() const { return clone1()->first; }
		SupercontigData::PositionnedRead const * positionned_read2() const { return clone2()->first; }
	};
	typedef std::vector<clone_link_t> clone_links_t;

private:
	SupercontigData _data;

	mutable clone_links_t _clone_links;
	mutable clones_t _clones;
	mutable contig_positions_t _contig_positions;
	mutable contig_positions_t _continuous_sequence_positions;
	mutable bool _valid;
	mutable double _biggest_clone_link_error;
	ObjectHandle<mekano::Parameters> const _parameters;
	void do_validate() const;
	void validate() const	{ if (!_valid) do_validate(); }
	SupercontigInfo(SupercontigInfo const &);
	void operator=(SupercontigInfo const &);
public:
	SupercontigData const & data() const  { return _data; }
	SupercontigInfo(ObjectHandle<mekano::Supercontig> s, ObjectHandle<mekano::Parameters> const & p ) :
		_data(s), _valid(false), _parameters(p) {}
	ObjectHandle<mekano::Supercontig> const & sctg() const { return _data.sctg(); }
	ObjectHandle<mekano::Supercontig> const & super_contig() const { return sctg(); }
	contig_positions_t const & contig_positions() { validate(); return _contig_positions; }
	contig_positions_t const & continuous_sequence_positions() { validate(); return _continuous_sequence_positions; }
	double biggest_clone_length_error() const
	{
		validate();
		return _biggest_clone_link_error;
	}
	clones_t::const_iterator clone_begin() const
	{
		validate();
		return _clones.begin();
	}
	clone_links_t  const & clone_links() const
	{
		validate();
		return _clone_links;
	}
	clones_t::const_iterator clone_end() const
	{
		validate();
		return _clones.end();
	}
	int length() const
	{
		return _data.c_sctg()->length();
	}
};
typedef std::vector<boost::shared_ptr<SupercontigInfo> > scaffolds_t;

void matches(scaffolds_t::value_type const &, int, int, scaffolds_t::value_type const &, int, int, bool);
#endif
