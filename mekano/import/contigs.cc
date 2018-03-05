/*	$Id: contigs.cc 645 2007-04-06 13:28:15Z stuart $
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
#include <unistd.h>
#include <iomanip>

#include <utils/templates.h>
#include <utils/eyedb.h>

#include "common.h"
#include "mekano.h"

namespace
{
	std::string const what("adding Contigs");
	void
	real_insert_contigs(Ifstream & in, ObjectHandle<mekano::Stage> const & stage)
	{
		eyedb::Database * const db = stage.database();
		std::clog << myname << ": " + what + " from " << in.fname() << std::endl;
		TransactionControl transaction(db, 300);
		ObjectHandle<mekano::Contig> contig;
		std::string contig_name;
		while (in.skip_white_space() >> contig_name >> skipspaces)
		{
			unsigned contig_length;
			if (!(in >> contig_length >> skipspaces))
				throw MyError("missing contig length");

			std::string read_name;
			if (!(in >> read_name >> skipspaces))
				throw MyError("missing read name");

			unsigned used;
			if (!(in >> used >> skipspaces))
				throw MyError("missing used");

			unsigned start_on_contig;
			if (!(in >> start_on_contig >> skipspaces))
				throw MyError("missing used");

			unsigned start_on_read;
			if (!(in >> start_on_read >> skipspaces))
				throw MyError("missing used");

			std::string direction;
			if (!(in >> direction >> skipspaces))
				throw MyError("missing direction");
			if (direction != "R" && direction != "F")
				throw MyError("bad direction: " + direction);

			ObjectHandle<mekano::ReadPosition> rp(db);
			rp->read_oid(lookup(db, "Read", read_name));
			rp->start_on_read(start_on_read);
			rp->start_on_contig(start_on_contig);
			rp->length(used);
			rp->forward(bool2eyedb(direction == "F"));
			
			if (!contig || contig->name() != contig_name)
			{
				if (contig)
				{
					contig.store();
					transaction.checkpoint();
				}
				contig = ObjectHandle<mekano::Contig>(db);
				contig->name(contig_name);
				contig->length(contig_length);
				contig->created_in_oid(stage.oid());
			}

			contig->reads(contig->reads_cnt(), rp);	

			if (in.get() != '\n')
				throw MyError("failed to read eol");
		}
		if (!in.eof())	
			throw MyError("count not read contig name");
		if (contig)
			contig.store();
		transaction.validate();
	}
}

void
insert_contigs(Ifstream & in, ObjectHandle<mekano::Stage> stage)
{
	try
	{
		real_insert_contigs(in, stage);
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
	unsigned transaction_size = 300;
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
	catch (eyedb::Exception const & e)
	{
		return fail(std::clog << myname << ": EyeDB exception opening " + db_name, e);
	}

	unsigned const stage_id = 0;
	eyedb::Oid stage;
	try
	{
		TransactionControl trans(&db);
		stage = lookup(&db, "Stage", stage_id);
	}
	catch (eyedb::Exception const & e)
	{
		return fail(std::clog << myname << ": eyedb::Exception& finding stage " << stage_id << ": " << e);
	}
	catch (MyError & e)
	{
		return fail(std::clog << myname << ": eyedb::Exception& finding stage " << stage_id << ": " << e);
	}

	if (remove_existing)
		try
		{
			remove_query(&db, "select Contig.created_in == " + to_string(stage));
		}
		catch (eyedb::Exception const & e)
		{
			return fail(std::clog << myname << ": EyeDB exception deleting Contigs from " << stage << ": " << e);
		}
		catch (MyError const & e)
		{
			return fail(std::clog << myname << ": exception deleting Contigs from " << stage << ": " << e);
		}

	stats.time(&db);
	while (*argv)
	{
		Ifstream arachne(*argv++);
		core_directory(change_dir, *argv);
		try
		{
			read_arachne_output(arachne, &db, transaction_size, stage);
		}
		catch (eyedb::Exception const & e)
		{
			return fail(std::clog << '\n' << myname << ": " << arachne.tag(), e);
		}
		catch (MyError & e)
		{
			return fail(std::clog << '\n' << myname << ": " << arachne.tag(), e);
		}
	}
	return 0;
}
#endif
