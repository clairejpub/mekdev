/*	Stuart Pook, Genoscope 2007 (enscript -E $%) (Indent ON)
 *	$Id: run_clonelinks.cc 797 2007-06-06 13:36:20Z stuart $
 */
#include <iostream>
#include <string>
#include "mekano.h"
#include "common.h"
#include <utils/eyedb.h>
#include <utils/sctg_data.h>

char const * myname;

int
main(int argc, char *argv[])
{
	if ((myname = strrchr(argv[0], '/')) == 0 || *++myname == 0)
		myname = argv[0];
	if (argc < 4)
	{
		std::cerr << myname << ": missing arguments" << std::endl;
		return 1;
	}
	char const * db_name = argv[1];
	int stage_number = atoi(argv[2]);
	unsigned group = atoi(argv[3]);
	
	mekano::mekano initializer(argc, argv);
	
	eyedb::Database::OpenFlag mode = eyedb::Database::DBRWLocal;
	eyedb::Connection conn;
	eyedb::Database db(db_name);
	eyedb::Exception::setMode(eyedb::Exception::ExceptionMode);
	try
	{
		conn.open();
		db.open(&conn, mode);
	}
	catch (eyedb::Exception & e)
	{
		std::clog << myname << ": open connection & db" << e;
		return 1;
	}
	ObjectHandle<mekano::Stage> stage;
	ObjectHandle<mekano::Calculated> counts;
	{
		TransactionControl trans(&db);
		stage.replace(&db, stage_number);
		counts.replace(&db, select_one(&db, counts.class_name()));
	}

	assert(stage->groups_cnt() > group);
	clonelinks(counts, stage->groups(group), false);
	return 0;
}
