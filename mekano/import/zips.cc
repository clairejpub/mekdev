/*	$Id: zips.cc 402 2006-10-17 15:58:05Z stuart $
 *	Stuart Pook, Genescope, 2006
 */
#include <iostream>
#include <string>
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <iomanip>
#include <set>

#include <utils/templates.h>
#include <utils/eyedb.h>

#include "common.h"
#include "mekano.h"

void add_junctions(Ifstream &, std::string const &, eyedb::Database *);

char const * myname;

static void
usage(std::string const & message)
{
	std::clog << "usage: " << myname << ": " << message << std::endl;
	exit(1);
}

static bool
forward_check(char const forward, Ifstream & f)
{
	if (forward == 'R')
		return false;
	 if (forward == 'F')
		return true;
	throw MyError(std::string("bad forward: ") + forward);
}

static mekano::ZipType
type_check(char const type, Ifstream & f)
{
	switch (type)
	{
	case 'r':
		return mekano::zip_right;
	case 'l':
		return mekano::zip_left;
	case 'i':
		return mekano::zip_included;
	}

	throw MyError(std::string("bad type: ") + type);
}

static void
insert_zips(eyedb::Database * db, Ifstream & f)
{
	TransactionControl trans(db, 1000);
	std::string name0;
	std::string name1;
	char cforward1;
	char cforward0;
	char dummy;
	char ctype;
	f.unsetf(std::ios_base::skipws);

	std::clog << myname << ": adding Zips from " << f.fname() << std::endl;
	while (f >> skipspaces >> name0 >> skipspaces >> name1 >> skipspaces >> ctype >> skipspaces >> dummy >> cforward0 >> skip(":") >> dummy >> cforward1 >> skipspaces)
	{
		ObjectHandle <mekano::Zip> zip(db);
		ObjectHandle <mekano::ZipSide> side1(db);
		ObjectHandle <mekano::ZipSide> side0(db);
		zip->long_side(side0);
		zip->short_side(side1);
		side0->sctg_oid(lookup(db, "Supercontig", name0));
		side0->forward(bool2eyedb(forward_check(cforward0, f)));
		side1->sctg_oid(lookup(db, "Supercontig", name1));
		side1->forward(bool2eyedb(forward_check(cforward1, f)));
		zip->ztype(type_check(ctype, f));
		zip.store();
		trans.checkpoint();
		if (f.get() != '\n')
			throw MyError("expected a newline");
	}

	if (!f.eof())
		throw MyError("format error");
	trans.validate();
}


static char const *
arg(char * argv[], std::string const & what)
{
	if (!*argv)
		usage("missing " + what);
	return *argv;
}

int
main(int argc, char *argv[])
{
	if ((myname = strrchr(argv[0], '/')) == 0 || *++myname == 0)
		myname = argv[0];
	Statistics stats;
	mekano::mekano initializer(argc, argv);
	
	eyedb::Database::OpenFlag mode = eyedb::Database::DBRW;
	bool remove_existing = false;
	argv += options(argc, argv, &mode, &remove_existing);
	
	std::string const db_name(arg(argv++, "database name"));
	Ifstream zips(arg(argv++, "Zip list"));
	char const * algorithm_name(arg(argv++, "algorithm name"));
	Ifstream junctions(arg(argv++, "junction list"));
	if (*argv)
		usage("too many arguments");

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
		return fail(std::clog << myname << ": open" << db_name, e);
	}
	if (remove_existing)
		try
		{
			remove(&db, "Zip");
			remove(&db, "Junction");
			remove(&db, "Algorithm");
		}
		catch (eyedb::Exception & e)
		{
			return fail(std::clog << myname << ": removing existing objects", e);
		}
	stats.time(db);
	try
	{
		insert_zips(&db, zips);
	}
	catch (eyedb::Exception & e)
	{
		return fail(std::clog << myname << ": " << zips.fname() << ":" << zips.lineno() << ": inserting Zips", e);
	}
	catch (MyError & e)
	{
		return fail(std::clog << myname << ": " << zips.fname() << ":" << zips.lineno() << ": inserting Zips", e);
	}

	try
	{
		add_junctions(junctions, algorithm_name, &db);
	}
	catch (eyedb::Exception & e)
	{
		return fail(std::clog << myname << ": " << junctions.fname() << ":" << junctions.lineno() << ": inserting Junctions", e);
	}
	catch (MyError & e)
	{
		return fail(std::clog << myname << ": " << junctions.fname() << ":" << junctions.lineno() << ": inserting Junctions", e);
	}
	return 0;
}
