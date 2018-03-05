/*	Stuart Pook, Genescope, 2006 (enscript -E $%) (Indent ON)
 *	$Id: banks.cc 570 2007-03-12 11:22:07Z stuart $
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

	void
	insert_stage_types(eyedb::Database * const db, Ifstream & in)
	{
		TransactionControl trans(db);
		in.unsetf(std::ios_base::skipws);
		std::string name;
		std::clog << myname << ": adding StageTypes from " << in.fname() << std::endl;
		while (in.skip_white_space() && (in >> name >> skipspaces))
		{
			std::string comment;
			if (!getline(in, comment))
				throw MyError("failed to read the comment");
			
			ObjectHandle<mekano::StageType> st(db);
			st->name(name);
			if (!comment.empty())
				st->comment(comment);
			st.store();
			trans.checkpoint();
		}
		if (!in.eof())	
			throw MyError("format error");
		trans.validate();
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
			remove(&db, "StageTypes");
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
		Ifstream stypes(*argv++);
		core_directory(change_dir, *argv);

		try
		{
			insert_types(stypes, &db);
		}
		catch (eyedb::Exception & e)
		{
			exit(fail(std::clog << myname << ": " << stypes.tag() << ": eyedb::Exception inserting StageTypes", e));
		}
		catch (MyError & e)
		{
			exit(fail(std::clog << myname << ": " << stypes.tag() << ": error processing inserting StageTypes", e));
		}
	}
	return 0;
}
#endif
