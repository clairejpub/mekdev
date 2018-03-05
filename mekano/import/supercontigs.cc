/*	$Id: supercontigs.cc 452 2006-12-12 09:35:00Z stuart $
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
#include <utils/sctg_infos.h>

#include "common.h"
#include "mekano.h"
char const * myname;

namespace
{

void
usage(std::string const & message)
{
	std::clog << "usage: " << myname << ": " << message << std::endl;
	exit(1);
}

void
cleanup
(
	TransactionControl * trans,
	ObjectHandle<mekano::Supercontig> const & scaf,
	unsigned * length,
	unsigned const hole_size,
	ObjectHandle<mekano::Stage> stage,
	eyedb::IndexImpl const * const idximpl
)
{
	if (scaf)
	{
		SupercontigData data(scaf);
		finish_sctg(&data, stage, hole_size);
		if (*length != unsigned(scaf->length()))
			throw MyError("internally and externally calculated lengths not equal: " + to_string(*length) + " " + to_string(scaf->length()));
		if (scaf->regions_cnt() != unsigned(scaf->nb_cseq()))
			throw MyError("internally and externally calculated cseq counts not equal: " + to_string(scaf->regions_cnt()) + " " + to_string(data.nb_cseq()));
		trans->checkpoint(stage);
	}
	*length = 0;
}

void
read_scafs
(
	Ifstream & f,
	eyedb::Database * const db,
	unsigned const hole_size,
	ObjectHandle<mekano::Stage> stage,
	eyedb::IndexImpl const * idximpl
)
{
	f.unsetf(std::ios_base::skipws);
	TransactionControl trans(db, 1000);
	char c;
	unsigned length;
	ObjectHandle<mekano::Supercontig> scaf;
	std::clog << myname << ": adding Supercontigs from " << f.fname() << std::endl;
	while (f >> c)
	{
		if (c == '>')
		{
			std::string name;
			cleanup(&trans, scaf, &length, hole_size, stage, idximpl);
			if (!(f >> name >> skipspaces))
				throw MyError("read of supercontig name failed");
			scaf.replace(db);
			scaf->name(name);
		}
		else if (c ==  'G')
		{
			std::string size;
			if (!(f >> skip("AP") >> skipspaces >> size >> skipspaces))
				throw MyError("gap size read failed");
			mekano::Region * region = scaf->regions(scaf->regions_cnt() - 1);
			if (!size.compare("?"))
			{
				region->gap_after(-1);
				length += hole_size;
			}
			else
			{
				int gap;
				std::istringstream issize(size);
				if (!(issize >> gap) || issize.get() != EOF)
					throw MyError("bad gap: " + size);
				region->gap_after(std::max(1, gap));
				length += region->gap_after();
			}
		}
		else
		{
			if (scaf->regions_cnt())
			{
				mekano::Region * region = scaf->regions(scaf->regions_cnt() - 1);
				eyedb::Bool null;
				region->gap_after(&null);
				if (null)
					region->gap_after(0);
			}

			unsigned start;
			unsigned stop;
			std::string cseq;
			if (!(f >> skipspaces >> cseq >> skipspaces >> start >> skipspaces >> stop >> skipspaces))
				throw MyError("read of contig information failed");
			if (!(f >> skip("-") >> skipspaces >> skip("-") >> skipspaces))
				throw MyError("read of dashes failed");

			cseq = c + cseq;

			ObjectHandle<mekano::Region> region(db);
			if (start == stop)
				throw MyError("start and stop must be different");
			region->start_on_first(start);
			region->stop_on_last(stop);
			scaf->regions(scaf->regions_cnt(), region);
			fix_conseq_name(&cseq);
			region->sequence_oid(lookup(db, "ContinuousSequence", cseq));
			region->first_oid(region->sequence_oid());
			region->last_oid(region->sequence_oid());
			length += stop - start + 1;
		}
		if (!f.get(c))
			throw MyError("I/O error on reading a newline");
		if (c != '\n')
			throw MyError(std::string("expected a newline, found: ") + c);
	}
	cleanup(&trans, scaf, &length, hole_size, stage, idximpl);
	stage.store();
	if (!f.eof())
		throw MyError("I/O error");
	trans.ok();
}

char const *
arg(char * argv[], std::string const & what)
{
	if (!*argv)
		usage("missing " + what);
	return *argv;
}

int
supercontigs
(
	std::string const & db_name,
	bool const remove_existing,
	eyedb::Database::OpenFlag const mode,
	unsigned const hole_size,
	unsigned const stage_id,
	char * argv[],
	Ifstream & input,
	Statistics * stats
)
{
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
		return fail(std::clog << myname << ": eyedb::Exception& opening \"" << db_name << "\": " << e);
	}
	ObjectHandle<mekano::Stage> stage;
	try
	{
		TransactionControl trans(&db);
		if (!stage.replace(&db, stage_id))
			return fail(std::clog << myname << ": unknown stage " << stage_id);

		if (remove_existing && stage->groups(0)->sctgs_cnt())
		{
			remove_query(&db, "contents(" + to_string(stage.oid()) + ".groups[0].sctgs)");
			stage->groups(0)->sctgs()->empty();
			trans.ok();
		}
		if (unsigned n = stage->groups(0)->sctgs_cnt())
			return fail(std::clog << myname << ": Stage " << stage_id << " already has " << n << " Supercontigs");
	}
	catch (eyedb::Exception & e)
	{
		return fail(std::clog << myname << ": emptying the Stage: ", e);
	}
	stats->time(db);
	try
	{
		eyedb::IndexImpl const * const idximpl = new eyedb::IndexImpl(eyedb::IndexImpl::BTree);
		read_scafs(input, &db, hole_size, stage, idximpl);
		for (; *argv; argv++)
		{
			input.open(*argv);
			read_scafs(input, &db, hole_size, stage, idximpl);
		}
	}
	catch (eyedb::Exception & e)
	{
		return fail(std::clog << myname << ": " << input.fname() << ":" << input.lineno() << ": eyedb::Exception&: " << e);
	}
	catch (MyError & e)
	{
		return fail(std::clog << myname << ": " << input.fname() << ":" << input.lineno() << ": error processing " << e);
	}
	return 0;
}
}

int
main(int argc, char * argv[])
{
	if ((myname = strrchr(argv[0], '/')) == 0 || *++myname == 0)
		myname = argv[0];
	Statistics stats;
	mekano::mekano initializer(argc, argv);
	
	bool remove_existing = false;
	eyedb::Database::OpenFlag mode = eyedb::Database::DBRW;
	char const * change_dir;
	argv += options(argc, argv, &mode, &remove_existing, &change_dir);
	
	std::string db_name(arg(argv++, "database name"));

	std::istringstream stage_string(arg(argv++, "Stage number"));
	unsigned stage_id;
	if (!(stage_string >> stage_id) || stage_string.get() != EOF)
		usage("bad stage id");

	std::istringstream hole_size_ss(arg(argv++, "hole size"));
	unsigned hole_size;
	if (!(hole_size_ss >> hole_size) || hole_size_ss.get() != EOF)
		return fail(std::clog << myname << ": bad hole size");

	Ifstream input(arg(argv++, "file containing the super contigs"));
	core_directory(change_dir, *argv);

	return supercontigs(db_name, remove_existing, mode, hole_size, stage_id, argv, input, &stats);
}
