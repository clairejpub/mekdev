/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: sctg_check.cc 723 2007-05-10 10:40:50Z stuart $
 */
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <iomanip>

#include "mekano.h"
#include <utils/templates.h>
#include <utils/iterators.h>
#include <utils/eyedb.h>
#include <utils/sctg_data.h>

#include "common.h"
char const * myname;

namespace
{
	void
	cleanup
	(
		eyedb::Database * const db,
		std::string const &  scaf,
		std::vector<ReadContigInfo> * top_level,
		bool const check_conseqs,
		bool const check_bottom_sctgs
	)
	{
		if (scaf.size())
		{
//			std::cout << "cleanup: " << scaf << std::endl;
			ObjectHandle<mekano::Supercontig> sctg(db, scaf);
			if (sctg)
			{
				SupercontigData data(sctg);
				check_read_config_info(*top_level, data, check_conseqs, check_bottom_sctgs);
				data.bottom_sctgs();
				data.top_sctgs();
			}
			else
				throw MyError("unknown supercontig " + scaf);
		}
		top_level->clear();
	}
	
	bool
	get_forward(std::string const & s)
	{
		if (s == "F")
			return true;
		if (s == "R")
			return false;
		throw MyError("bad forward indicator " + s);
	}
	
	void
	read_scafs(Ifstream & f, eyedb::Database * const db, bool check_conseqs, bool check_bottom_sctgs)
	{
		TransactionControl trans(db);
		std::clog << myname
			<< ": checking " << (check_conseqs ? "continuous sequences" : "contigs") << " and "
			<< (check_bottom_sctgs ? "bottom" : "top") << " Supercontigs"
			<< " from " << f.fname() << std::endl;
		f.unsetf(std::ios_base::skipws);
		char c;
		std::vector<ReadContigInfo> top_level;
		std::string name;
		while (f >> c)
		{
			if (c == '>')
			{
				cleanup(db, name, &top_level, check_conseqs, check_bottom_sctgs);
				if (!(f >> name))
					throw MyError("read of supercontig name failed");
			}
			else if (c ==  'G')
			{
				if (!(f >> skip("AP") >> skipspaces))
					throw MyError("gap header read failed");
				unsigned size;
				if (f.peek() == '?')
				{
					f.get();
					top_level.push_back(ReadContigInfo());
				}
				else if (!(f >> size))
					throw MyError("gap size read failed");
				else
					top_level.push_back(ReadContigInfo(std::max(unsigned(minimum_gap_size), size)));
			}
			else
			{
				std::string cseq;
				unsigned start;
				unsigned stop;
				std::string cforward;
				std::string from_name;
				if (!(f >> cseq >> skipspaces >> start >> skipspaces >> stop >> skipspaces >> from_name >> skipspaces >> cforward))
					throw MyError("read of contig information failed");
				cseq = c + cseq;
				if (from_name != "-")
					top_level.push_back(ReadContigInfo(cseq, start, stop, from_name, get_forward(cforward)));
				else	
				{
					if (cforward != "-")
						throw MyError("expected a '-' for the direction");
					if (stop <= start)
						throw MyError("start must be greater than stop: " + cseq);
					top_level.push_back(ReadContigInfo(cseq, start, stop));
				}
			}
			if (!(f >> skipspaces))
				throw MyError("skipspaces failed");
			if (!f.get(c))
				throw MyError("failed to read eol");
			if (c != '\n')
				throw MyError(std::string("expected a newline, found: ") + c + "(" + to_string(int(c)) + ")");
		}
		cleanup(db, name, &top_level, check_conseqs, check_bottom_sctgs);
		if (!f.eof())
			throw MyError("I/O error");
	}
	
	void
	bad_option(char c)
	{
		std::clog << myname << ": bad option: " << char(c) << std::endl;
		exit(1);
	}
	
	unsigned
	options
	(
		int argc,
		char *argv[],
		eyedb::Database::OpenFlag * mode,
		bool * remove,
		char const * * chdir,
		bool * alternative,
		unsigned * transaction_size,
		bool * alternative2
	)
	{
		if ((myname = strrchr(argv[0], '/')) == 0 || *++myname == 0)
			myname = argv[0];
	
		*mode = eyedb::Database::DBReadLocal;
	
		if (chdir)
			*chdir = ".";
		if (remove)
			*remove = false;
		if (alternative)
			*alternative = false;
		if (alternative2)
			*alternative2 = false;
	
		char const * const flags = "+t:C:lrRaA";
		while (int c = getopt(argc, argv, flags))
			if (c == -1)
				break;
			else if (c == 't')
				if (transaction_size)
					*transaction_size = atoi(optarg);
				else
					bad_option(c);
			else if (c == 'C')
			{
				if (chdir)
					*chdir = optarg;
			}
			else if (c == 'a')
				if (alternative)
					*alternative = true;
				else
					bad_option(c);
			else if (c == 'A')
				if (alternative2)
					*alternative2 = true;
				else
					bad_option(c);
			else if (c == 'R')
				if (remove)
					*remove = true;
				else
					bad_option(c);
			else if (c == 'r')
				*mode = eyedb::Database::DBRead;
			else if (c == 'l')
				*mode = eyedb::Database::DBReadLocal;
			else
			{
				std::clog << myname << ": bad option: " << char(optopt) << ", legal options are: ";
				std::clog << (flags + (flags[0] == '+')) << std::endl;
				exit(1);
			}
		return optind ;
	}
}

int
main(int argc, char *argv[])
{
	if ((myname = strrchr(argv[0], '/')) == 0 || *++myname == 0)
		myname = argv[0];
	Statistics stats;
	mekano::mekano initializer(argc, argv);
	
	eyedb::Database::OpenFlag mode = eyedb::Database::DBRead;
	char const * change_dir;
	bool check_conseqs;
	bool check_bottom_sctgs;
	argv += options(argc, argv, &mode, 0, &change_dir, &check_conseqs, 0, &check_bottom_sctgs);
	
	std::string const db_name(arg(argv++, "database name"));
	
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
		return fail(std::clog << myname << ": open " << db_name, e);
	}
	stats.time(db);
	while (*argv)
	{
		Ifstream supercontigs(*argv++);
		core_directory(change_dir, *argv);
		try
		{
			read_scafs(supercontigs, &db, check_conseqs, check_bottom_sctgs);
		}
		catch (eyedb::Exception & e)
		{
			return fail(std::clog << myname << ": " << supercontigs.tag() << ": failed", e);
		}
		catch (MyError & e)
		{
			return fail(std::clog << myname << ": " << supercontigs.tag() << ": failed" , e);
		}
	}
	return 0;
}
