/*	$Id: junctions.cc 225 2006-06-19 16:03:41Z stuart $
 *	Stuart Pook, Genescope, 2006
 */
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <iomanip>

#include <utils/templates.h>
#include <utils/eyedb.h>

#include "common.h"
#include "mekano.h"
extern char const * myname;

static eyedb::Oid
get_sequence(Ifstream & in, eyedb::Database * const db)
{
	std::string cseq;
	if (!(in >> cseq >> skipspaces))
		throw MyError("missing ContinuousSequence name");
	fix_conseq_name(&cseq);
	eyedb::Oid const conseq = lookup(db, "ContinuousSequence", cseq);
	return conseq;
}

template <typename T>
static T
get_integer(Ifstream & in)
{
	unsigned long u;
	if (!(in >> u >> skipspaces))
		throw MyError("missing integer");
	if (u > (unsigned long)std::numeric_limits<T>::max())
		throw MyError("integer too big");
	return u;
}

static mekano::JunctionType
get_junction_type(Ifstream & in, bool * forward)
{
	char type, fchar;
	unsigned u1, u2;
	if (!(in >> u1 >> skipspaces >> skip("/") >> u2 >> skipspaces >> skip(".") >> type >> skipspaces >> skip(".") >> fchar >> skipspaces))
		throw MyError("missing type info");
	if (fchar == 'F')
		*forward = true;
	else if (fchar == 'R')
		*forward = false;
	else
		throw MyError(std::string("bad forward char ") + fchar);
	
	switch (type)
	{
	case 'b':
		return mekano::junction_right;
	case 'a':
		return mekano::junction_left;
	case 'i':
		return mekano::junction_included;
	case 'c':
		return mekano::junction_centre;
	}
	throw MyError(std::string("bad junction type ") + type);
}

static void
get_side(Ifstream & in, eyedb::Database * const db, mekano::Junction * jp, int const side_no)
{
	ObjectHandle<mekano::MatchSide> side(db);
	jp->sides(side_no, side);
	side->cseq_oid(get_sequence(in, db));
	side->start(get_integer<int>(in));
	side->stop(get_integer<int>(in));
	bool forward;
	jp->jtype(side_no, get_junction_type(in, &forward));
	side->forward(bool2eyedb(forward));
}

static bool
add_to_zip
(
	eyedb::Database * const db,
	eyedb::Oid const & junction,
	std::string const & super_contig1,
	std::string const & super_contig2
)
{
	std::stringstream squery;
	squery << "(select z from Zip z where";
	squery << " z.long_side.sctg.name==" << '"' << super_contig1 << '"';
	squery << " and z.short_side.sctg.name==" << '"' << super_contig2 << '"' << ')';
	squery << " union";
	squery << " (select z from Zip z where";
	squery << " z.long_side.sctg.name==" << '"' << super_contig2 << '"';
	squery << " and z.short_side.sctg.name==" << '"' << super_contig1 << '"' << ')';
	std::string const query = squery.str();

	eyedb::OidArray oids;
	eyedb::OQL(db, query.c_str()).execute(oids);
	if (unsigned const n = oids.getCount())
	{
		if (n > 1)
			throw MyError(to_string(n) + " Zips for " + query);
//			std::clog << n << " Zips for " << query << std::endl;
	}
	else
		return false;

	ObjectHandle<mekano::Zip> zip(db, oids[0]);
	zip->junctions_oid(zip->junctions_cnt(), junction);
	zip.store();
	return true;
}

static void
insert_junctions(Ifstream & in, eyedb::Database * const db, eyedb::Oid const & algorithm)
{
	in.unsetf(std::ios_base::skipws);
	unsigned no_zip = 0;
	unsigned count;
	{
		TransactionControl trans(db, 300);
		std::clog << myname << ": adding Junctions from " << in.fname() << std::endl;
		std::string superid0;
		while (in >> superid0 >> skipspaces)
		{
			ObjectHandle<mekano::Junction> junction(db);	
			get_side(in, db, junction, 0);
			std::string superid1;
			if (!(in >> superid1 >> skipspaces))
				throw MyError("unable to read 2nd SuperContig name");
			get_side(in, db, junction, 1);
			junction->algorithm_oid(algorithm);
			junction.store();
	
			if (!add_to_zip(db, junction.oid(), superid0, superid1))
				no_zip++;
	
			trans.checkpoint();
			if (in.get() != '\n')
				throw MyError("expected a newline");
		}
		if (!in.eof())	
			throw MyError("bad 1st SuperContig name");
		trans.ok();
		count = trans.count();
	}
	std::clog << myname << ": " << no_zip << " (" << no_zip * 100.0 / count << "%) Junctions not in a Zip" << std::endl;
}

static eyedb::Oid
make_algorithm(eyedb::Database * const db, std::string const & name)
{
	TransactionControl trans(db);
	ObjectHandle<mekano::Algorithm> algorithm(db);
	algorithm->name(name);
	algorithm.store();
	trans.ok();
	return algorithm.oid();
}

void
add_junctions(Ifstream & in, std::string const & algorithm, eyedb::Database * const db)
{
	insert_junctions(in, db, make_algorithm(db, algorithm));
}

#if 0
char const * myname;

void
usage(std::string const & message)
{
	std::clog << "usage: " << myname << ": " << message << std::endl;
	exit(1);
}

static char const *
arg(char * argv[], std::string const & what)
{
	if (!*argv)
		usage("missing " + what);
	return *argv;
}

static int
fail(std::ostream & o)
{
	o << std::endl;
	return 1;
}

int
main(int argc, char *argv[])
{
	if ((myname = strrchr(argv[0], '/')) == 0 || *++myname == 0)
		myname = argv[0];
	eyedb::init(argc, argv);
	
	eyedb::Database::OpenFlag mode = eyedb::Database::DBRW;
	argv += options(argc, argv, &mode);
	
	std::string db_name(arg(argv++, "database name"));
	char const * algorithm_name(arg(argv++, "algorithm name"));	
	Ifstream junctions(arg(argv++, "junction info"));
	if (*argv)
		usage("too many arguments");
	
	mekano::mekano::init();
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
		return (std::clog << myname << ": eyedb::Exception& opening \"" << db_name << "\": " << e), 1;
	}
	try
	{
		remove(&db, "Junction");
		remove(&db, "Algorithm");
	}
	catch (eyedb::Exception & e)
	{
		return fail(std::clog << myname << ": EyeDB exception removing Junctions or Algorithm: " << e);
	}
	struct timeval start_import;
	if (gettimeofday(&start_import, 0) != 0)
		fcouldnot("gettimeofday");
	try
	{
		insert_junctions(junctions, &db, make_algorithm(&db, algorithm_name));
	}
	catch (eyedb::Exception & e)
	{
		return fail(std::clog << myname << ": eyedb::Exception&: " << e);
	}
	catch (MyError & e)
	{
		return fail(std::clog << myname << ": error processing " << e);
	}
	mekano::mekano::release();
	eyedb::release();
	print_time(myname, start_import, mode);
	return 0;
}
#endif
