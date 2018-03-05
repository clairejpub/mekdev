/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: common.cc 797 2007-06-06 13:36:20Z stuart $
 */
#include <errno.h>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <ctype.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>

#include <utils/sctg_data.h>
#include "common.h"
#include <boost/utility.hpp>

extern char const * myname;

eyedb::Oid
lookup(eyedb::Database * const db, std::string const & type, int id, bool force)
{
	std::string query = "select " + type + ".id==" + to_string(id);
	eyedb::OidArray oids;
	eyedb::OQL(db, query.c_str()).execute(oids);
	if (oids.getCount() > 1)
		throw MyError("multiple objects from: " + query);
	if (oids.getCount() == 0)
		if (force)
			throw MyError("zero objects from: " + query);
		else
			return eyedb::Oid::nullOid;
	return oids[0];
}

eyedb::Oid
lookup(eyedb::Database * const db, std::string const & type, std::string const & name, bool force)
{
	std::string query = "select " + type + ".name == \"" + name + '"';
	eyedb::OidArray oids;
	eyedb::OQL(db, query.c_str()).execute(oids);
	if (oids.getCount() > 1)
		throw MyError("multiple objects from: " + query);
	if (oids.getCount() == 0)
		if (force)
			throw MyError("zero objects from: " + query);
		else
			return eyedb::Oid::nullOid;
	return oids[0];
}

void
fcouldnot(char const * what, char const * which)
{
	extern char const * myname;
	std::clog << myname << ": could not " << what;
	if (which)
		std::clog << ' ' << which;
	std::clog << ": " << strerror(errno) << std::endl;
	exit(1);
}

namespace
{
	double
	seconds(struct timeval const & tv)
	{
		return tv.tv_sec +tv.tv_usec / 1000000.0;
	}
	double
	time_diff(struct timeval const & start)
	{
		struct timeval end;
		if (gettimeofday(&end, 0) != 0)
			fcouldnot("gettimeofday");
		return seconds(end) - seconds(start);
	}
}

void
remove_query(eyedb::Database * const db, std::string const & loop)
{
	struct timeval start;
	if (gettimeofday(&start, 0) != 0)
		fcouldnot("gettimeofday");
	std::string query = "begin(); c:=\"?\"; n:=0; i:=0; for (x in (" + loop;
	query += ")) { c := x.class.name; if (i++ == 1000) { i:=0; commit(); begin();} delete x; n++;}; commit(); list(n,c);";
	eyedb::Value result;
	eyedb::OQL(db, string2eyedb(query)).execute(result);
	std::clog << myname << ": removed " << result << " in "
		<< std::setprecision(2) << std::fixed << time_diff(start) << " seconds" << std::endl;
}

void
remove(eyedb::Database * const db, std::string const & class_name, int id)
{
	std::string query = "(select " + class_name;
	if (id >= 0)
		query += ".id=" + to_string(id);
	query += ")";
	remove_query(db, query);
}

ObjectHandle<mekano::Stage>
make_stage(eyedb::Database * const db, unsigned id)
{
	ObjectHandle<mekano::Stage> stage(db);
	stage->id(id);
	ObjectHandle<mekano::Group> group(db);
	stage->groups(0 , group);
	stage.store();
	return stage;
}

// ugly hack: http://gcc.gnu.org/ml/gcc-help/2003-02/msg00145.html
namespace std
{
	template <typename W>
	std::ostream & operator<<(std::ostream & o, std::pair<std::type_info const * const, W> const & v)
	{
		return o << '(' << v.first->name() << ", " << v.second << ')';
	}
}

typedef std::map<std::type_info const *, unsigned> counts_t;

namespace
{
	void add_to_type(counts_t * map, eyedb::Object * o)
	{
		(*map)[&typeid(*o)]++;
	}
}


Statistics::~Statistics()
{
	assert(_start.tv_sec || _start.tv_usec);
	double const sec = time_diff(_start);
	std::vector<eyedb::Object *> unfreed;
	observer.getObjects(unfreed);
	std::clog << myname << ": import took " << std::fixed << std::setprecision(0) << sec / 60
		<< " minutes (" << std::setprecision(2) << sec << "s) in mode "
		<< ((_mode & eyedb::_DBOpenLocal) ? "local" : "remote")
		<< ", " << unfreed.size() << " unfreed objects"
		<< std::endl;
	struct rusage usage;
	if (getrusage(RUSAGE_SELF, &usage) != -1)
	{
		std::clog << myname << ": "
			<< std::setprecision(0) << seconds(usage.ru_stime) / 60 << " minutes ("
			<< std::setprecision(2) << seconds(usage.ru_stime) << "s) system & "
			<< std::setprecision(0) << seconds(usage.ru_utime) / 60 << " minutes ("
			<< std::setprecision(2) << seconds(usage.ru_utime) << "s) user cpu time" << std::endl;
	}
	counts_t counts;
	for_each(unfreed.begin(), unfreed.end(), std::bind1st(std::ptr_fun(add_to_type), &counts));
//#define ERIC_VERBOSE
#ifdef ERIC_VERBOSE
	std::vector<eyedb::Object *>::iterator const end = unfreed.end();
	for (std::vector<eyedb::Object *>::iterator begin = unfreed.begin(); begin != end; begin++)
	{
		const std::type_info &t = typeid(**begin);
		std::clog << (void *)*begin << ' ' << t.name() << ' ' << (*begin)->getRefCount() << std::endl;
	}
#endif
	std::copy(counts.begin(), counts.end(), std::ostream_iterator<counts_t::value_type>(std::clog, "\n"));
}

std::istream &
skipspaces(std::istream & s)
{
	while (s && (s.peek() == ' ' || s.peek() == '\t'))
	{
		int i = s.get();
		if (i != ' ' && i != '\t')
			std::cerr << "skipspaces(std::istream& s) read a " << i << std::endl;
	}
         return s;
}

std::istream & skip::operator()(std::istream & s) const
{
	char c;
	for (char const * t = str; *t && s.get(c); )
		if (*t++ != c)
		{
			s.setstate(std::ios_base::failbit);
			return s;
		}
		return s;
}

inline bool
lower_case(std::string * s, char const * t)
{
	int len = strlen(t);
	if (s->compare(0, len, t) == 0)
	{
		std::transform(s->begin(), s->begin() + len, s->begin(), std::ptr_fun(tolower));
		return true;
	}
	return false;
}

int
fail(std::ostream & o)
{
	o << std::endl;
	exit(1);
	return 1;
}

int
fail(std::ostream & o, eyedb::Exception const & e)
{
	o << ": eyedb::Exception: " << e << std::endl;
	exit(1);
	return 1;
}

int
fail(std::ostream & o, MyError const & e)
{
	o << ": MyError: " << e << std::endl;
	exit(1);
	return 1;
}

namespace
{
	static void
	mk_LinkCount(mekano::Supercontig * sctg1, mekano::LinkCounts * sctg2, unsigned n)
	{
		ObjectHandle<mekano::LinkCount> clc(sctg1->eyedb::Struct::getDatabase());
#if 1
		/*	If I use clc->sctg(sctg1), when running store_clonelinks I get a 
			eyedb::Exception: attribute clone_links of object 0x69aac0 of
			class Supercontig has been damaged during a prematured release
		*/
		clc->sctg_oid(sctg1->eyedb::Struct::getOid());
#else
		clc->sctg(sctg1);
#endif
		clc->n(n);
		sctg2->link_counts(sctg2->link_counts_cnt(), clc);
	}
}

mekano::SupercontigSet *
get_sctg_set_from_array(eyedb::CollArray const * const array, int n)
{
	eyedb::Object * o = 0;
	array->retrieveAt(n, o);
	if (o)
		if (mekano::Root * r = mekano::mekanoDatabase::asRoot(o))
			if (mekano::SupercontigSet * ss = r->asSupercontigSet())
				return ss;
			else
				throw MyError(to_string(o->getOid()) + " not a SupercontigSet");
		else
			throw MyError(to_string(o->getOid()) + " not a mekano objet");
	return 0;
}

namespace
{
	mekano::SupercontigSet *
	make_elem_at
	(
		mekano::Group * group,
		int n,
		eyedb::CollArray * const array,
		eyedb::Status (mekano::Group::*setin)(int, mekano::SupercontigSet *, eyedb::IndexImpl const *)
	)
	{
		if (mekano::SupercontigSet * set = get_sctg_set_from_array(array, n))
			return set;
		eyedb::Database * const db = group->eyedb::Struct::getDatabase();
		ObjectHandle<mekano::SupercontigSet> set(db);
	//	std::cerr << "insertAt " << n << " in " << static_cast<void *>(array) << std::endl;
		int const ncount = array->getCount() + 1;
		(group->*setin)(n, set, 0);
		// otherwise "collection error: cannot insert a null oid into a collection: must store first collection elements"
		set.store();
		array->store();
		if (array->getCount() != ncount)
			throw MyError("after setin(" + to_string(n) + ") array->getCount() should be " + to_string(ncount) + " but is " + to_string(array->getCount()));
		if (mekano::SupercontigSet * nset = get_sctg_set_from_array(array, n))
			return nset;
		throw MyError("unable to find the new set at " + to_string(n));
	}
	
	void
	add_to_set_in_array
	(
		mekano::Group * group,
		mekano::Supercontig * sctg,
		int n,
		eyedb::CollArray * const array,
		eyedb::Status (mekano::Group::*setin)(int, mekano::SupercontigSet *, eyedb::IndexImpl const *)
	)
	{
		if  (mekano::SupercontigSet * set = make_elem_at(group, n, array, setin))
		{
			set->s()->insert(sctg);
			set->store();
		}
		else
			throw MyError("no new set at " + to_string(n));
	}
}

static void
add_to_link_counts
(
	mekano::Calculated * calculated,
	mekano::Supercontig * const sctg1, // must be in the database
	mekano::Supercontig * const sctg2, // must be in the database
	unsigned const where,
	eyedb::CollBag * (mekano::Calculated::* at)(unsigned, eyedb::Bool *, eyedb::Status *),
	eyedb::Status (mekano::Calculated::* setin)(int, eyedb::CollBag *, eyedb::IndexImpl const *),
	mekano::LinkCounts * (mekano::Supercontig::* link_counts)(eyedb::Bool *, eyedb::Status *)
)
{
	eyedb::Database * const db = calculated->eyedb::Struct::getDatabase();
	ObjectHandle<mekano::SupercontigPair> sctgp(db);
	sctgp->sctg0(sctg1);
	sctgp->sctg1(sctg2);
	sctgp.store();
	ObjectHandle<eyedb::CollBag> clinks(((calculated)->*at)(where, 0, 0));
	bool need_setin = false;
	if (!clinks)
	{
		clinks.replace(db, sctgp.get_class(), eyedb::False);
		need_setin = true;
	}

	clinks->insert(eyedb::Value(sctgp.rep()));
	clinks.store(); // it would be good to avoid this store

	if (need_setin)
	{
		((calculated)->*setin)(where, clinks, 0);
		calculated->eyedb::Struct::store(); // it would be easy to avoid this store
	}

	mk_LinkCount(sctg1, (sctg2->*link_counts)(0, 0), where);
	mk_LinkCount(sctg2, (sctg1->*link_counts)(0, 0), where);
}

void
add_to_clone_link_counts
(
	ObjectHandle<mekano::Calculated> counts,
	mekano::Supercontig * const sctg1, // must be in the database
	mekano::Supercontig * const sctg2, // must be in the database
	unsigned const where
)
{
	add_to_link_counts(counts, sctg1, sctg2, where, &mekano::Calculated::clone_links_at, &mekano::Calculated::setin_clone_links_at, &mekano::Supercontig::clone_links);
}

void
add_to_matches_counts
(
	mekano::Calculated * counts,
	mekano::Supercontig * const sctg1, // must be in the database
	mekano::Supercontig * const sctg2, // must be in the database
	unsigned const where
)
{
	add_to_link_counts(counts, sctg1, sctg2, where, &mekano::Calculated::matches_at, &mekano::Calculated::setin_matches_at, &mekano::Supercontig::matches);
}

void
add_to_group(mekano::Group * g, mekano::Supercontig * sctg)
{
	add_to_set_in_array(g, sctg, sctg->length(), g->length_counts(), &mekano::Group::setin_length_counts_at);
	add_to_set_in_array(g, sctg, sctg->nb_ctg(), g->ctg_counts(), &mekano::Group::setin_ctg_counts_at);
	add_to_set_in_array(g, sctg, sctg->nb_cseq(), g->cseq_counts(), &mekano::Group::setin_cseq_counts_at);
	g->addto_sctgs(sctg);
}

void
finish_sctg(SupercontigData * data, mekano::Stage * stage, mekano::Group * group,
	unsigned hole_size
)
{
	data->sctg()->created_in(stage);
	data->sctg()->length(data->length(hole_size));
	data->sctg()->nb_cseq(data->nb_cseq());
	data->sctg()->nb_ctg(data->nb_ctg());
	data->sctg().store();
	add_to_group(group, data->sctg());
}

namespace
{
	void
	suppress_from_sctg_set(eyedb::CollArray * a, int i, eyedb::Oid const & sctg, eyedb::Bool absent_ok = eyedb::False)
	{
		if (mekano::SupercontigSet * ss = get_sctg_set_from_array(a, i))
		{
			try
			{
				ss->s()->suppress(sctg, absent_ok);
			}
			catch (...)
			{
				std::clog << "failed to delete " << sctg << ' ' << std::flush;
				std::clog << ObjectHandle<mekano::Supercontig>(a->getDatabase(), sctg)->name() << std::endl;
				throw;
			}
			if (ss->s()->getCount())
				ss->store();
			else
#if 1
				ss->store();
#else
			{
				try
				{
					std::cout << "suppressAt " << i << " in " << static_cast<void *>(a) << std::endl;
					a->suppressAt(i);
				}
				catch (...)
				{
					std::clog << "failed to suppressAt " << i << std::endl;
					throw;
				}
				//s->remove();
			}
#endif
		}
		else if (!absent_ok)
			throw MyError("tried to remove " + to_string(sctg) + " from nonexistant set number " + to_string(i));
	}
}

void
suppress_from_group(mekano::Group * g, mekano::Supercontig * sctg)
{
	try
	{
		g->rmvfrom_sctgs(sctg);
	}
	catch (...)
	{
		std::clog << "failed to rmvfrom_sctgs " << sctg->name() << ' ' << sctg->getOid() << std::endl;
		throw;
	}
	suppress_from_sctg_set(g->length_counts(), sctg->length(), sctg->getOid());
	suppress_from_sctg_set(g->ctg_counts(), sctg->nb_ctg(), sctg->getOid());
	suppress_from_sctg_set(g->cseq_counts(), sctg->nb_cseq(), sctg->getOid());
}

void
core_directory(char const * dir, char const * next)
{
	if (!next)
	{
		if (chdir(dir) < 0)
			fcouldnot("chdir", dir);
		struct rlimit limit;
		if (getrlimit(RLIMIT_CORE, &limit) < 0)
			fcouldnot("getrlimit");
		limit.rlim_cur = limit.rlim_max;
		if (setrlimit(RLIMIT_CORE, &limit) < 0)
			fcouldnot("setrlimit");
	}
}

std::ostream &
operator<<(std::ostream & o, ReadContigInfo const & ci)
{
	if (ci.unknown_gap())
		return o << "unknown_gap";
	if (ci.known_gap())
		return o << "gap " << ci.gap_size();
	if (!ci.name_forward() && !ci.name_reverse())
		return o << "bad";

	o << ci.name() << ' ' << ci.start() << ' ' << ci.length() << (ci.name_forward() ? " forward" : " reverse");
	if (!ci.sctg().empty())
		o << " sctg " << ci.sctg() << (ci.sctg_forward() ? " forward" : " reverse");
	return o;
}

namespace
{
	bool
	con_seq_eq(ReadContigInfo const & ci, SupercontigData::ContigData const & cd)
	{
		if (cd.conseq()->name() != ci.name())
			return false;
		if (cd.length() != ci.length())
			return false;
		if (cd.start() != ci.start() - 1)
			return false;
		if (ci.name_forward())
			return cd.cs_forward();
		if (ci.name_reverse())
			return cd.cs_reverse();
		return false;
	}
	bool
	con_seq_eq_current(ReadContigInfo const & ci, SupercontigData::ContigData const & cd)
	{
		if (ci.unknown_gap())
			return cd.unknown_gap();
		if (ci.known_gap())
			return cd.known_gap() && cd.gap_size() == ci.gap_size();
		if (cd.bottom())
		{
			if (ci.sctg() != cd.bottom()->name() || ci.sctg_forward() != cd.bottom_forward())
				return false;
		}
		else if (!ci.sctg().empty())
			return false;
		return con_seq_eq(ci, cd);
	}
	bool
	con_seq_eq_top(ReadContigInfo const & ci, SupercontigData::ContigData const & cd)
	{
		if (ci.unknown_gap())
			return cd.unknown_gap();
		if (ci.known_gap())
			return cd.known_gap() && cd.gap_size() == ci.gap_size();
		if (cd.top())
		{
			if (ci.sctg() != cd.top()->name() || ci.sctg_forward() != cd.top_forward())
				return false;
		}
		else if (!ci.sctg().empty())
			return false;
		return con_seq_eq(ci, cd);
	}
	
}

void
check_read_config_info
(
	std::vector<ReadContigInfo> const & read,
	SupercontigData const & data,
	bool const check_conseqs,
	bool const check_current_sctgs
)
{
	SupercontigData::ConSeqContainer const & conseqs = check_conseqs ? data.conseqs() : data.contigs();
	std::string const what(check_conseqs ? "continuous sequences" : "contigs");
	if (conseqs.size() != read.size())
	{
		std::clog << "calculated: "<< std::endl;
		copy(conseqs.begin(), conseqs.end(), std::ostream_iterator<SupercontigData::ContigData>(std::clog, " \n"));
		std::clog << "read: " << std::endl;
		copy(read.begin(), read.end(), std::ostream_iterator<ReadContigInfo>(std::clog, " \n"));
		std::clog << std::flush;
//		std::clog << *data.sctg() << std::endl;
		throw MyError("SupercontigData calculated "
			+ to_string(conseqs.size())
			+ " " + what + " for "
			+ data.sctg()->name() + " (should be "
			+  to_string(read.size()) + ")");
	}

	std::pair<std::vector<ReadContigInfo>::const_iterator, SupercontigData::ConSeqContainer::const_iterator> r = mismatch(read.begin(), read.end(), conseqs.begin(), check_current_sctgs ? con_seq_eq_current : con_seq_eq_top);
	if (r.first != read.end())
	{
		std::clog << "difference at: "<< *r.first << "\n\t" << *r.second << std::endl;
		std::clog << "calculated: "<< std::endl;
		copy(conseqs.begin(), conseqs.end(), std::ostream_iterator<SupercontigData::ContigData>(std::clog, " \n"));
		std::clog << "read: "<< std::endl;
		copy(read.begin(), read.end(), std::ostream_iterator<ReadContigInfo>(std::clog, " \n"));
		std::clog << std::flush;
		throw MyError("SupercontigData calculated a bad list of " + what + " for " + data.sctg()->name());
	}
} 

void
usage(std::string const & message)
{
	std::clog << "usage: " << myname << ": " << message << std::endl;
	exit(1);
}
	
char const *
arg(char * argv[], std::string const & what)
{
	if (!*argv)
		usage("missing " + what);
	return *argv;
}

class DropEGid : private boost::noncopyable
{
	gid_t const egid;
	gid_t const rgid;
public:
	DropEGid() : egid(getegid()), rgid(getgid())
	{
		if (rgid != egid && setgid(rgid) < 0)
			throw MyError("could not drop effective gid " + to_string(egid) + ": " + strerror(errno));
	}
	~DropEGid()
	{
		if (rgid != egid && setegid(egid) < 0)
			throw MyError("could not regain effective gid " + to_string(egid) + ": " + strerror(errno));
	}
};

void
Ifstream::open(std::string const & n)
{
	DropEGid egid;
	std::ifstream::open(n.c_str());
	_fname = n;
	if (!*this)
		throw MyError("could not open " + n + ": " + strerror(errno));
	unsetf(std::ios_base::skipws);
	_lineno = 1;
}

Ifstream &
Ifstream::skipline()
{
	std::ifstream::ignore(100000, '\n');
	if (*this)
		_lineno++;
	return *this;
}

namespace
{
	std::string
	dirname(std::string const & f)
	{
		std::string::size_type p = f.find_last_of('/');
		return (p == f.npos) ? "" : f.substr(0, p + 1);
	}
}

std::string
qualify(std::string fname, std::string const & base)
{
	if (fname.at(0) != '/')
		fname.insert(0, dirname(base));
	return fname;
}
