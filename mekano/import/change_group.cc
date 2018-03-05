/*	$Id: change_group.cc 685 2007-04-23 14:37:14Z stuart $
 *	Stuart Pook, Genescope, 2006
 */
#include <iostream>
#include <string>
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iomanip>
#include <set>

#include <utils/templates.h>
#include <utils/eyedb.h>

#include "common.h"
#include "mekano.h"

namespace
{
	std::string const what("moving Supercontigs");
	ObjectHandle<mekano::Group>
	find_group(eyedb::Oid const & groupname, mekano::Stage * stage)
	{
		int const n = stage->groups_cnt();
		for (int i = n - 1; i >= 0; i--)
			if (mekano::Group * g = stage->groups(i))
				if (g->name_oid() == groupname)
					return g;
		ObjectHandle<mekano::Group> group(stage->eyedb::Struct::getDatabase());
		group->name_oid(groupname);
		stage->groups(n, group);
		return group;
	}
}

void
change_group
(
	ObjectHandle<mekano::Stage> stage,
	Ifstream * in,
	std::string const & from_name,
	std::string const & to_name
)
{
	eyedb::Database * const db = stage.database();
	TransactionControl transaction(db, 1000);
	mekano::Group	* from = find_group(lookup(db, "GroupName", from_name), stage);
	mekano::Group	* to = find_group(lookup(db, "GroupName", to_name), stage);

	std::clog << myname << ": " + what + " in " << in->fname() << std::endl;
	try
	{
		std::string sctg_name;
		while (in->skip_white_space() >> sctg_name)
		{
			ObjectHandle<mekano::Supercontig> sctg(db, sctg_name);
			if (!sctg)
				throw MyError("unknown supercontig " + sctg_name);
			suppress_from_group(from, sctg);
			add_to_group(to, sctg);
			transaction.checkpoint(stage);
		}
		if (!in->eof())
			throw MyError("I/O error");
	}
	catch (eyedb::Exception & e)
	{
		throw MyError(in->tag() + ": eyedb::Exception " + what + ": " + to_string(e));
	}
	catch (MyError & e)
	{
		throw MyError(in->tag() + ": " + what + ": " + to_string(e));
	}
	stage.store();
	transaction.ok();
}
#if 0
char const * myname;

int
main(int argc, char *argv[])
{
	if ((myname = strrchr(argv[0], '/')) == 0 || *++myname == 0)
		myname = argv[0];
	char const * change_dir;
	Statistics stats;
	mekano::mekano initializer(argc, argv);
	
	eyedb::Database::OpenFlag mode = eyedb::Database::DBRW;
	unsigned transaction_size = 1000;
	argv += options(argc, argv, &mode, 0, &change_dir, 0, &transaction_size);
	
	std::string const db_name(arg(argv++, "database name"));

	std::istringstream stage_number_s(arg(argv++, "stage number"));
	unsigned stage_number;
	if (!(stage_number_s >> stage_number) || stage_number_s.get() != EOF)
		usage("bad stage id");

	std::string const from_groupname_key(arg(argv++, "original group name"));
	std::string const to_groupname_key(arg(argv++, "destination group name"));
	
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
		return fail(std::clog << myname << ": open " << db_name , e);
	}

	eyedb::Oid from_groupname;
	eyedb::Oid to_groupname;
	try
	{
		TransactionControl trans(&db);
		from_groupname = lookup(&db, "GroupName", from_groupname_key);
		to_groupname = lookup(&db, "GroupName", to_groupname_key);
	}
	catch (eyedb::Exception & e)
	{
		return fail(std::clog << myname << ": EyeDB exception finding groupnames", e);
	}
	catch (MyError & e)
	{
		return fail(std::clog << myname << ": exception finding groupnames", e);
	}
	stats.time(&db);
	while (*argv)
	{
		Ifstream supercontigs(*argv++);
		core_directory(change_dir, *argv);
		try
		{
			TransactionControl transaction(&db, transaction_size);
			ObjectHandle<mekano::Stage> stage(&db, stage_number);
			std::string sctg_name;
			mekano::Group	* from = find_group(from_groupname, stage);
			mekano::Group	* to = find_group(to_groupname, stage);
			std::clog << myname << ": moving sctgs in " << supercontigs.fname() << std::endl;
			while (supercontigs >> sctg_name)
			{
				ObjectHandle<mekano::Supercontig> sctg(&db, sctg_name);
				if (!sctg)
					return fail(std::clog << myname << ": unknown supercontig " << sctg_name);
				suppress_from_group(from, sctg);
				add_to_group(to, sctg);
				transaction.checkpoint(stage);
			}
			stage.store();
			transaction.ok();
		}
		catch (eyedb::Exception & e)
		{
			return fail(std::clog << myname << ": EyeDB exception at " << supercontigs.tag(), e);
		}
		catch (MyError & e)
		{
			return fail(std::clog << myname << ": moving at " << supercontigs.tag(), e);
		}
		catch (...)
		{
			std::clog << "foo" << std::endl;
		}
	}
	return 0;
}
#endif
#if 0
	
	class move : public std::unary_function<std::string, void>
	{
		ObjectHandle<mekano::Group> _from;
		ObjectHandle<mekano::Group> _to;
		ObjectHandle<mekano::Stage> _stage;
		TransactionControl * const _transaction;
	public:
		move
		(
			ObjectHandle<mekano::Group> const & from,
			ObjectHandle<mekano::Group> const & to,
			ObjectHandle<mekano::Stage> const & stage,
			TransactionControl * transaction
		) :
			_from(from),
			_to(to),
			_stage(stage),
			_transaction(transaction)
		{}
		result_type operator()(argument_type const & sctg_name)
		{
			eyedb::Oid const sctg = lookup(_from.database(), "Supercontig", sctg_name);
			_from->rmvfrom_sctgs(sctg);
			_to->addto_sctgs(sctg);
			_transaction->checkpoint(_stage);
		}
	};

			for_each
			(
				std::istream_iterator<std::string>(supercontigs),
				std::istream_iterator<std::string>(),
				move
				(
					find_group(from_groupname, stage, from_groupname_key),
					find_group(to_groupname, stage, to_groupname_key),
					stage,
					&transaction
				)
			);
#endif
