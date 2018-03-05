/*	Stuart Pook, Genescope, 2006 (enscript -E $%) (Indent ON)
 *	$Id: banks.cc 570 2007-03-12 11:22:07Z stuart $
 */
#include <iostream>
#include <string>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iomanip>
#include <locale>

#include <utils/eyedb.h>
#include <boost/bind.hpp>

#include "mekano.h"
#include "common.h"
/*
		std::string c(*comment++);
		while (*comment)
			c += std::string(" ") + *comment++;
*/

void
add_group(ObjectHandle<mekano::Stage> stage, std::string const & group_name, std::string const & comment)
{
	ObjectHandle<mekano::GroupName> gn(stage.database());
	gn->name(group_name);
	if (!comment.empty())
	{
#if 0			
		std::locale loc();
		std::isprint<char>(*c.begin(), loc);
		
//			if (find_if(c.begin(), c.end(), boost::bind(std::logical_not<bool>(), bind(std::isprint<char>, _1, boost::ref(loc)))) != c.end())
//				throw MyError("comment contains unprintable characters");
		std::clog << "comment " << c << std::endl;
#endif
		gn->comment(comment);
	}
	gn.store();
	ObjectHandle<mekano::Group> group(stage.database());
	group->name(gn);
	stage->groups(stage->groups_cnt(), group);
	stage->store();
}
#ifdef NEED_MAIN
char const * myname;

int
main(int argc, char *argv[])
{
	if ((myname = strrchr(argv[0], '/')) == 0 || *++myname == 0)
		myname = argv[0];
	mekano::mekano initializer(argc, argv);
	
	eyedb::Database::OpenFlag mode = eyedb::Database::DBRW;
	char const * change_dir;
	argv += options(argc, argv, &mode, 0, &change_dir);
	
	std::string db_name(arg(argv++, "database name"));	

	std::istringstream stage_string(arg(argv++, "stage number"));
	unsigned stage_id;
	if (!(stage_string >> stage_id) || stage_string.get() != EOF)
		usage("bad stage id");

	std::string group_name(arg(argv++, "group name"));

	core_directory(change_dir);
	
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

	try
	{
		add_group(&db, stage_id, group_name, argv);
	}
	catch (eyedb::Exception & e)
	{
		exit(fail(std::clog << myname << ": eyedb::Exception adding Group", e));
	}
	catch (MyError & e)
	{
		exit(fail(std::clog << myname << ": error adding Group", e));
	}
	return 0;
}
#endif
