/*	Stuart Pook, Genescope, 2006 (enscript -E $%) (Indent ON)
 *	$Id: banks.cc 645 2007-04-06 13:28:15Z stuart $
 */
#include <iostream>
#include <string>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iomanip>

#include <utils/eyedb.h>

#include "mekano.h"
#include "common.h"

namespace
{
	std::string const what("adding Banks");

	void
	real_insert_banks(eyedb::Database * const db, Ifstream & in)
	{
		TransactionControl trans(db);
		std::string name;
		std::clog << myname << ": " + what + " from " << in.fname() << std::endl;
		while (in.skip_white_space() && (in >> name >> skipspaces))
		{
			unsigned length;
			if (!(in >> length >> skipspaces))
				throw MyError("failed to read the length of bank " + name);
			
			ObjectHandle<mekano::Bank> b(db);
			b->name(name);
			b->length(length);
			b.store();
			trans.checkpoint();
			if (in.get() != '\n')
				throw MyError("missing eol");
		}
		if (!in.eof())	
			throw MyError("format error");
		trans.validate();
	}
}

void
insert_banks(eyedb::Database * const db, Ifstream & in)
{
	try
	{
		real_insert_banks(db, in);
	}
	catch (eyedb::Exception const & e)
	{
		throw MyError(in.tag() + ": eyedb::Exception " + what + ": " + to_string(e));
	}
	catch (MyError & e)
	{
		throw MyError(in.tag() + ": " + what + ": " + to_string(e));
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
	argv += options(argc, argv, &mode, &remove_existing, &change_dir);
	
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
			remove(&db, "Bank");
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
		Ifstream banks(*argv++);
		core_directory(change_dir, *argv);

		try
		{
			insert_banks(banks, &db);
		}
		catch (eyedb::Exception & e)
		{
			exit(fail(std::clog << myname << ": " << banks.tag() << ": eyedb::Exception inserting banks", e));
		}
		catch (MyError & e)
		{
			exit(fail(std::clog << myname << ": " << banks.tag() << ": error processing inserting banks", e));
		}
	}
	return 0;
}
#endif
