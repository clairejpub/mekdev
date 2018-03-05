/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: metacontigs.cc 623 2007-04-03 08:46:32Z stuart $
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

extern char const * myname;

namespace
{
	void
	cleanup(TransactionControl * trans, ObjectHandle<mekano::Metacontig> meta_contig, unsigned * length)
	{
		if (*length)
		{
			meta_contig->length(*length);
			meta_contig.store();
			trans->checkpoint();
		}
		*length = 0;
	}

	void
	meta_contigs(Ifstream * f, ObjectHandle<mekano::Stage> stage)
	{
		eyedb::Database * const db = stage.database();
		TransactionControl trans(db, 300);
		char c;
		unsigned length = 0;
		ObjectHandle<mekano::Metacontig> meta_contig;
		std::clog << myname << ": adding Metacontigs from " << f->fname() << std::endl;
		std::string mctg;
		bool need_start = true;
		while (f->skip_white_space() >> c)
		{
			if (c == '>')
			{
				cleanup(&trans, meta_contig, &length);
				if (!(*f >> mctg >> skipspaces))
					throw MyError("read of metacontig name failed");
				need_start = false;
			}
			else
			{
				if (need_start)
					throw MyError("expected a new Metacontig");
				std::string cseq;
				unsigned start;
				unsigned stop;
				std::string sctg;
				std::string direction;
				if (!(*f >> cseq >> skipspaces >> start >> skipspaces >> stop >> skipspaces >> sctg >> skipspaces >> direction >> skipspaces))
					throw MyError("read of contig information failed");
				cseq = c + cseq;
	
				if (start == stop)
					throw MyError("must take more than one base from continuous sequence " + cseq);
				if (sctg == "-")
					sctg.clear();
	
				if (sctg.empty() ? direction != "-" : direction != "R" && direction != "F")
					throw MyError("bad direction " + direction);
	
				if (cseq == mctg)
				{
					if (length)
						throw MyError("metacontig " + mctg + " contains itself");
					need_start = true;
				}
				else
				{
					if (!length)
					{
						meta_contig.replace(db);
						meta_contig->name(mctg);
						meta_contig->created_in_oid(stage.oid());
					}
					ObjectHandle<mekano::Cut> cut(db);
					fix_conseq_name(&cseq);
					cut->cseq_oid(lookup(db, "ContinuousSequence", cseq));
					cut->start(start);
					cut->stop(stop);
					if (!sctg.empty())
					{
						cut->source_oid(lookup(db, "Supercontig", sctg));
						if (direction != "R" && direction != "F")
							throw MyError("bad direction " + direction);
						cut->source_forward(bool2eyedb(direction == "F"));
					}
					length += std::abs(int(stop) - int(start)) + 1;
					meta_contig->cuts(meta_contig->cuts_cnt(), cut);
				}
			}
			if (!f->get(c))
				throw MyError("I/O error");
			if (c != '\n')
				throw MyError(std::string("expected a newline, found: ") + c);
		}
		cleanup(&trans, meta_contig, &length);
		if (!f->eof())
			throw MyError("I/O error");
		trans.ok();
	}
}

void
insert_meta_contigs(Ifstream * f, ObjectHandle<mekano::Stage> stage)
{
	try
	{
		meta_contigs(f, stage);
	}
	catch (eyedb::Exception & e)
	{
		throw MyError("metacontigs: " + f->tag() + ": " + to_string(e));
	}
	catch (MyError & e)
	{
		throw MyError("metacontigs: " + f->tag() + ": " + e.toString());
	}
}

#if 0
void
meta_contigs(ObjectHandle<mekano::Stage> stage, std::string const & n)
{
	Ifstream f(n);
	if (!f)
		throw MyError("count not open " + n + ": " + strerror(errno));
	insert_meta_contigs(f, stage);
}
#endif

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
	argv += options(argc, argv, &mode, &remove_existing);
	
	std::string db_name(arg(argv++, "database name"));	
	std::istringstream sstage(arg(argv++, "Stage number"));
	Ifstream meta_contigs(arg(argv++, "metacontig info"));
	if (*argv)
		usage("too many arguments");

	unsigned stage_id;
	if (!(sstage >> stage_id) || sstage.get() != EOF)
		usage("bad stage id");
	
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
		return fail(std::clog << myname << ": open: " << db_name, e);
	}
	eyedb::Oid stage;
	try
	{
		TransactionControl trans(&db);
		stage = lookup(&db, "Stage", stage_id);
	}
	catch (eyedb::Exception const & e)
	{
		return fail(std::clog << myname << ": finding stage " << stage_id, e);
	}
	catch (MyError & e)
	{
		return fail(std::clog << myname << ": finding stage " << stage_id, e);
	}
	if (remove_existing)
		try
		{
			remove_query(&db, "select Metacontig.created_in.id==" + to_string(stage));
		}
		catch (eyedb::Exception const & e)
		{
			return fail(std::clog << myname << ": removing Supercontigs and Metacontigs", e);
		}
		catch (MyError & e)
		{
			return fail(std::clog << myname << ": error removing ", e);
		}
	stats.time(db);
	try
	{
		read_meta_contigs(meta_contigs, &db, stage);
	}
	catch (eyedb::Exception const & e)
	{
		return fail(std::clog << myname << ": " << meta_contigs.fname() << ":" << meta_contigs.lineno(), e);
	}
	catch (MyError & e)
	{
		return fail(std::clog << myname << ": " << meta_contigs.fname() << ":" << meta_contigs.lineno(), e);
	}
	return 0;
}
#endif
