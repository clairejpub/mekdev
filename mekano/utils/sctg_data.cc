/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: sctg_data.cc 795 2007-06-04 17:19:56Z stuart $
 */
#include <vector>
#include "mekano.h"
#include <utils/templates.h>
#include <utils/eyedb.h>
#include "iterators.h"
#include "templates.h"
#include "sctg_data.h"
#include "myerror.h"

#define CHECK 0

enum { StartPosition = SupercontigData::start_in_db, start_in_db = SupercontigData::start_in_db, };

namespace
{
	struct CollectConseq : public std::unary_function<mekano::Region *, void>
	{
		typedef std::list<SupercontigData::ConseqData> Container;
		result_type operator()(argument_type region) const;
		CollectConseq(Container * c) : _c(c), _set_top(true), _bottom(0) {}
	private:
		CollectConseq(Container * c, mekano::Supercontig * bottom) :
			_c(c),
			_set_top(false),
			_bottom(bottom)
		{}
		Container * const _c;
		bool const _set_top;
		mekano::Supercontig * const _bottom;
		void append(Container::value_type const & v) const { _c->push_back(v); }
		Container::iterator append
		(
			mekano::ContinuousSequence * cs,
			int start,
			int stop,
			bool forward,
			mekano::Supercontig * bottom,
			bool bottom_forward
		) const
		{
#if 0
			std::cout << "inserting " << cs->name()
//				<< " start " << start << " stop " << stop
					<< " forward " << forward
					<< " top " << (top ? top->name() : "0")
					<< " bottom " << (bottom ? bottom->name() : "0")
					<< " top_forward " << top_forward
					<< " bottom_forward " << bottom_forward
					<< std::endl;
#endif
			return _c->insert(_c->end(), Container::value_type(cs, start, end2length(start, stop), forward, bottom, bottom_forward));
		};
#if 1
		Container::iterator append(Container::value_type const & p, int start, int stop, bool forward) const
		{
			return append(p.conseq(), start, stop, forward == p.cs_forward(), p.bottom(), forward == p.bottom_forward());
		}
#endif
		void add_gap(mekano::Region const * region) const;
	};

	void
	CollectConseq::operator()(argument_type region) const
	{
		int start = region->start_on_first() - StartPosition;
		int stop = region->stop_on_last() - StartPosition;
		if (mekano::ContinuousSequence * const cs = region->sequence()->asContinuousSequence())
		{
			assert(region->first_oid() == cs->getOid());
			assert(region->last_oid() == cs->getOid());
	
			bool const forward = region->start_on_first() < region->stop_on_last();
			if (!forward)
				std::swap(start, stop);
			append(cs, start, stop, forward, _bottom, forward);
		}
		else if (mekano::Supercontig * const sctg = region->sequence()->asSupercontig())
		{
			Container c;
			std::for_each(begin_regions(sctg), end_regions(sctg), CollectConseq(&c, sctg));
#if 0
			std::cout << "CollectConseq(" << sctg->name() << ") start " << start << " stop " << stop
				<< " " << region->first()->name() << " " << region->last()->name()
				<< " -> \n\t";
			std::copy(c.begin(), c.end(), std::ostream_iterator<Container::value_type>(std::cout, "\n\t"));
			std::cout << std::endl;
#endif

			eyedb::Oid oids[] = { region->first_oid(), region->last_oid() };
			Container::iterator first = find_first_of(c.begin(), c.end(), oids, oids + 2);
			if (first == c.end())
				throw MyError(region->first()->name() + " & " + region->last()->name() + " not found in " + sctg->name());
			Container::iterator first_added;
			bool forward;
			if (region->first_oid() == region->last_oid())
			{
				forward = region->start_on_first() < region->stop_on_last();
				if (!forward)
					std::swap(start, stop);
				bool const top_forward = forward == first->forward();
				first_added = append(*first, start, stop, forward, first->bottom(), top_forward ? first->bottom_forward() : !first->bottom_forward());
#if 0
				std::cout << "single conseq from region" << " first " << first->name()
						<< " forward " << forward
						<< " first->forward() " << first->forward();
#endif
				forward = top_forward;
//				std::cout << " forward " << forward << std::endl;
			}
			else
			{
				Container::iterator p = first;
				forward = first->oid() == region->first_oid();
				eyedb::Oid const other = forward ? region->last_oid() : region->first_oid();
				Container::iterator last = find(++p, c.end(), other);
				if (last == c.end())
					throw MyError(to_string(other) + " not found in " + sctg->name());
				if (!forward)
				{
					std::swap(last, first);
					c.reverse();
					Container::iterator q = first;
					for_each(++q, last, std::mem_fun_ref(&SupercontigData::ConseqData::reverse));
				//	std::cout << "reverse" << " first " << first->name() << " last " <<  last->name() << std::endl;
				}
				if (forward == first->forward())
					first_added = append(*first, start, first->stop(), forward);
				else
					first_added = append(*first, first->start(), start, forward);
				_c->splice(_c->end(), c, ++first, last);
				if (forward == last->forward())
					append(*last, last->start(), stop, forward);
				else
					append(*last, stop, last->stop(), forward);
				
			}
			if (_set_top)
				for_each(first_added, _c->end(), std::bind2nd(std::mem_fun_ref(&SupercontigData::ConseqData::set_top), std::make_pair(sctg, forward)));
#if 0
			std::cout << "trimmed " << sctg->name() << " -> \n\t";
			std::copy(first_added, _c->end(), std::ostream_iterator<Container::value_type>(std::cout, "\n\t"));
			std::cout << std::endl;
#endif
		}
		add_gap(region);
	}
	
	void
	CollectConseq::add_gap(mekano::Region const * region) const
	{
		eyedb::Bool no_gap;
		int const gap = region->gap_after(&no_gap);
		if (!no_gap && gap)
		{
			if (gap > 0)
				append(Container::value_type(gap));
			else
				append(Container::value_type());
		}
	}

	struct ReadCut : public std::unary_function<mekano::Cut *, void>
	{
		typedef std::list<SupercontigData::ContigData> container_type;
	public:
		ReadCut
		(
			container_type * c,
			bool forward,
			mekano::Supercontig * top,
			bool top_forward,
			mekano::Supercontig * bottom,
			bool bottom_forward
		) :
			_container(c),
			_forward(forward),
			_top(top),
			_top_forward(top_forward),
			_bottom(bottom),
			_bottom_forward(bottom_forward)
			{}
		result_type operator()(argument_type const & cut) const;
	private:
		container_type * const _container;
		bool const _forward;
		mekano::Supercontig * _top;
		bool const _top_forward;
		mekano::Supercontig * _bottom;
		bool const _bottom_forward;
	};
	
	void
	read_metacontig
	(
		ReadCut::container_type * c,
		mekano::Metacontig * mcntg,
		unsigned start,
		unsigned stop,
		bool forward,
		mekano::Supercontig * top,
		bool top_forward,
		mekano::Supercontig * bottom,
		bool bottom_forward
	)
	{
#if 0
		std::cout << "read_metacontig " << mcntg->name() << " start " << start << " stop " << stop
			<< " forward " << forward
			<< " bottom_forward " << bottom_forward
			<< std::endl;
#endif
		ReadCut::container_type included;
		typedef ReadCut::container_type::value_type vt;
		std::for_each(begin_cuts(mcntg), end_cuts(mcntg), ReadCut(&included, forward, top, top_forward, bottom, bottom_forward));
		ReadCut::container_type::iterator e = included.end();
		unsigned pos = 0;
		ReadCut::container_type::iterator first = included.begin();
		for (; first != e && pos + first->length() <= start; first++)
			pos += first->length();
		unsigned const skip = start - pos;
		assert(first != e);
		ReadCut::container_type::iterator last = first;
		for (; last != e && pos + last->length() < stop; last++)
			pos += last->length();
		assert(last != e);
#if 0
		std::cout << "read_metacontig first = " << first->name() << " pos " << pos << " skip " << skip << " first->length() " << first->length() << std::endl;
		std::cout << "read_metacontig last = " << last->name() << " pos " << pos << " last->length() " << last->length() << std::endl;
#endif
		assert(int(stop) - int(pos) + 1 >= 0);
		unsigned const end_skip = last->length() - (stop - pos + 1);
		if (first == last)
		{
			unsigned const len = stop - start + 1;
			unsigned const nstart = first->forward() ? first->start() + skip : first->stop() - len + 1;
#if 0
			std::clog << "adding first==last " << last->conseq()->name() << " size= " << c->size()
				<< " nstart " << nstart
				<< " len " << len
				<< " first->forward() " << first->forward()
				<< " first->start() " << first->start()
				<< std::endl;
#endif
			included.assign(1, vt(*first, nstart, len));
		}
		else
		{
			included.erase(included.begin(), first);
			ReadCut::container_type::iterator to_cut = last;
			included.erase(++to_cut, included.end());
			unsigned const first_start = first->start() + (first->forward() ? skip : 0);
			included.push_front(vt(*first, first_start, first->length() - skip));
			included.erase(first);
			included.push_back(vt(*last, last->start() + (last->forward() ? 0 : end_skip), last->length() - end_skip));
			included.erase(last);
		}
		if (!forward)
		{
			for_each(included.begin(), included.end(), std::mem_fun_ref(&SupercontigData::ConseqData::contig_reverse));
			included.reverse();
		}
#if 0
		std::cout << "done read_metacontig " << mcntg->name() << " size() " << included.size() << std::endl;
		copy(included.begin(), included.end(), std::ostream_iterator<ReadCut::container_type::value_type>(std::cout, "\n"));
#endif
		c->splice(c->end(), included);
	}
}

ReadCut::result_type
ReadCut::operator()(argument_type const & cut) const
{
	unsigned start = cut->start() - StartPosition;
	unsigned stop = cut->stop() - StartPosition;
	bool const forward = start < stop;
	mekano::Supercontig * top = _top ? _top : cut->source();
	bool top_forward = _top ? _top_forward : !!cut->source_forward();
	mekano::Supercontig * bottom = cut->source() ? cut->source() : _bottom;
	bool bottom_forward = cut->source() ? (_forward == cut->source_forward()) : (forward == _bottom_forward);
#if 0
	std::cout << "ReadCut::operator(): " << cut->cseq()->name()
		<< " cut->source() " << (void *)cut->source()
		<< " forward " << forward
		<< " _forward " << _forward
		<< " cut->source_forward() " << cut->source_forward()
		<< " _bottom_forward " << _bottom_forward
		<< " bottom_forward " << bottom_forward
		<< std::endl;
#endif
	if (!forward)
		std::swap(start, stop);
	int const len = end2length(start, stop);
	if (mekano::Metacontig * const mcntg = cut->cseq()->asMetacontig())
	{
		read_metacontig
		(
			_container,
			mcntg,
			start,
			stop,
			forward,
			top,
			top_forward,
			bottom,
			bottom_forward
		);
	}
	else if (mekano::Contig * const cntg = cut->cseq()->asContig())
	{
		_container->push_back(container_type::value_type(cntg, start, len, forward, top, bottom, top_forward, bottom_forward));
#if 0
		std::clog << "ReadCut::() "
			<< " cntg " <<cntg->name()
			<< " start " << start
			<< " stop " << stop
			<< " forward " << forward
			<< " _top " << (_top ? _top->name() : "0")
			<< " top " << (top ? top->name() : "0")
			<< " _top_forward " << _top_forward
			<< " top_forward " << top_forward
			<< " cut->source_forward() " << cut->source_forward()
//			<< " bottom " << (bottom ? bottom->name() : "0")
			<< " size " << _container->size()
			<< std::endl;
#endif
	}
	else
		assert(!"unknown object");
}

class CollectContigs : public std::unary_function<SupercontigData::ConSeqData, void>
{
	typedef SupercontigData::ContigContainer Container;
	Container * const _container;
public:
	CollectContigs(Container * c) : _container(c) {}
	result_type operator()(argument_type const & conseq);
};

CollectContigs::result_type
CollectContigs::operator()(argument_type const & conseq)
{
	if (conseq.is_gap() || conseq.conseq()->asContig())
		_container->push_back(conseq);
	else if (mekano::Metacontig * meta = conseq.conseq()->asMetacontig())
	{
		ReadCut::container_type contigs;
		read_metacontig
		(
			&contigs,
			meta,
			conseq.start(),
			length2end(conseq.start(), conseq.length()),
			conseq.cs_forward(),
			conseq.top(),
			conseq.top_forward(),
			conseq.bottom(), 
			conseq.bottom_forward()
		);
		copy(contigs.begin(), contigs.end(), back_inserter(*_container));
	}
	else
		assert(!"unknown ContinuousSequence");
}

void
SupercontigData::make_contig_data() const
{
//	std::clog << "make_contig_data " << _supercontig->name() << std::endl;
	ConSeqContainer const & cs = continuous_sequences();
	std::for_each(cs.begin(), cs.end(), CollectContigs(&_contig_data));
	_nb_ctg = count_if(_contig_data.begin(), _contig_data.end(), not1(std::mem_fun_ref(&ContigData::is_gap)));
	_contig_data_initialized = true;
	
#if CHECK
	unsigned nholes = count_if(_contig_data.begin(), _contig_data.end(), std::mem_fun_ref(&ContigData::unknown_gap));
	assert(_holes == nholes);
	unsigned nbases = sum_if
	(
		_contig_data.begin(),
		_contig_data.end(),
		std::mem_fun_ref(&ContigData::length),
		not1(std::mem_fun_ref(&ContigData::unknown_gap))
	);
	if (_bases != nbases)
		throw MyError("SupercontigData::make_contig_data(): nbases = " + to_string(nbases) + " _bases = " + to_string(_bases));
#endif
}

class collect_sctgs : public std::unary_function<SupercontigData::ConseqData, void>
{
	typedef std::back_insert_iterator<SupercontigData::SupercontigContainer> insertor_type;
	insertor_type _inserter;
	SupercontigData::get_sctg_t const _get_sctg;
	mekano::Supercontig * _bottom;
	unsigned _bases;
	unsigned _holes;
	unsigned _unclaimed_bases;
	unsigned _unclaimed_holes;
	bool _forward;
public:
	collect_sctgs(insertor_type i, SupercontigData::get_sctg_t g) :
		_inserter(i),
		_get_sctg(g),
		_bottom(0),
		_bases(0),
		_holes(0),
		_unclaimed_bases(0),
		_unclaimed_holes(0),
		_forward(true)
	{}
	result_type operator()(argument_type const & conseq)
	{
		if (conseq.known_gap())
			_unclaimed_bases += conseq.length();
		else if (conseq.unknown_gap())
			_unclaimed_holes++;
		else
		{
			bool forward;
			mekano::Supercontig * const nsctg = (conseq.*_get_sctg)(&forward);
#if 0
			std::cout << conseq.conseq()->name() << " " << (_bottom ? _bottom->name() : "0")
				<< " " << (nsctg ? nsctg->name() : "NULL") << " forward= " << forward << std::endl;
#endif
			if (nsctg && _bottom && nsctg != _bottom)
				assert(nsctg->name() != _bottom->name());
			if (nsctg == _bottom)
			{
				_bases += conseq.length() + _unclaimed_bases;
				_holes += _unclaimed_holes;
				_unclaimed_bases = _unclaimed_holes = 0;
			}
			else
			{
				finish();
				_bottom = nsctg;
				_bases = conseq.length();
				_holes = _unclaimed_bases = _unclaimed_holes = 0;
				_forward = forward;
			}
			if (nsctg && _forward != forward)
				throw MyError
				(
					"collect_sctgs::operator()(" + conseq.name() + "):"
					+ " nsctg=" + nsctg->name()
					+ " _bottom=" + (_bottom ? _bottom->name() : "0")
					+ ": _forward=" + to_string(_forward) + " != forward=" + to_string(forward)
				);
			assert(!nsctg || _forward == forward);
		}
	}
	void finish()
	{
		if (_bases || _holes)
		{
			*_inserter++ = SupercontigData::SupercontigRange(_bottom, _bases, _holes, _forward);
//			std::cout << "adding  " << _bases << " + " << _holes << std::endl;
		}
		if (_unclaimed_bases || _unclaimed_holes)
		{
			*_inserter++ = SupercontigData::SupercontigRange(0, _unclaimed_bases, _unclaimed_holes, true);
//			std::cout << "adding unclaimed " << _unclaimed_bases << " + " << _unclaimed_holes << std::endl;
		}
	}
};

class add_lengths : public std::unary_function<SupercontigData::SupercontigRange, void>
{
	unsigned _bases;
	unsigned _holes;
public:
	add_lengths() : _bases(0), _holes(0) {}
	result_type operator()(argument_type const & sctg)
	{
		_bases += sctg.bases();
		_holes += sctg.holes();
	}
	unsigned bases() const { return _bases; }
	unsigned holes() const { return _holes; }
};

void
SupercontigData::make_top_sctgs() const
{
	ConSeqContainer const & cs = contigs();
	try
	{
		for_each(cs.begin(), cs.end(), collect_sctgs(back_inserter(_top_sctgs), &ConSeqData::top)).finish();
	}
	catch (MyError e)
	{
//		std::clog << "SupercontigData::make_top_sctgs() failed for " << sctg()->name() << std::endl;
		throw MyError("make_top_sctgs(" + sctg()->name() + "): " + e.toString());
	}
	_top_sctgs_initialized = true;
#if CHECK
	add_lengths l = for_each(_top_sctgs.begin(), _top_sctgs.end(), add_lengths());
	if (l.bases() != _bases)
		std::clog << "l.bases = " << l.bases() << " l.holes = " << l.holes() << std::endl;
	assert(l.bases() == _bases);
	assert(l.holes() == _holes);
#endif
}

void
SupercontigData::make_bottom_sctgs() const
{
	ConSeqContainer const & cs = contigs();
	try
	{
		for_each(cs.begin(), cs.end(), collect_sctgs(back_inserter(_bottom_sctgs), &ConSeqData::bottom)).finish();
	}
	catch (MyError e)
	{
//		std::clog << "SupercontigData::make_bottom_sctgs() failed for " << sctg()->name() << std::endl;
		throw MyError("make_bottom_sctgs(" + sctg()->name() + "): " + e.toString());
		throw;
	}
	_bottom_sctgs_initialized = true;
#if CHECK
	add_lengths l = for_each(_bottom_sctgs.begin(), _bottom_sctgs.end(), add_lengths());
	assert(l.bases() == _bases);
	assert(l.holes() == _holes);
#endif
}

void
SupercontigData::make_conseq_data() const
{
	CollectConseq::Container c;
	std::for_each(begin_regions(_supercontig), end_regions(_supercontig), CollectConseq(&c));
	copy(c.begin(), c.end(), back_inserter(_conseq_data));
	
//	std::for_each(begin_regions(_supercontig), end_regions(_supercontig), CollectConSeq(&_conseq_data));
	_holes = count_if(_conseq_data.begin(), _conseq_data.end(), std::mem_fun_ref(&ConSeqData::unknown_gap));
	_bases = sum_if
	(
		_conseq_data.begin(),
		_conseq_data.end(),
		std::mem_fun_ref(&ConSeqData::length),
		not1(std::mem_fun_ref(&ConSeqData::unknown_gap))
	);
	_nb_cseq = count_if(_conseq_data.begin(), _conseq_data.end(), not1(std::mem_fun_ref(&ConSeqData::is_gap)));
	if (false)
	{
//		std::transform(rbegin_regions(_supercontig), rend_regions(_supercontig), std::mem_fun(&mekano::Region::toString), std::ostream_iterator<std::string>(std::clog, " \n"));
		std::vector<mekano::Region *> v;
		std::copy(rbegin_regions(_supercontig), rend_regions(_supercontig), std::back_inserter(v));
	}
	_conseq_data_initialized = true;
}

std::ostream &
operator<<(std::ostream & s, SupercontigData::ConseqData const & d)
{
	if (!d.is_gap())
	{
		s << "name=" << d.conseq()->name() << '/' << d.conseq()->getOid()
			<< ",start=" << d.start() << ",length=" << d.length()
			<< ',' << (d.cs_forward() ? "forward" : "reverse");
		if (d.top())
			s << ",top=" << d.top()->name() << '/' << (d.top_forward() ? "forward" : "reverse");
		if (d.bottom())
			s << ",bottom=" <<d.bottom()->name() << '/' << (d.bottom_forward() ? "forward" : "reverse");
		return s;
	}
	if (d.unknown_gap())
		return s << "unknown_gap";
	if (d.known_gap())
		return s << "gap=" << d.gap_size();
	return s << "bad";
}

#if CHECK
struct overlaps : public std::binary_function<SupercontigData::Range, mekano::ReadPosition const *, bool>
{
	result_type operator()(first_argument_type const & contig, second_argument_type const & rp) const
	{
		if (rp->length() == 0 || contig.length() == 0)
			return false;
		if (length2end(unsigned(rp->start_on_contig() - start_in_db), unsigned(rp->length())) < contig.start())
			return false;
		if (length2end(unsigned(contig.start()), contig.length()) < unsigned(rp->start_on_contig() - start_in_db))
			return false;
		return true;
	}
};
typedef std::vector<std::pair<mekano::ReadPosition const *, SupercontigData::Range const *> > clone_positions_t;

template <class Insertor>
struct AddClones : public std::unary_function<SupercontigData::Range, void>
{
	typedef Insertor T;
	T insert;
public:
	explicit AddClones(T i) : insert(i) {}
	result_type operator()(argument_type const & contig)
	{
		typedef clone_positions_t::value_type t;
		if (mekano::Contig const * ctg = contig.contig())
			transform_if
			(
				begin_reads(ctg),
				end_reads(ctg),
				insert,
				std::bind2nd(std::ptr_fun(std::make_pair<t::first_type, t::second_type>), &contig),
				std::bind1st(overlaps(), contig)
			);
	}
};
template <class Insertor>
struct AddClones<Insertor>
add_clones(Insertor i)
{
	return AddClones<Insertor>(i);
}
 
static eyedb::Oid
get_clone(clone_positions_t::value_type const & cp)
{
	return cp.first->read()->clone_oid();
}

void
check_clone_positions(SupercontigData const & data)
{
	clone_positions_t clone_positions;
	for_each(data.contigs().begin(), data.contigs().end(), add_clones(std::back_inserter(clone_positions)));
	std::vector<eyedb::Oid> clones;
	transform(clone_positions.begin(), clone_positions.end(), std::back_inserter(clones), get_clone);
	std::sort(clones.begin(), clones.end());
	clones.erase(std::unique(clones.begin(), clones.end()), clones.end());
	assert(clones == data.clones());
}
#endif

class if_overlap_add : public std::unary_function<mekano::ReadPosition *, void>
{
	SupercontigData::Range const * _contig;
	unsigned _number;
	typedef std::back_insert_iterator<SupercontigData::PositionnedReadContainer> Insertor;
	Insertor _insertor;
public:
	if_overlap_add(SupercontigData::Range const * c, unsigned n, Insertor const & i) :
		_contig(c),
		_number(n),
		_insertor(i)
	{
	}
	result_type operator()(argument_type const & rp)
	{
		unsigned start = std::max(unsigned(rp->start_on_contig() - start_in_db), _contig->start());
		unsigned stop = std::min(unsigned(length2end(rp->start_on_contig() - start_in_db, rp->length())), length2end(_contig->start(), _contig->length()));
		int offset = -_contig->start();
		if (stop >= start) // check > ou >= ?
			*_insertor++ = SupercontigData::PositionnedRead(rp, _number, start + offset, stop + offset);
	}
};

template <class Iterator>
class AddReads : public std::unary_function<SupercontigData::Range, void>
{
	unsigned i;
	Iterator _insertor;
public:
	explicit AddReads(Iterator it) : i(0), _insertor(it) {}
	result_type operator()(argument_type const & contig)
	{
		if (mekano::Contig * ctg = contig.contig())
			for_each
			(
				begin_reads(ctg),
				end_reads(ctg),
				if_overlap_add(&contig, i, _insertor)
			);
		i++;
	}
};
template <class Iterator>
AddReads<Iterator>
add_reads(Iterator const & i)
{
	return AddReads<Iterator>(i);
}

static bool
CmpCloneRead(SupercontigData::PositionnedRead const & x, SupercontigData::PositionnedRead const & y)
{
	if (x.clone_oid() < y.clone_oid())
		return true;
	if (y.clone_oid() < x.clone_oid())
		return false;
	if (x.read_position()->read_oid() < y.read_position()->read_oid())
		return true;
	if (y.read_position()->read_oid() < x.read_position()->read_oid())
		return false;
	return x.start_on_contig() < y.start_on_contig();
}

void
SupercontigData::initialize_reads() const
{
	for_each(contigs().begin(), contigs().end(), add_reads(back_inserter(_reads)));
	std::sort(_reads.begin(), _reads.end(), CmpCloneRead);
	_reads_initialized = true;
}

void
SupercontigData::initialize_clones() const
{
	transform(reads().begin(), reads().end(), std::back_inserter(_clones), std::mem_fun_ref(&PositionnedRead::clone_oid));
	_clones.erase(std::unique(_clones.begin(), _clones.end()), _clones.end());
	_clones_initialized = true;
#if CHECK
	check_clone_positions(*this);
#endif
}

std::ostream &
operator<<(std::ostream & o, SupercontigData const & d)
{
	o << '(';
	o << d._supercontig->name() << ' ' << d._supercontig->eyedb::Struct::getOid();
	o << " _conseq_data_initialized=" << d._conseq_data_initialized;
	o << " _contig_data_initialized=" << d._contig_data_initialized;
	o << " _clones_initialized=" << d._clones_initialized;
	o << " _bases=" << d._bases;
	o << " _holes=" << d._holes;
	o << ')';
	return o;
}

std::ostream &
operator<<(std::ostream & o, SupercontigData::SupercontigRange const & d)
{
	o << '(';
	if (d._sctg)
		o << d._sctg->name() << ' ' << d._sctg->getOid();
	o << " _type=" << d._type;
	o << " _bases=" << d._bases;
	o << " _holes=" << d._holes;
	o << ')';
	return o;
}

unsigned
SupercontigData::Range::hlength(unsigned hole_size) const
{
	switch (_type)
	{
	case _known_gap:
		return _gap_size;

	case _unknown_gap:
		return hole_size;

	case _cs_forward:
	case _cs_reverse:
		return _contig.length;
	}
	assert(0);
}
