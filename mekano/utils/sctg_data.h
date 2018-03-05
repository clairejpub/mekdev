/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: sctg_data.h 771 2007-05-31 09:42:06Z stuart $
 */
#include <cassert>
#include <list>
#include "mekano.h"
#include <utils/eyedb.h>

#ifndef MEKANO_UTILS_SCTG_DATA_H
#define MEKANO_UTILS_SCTG_DATA_H MEKANO_UTILS_SCTG_DATA_H

struct SupercontigData
{
	enum { start_in_db = 1 };

	class PositionnedRead
	{
		mekano::ReadPosition * _rp;
		unsigned _ctg_number;
		unsigned _start_on_contig;
		unsigned _stop_on_contig;
	public:
		PositionnedRead(mekano::ReadPosition * rp, unsigned n, unsigned start, unsigned stop) :
			_rp(rp), _ctg_number(n), _start_on_contig(start), _stop_on_contig(stop)
		{
		}
		mekano::ReadPosition * read_position() const { return _rp; }
		unsigned ctg_number() const { return _ctg_number; }
		unsigned start_on_contig() const { return _start_on_contig; }
		unsigned stop_on_contig() const { return _stop_on_contig; }
		eyedb::Oid clone_oid() const { return _rp->read()->clone_oid(); }
		mekano::Bank * bank() const { return _rp->read()->clone()->bank(); }
	};
	typedef std::vector<PositionnedRead> PositionnedReadContainer; // sort on rp->read->clone_oid
	class ConseqData
	{
		typedef mekano::ContinuousSequence * conseq_t;
		enum { _known_gap, _unknown_gap, _cs_forward, _cs_reverse } _type;
		union
		{
			struct
			{
				mekano::ContinuousSequence * cs;
				unsigned start;
				unsigned length;
				mekano::Supercontig * top;
				mekano::Supercontig * bottom;
				bool top_forward;
				bool bottom_forward;
			} _contig;
			unsigned _gap_size;
		};
	public:
		bool unknown_gap() const { return _type == _unknown_gap; }
		bool known_gap() const { return _type == _known_gap; }
		bool cs_forward() const { return _type == _cs_forward; }
		bool cs_reverse() const { return _type == _cs_reverse; }

		bool is_gap() const { return unknown_gap() || known_gap(); }
		bool is_conseq() const { return !is_gap(); }

		mekano::ContinuousSequence * conseq() const { return (is_gap()) ? 0 : _contig.cs; }
		mekano::Contig * contig() const { return (is_gap()) ? 0 : conseq()->asContig(); }
		mekano::Supercontig * top(bool * f = 0) const
		{
			assert(!is_gap());
			if (f)
				*f = _contig.top_forward;
			return _contig.top;
		};
		mekano::Supercontig * bottom(bool * f = 0) const
		{
			assert(!is_gap());
			if (f)
				*f = _contig.bottom_forward;
			return _contig.bottom;
		};
		unsigned start() const { assert(!is_gap()); return _contig.start; }
		unsigned stop() const { assert(!is_gap()); return _contig.start + _contig.length - 1; }
		unsigned length() const {
			assert(!unknown_gap());
			return (known_gap()) ? _gap_size : _contig.length;
		}
		unsigned hlength(unsigned hole_size) const;
		bool forward() const { assert(!is_gap()); return _type == _cs_forward; }

		bool gap_unknown() const { assert(is_gap()); return unknown_gap(); }
		unsigned gap_size() const { assert(known_gap()); return _gap_size; }
		ConseqData() : _type(_unknown_gap) {}
		ConseqData(unsigned g) : _type(_known_gap), _gap_size(g) {}
		ConseqData
		(
			mekano::ContinuousSequence * cs,
			unsigned s,
			unsigned l,
			bool f,
			mekano::Supercontig * t,
			mekano::Supercontig * c,
			bool top_forward,
			bool bottom_forward
		) :
			_type(f ? _cs_forward : _cs_reverse)
		{
			_contig.cs = cs;
			_contig.start = s;
			_contig.length = l;
			_contig.top = t;
			_contig.bottom = c;
			_contig.top_forward = top_forward;
			_contig.bottom_forward = bottom_forward;
#if 0
			std::cout << "inserting " << cs->name()
				<< " start " << _contig.start << " length " << _contig.length
				<< " forward " << f
				<< " t " << (t ? t->name() : "0")
				<< " c " << (c ? c->name() : "0")
				<< " top_forward " << _contig.top_forward
				<< " bottom_forward " << _contig.bottom_forward
				<< std::endl;
#endif
			assert(l < 99999999);
		}
		ConseqData
		(
			mekano::ContinuousSequence * cs,
			unsigned s,
			unsigned l,
			bool f,
			mekano::Supercontig * bottom,
			bool bottom_forward
		) :
			_type(f ? _cs_forward : _cs_reverse)
		{
			_contig.cs = cs;
			_contig.start = s;
			_contig.length = l;
			_contig.top = 0;
			_contig.bottom = bottom;
			_contig.bottom_forward = bottom_forward;
			_contig.top_forward = 2; // should not be used
#if 0
			std::cout << "inserting " << cs->name()
				<< " start " << _contig.start << " length " << _contig.length
				<< " forward " << f
				<< " bottom " << (bottom ? bottom->name() : "0")
				<< " bottom_forward " << _contig.bottom_forward
				<< std::endl;
#endif
			assert(l < 99999999);
		}
		ConseqData
		(
			ConseqData const & copy,
			unsigned s,
			unsigned l
		) :
			_type(copy.forward() ? _cs_forward : _cs_reverse)
		{
			_contig.cs = copy.contig();;
			_contig.start = s;
			_contig.length = l;
			_contig.top = copy.top();
			_contig.bottom = copy.bottom();
			_contig.top_forward = copy.top_forward();
			_contig.bottom_forward = copy.bottom_forward();
			assert(l < 99999999);
#if 0
			std::cout << "copy-inserting " << _contig.cs->name()
//				<< " start " << start << " stop " << stop
					<< " forward " << copy.forward()
					<< " top " << (_contig.top ? _contig.top->name() : "0")
					<< " bottom " << (_contig.bottom ? _contig.bottom->name() : "0")
					<< " top_forward " << _contig.top_forward
					<< " bottom_forward " << _contig.bottom_forward
					<< std::endl;
#endif
		}
		operator conseq_t() const { return conseq(); }
		eyedb::Oid oid() const { return (is_gap()) ? eyedb::Oid::nullOid : _contig.cs->eyedb::Struct::getOid(); }
		std::string name() const { return (is_gap()) ? "gap" : conseq()->name(); };
		operator eyedb::Oid() const { return oid(); }
		void contig_reverse()
		{
			if (!is_gap())
			{
				if (_type == _cs_forward)
					_type = _cs_reverse;
				else if (_type == _cs_reverse)
					_type = _cs_forward;
//				std::cout << "contig_reverse " << _contig.cs->name() << " _type==_cs_forward = " << (_type == _cs_forward) << std::endl;
			}
		}
		void reverse()
		{
			contig_reverse();
			if (!is_gap())
				_contig.bottom_forward = !_contig.bottom_forward;
		}
		void set_top(std::pair<mekano::Supercontig *, bool> p)
		{
			if (!is_gap())
			{
				_contig.top = p.first;
				_contig.top_forward = p.second;
//				std::cout << "setting top " << _contig.cs->name() << " to " << _contig.top->name() << ' ' << _contig.top_forward << std::endl;
			}
		}
		bool top_forward() const { assert(!is_gap()); return _contig.top_forward; }
		bool bottom_forward() const { assert(!is_gap()); return _contig.bottom_forward; }
	};
	typedef mekano::Supercontig * (ConseqData::*get_sctg_t)(bool *) const;
	typedef ConseqData ConSeqData; // deprecated
	typedef ConseqData ContigData; // deprecated
	typedef ConSeqData Range; // deprecated
	class SupercontigRange
	{
		enum { _no_sctg, _forward, _reverse } _type;
		typedef mekano::Supercontig * sctg_t;
		sctg_t _sctg;
		unsigned _bases;
		unsigned _holes;
	public:
		bool is_sctg() const { return _type != _no_sctg; }
		unsigned hlength(unsigned hole_size) const
		{
			return _bases + _holes * hole_size;
		}
		unsigned bases() const { return _bases; }
		unsigned holes() const { return _holes; }
		unsigned length(unsigned hole_size) const { return hlength(hole_size); }
//		mekano::Supercontig * sctg() { return _sctg; }
		mekano::Supercontig * sctg() const {  return is_sctg() ? _sctg : 0; }
		bool forward() const { assert(is_sctg()); return _type == _forward; }
		SupercontigRange(mekano::Supercontig * sctg, unsigned b, unsigned g, bool f) :
			_type((!sctg) ? _no_sctg : f ? _forward : _reverse),
			_sctg(sctg),
			_bases(b),
			_holes(g)
		{
		}
		SupercontigRange(mekano::Supercontig * sctg, unsigned b, unsigned g) : // gap
			_type(_no_sctg),
			_sctg(sctg),
			_bases(b),
			_holes(g)
		{}
		operator eyedb::Oid() const { assert(_sctg); return _sctg->getOid(); }
		operator sctg_t() const { assert(_sctg); return _sctg; }
		friend std::ostream & operator<<(std::ostream &, SupercontigRange const &);
	};

	typedef std::vector<ConSeqData> ConSeqContainer;
	typedef ConSeqContainer ContigContainer; // deprecated
	typedef ConSeqContainer const & (SupercontigData::*get_conseqdata_t)() const;
	typedef std::vector<SupercontigRange> SupercontigContainer;
	typedef SupercontigContainer sctg_ranges_t;

	SupercontigData(ObjectHandle<mekano::Supercontig> const & top) :
		_supercontig(top),
		_clones_initialized(false),
		_contig_data_initialized(false),
		_conseq_data_initialized(false),
		_top_sctgs_initialized(false),
		_bottom_sctgs_initialized(false),
		_reads_initialized(false),
		_bases(0),
		_holes(0)
	{
//		std::cout << "SupercontigData " << top->name() << std::endl;
	}
	ContigContainer const & contigs() const
	{
		while (!_contig_data_initialized)
			make_contig_data();
		return _contig_data;
	}
	ConSeqContainer const & continuous_sequences() const
	{
		while (!_conseq_data_initialized)
			make_conseq_data();
		return _conseq_data;
	}
	ConSeqContainer const & conseqs() const { return continuous_sequences(); }

	sctg_ranges_t const & top_sctgs() const
	{
		while (!_top_sctgs_initialized)
			make_top_sctgs();
		return _top_sctgs;
	}
	sctg_ranges_t const & bottom_sctgs() const
	{
		while (!_bottom_sctgs_initialized)
			make_bottom_sctgs();
		return _bottom_sctgs;
	}

	typedef std::vector<eyedb::Oid> clones_t;
	clones_t const & clones() const
	{
		while (!_clones_initialized)
			initialize_clones();
		return _clones;
	}
	
	PositionnedReadContainer const & reads() const
	{
		while (!_reads_initialized)
			initialize_reads();
		return _reads;
	}

	unsigned nb_cseq() { while (!_conseq_data_initialized) make_conseq_data(); return _nb_cseq; }
	unsigned nb_ctg() { while (!_contig_data_initialized) make_contig_data(); return _nb_ctg; }
	unsigned length(unsigned hole_size) const
	{
		while (!_conseq_data_initialized)
			make_conseq_data();
		assert(_bases || _holes);
		return _bases + _holes * hole_size;
	}
		
	ObjectHandle<mekano::Supercontig> const & sctg() const { return _supercontig; }
	mekano::Supercontig const * c_sctg() const { return _supercontig; }
	ObjectHandle<mekano::Supercontig> & sctg() { return _supercontig; }
private:
//	SupercontigData(SupercontigData const &); // copy constructor
//	SupercontigData & operator=(SupercontigData const &); // copy assignment
	ObjectHandle<mekano::Supercontig> _supercontig;
	mutable bool _clones_initialized;
	mutable clones_t _clones;
	mutable ContigContainer _contig_data;
	mutable bool _contig_data_initialized;
	mutable bool _conseq_data_initialized;
	mutable bool _top_sctgs_initialized;
	mutable bool _bottom_sctgs_initialized;
	mutable bool _reads_initialized;
	mutable ConSeqContainer _conseq_data;
	mutable SupercontigContainer _bottom_sctgs;
	mutable SupercontigContainer _top_sctgs;
	mutable PositionnedReadContainer _reads;
	mutable unsigned _nb_cseq;
	mutable unsigned _nb_ctg;
	mutable unsigned _bases;
	mutable unsigned _holes;

	void make_conseq_data() const;
	void make_contig_data() const;
	void make_top_sctgs() const;
	void make_bottom_sctgs() const;
	void initialize_clones() const;
	void initialize_reads() const;

	friend std::ostream & operator<<(std::ostream &, SupercontigData const &);
};

std::ostream &
operator<<(std::ostream & s, SupercontigData::ConseqData const & d);

inline bool
operator==(SupercontigData::ConseqData const & o1, eyedb::Oid const & o2)
{
	return eyedb::Oid(o1) == o2;
}

inline bool
operator<(SupercontigData const & sctg1, SupercontigData const & sctg2)
{
	return sctg1.sctg().oid() < sctg2.sctg().oid();
}
#endif
