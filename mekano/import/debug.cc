/*	$Id: debug.cc 452 2006-12-12 09:35:00Z stuart $
 *	Stuart Pook, Genescope, 2006
 */
#include <assert.h>
#include "mekano.h"

int
main(int argc, char * argv[])
{
	assert(argc > 2);
	try
	{
		mekano::mekano initializer(argc, argv);
		eyedb::Exception::setMode(eyedb::Exception::ExceptionMode);
		eyedb::Connection conn;
		conn.open();
		eyedb::Database * db = new mekano::mekanoDatabase(argv[1]);
		db->open(&conn, eyedb::Database::DBRW);

		std::string const filename(db->getName() + std::string(argv[2]) + std::string(".goo.dat"));

		db->transactionBeginExclusive();
		db->createDatafile(0, filename.c_str(), argv[2], 1301 * 1024, 16, eyedbsm::PhysicalOidType);
		db->transactionCommit();
		std::clog << "after transactionCommit " << filename << std::endl;

		if (argc > 3)
		{
			db->close();
			std::clog << "after close " << filename << std::endl;
		}
	}
	catch (eyedb::Exception & e)
	{
		std::clog << "exception " << e << std::endl;
		return 1;
	}
	std::clog << "all went well" << std::endl;
	return 0;
}
