/*	$Id: common.h 797 2007-06-06 13:36:20Z stuart $
 *	Stuart Pook, Genescope, 2006 (enscript -E $%)
 */
#include <eyedb/eyedb.h>
#include <string.h>
#include <set>
#include <utils/templates.h>
#include <fstream>
#include <iostream>
#include <sys/resource.h>
#include <unistd.h>
#include <string>
#include <errno.h>

#include <utils/eyedb.h>
#include <utils/myerror.h>
#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>

#include "mekano.h"

#ifndef MEKANO_IMPORT_COMMON_H
#define MEKANO_IMPORT_COMMON_H MEKANO_IMPORT_COMMON_H
extern char const * myname;

enum { minimum_gap_size = 1 };

inline eyedb::Bool bool2eyedb(bool b) { return (b) ? eyedb::True : eyedb::False; }

void fcouldnot(char const * what, char const * which = 0);
inline void
fcouldnot(char const * what, std::string const & which)
{
	fcouldnot(what, which.c_str());
}

void core_directory(char const * dir, char const * next = 0);

class Ifstream : public std::ifstream
{
	std::string _fname;
	Ifstream(Ifstream const &);
	unsigned _lineno;
public:
	std::string fname() const { return _fname; }
	unsigned lineno() const { return _lineno; }
	std::string tag() const { return (eof()) ? _fname : _fname + ':' + to_string(_lineno); }
	Ifstream(std::string const & n) { open(n); }
	bool is_open() const { return !_fname.empty(); }
	Ifstream() {}
	Ifstream & skipline();
	Ifstream & get(char & c)
	{
		if (std::ifstream::get(c) && c == '\n')
			_lineno++;
		return *this;
	}
	int_type get()
	{
		int_type r = std::ifstream::get();
		if (r == '\n')
			_lineno++;
		return r;
	}
	void open(std::string const &);
	friend Ifstream & getline(Ifstream & f, std::string & s, char eol)
	{
		std::getline(static_cast<std::ifstream &>(f), s, eol);
		if (f && eol == '\n')
			f._lineno++;
		return f;
	}
	friend Ifstream & getline(Ifstream & f, std::string & s)
	{
		std::getline(static_cast<std::ifstream &>(f), s);
		if (f)
			f._lineno++;
		return f;
	}
	friend Ifstream & getcomment(Ifstream & f, std::string & s)
	{
		std::getline(static_cast<std::ifstream &>(f), s);
		f.unget();
		return f;
	}
	Ifstream & skip_white_space()
	{
		while (*this && (peek() == '\n' || peek() == ' ' || peek() == '\t'))
			get();
 		return *this;
	}
};

eyedb::Oid lookup(eyedb::Database *, std::string const &, int id, bool = true);
eyedb::Oid lookup(eyedb::Database *, std::string const &, std::string const &, bool = true);
eyedb::Oid select_one(eyedb::Database *, std::string const &);

struct NoTransactionTransactionParams : eyedb::TransactionParams
{
	NoTransactionTransactionParams()
	{
		trsmode = eyedb::TransactionOff;
		recovmode = eyedb::RecoveryOff;
		lockmode = eyedb::ReadNWriteN;
	}
};

void remove(eyedb::Database * const db, std::string const & class_name, int id = -1);
void remove_query(eyedb::Database *, std::string const &);
ObjectHandle<mekano::Stage> make_stage(eyedb::Database * const db, unsigned id);

struct GetEGid : private boost::noncopyable
{
	GetEGid(gid_t const egid)
	{
		if (setegid(egid) < 0)
			throw MyError("count not open setegid(" + to_string(egid) + "): " + strerror(errno));
	}
	~GetEGid()
	{
		gid_t const rgid = getgid();
		if (setgid(rgid) < 0)
			throw MyError("count not open setgid(" + to_string(rgid) + "): " + strerror(errno));
	}
};

std::istream & skipspaces(std::istream& s);

class skip
{
	char const * str;
public:
	skip(char const * s) : str(s) {}
	std::istream & operator()(std::istream & s) const;
};

inline std::istream &
operator>>(std::istream &ostr, skip const & skip)
{
	return skip(ostr);
}

inline void fix_conseq_name(std::string *) {}

int fail(std::ostream &);
int fail(std::ostream &, eyedb::Exception const &);
int fail(std::ostream &, MyError const &);
//int fail(char const *, Ifstream const &, str::string const &, eyedb::Exception const &);

template <class E>
int
fail(char const * n, Ifstream const & f, std::string const & m, E const & e)
{
	return fail(std::clog << n << ": " << f.fname() << ':' << f.lineno() << ": " << m, e);
}
	
void
add_to_clone_link_counts
(
	ObjectHandle<mekano::Calculated>,
	mekano::Supercontig *,
	mekano::Supercontig *,
	unsigned
);
	
void
add_to_matches_counts
(
	mekano::Calculated *,
	mekano::Supercontig *,
	mekano::Supercontig *,
	unsigned
);

void
add_to_cseq_counts(mekano::Stage * stage, mekano::Supercontig * sctg);

void
add_to_ctg_counts(mekano::Stage * stage, mekano::Supercontig * sctg);

void
add_to_length_counts(mekano::Stage * stage, mekano::Supercontig * sctg);

mekano::SupercontigSet * get_sctg_set_from_array(eyedb::CollArray const *, int);

#if 0
eyedb::CollSet *
make_elem_at
(
	mekano::Stage * stage,
	int n,
	eyedb::CollArray * array,
	eyedb::Status (mekano::Stage::*setin)(int, eyedb::CollSet *, eyedb::IndexImpl const *)
);
inline eyedb::CollSet *
make_elem_at
(
	mekano::Stage * stage,
	int n,
	eyedb::CollArray * (mekano::Stage::*getarray)(eyedb::Bool *, eyedb::Status *),
	eyedb::Status (mekano::Stage::*setin)(int, eyedb::CollSet *, eyedb::IndexImpl const *)
)
{
	return make_elem_at(stage, n, (stage->*getarray)(0, 0), setin, idximpl);
}
#endif

void suppress_from_group(mekano::Group *, mekano::Supercontig *);
void add_to_group(mekano::Group *, mekano::Supercontig *);

class SupercontigData;
void finish_sctg(SupercontigData *, mekano::Stage *, mekano::Group *, unsigned);

class Statistics
{
	eyedb::ObjectObserver observer;
	struct timeval _start;
	eyedb::Database::OpenFlag _mode;
public:
	Statistics()
	{
		_start.tv_sec = _start.tv_usec = 0;
	}
	void time(eyedb::Database const * db)
	{
		if (gettimeofday(&_start, 0) != 0)
			fcouldnot("gettimeofday");
		_mode = db->getOpenFlag();
	}
	void time(eyedb::Database const & db) { time(&db); }
	~Statistics();
	Statistics(eyedb::Database const * db) { time(db); }
//	Statistics::Statistics(const char* m, eyedb::Database::OpenFlag) { _start.tv_sec = _start.tv_usec = 0; }
//	void close(eyedb::Database *) const {}
};

class ReadContigInfo
{
	enum { _known_gap, _unknown_gap, _name_forward, _name_reverse } _type;
	std::string _name;
	std::string _sctg_name;
	union
	{
		struct
		{
			unsigned start;
			unsigned stop;
			bool sctg_forward;
		}
			_named;
		unsigned _gap_size;
	};
	bool is_name() const { return _type == _name_forward || _type == _name_reverse; }
public:
	ReadContigInfo() : _type(_unknown_gap) {}
	ReadContigInfo(unsigned g) : _type(_known_gap), _gap_size(g ? g : 1) {}
	ReadContigInfo(std::string const & n, unsigned start, unsigned stop, std::string const & sctg = "", bool sctg_forward = true) :
		_type(start < stop ? _name_forward : _name_reverse),
		_sctg_name(sctg)
	{
		_name = n;
		_named.start = (_type == _name_forward) ? start : stop;
		_named.stop = (_type == _name_forward) ? stop : start;
		_named.sctg_forward = sctg_forward;
	}
	bool unknown_gap() const { return _type == _unknown_gap; }
	bool known_gap() const { return _type == _known_gap; }
	bool name_forward() const { return _type == _name_forward; }
	bool name_reverse() const { return _type == _name_reverse; }
	unsigned gap_size() const { return _gap_size; }
	std::string name() const { return _name; }
	unsigned length() const { return _named.stop - _named.start + 1; }
	unsigned start() const { return _named.start; }
	std::string const & sctg() const { return _sctg_name; }
	bool sctg_forward() const { assert(is_name()); return _named.sctg_forward; }
};
void check_read_config_info(std::vector<ReadContigInfo> const &, SupercontigData const &, bool = true, bool = false);
std::ostream & operator<<(std::ostream &, ReadContigInfo const &);

std::string qualify(std::string, std::string const &);

int copy_stage(eyedb::Database *, std::string const &);
void add_group(ObjectHandle<mekano::Stage>, std::string const &, std::string const &);
void joins(
	ObjectHandle<mekano::Stage>,
	ObjectHandle<mekano::Calculated>,
	int hole_size,
	Ifstream *,
	std::string const &
);
void change_group
(
	ObjectHandle<mekano::Stage> stage,
	Ifstream *,
	std::string const & from_name,
	std::string const & to_name
);
void insert_stage_types(eyedb::Database *, Ifstream &);
void insert_clones_and_reads(eyedb::Database *, Ifstream &);
void insert_banks(eyedb::Database *, Ifstream &);
void insert_contigs(Ifstream &, ObjectHandle<mekano::Stage>);
void insert_meta_contigs(Ifstream *, ObjectHandle<mekano::Stage>);
void insert_supercontigs
(
	Ifstream *,
	ObjectHandle<mekano::Stage> stage,
	mekano::Group * group,
	ObjectHandle<mekano::Calculated> counts,
	int unsized_gap,
	int transaction_size
);
char const * arg(char **, std::string const &);
void clonelinks(ObjectHandle<mekano::Calculated>, mekano::Group *, bool = true);
void process_stage(Ifstream * in, ObjectHandle<mekano::Calculated>, int, gid_t);
void setup_insert_matches(eyedb::Database *, std::string const &, Ifstream *);
boost::shared_ptr<Ifstream>  get_file(Ifstream & in);

typedef std::vector<std::pair<std::string, boost::shared_ptr<Ifstream> > > matches_t;
void insert_matches(eyedb::Database *, matches_t const &);

typedef std::vector<std::pair<std::string, std::string> > algorithms_t;
void insert_algorithms(eyedb::Database * db, algorithms_t const & algorithms);

#endif
