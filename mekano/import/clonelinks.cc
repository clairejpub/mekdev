/*	$Id: clonelinks.cc 797 2007-06-06 13:36:20Z stuart $
 *	Stuart Pook, Genescope, 2006 (enscript -Ecpp $%)
 */
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

#include "mekano.h"
#include "common.h"
#include <utils/templates.h>
#include <utils/eyedb.h>
#include <utils/sctg_data.h>
#include <utils/utilities.h>
#include <boost/lambda/bind.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/construct.hpp>

namespace
{

typedef std::map<std::pair<mekano::Supercontig *, mekano::Supercontig *>, int> matches_t;
typedef std::vector<std::pair<eyedb::Oid, mekano::Supercontig *> > cseq_mapping_t;
typedef std::vector<std::pair<eyedb::Oid, eyedb::Oid> > sides_t;

struct add_match
{
	typedef void result_type;
	result_type operator()
	(
		mekano::Supercontig * sctg0,
		mekano::Supercontig * sctg1,
		matches_t * matches
	) const
	{
		if (sctg0->eyedb::Struct::getOid() != sctg1->eyedb::Struct::getOid())
		{
			matches_t::key_type v(sctg0, sctg1);
			using std::swap;
			if (v.second->eyedb::Struct::getOid() < v.first->eyedb::Struct::getOid())
				swap(v.first, v.second);
			matches->insert(std::make_pair(v, 0)).first->second++;
		}
	}
};

struct sides_cmp
{
	bool operator()(eyedb::Oid const & x, eyedb::Oid const & y) const
	{
		return x < y;
	}
	bool operator()(sides_t::value_type const & x, eyedb::Oid const & y) const
	{
		return (*this)(x.first, y);
	}
	bool operator()(eyedb::Oid const & x, sides_t::value_type const & y) const
	{
		return (*this)(x, y.first);
	}
	bool operator()(sides_t::value_type const & x, sides_t::value_type const & y) const
	{
		return (*this)(x.first, y.first);
	}
};

struct mapping_cmp
{
	bool operator()(eyedb::Oid const & x, eyedb::Oid const & y) const
	{
		return x < y;
	}
	bool operator()(cseq_mapping_t::value_type const & x, eyedb::Oid const & y) const
	{
		return (*this)(x.first, y);
	}
	bool operator()(eyedb::Oid const & x, cseq_mapping_t::value_type const & y) const
	{
		return (*this)(x, y.first);
	}
	bool operator()(cseq_mapping_t::value_type const & x, cseq_mapping_t::value_type const & y) const
	{
		return (*this)(x.first, y.first);
	}
};

struct make_mapping_from_ctg
{
	typedef void result_type;
	result_type operator()
	(
		SupercontigData::ConSeqData const & ctg,
		mekano::Supercontig * sctg,
		sides_t const * all_sides,
		std::back_insert_iterator<cseq_mapping_t> mapping
	) const
	{
		if (ctg.is_conseq())
		{
			std::pair<sides_t::const_iterator, sides_t::const_iterator> sides = equal_range
			(
				all_sides->begin(),
				all_sides->end(),
				ctg.conseq()->getOid(), 
				sides_cmp()
			);
			using namespace boost::lambda;
			transform
			(
				sides.first,
				sides.second,
				mapping,
				bind(constructor<cseq_mapping_t::value_type>(), bind(&sides_t::value_type::second, _1), sctg)
			);
		}
	};
};

struct make_mapping_from_sctg
{
	typedef void result_type;
	result_type operator()
	(
		SupercontigData const & sctg,
		sides_t const * sides,
		std::back_insert_iterator<cseq_mapping_t> mapping
	) const
	{
		using namespace boost::lambda;
		sctg.continuous_sequences().begin();
		for_each
		(
			sctg.continuous_sequences().begin(),
			sctg.continuous_sequences().end(),
			bind(make_mapping_from_ctg(), _1, sctg.sctg(), sides, mapping)
		);
	}
};

struct matches_in_ctg
{
	typedef void result_type;
	result_type operator()
	(
		SupercontigData::ConSeqData const & ctg,
		mekano::Supercontig * sctg,
		cseq_mapping_t const * mapping,
		matches_t * matches
	) const
	{
		if (ctg.is_conseq())
		{
			using namespace boost::lambda;
			std::pair<cseq_mapping_t::const_iterator, cseq_mapping_t::const_iterator> r = equal_range(mapping->begin(), mapping->end(), ctg.conseq()->getOid(), mapping_cmp());
			for_each(r.first, r.second, bind(add_match(), bind(&cseq_mapping_t::value_type::second, _1), sctg, matches));
		}
	};
};

struct matches_in_sctg
{
	typedef void result_type;
	result_type operator()
	(
		SupercontigData const & sctg,
		cseq_mapping_t const * mapping,
		matches_t * matches
	) const
	{
		using namespace boost::lambda;
		sctg.continuous_sequences().begin();
		for_each
		(
			sctg.continuous_sequences().begin(),
			sctg.continuous_sequences().end(),
			bind(matches_in_ctg(), _1, sctg.sctg(), mapping, matches)
		);
	}
};

struct store_matches
{
	typedef void result_type;
	result_type operator()
	(
		matches_t::value_type const & match,
		mekano::Calculated * counts,
		TransactionControl * trans
	) const
	{
		add_to_matches_counts(counts, match.first.first, match.first.second, match.second);
		match.first.first->eyedb::Struct::store();
		match.first.second->eyedb::Struct::store();
		trans->checkpoint(counts);
	}
};

void
copy_match_sides_sequence(eyedb::Database * const db, sides_t * map)
{
	Performance perf("loading Matches into memory");
	TransactionControl trans(db);
	std::string q = "select struct(a:m.sides[0].sequence, b:m.sides[1].sequence) from Match m";
	eyedb::OidArray oids;
	eyedb::OQL(db, q.c_str()).execute(oids);
	map->reserve(oids.getCount());
	for (int i = 0; i < oids.getCount(); i++)
	{
		i++;
		assert(i < oids.getCount());
		map->push_back(sides_t::value_type(oids[i], oids[i - 1]));
		map->push_back(sides_t::value_type(oids[i - 1], oids[i]));
	}
	sort(map->begin(), map->end(), sides_cmp());
}

void
calculate_matches(mekano::Calculated * counts, std::vector<SupercontigData> * scafs)
{
	std::clog << myname << ": calculating counts for matches of " << scafs->size() << " sctgs" << std::endl;
	std::string func = "calculate_matches: ";
	sides_t sides;
	eyedb::Database * const db = counts->eyedb::Struct::getDatabase();
	copy_match_sides_sequence(db, &sides);
	if (!sides.empty())
	{
		using namespace boost::lambda;
		cseq_mapping_t mapping;
		mapping.reserve(sides.size());
		{
			TransactionControl trans(db);
			Performance perf2(func + "make_mapping_from_sctg");
			for_each(scafs->begin(), scafs->end(), bind(make_mapping_from_sctg(), _1, &sides, back_inserter(mapping)));
		}
		{
			Performance perf2(func + "sort mapping");
			sort(mapping.begin(), mapping.end(), mapping_cmp());
		}
		matches_t matches;
		{
			TransactionControl trans(db);
			Performance perf2(func + "matches_in_sctg");
			for_each(scafs->begin(), scafs->end(), bind(matches_in_sctg(), _1, &mapping, &matches));
		}
		std::clog << myname << ": " << mapping.size() << " mappings, " << matches.size() << " matches" << std::endl;
		{
			TransactionControl trans(db, 400);
			for_each(matches.begin(), matches.end(), bind(store_matches(), _1, counts, &trans));
			counts->eyedb::Struct::store();
			trans.ok();
		}
	}
}

struct compare
{
	typedef void result_type;
	result_type operator()
	(
		ObjectHandle<mekano::Calculated> const & counts,
		TransactionControl * trans,
		SupercontigData const & s1,
		SupercontigData const & s2
	) const
	{
		if (unsigned n = set_intersection(s1.clones().begin(), s1.clones().end(), s2.clones().begin(), s2.clones().end(), counter(s1.clones())).n())
		{
			add_to_clone_link_counts(counts, s1.sctg(), s2.sctg(), n);
			s1.sctg().store();
			s2.sctg().store();
			trans->checkpoint(counts);
		}
	}
};

} //namespace

void
clonelinks(ObjectHandle<mekano::Calculated> counts, mekano::Group * group, bool do_clones)
{
	std::vector<SupercontigData> scafs;
	eyedb::Database * const db = counts.database();
	{
		Performance perf("clonelinks: loading supercontigs");
		TransactionControl trans(db);
		if (unsigned const n = group->sctgs_cnt())
		{
			eyedb::OidArray oids;
			group->sctgs()->getElements(oids);
			scafs.reserve(n);
			for (unsigned i = 0; i < n; i++)
				scafs.push_back(ObjectHandle<mekano::Supercontig>(db, oids[i]));
		}
	}
	if (do_clones)
	{
		std::clog << myname << ": calculating counts for clone links" << std::endl;
		TransactionControl trans(db, 200);
		using namespace boost::lambda;
		product(scafs.begin(), scafs.end(), bind(compare(), counts, &trans, _1, _2));
		counts.store();
		trans.ok();
	}
	calculate_matches(counts, &scafs);
}
