/*	$Id: newstage.cc 792 2007-06-04 14:50:16Z stuart $
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
#include <utils/utilities.h>

#include "common.h"
#include "mekano.h"

namespace
{
	void
	copy_set(std::ostringstream * query, std::string const & oname, std::string const & nname)
	{
#if 1
		*query << "  new_set := " << nname << ";\n";
		*query << "  for (z in contents " << oname << ")\n";
		*query << "    add z to new_set;\n";
#else
		*query << "  tmp := " << oname << ".getLiteralOid();\n";
		*query << "  tmp.setLiteralObject();\n";
		*query << "  " << oname << " := new set<Supercontig *>();\n";
		*query << "  " << nname << " := tmp;\n";
#endif
	}
	void
	copy_array(std::ostringstream * query, std::string const & name)
	{
		*query << " for (y in contents o.groups[g]." << name << ")\n";
		*query << " {\n";
		*query << "  i := y.index;\n";
		*query << "  old_sctg_set := y.value;\n";
#if 1
		copy_set(query, "old_sctg_set.s", "(n.groups[g]." + name + "[i] := new SupercontigSet()).s");
#else
		*query << "  old_sctg_set.s.setLiteralObject();\n";
		*query << "  n.groups[g]." + name + "[i] := new SupercontigSet(s : old_sctg_set.s);\n";
#endif
		*query << " };\n";
	}
}

int
copy_stage(eyedb::Database * const db, std::string const & type)
{
//	std::clog << myname << ": copied stage to new stage of type " << type << std::endl;
	Performance perf("copied stage to new stage of type " + type, 0);
	std::ostringstream query;
	query << "id := -(select -s.id from Stage s order by -s.id)[0];\n";
	query << "o := (select one Stage.id = id);\n";
	query << "t := (select one StageType.name = \"" << type << "\");\n";
	query << "n := Stage(id : o.id + 1, stage_type : t);\n";
	query << "for (g := 0; g < o.groups[!]; g++)\n";
	query << "{\n";
	query << " n.groups[g].name := o.groups[g].name;\n";
	copy_set(&query, "o.groups[g].sctgs", "n.groups[g].sctgs");
	copy_array(&query, "length_counts");
	copy_array(&query, "ctg_counts");
	copy_array(&query, "cseq_counts");
	query << "};\n";
	query << "n.id;\n";
	std::string q(query.str());
//	std::clog << q << std::flush;
	eyedb::ValueArray results;
	eyedb::OQL(db, q.c_str()).execute(results);
	if (results.getCount() == 0)
		throw MyError("no Stages found");
	if (int const * ip = value_cast<int>(&results[0]))
		return *ip;
	return value_cast<long long>(results[0]);
}

#ifdef NEED_MAIN
char const * myname;
static void
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

int
main(int argc, char *argv[])
{
	if ((myname = strrchr(argv[0], '/')) == 0 || *++myname == 0)
		myname = argv[0];
	char const * change_dir;
	Statistics stats;
	mekano::mekano initializer(argc, argv);
	
	eyedb::Database::OpenFlag mode = eyedb::Database::DBRW;
	bool remove_existing = false;
	argv += options(argc, argv, &mode, &remove_existing, &change_dir);
	
	std::string const db_name(arg(argv++, "database name"));
	std::string stage_type = arg(argv++, "new stage type");
	std::string group_name;
	if (*argv)
		group_name = *argv++;
	if (*argv)
		usage("too many arguments");

	core_directory(change_dir, *argv);
	
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

	int last_stage_id;
	try
	{
		TransactionControl trans(&db);

		eyedb::ValueArray results;
		eyedb::OQL(&db, "select -s.id from Stage s order by -s.id;").execute(results);
		if (results.getCount() == 0)
			return fail(std::clog << myname << ": no Stages found");
		else if (int const * ip = value_cast<int>(&results[0]))
			last_stage_id = -*ip;
		else
			last_stage_id = -value_cast<long long>(results[0]);
	}
	catch (eyedb::Exception & e)
	{
		return fail(std::clog << myname << ": making old Stage: ", e);
	}

	if (remove_existing && last_stage_id >= 0)
		try
		{
			remove_query(&db, "select Metacontig.created_in.id==" + to_string(last_stage_id));
			remove_query(&db, "select Supercontig.created_in.id=" + to_string(last_stage_id));
			remove(&db, "Stage", last_stage_id);
			--last_stage_id;
		}
		catch (eyedb::Exception & e)
		{
			return fail(std::clog << myname << ": removing existing", e);
		}

	stats.time(&db);
	try
	{
		TransactionControl trans(&db);
		copy_stage(&db, last_stage_id, stage_type, group_name);
		trans.ok();
		std::cout << (last_stage_id + 1) << std::endl;
	}
	catch (eyedb::Exception & e)
	{
		return fail(std::clog << myname << ": making new Stage: ", e);
	}
	catch (MyError & e)
	{
		return fail(std::clog << myname << ": making new Stage: ", e);
	}
	return 0;
}
#endif
