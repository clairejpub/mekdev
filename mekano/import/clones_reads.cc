/*	Stuart Pook, Genescope, 2006 (enscript -E $%) (Indent ON)
 *	$Id: clones_reads.cc 645 2007-04-06 13:28:15Z stuart $
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
#include <unistd.h>
#include <iomanip>

#include <utils/templates.h>
#include <utils/eyedb.h>

#include "mekano.h"
#include "common.h"

namespace
{
	std::string const what("adding Clone & Read");

	void
	real_insert_clones_and_reads(eyedb::Database * const db, Ifstream & f)
	{
		TransactionControl trans(db, 4000);
		std::clog << myname << ": " + what + " from " << f.fname() << std::endl;
		ObjectHandle<mekano::Clone> clone;
		std::string clone_name;
		while (f.skip_white_space() >> clone_name >> skipspaces)
		{
			std::string bank_name;
			if (!(f >> bank_name >> skipspaces))
				throw MyError("read of the bank name failed");
			std::string read_name;
			if (!(f >> read_name >> skipspaces))
				throw MyError("read of the read name failed");
			unsigned length;
			if (!(f >> length >> skipspaces))
				throw MyError("read of the length failed");
	
			if (!clone || clone_name != clone->name())
			{
				clone = ObjectHandle<mekano::Clone>(db);
				clone->name(clone_name);
				clone->bank_oid(lookup(db, "Bank", bank_name));
				clone.store();
			}
			ObjectHandle<mekano::Read> read(db);
			read->name(read_name);
			read->clone(clone);
			read->length(length);
			read.store();
			trans.checkpoint();
			if (f.get() != '\n')
				throw MyError("failed to read eol");
		}
		if (!f.eof())
			throw MyError("read of the clone name failed");
		trans.validate();
	}
}

void
insert_clones_and_reads(eyedb::Database * const db, Ifstream & in)
{
	std::string const mess(" " + what + ": ");
	try
	{
		real_insert_clones_and_reads(db, in);
	}
	catch (eyedb::Exception const & e)
	{
		throw MyError(in.tag() + ": eyedb::Exception" + mess + to_string(e));
	}
	catch (MyError & e)
	{
		throw MyError(in.tag() + ": " + mess + to_string(e));
	}
}

#if 0
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
	unsigned transaction_size = 2000;
	argv += options(argc, argv, &mode, &remove_existing, &change_dir, 0, &transaction_size);
	
	std::string db_name(arg(argv++, "database name"));	
	
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
		exit(fail(std::clog << myname << ": eyedb::Exception opening the database", e));
	}

	if (remove_existing)
		try
		{
			remove(&db, "Read");
			remove(&db, "Clone");
		}
		catch (eyedb::Exception & e)
		{
			exit(fail(std::clog << myname << ": eyedb::Exception removing existing" , e));
		}
		catch (MyError & e)
		{
			exit(fail(std::clog << myname << ": error removing existing", e));
		}

	stats.time(&db);
	while (*argv)
	{
		Ifstream input(*argv++);
		core_directory(change_dir, *argv);
		try
		{
			make_clones_and_reads(input, &db, transaction_size);
		}
		catch (eyedb::Exception & e)
		{
			exit(fail(std::clog << myname << ": " << input.tag() << ": eyedb::Exception creating clones & reads", e));
		}
		catch (MyError & e)
		{
			exit(fail(std::clog << myname << ": " << input.tag() << ": error creating clones & reads", e));
		}
	}
	return 0;
}
#endif
