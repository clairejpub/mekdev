/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: joins.cc 771 2007-05-31 09:42:06Z stuart $
 */
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <iomanip>

#include "mekano.h"
#include <utils/templates.h>
#include <utils/iterators.h>
#include <utils/eyedb.h>
#include <utils/sctg_data.h>

#include "common.h"

inline bool
set_contains(eyedb::CollSet const * set, eyedb::Oid const & o)
{
	eyedb::Bool found;
	set->isIn(o, found);
	return found;
}

template <class Inserter>
class SelectPeersInStage : public std::unary_function<mekano::Supercontig *, void>
{
	Inserter _ins;
	ObjectHandle<mekano::Stage> _stage;
public:
	SelectPeersInStage(Inserter ins, ObjectHandle<mekano::Stage> const & stage) : _ins(ins), _stage(stage) {}
	void operator()(argument_type sctg)
	{
		mekano::LinkCounts * cl = sctg->clone_links();
		std::for_each(begin_link_counts(cl), end_link_counts(cl), *this);
	}
	void operator()(mekano::LinkCount * cl)
	{
		for (int i = _stage->groups_cnt(); --i >= 0; )
			if (set_contains(_stage->groups(i)->sctgs(), cl->sctg_oid()))
				*_ins++ = cl->sctg();
	}
};
template <class Inserter>
SelectPeersInStage<Inserter>
select_peers_in_stage(Inserter ins, ObjectHandle<mekano::Stage> const & stage)
{
	return SelectPeersInStage<Inserter>(ins, stage);
}

class if_links_add : public std::unary_function<mekano::Supercontig *, void>
{
	SupercontigData * data;
	ObjectHandle<mekano::Calculated> counts;
public:
	if_links_add
	(
		SupercontigData * d,
		ObjectHandle<mekano::Calculated> const & s
	) : data(d), counts(s) {}
	result_type operator()(argument_type sctg)
	{
		SupercontigData peer(sctg);
		if (unsigned n = set_intersection(data->clones().begin(), data->clones().end(), peer.clones().begin(), peer.clones().end(), counter(peer.clones())).n())
		{
			add_to_clone_link_counts(counts, sctg, data->sctg(), n);
			sctg->eyedb::Struct::store();
		}
	}
};

class remove_from : public std::unary_function<mekano::Supercontig *, void>
{
	ObjectHandle<mekano::Stage> _stage;
	std::set<eyedb::Oid> * const _deleted;
public:
	remove_from(ObjectHandle<mekano::Stage> stage, std::set<eyedb::Oid> * const deleted) :
		_stage(stage),
		_deleted(deleted)
	{}
	result_type operator()(argument_type const & sctg)
	{
		if (_deleted->insert(sctg->getOid()).second)
			suppress_from_group(_stage->groups(0), sctg);
	}
};
		
inline bool
equal_to_last_sctg_name(std::string const & name, mekano::Supercontig * scaf)
{
	if (int i = scaf->regions_cnt())
		if (mekano::Supercontig * sctg = scaf->regions(i - 1)->sequence()->asSupercontig())
			return name == sctg->name();
	return false;
}

static void
finish_inserted_sctg(mekano::Supercontig * scaf, std::string const & last_cseq, bool forward, int * gap = 0)
{
	if (int i = scaf->regions_cnt())
	{
		mekano::Region * region =  scaf->regions(i - 1);
		if (gap)
		{
			region->gap_after(*gap);
			assert(*gap >= -1);
		}
		if (region->sequence()->asSupercontig())
			region->last_oid(lookup(scaf->getDatabase(), "ContinuousSequence", last_cseq));
	}
	if (gap)
		*gap = 0;
}

namespace
{
	template <class T>
	struct print_name : public std::iterator<std::output_iterator_tag, void, void, void, void>
	{
		print_name & operator*() { return *this; }
		print_name & operator++() { return *this; }
		print_name & operator++(int) { return *this; }
		print_name & operator=(T * const & s)
		{
			std::cout << s->name() << ' ';
			return *this;
		}
	};

	struct Struct_lt : std::binary_function<eyedb::Struct const *, eyedb::Struct const *, bool>
	{
		result_type operator()(first_argument_type s1, second_argument_type s2) const
		{
			return s1->getOid() < s2->getOid();
		}
	};

	template <typename Cmp>
	struct my_eq : std::binary_function<typename Cmp::first_argument_type, typename Cmp::second_argument_type, bool>
	{
		bool operator()(typename Cmp::first_argument_type s1, typename Cmp::second_argument_type s2) const
		{
			return !Cmp()(s1, s2) && !Cmp()(s2, s1);
		}
	};

	template <typename Vector, typename WeakOrdering>
	inline void
	make_set(Vector * v, WeakOrdering cmp)
	{
		sort(v->begin(), v->end(), cmp);
		v->erase(unique(v->begin(), v->end(), my_eq<WeakOrdering>()), v->end());
	}
}

static void
cleanup
(
	TransactionControl * trans,
	ObjectHandle<mekano::Supercontig> const & scaf,
	ObjectHandle<mekano::Stage> const & stage,
	mekano::Group * group,
	ObjectHandle<mekano::Calculated> const & counts,
	std::string const & last_cseq,
	unsigned hole_size,
	bool forward,
	std::vector<ReadContigInfo> * top_level,
	std::set<eyedb::Oid> * const deleted
)
{
	if (scaf)
	{
		finish_inserted_sctg(scaf, last_cseq, forward);
//		std::cout << *scaf << std::endl;
//		std::cout << "<< " << scaf->name() << ' ' << std::endl;
		SupercontigData data(scaf);

		check_read_config_info(*top_level, data);
		typedef std::vector<mekano::Supercontig *> set_t;
		Struct_lt set_ordering;
		set_t parents;
		copy_if(data.top_sctgs().begin(), data.top_sctgs().end(), back_inserter(parents), std::mem_fun_ref(&SupercontigData::SupercontigRange::is_sctg));
		make_set(&parents, set_ordering);

		set_t peers_of_parents;
		std::for_each
		(
			parents.begin(),
			parents.end(),
			select_peers_in_stage(back_inserter(peers_of_parents), stage)
		);
		make_set(&peers_of_parents, set_ordering);

		set_t possible_peers;
		set_difference(peers_of_parents.begin(), peers_of_parents.end(), parents.begin(), parents.end(), back_inserter(possible_peers), set_ordering);

		scaf.store();
		std::for_each(possible_peers.begin(), possible_peers.end(), if_links_add(&data, counts));

		for_each(parents.begin(), parents.end(), remove_from(stage, deleted));

		finish_sctg(&data, stage, group, hole_size);
		trans->checkpoint(stage);
	}
	top_level->clear();
}

namespace
{
	static bool
	get_forward(std::string const & s)
	{
		if (s == "F")
			return true;
		if (s == "R")
			return false;
		throw MyError("bad forward indicator " + s);
	}
	std::string const what("adding Supercontigs");
}

static void
read_scafs
(
	Ifstream & f,
	ObjectHandle<mekano::Stage> const  & stage,
	mekano::Group * group,
	ObjectHandle<mekano::Calculated> const  & counts,
	int const hole_size,
	unsigned transaction_size,
	std::set<eyedb::Oid> * const deleted
)
{
	std::clog << myname << ": " + what + " from " << f.fname() << std::endl;
	f.unsetf(std::ios_base::skipws);
	eyedb::Database * const db = stage.database();
	TransactionControl trans(db, transaction_size);
	char c;
	ObjectHandle<mekano::Supercontig> scaf;
	int last_gap = -10; // should not be used
	std::string last_cseq;
	bool oforward = 3; // should never be used, shutup gcc
	std::vector<ReadContigInfo> top_level;
	while (f >> c)
	{
		if (c == '>')
		{
			cleanup(&trans, scaf, stage, group, counts, last_cseq, hole_size, oforward, &top_level, deleted);
			std::string name;
			if (!(f >> name))
				throw MyError("read of supercontig name failed");
			scaf.replace(db);
			scaf->name(name);
		}
		else if (c ==  'G')
		{
			if (!(f >> skip("AP") >> skipspaces))
				throw MyError("gap header read failed");
			unsigned size;
			if (f.peek() == '?')
			{
				f.get();
				last_gap = -1;
				top_level.push_back(ReadContigInfo());
			}
			else if (!(f >> size))
				throw MyError("gap size read failed");
			else
				top_level.push_back(ReadContigInfo(last_gap = std::max(unsigned(minimum_gap_size), size)));
		}
		else
		{
			std::string cseq;
			unsigned start;
			unsigned stop;
			std::string cforward;
			std::string from_name;
			if (!(f >> cseq >> skipspaces >> start >> skipspaces >> stop >> skipspaces >> from_name >> skipspaces >> cforward))
				throw MyError("read of contig information failed");
			cseq = c + cseq;
			if (from_name != "-")
			{
				bool const forward = get_forward(cforward);
				if (!equal_to_last_sctg_name(from_name, scaf))
				{
					finish_inserted_sctg(scaf, last_cseq, oforward, &last_gap);

					ObjectHandle<mekano::Region> region(db);
					scaf->regions(scaf->regions_cnt(), region);
					region->sequence_oid(lookup(db, "Supercontig", from_name));
					region->first_oid(lookup(db, "ContinuousSequence", cseq));
//					region->start_on_first(std::min(start, stop));
					region->start_on_first(start);
					oforward = forward;
				}
				last_cseq = cseq;
				mekano::Region * region = scaf->regions(scaf->regions_cnt() - 1);
//				region->stop_on_last(std::max(start, stop));
				region->stop_on_last(stop);
				if (forward != oforward)
					throw MyError("a region must be either all forward or all reverse");
				top_level.push_back(ReadContigInfo(cseq, start, stop, from_name, forward));
			}
			else	
			{
				if (cforward != "-")
					throw MyError("expected a '-' for the direction");
				finish_inserted_sctg(scaf, last_cseq, oforward, &last_gap);

				ObjectHandle<mekano::Region> region(db);
// RX?				if (stop <= start) throw MyError("start must be greater than stop: " + cseq);
				region->start_on_first(start);
				region->stop_on_last(stop);
				scaf->regions(scaf->regions_cnt(), region);
				region->sequence_oid(lookup(db, "ContinuousSequence", cseq));
				region->first_oid(region->sequence_oid());
				region->last_oid(region->sequence_oid());
				top_level.push_back(ReadContigInfo(cseq, start, stop));
				oforward = true;
			}
		}
		if (!(f >> skipspaces))
			throw MyError("skipspaces failed");
		if (!f.get(c))
			throw MyError("failed to read eol");
		if (c != '\n')
			throw MyError(std::string("expected a newline, found: ") + c + "(" + to_string(int(c)) + ")");
	}
	cleanup(&trans, scaf, stage, group, counts, last_cseq, hole_size, oforward, &top_level, deleted);
	if (!f.eof())
		throw MyError("I/O error");
	stage.store();
	trans.ok();
}

void
insert_supercontigs
(
	Ifstream * f,
	ObjectHandle<mekano::Stage> stage,
	mekano::Group * group,
	ObjectHandle<mekano::Calculated> counts,
	int unsized_gap,
	int transaction_size
)
{
	try
	{
		std::set<eyedb::Oid> deleted;
		read_scafs(*f, stage, group, counts, unsized_gap, transaction_size, &deleted);
	}
	catch (eyedb::Exception & e)
	{
		throw MyError(f->tag() + ": eyedb::Exception " + what + ": " + to_string(e));
	}
	catch (MyError & e)
	{
		throw MyError(f->tag() + ": " + what + ": " + e.toString());
	}
}

namespace
{
	template <typename In, typename Op>
	mekano::Group *
	finder(In begin, In end, Op op)
	{
		In p = std::find_if(begin, end, op);
		if (p == end)
			return 0;
		return *p;
	}

	struct same_group_name : std::binary_function<mekano::Group const *, eyedb::Oid, bool>
	{
		result_type operator()(first_argument_type g, second_argument_type const & o) const
		{
			return g->name_oid() == o;
		}
	};
}

void
joins
(
	ObjectHandle<mekano::Stage> stage,
	ObjectHandle<mekano::Calculated> counts,
	int hole_size,
	Ifstream * f,
	std::string const & group_name
)
{
	mekano::Group * group;
	{
		TransactionControl trans(counts.database());
		ObjectHandle<mekano::GroupName> gn(counts.database(), group_name);
		if (!gn)
			MyError("no groups called " + group_name);
		group = finder(begin_groups(stage), end_groups(stage), std::bind2nd(same_group_name(), gn.oid()));
		if (!group)
			throw MyError("no group " + group_name + " in Stage " + to_string(stage->id()));
	}

	insert_supercontigs(f, stage, group, counts, hole_size, 10);
}


#ifdef NEED_MAIN
char const * myname;

int
main(int argc, char *argv[])
{
	if ((myname = strrchr(argv[0], '/')) == 0 || *++myname == 0)
		myname = argv[0];
	Statistics stats;
	mekano::mekano initializer(argc, argv);
	
	eyedb::Database::OpenFlag mode = eyedb::Database::DBRW;
	bool remove_existing = false;
	char const * change_dir;
	unsigned transaction_size = 10;
	argv += options(argc, argv, &mode, &remove_existing, &change_dir, 0, &transaction_size);
	
	std::string const db_name(arg(argv++, "database name"));

	std::istringstream stage_string(arg(argv++, "Stage number"));
	unsigned stage_id;
	if (!(stage_string >> stage_id) || stage_string.get() != EOF)
		usage("bad stage id");

	std::string group_name(arg(argv++, "group name"));

//	if (remove_existing)
//		return fail(std::clog << myname << ": cannot use -R option here (must recreate the Stage)");
	
	eyedb::Exception::setMode(eyedb::Exception::ExceptionMode);
	eyedb::Connection conn;
	mekano::mekanoDatabase db(string2eyedb(db_name));
	try
	{
		conn.open();
		db.open(&conn, mode);
	}
	catch (eyedb::Exception & e)
	{
		return fail(std::clog << myname << ": open " << db_name, e);
	}
	stats.time(db);
	ObjectHandle<mekano::Stage> stage;
	ObjectHandle<mekano::CloneLinkCounts> counts;
	ObjectHandle<mekano::Parameters> parameters;
	mekano::Group * group;
	try
	{
		TransactionControl trans(&db);
		if (!stage.replace(&db, stage_id))
			return fail(std::clog << myname << ": unknown stage: " << stage_id);
		if (!counts.replace(&db, select_one(&db, counts.class_name())))
			return fail(std::clog << myname << ": no " << counts.class_name() << " in db");
		if (!parameters.replace(&db, select_one(&db, parameters.class_name())))
			return fail(std::clog << myname << ": no " << parameters.class_name() << " in db");

		ObjectHandle<mekano::GroupName> gn(&db, group_name);
		if (!gn)
			return fail(std::clog << myname << ": no groups called " << group_name << " exist");
		group = finder(begin_groups(stage), end_groups(stage), std::bind2nd(same_group_name(), gn.oid()));
		if (!group)
			return fail(std::clog << myname << ": no group " << group_name << " in Stage " << stage_id);
		if (remove_existing && stage->groups(0)->sctgs_cnt())
		{
			remove_query(&db, "contents(" + to_string(stage.oid()) + ".groups[0].sctgs)");
			stage->groups(0)->sctgs()->empty();
			/*	Should delete the sets not just empty the arrays of sets
			*/
			std::cout << myname << ": removing existing in " << stage->id() << std::endl;
			stage->groups(0)->length_counts()->empty();
			stage->groups(0)->ctg_counts()->empty();
			stage->groups(0)->cseq_counts()->empty();
			stage.store();
			trans.ok();
			std::cout << myname << ": removed existing in " << stage->id() << std::endl;
		}
	}
	catch (eyedb::Exception & e)
	{
		return fail(std::clog << myname << ": finding Stage " << stage_id, e);
	}
	catch (MyError & e)
	{
		return fail(std::clog << myname << ": initializing " <<db_name , e);
	}
	std::set<eyedb::Oid> deleted;
	while (*argv)
	{
		Ifstream supercontigs(*argv++);
		core_directory(change_dir, *argv);
		try
		{
			read_scafs(supercontigs, stage, group, counts, parameters, transaction_size, &deleted);
		}
		catch (eyedb::Exception & e)
		{
			return fail(std::clog << myname << ": " << supercontigs.tag() << ": reading supercontigs", e);
		}
		catch (MyError & e)
		{
			return fail(std::clog << myname << ": " << supercontigs.tag() << ": reading supercontigs", e);
		}
	}
	return 0;
}
#endif
