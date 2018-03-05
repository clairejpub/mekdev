/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: mekanob.cc 799 2007-06-06 15:44:22Z stuart $
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
#include <math.h>
#include <errno.h>
#include <iomanip>
#include <getopt.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>

#include "mekano.h"
#include <utils/templates.h>
#include <utils/iterators.h>
#include <utils/eyedb.h>
#include <utils/sctg_data.h>
#include <utils/utilities.h>
#include <boost/tuple/tuple.hpp>
#include <boost/utility.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <eyedb/utils.h>
#include "common.h"
char const * myname;

#define	sys(s) (void((s) >= 0 || (_sys(#s), 1)))

namespace
{
	void
	_sys(char const * what)
	{
		std::clog << myname << ": " << what << " failed: " << strerror(errno);
		exit(1);
	}
	
	void
	get_file(Ifstream & in, std::string const & tag, Ifstream * file)
	{
		if (file->is_open())
			throw MyError("can only specify one file containing " + tag);
		std::string fname;
		if (!(in >> fname))
			throw MyError("no file specified for " + tag);
		fname = qualify(fname, in.fname());
		file->open(fname.c_str());
		if (!*file)
			throw MyError("count not open " + fname + ": " + strerror(errno));
	}
	
	inline void
	check_file(Ifstream const & file, std::string const & tag)
	{
		if (!file)
			throw MyError("must specify a list of " + tag);
	}

	struct create_algorithm
	{
		typedef void result_type;
		result_type operator()(eyedb::Database * db, std::string const & name, std::string const & comment)
		{
			ObjectHandle<mekano::Algorithm> algo(db);
			algo->name(name);
			algo->comment(comment);
			algo.store();
		}
	};

	struct InitialisationFile
	{
		std::string database;
		typedef std::list<std::string> users_t;
		users_t users;
		Ifstream banks;
		Ifstream reads;
		Ifstream ctg;
		int unsized_gap;
		Ifstream stagetypes;
		Ifstream mctg;
		Ifstream sctg;
		typedef std::vector<boost::shared_ptr<Ifstream> > stages_t;
		stages_t stages;
		algorithms_t algorithms;
		matches_t matches;
		InitialisationFile(Ifstream & in);
	};
	InitialisationFile::InitialisationFile(Ifstream & in)
	{
		unsized_gap = -1;
		in.unsetf(std::ios_base::skipws);
		std::string tag;
		while (in.skip_white_space() >> tag >> skipspaces)
		{
			if (tag == "database")
			{
				if (!database.empty())
					throw MyError("can only specify the database once");
				if (!(in >> database))
					throw MyError("no database specified");
			}
			else if (tag == "users")
			{
				while (in.peek() != '\n')
				{
					std::string us;
					if (!(in >> us >> skipspaces))
						throw MyError("failed to read the list of users");
					users.push_back(us);
				}
			}
			else if (tag == "algo")
			{
				std::string algo;
				if (!(in >> algo >> skipspaces))
					throw MyError("no algorithm specified");
				std::string comment;
				if (!getcomment(in, comment))
					throw MyError("failed to read the algorithm's comment");
				algorithms.push_back(std::make_pair(algo, comment));
			}
			else if (tag == "banks")
				get_file(in, tag, &banks);
			else if (tag == "reads")
				get_file(in, tag, &reads);
			else if (tag == "ctg")
				get_file(in, tag, &ctg);
			else if (tag == "sctg")
				get_file(in, tag, &sctg);
			else if (tag == "mctg")
				get_file(in, tag, &mctg);
			else if (tag == "stagetypes")
				get_file(in, tag, &stagetypes);
			else if (tag == "stage")
				stages.push_back(get_file(in));
			else if (tag == "match")
			{
				std::string algo;
				if (!(in >> algo >> skipspaces))
					throw MyError("no algorithm specified");
				matches.push_back(std::make_pair(algo, get_file(in)));
			}
			else if (tag == "unsized_gap")
			{
				unsigned u;
				if (!(in >> u))
					throw MyError("no gap size specified");
				unsized_gap = u;
			}
			else
				throw MyError("unknown tag " + tag);
			if (!(in >> skipspaces))
				throw MyError("failed to skip spaces!");
			switch (Ifstream::int_type c = in.get())
			{
			case '#':
				in.skipline();
			case '\n':
				break;
			default:
				throw MyError("extra junk: " + to_string(char(c)));
			}
		}
		
		if (database.empty())
			throw MyError("must specify a database");
		check_file(banks, "banks");
		check_file(reads, "reads");
		check_file(ctg, "ctg");
		check_file(stagetypes, "stagetypes");
		check_file(sctg, "sctg");
		if (unsized_gap < 0)
			throw MyError("must specify the unsized_gap");
	}

#if 0
	std::string
	get_datadir()
	{
		eyedb::ServerConfig * const server_cfg = eyedb::ServerConfig::getInstance();
		if (!server_cfg)
			throw MyError("no eyedb::ServerConfig::getInstance");

		char const * const datadir = server_cfg->getValue("datadir");
		if (!datadir)
			throw MyError("no datadir value in server config");
		return datadir;
	}

	std::string
	get_new_database_name(std::string base, gid_t const egid)
	{
		std::string const datadir(get_datadir());
		base += '#';
		GetEGid privilige(egid);
		if (DIR * dir = opendir(datadir.c_str()))
		{
			int n = -1;
			while (struct dirent * de = readdir(dir))
				if (base.compare(0, base.length(), de->d_name, base.length()) == 0)
					n = std::max(atoi(de->d_name + base.length()), n);
			if (closedir(dir) < 0)
				throw MyError(std::string("failed to close datadir ") + datadir);
			return base + to_string(n + 1);
		}
		else
			throw MyError(std::string("failed to open datadir ") + datadir + std::string(": ") + strerror(errno));
	}
#endif
	
	ObjectHandle<mekano::Parameters> mk_parameters(eyedb::Database * db, int unsized_gap)
	{
		ObjectHandle<mekano::Calculated> calculated(db);
		calculated.store();
		ObjectHandle<mekano::Parameters> parameters(db);
		parameters->hole_size(unsized_gap);
		parameters->calculated(calculated);
		parameters.store();
		return parameters;
	}
	
	ObjectHandle<mekano::Group> mk_group(eyedb::Database * db)
	{
		ObjectHandle<mekano::GroupName> group_name(db);
		group_name->name("global");
		group_name.store();

		ObjectHandle<mekano::Group> group(db);
		group->name(group_name);
		return group;
	}
	
	ObjectHandle<mekano::Stage> mk_stage(ObjectHandle<mekano::Group> group)
	{
		ObjectHandle<mekano::StageType> type(group.database());
		type->name("initialisation");
		type.store();

		ObjectHandle<mekano::Stage> stage(group.database());
		stage->stage_type(type);
		stage->id(0);
		stage->groups(0, group);
		stage.store();
		return stage;
	}
	struct add_user
	{
		typedef void result_type;
		result_type operator()(eyedb::Database * db, eyedb::Connection * conn, std::string const & user)
		{
			db->setUserDBAccess(conn, user.c_str(), eyedb::ReadDBAccessMode);
		}
	};
	void
	resize_shmem(eyedb::Connection * const conn, std::string const & db_name, gid_t egid, off_t size)
	{
		eyedb::Database db(string2eyedb(db_name));
		try
		{
			db.open(conn, eyedb::Database::DBRead);
		}
		catch (eyedb::Exception & e)
		{
			throw MyError("Exception opening the database" + to_string(e));
		}

		size *= 1024;
		char const * dbfile;
		db.getDatabasefile(dbfile);
#if 1
		std::string file(dbfile);
		std::string::size_type where = file.rfind('.');
		if (where == file.npos)
			throw MyError("bad dbfile " + file);
		file.replace(where + 1, file.npos, "shm");
#else
		char const * const shmfile = eyedbsm::shmfileGet(dbfile);
#endif
//		std::clog << myname << ": resize_shmem " << file << " to " << size << " bytes" << std::endl;
		GetEGid privilige(egid);
		if (truncate(file.c_str(), size) < 0)
			throw MyError("could not truncate(" + file + ", " + to_string(size) + "): " + strerror(errno));
	}
	unsigned
	linecount(std::ifstream & in)
	{
		unsigned n = 0;
		while (in.ignore(100000, '\n'))
			n++;
		in.clear();              // forget we hit the end of file
		in.seekg(0, std::ios::beg);   // move to the start of the file
		return n;
	}

	void
	update_hash_size(eyedb::Database * db, char const * name, unsigned size)
	{
		eyedb::Attribute const * attr = 0;
		eyedb::Class const * cls = 0;
		eyedb::Attribute::checkAttrPath(db->getSchema(), cls, attr, name);
		eyedb::Index * idx = 0;
 		eyedb::Attribute::getIndex(db, name, idx);
		if (!idx)
			throw MyError("could not find index " + std::string(name));
		if (!idx->asHashIndex())
			return;
		unsigned v = unsigned(pow(2, int(log(size) / M_LN2)));
		std::string hints = "key_count=" + to_string(v);
   		eyedb::IndexImpl * idximpl = 0;
		eyedb::IndexImpl::make(db, eyedb::IndexImpl::Hash, hints.c_str(), idximpl, idx->getIsString());
		if (!idximpl)
			throw MyError("could not make index implemetation for " + std::string(name));
		idx->setImplementation(idximpl);
		idx->store();
//		std::clog << myname << ": index " << name << " resized to: " << hints << std::endl;
	}

	struct mk_datafile
	{
		typedef void result_type;
		result_type operator()(eyedb::Database * db, std::string name)
		{
			std::string const file(db->getName() + std::string(".") + name + ".dat");
			enum { slotsize = 16 };
			db->createDatafile(0, file.c_str(), name.c_str(), 1301 * 1024, slotsize, eyedbsm::PhysicalOidType);
		}
	};

	struct mk_dataspace
	{
		typedef void result_type;
		result_type operator()(eyedb::Database * db, std::string name)
		{
			eyedb::Datafile const * datafile = 0;
			db->getDatafile(name.c_str(), datafile);
			db->createDataspace(name.c_str(), &datafile, 1);
		}
	};

	struct find_dataspace
	{
		typedef eyedb::Dataspace const * result_type;
		result_type operator()(eyedb::Database * db, std::string name)
		{
			eyedb::Dataspace const * dataspace = 0;
			db->getDataspace(name.c_str(), dataspace);
			return dataspace;
		}
	};

	struct set_instance_dataspace
	{
		typedef void result_type;
		result_type operator()(eyedb::Database * db, std::string name, eyedb::Dataspace const * dataspace)
		{
			if (eyedb::Schema * schema = db->getSchema())
				if (eyedb::Class * cls = schema->getClass(name.c_str()))
				{
    					cls->setDefaultInstanceDataspace(dataspace);
//					std::clog << myname << ": class " << name << " on dataspace " << name << std::endl;
				}
		}
	};

	struct set_default_dataspace
	{
		typedef void result_type;
		result_type operator()(eyedb::Database * db, std::string name, eyedb::Dataspace const * dataspace)
		{
			eyedb::Index *idx = 0;
 			eyedb::Attribute::getIndex(db, name.c_str(), idx);
			if (!idx)
				throw MyError("could not find index " + std::string(name));
			idx->setDefaultDataspace(dataspace);
//			std::clog << myname << ": index " << name << " on dataspace " << name << std::endl;
		}
	};

	void
	update_hash_sizes(eyedb::Connection * conn, std::string const & db_name, unsigned const reads_count)
	{
		eyedb::Database db(string2eyedb(db_name));
		db.open(conn, eyedb::Database::DBRW);
		db.transactionBeginExclusive();
		update_hash_size(&db, "Clone.name", reads_count / 2);
		update_hash_size(&db, "Read.name", reads_count);
		db.transactionCommit();
	}

	void
	optimisations(eyedb::Connection * conn, std::string const & db_name, unsigned const reads_count)
	{
		update_hash_sizes(conn, db_name, reads_count);

		std::vector<std::string> indexes;
		indexes.push_back("Read.name");
		indexes.push_back("Clone.name");
		indexes.push_back("Contig.name");
		indexes.push_back("Metacontig.name");
		indexes.push_back("Supercontig.name");

		std::vector<std::string> classes;
		classes.push_back(mekano::Read::_classname());
		classes.push_back(mekano::Clone::_classname());
		classes.push_back(mekano::Metacontig::_classname());
		classes.push_back(mekano::SupercontigPair::_classname());

		{
			eyedb::Database * db = new eyedb::Database(string2eyedb(db_name));
			db->open(conn, eyedb::Database::DBRW);
			TransactionControl trans(db, TransactionControl::exclusive);
			for_each(classes.begin(), classes.end(), boost::bind(mk_datafile(), db, _1));
			for_each(indexes.begin(), indexes.end(), boost::bind(mk_datafile(), db, _1));
			trans.validate();
			// bug in eyedb 2.7.13 thus cannot db->close
		}
		{
			eyedb::Database db(string2eyedb(db_name));
			db.open(conn, eyedb::Database::DBRW);
			TransactionControl trans(&db, TransactionControl::exclusive);
			for_each(classes.begin(), classes.end(), boost::bind(mk_dataspace(), &db, _1));
			for_each(indexes.begin(), indexes.end(), boost::bind(mk_dataspace(), &db, _1));
			trans.validate();
		}
		{
			eyedb::Database db(string2eyedb(db_name));
			db.open(conn, eyedb::Database::DBRW);
			TransactionControl trans(&db);
			for_each(indexes.begin(), indexes.end(), boost::bind(set_default_dataspace(), &db, _1, boost::bind(find_dataspace(), &db, _1)));
			for_each(classes.begin(), classes.end(), boost::bind(set_instance_dataspace(), &db, _1, boost::bind(find_dataspace(), &db, _1)));
			trans.validate();
		}
	}

	void
	mk_database(eyedb::Connection * conn, std::string const & db_name)
	{
		eyedb::DbCreateDescription dbdesc;
		eyedb::Database db(db_name.c_str());

		strcpy(dbdesc.dbfile, (db_name + ".dbs").c_str());
		eyedbsm::DbCreateDescription * const d = &dbdesc.sedbdesc;

		d->dbid = 0;
		d->nbobjs = 50000000;

		d->ndat = 1;
		strcpy(d->dat[0].file, std::string(db_name + ".dat").c_str());
		d->dat[0].name[0] = 0;
		d->dat[0].maxsize = 1024 * 1023 * 2;
		d->dat[0].mtype = eyedbsm::BitmapType;
		d->dat[0].sizeslot = 16;
		d->dat[0].dspid = 0;
		d->dat[0].extflags = 0;
		d->dat[0].dtype = eyedbsm::LogicalOidType;

		// I don't think that these are used but initialise just in case
		d->ndsp = 0;
		d->dbsfilesize = 0;
		d->dbsfileblksize = 0;
		d->ompfilesize = 0;
		d->ompfileblksize = 0;
		d->shmfilesize = 350000123;
		d->shmfileblksize = 0;

		db.create(conn, &dbdesc);
	}

	void
	add_schema(eyedb::Connection * conn, std::string const & db_name)
	{
#if 1
		char const schema[] =
#include "schema.h"
		;
		eyedb::Database db(string2eyedb(db_name));
		db.open(conn, eyedb::Database::DBRWAdmin);
		char fname[] = "/tmp/mekanobXXXXXX";
		int fd = mkstemp(fname);
		if (fd < 0)
			throw MyError(std::string("failed to create a tempory file : ") + strerror(errno));
		if (write(fd, schema, sizeof schema - 1) != sizeof schema - 1)
			throw MyError("failed to write the schema into " + std::string(fname) + ": " + strerror(errno));
		if (close(fd) < 0)
			throw MyError("failed to close " + std::string(fname) + ": " + strerror(errno));
		db.updateSchema(fname, "mekano", 0, 0, 0);
		if (unlink(fname) < 0)
			throw MyError("failed to unlink " + std::string(fname) + ": " + strerror(errno));
#else
		eyedb::Database * db = new eyedb::Database(string2eyedb(db_name));
		db->open(conn, eyedb::Database::DBRWAdmin);
		db->transactionBeginExclusive();
		eyedb::Schema * m = new eyedb::Schema();
		m->init();
		m->setName("mekano");
//		eyedb::Database::updateSchema(m);
		eyedb::syscls::updateSchema(m);
//		eyedb::oqlctb::updateSchema(m);
		eyedb::utils::updateSchema(m);
		db->setSchema(m);
		mekano::mekano::updateSchema(db);
		db->transactionCommit();
#endif
	}

	void
	initialise
	(
		Ifstream & in,
		eyedb::Connection * const conn,
		eyedb::Database::OpenFlag const mode,
		gid_t const egid,
		char const * force_db_name,
		char * argv[]
	)
	{
		InitialisationFile tags(in);
		for (; *argv; argv++)
			tags.stages.push_back(InitialisationFile::stages_t::value_type(new Ifstream(*argv)));
		
		unsigned const reads_count = linecount(tags.reads);
		std::string const db_name = (force_db_name) ? force_db_name : tags.database;

		mk_database(conn, db_name);
		resize_shmem(conn, db_name, egid, 350000);
		add_schema(conn, db_name);

		optimisations(conn, db_name, reads_count);

		// don't swap the following 2 lines or core dump
		mekano::mekanoDatabase db(string2eyedb(db_name));
		ObjectHandle<mekano::Calculated> clc;

			ObjectHandle<mekano::Group> group0;
			ObjectHandle<mekano::Stage> stage;
		{
			GetEGid privilige(egid);
			db.open(conn, mode);
			InitialisationFile::users_t::iterator me = find(tags.users.begin(), tags.users.end(), std::string(db.getUser()));
			if (me != tags.users.end())
				tags.users.erase(me);
//			Statistics stats(&db);
			{
				TransactionControl trans(&db);
				for_each(tags.users.begin(), tags.users.end(), boost::bind(add_user(), &db, conn, _1));
				clc = mk_parameters(&db, tags.unsized_gap)->calculated();
				group0 = mk_group(&db);
				stage = mk_stage(group0);
				trans.validate();
			}
			insert_banks(&db, tags.banks);
			insert_stage_types(&db, tags.stagetypes);
			insert_clones_and_reads(&db, tags.reads);
			insert_contigs(tags.ctg, stage);
			if (tags.mctg.is_open())
				insert_meta_contigs(&tags.mctg, stage);
			insert_algorithms(&db, tags.algorithms);
			insert_matches(&db, tags.matches);
			insert_supercontigs(&tags.sctg, stage, group0, clc, tags.unsized_gap, 1000);
			clonelinks(clc, group0);
		}
		for_each
		(
			tags.stages.begin(),
			tags.stages.end(),
			boost::bind
			(
				process_stage,
				boost::bind(&InitialisationFile::stages_t::value_type::get, _1),
				clc,
				tags.unsized_gap,
				egid
			)
		);
	}
	void
	usage(struct option const * long_options)
	{
		std::clog << "usage: " << myname;
		for (; long_options->name; ++long_options)
		{
			std::clog << " --" << long_options->name;
			if (long_options->has_arg)
				std::clog << "=<arg>";
		}
		std::clog << " <file> ..."<< std::endl;
		exit(1);
	}
			
	unsigned options
	(
		int argc,
		char * argv[],
		eyedb::Database::OpenFlag * mode,
		bool * stages,
		char const * * db,
		char const * * chdir
	)
	{
		static struct option long_options[] =
		{
			{ "database", 1, 0, 'd' },
			{ "stages", 1, 0, 's' },
			{ "local", 0, 0, 'l' },
			{ "remote", 0, 0, 'r' },
			{ "chdir", 1, 0, 'c' },
			{ 0, 0, 0, 0 }
		};
		int c;
		while ((c = getopt_long(argc, argv, "d:s:lrc:", long_options, 0)) != -1)
		{
			switch (c)
			{
			case 'd':
				*db = optarg;
				break;

			case 's':
				*stages = true;
				*db = optarg;
				break;

			case 'l':
				*mode = eyedb::Database::DBRWLocal;
				break;

			case 'r':
				*mode = eyedb::Database::DBRW;
				break;

			case 'c':
				*chdir = optarg;
				break;

			default:
				usage(long_options);
				break;
			}
		}
		return optind;
	}
	void
	do_stages(Ifstream * in, eyedb::Database * db, gid_t const egid, char * argv[])
	{
		ObjectHandle<mekano::Calculated> counts;
		ObjectHandle<mekano::Parameters> parameters;
		{
			GetEGid privilige(egid);
			TransactionControl trans(db);
			if (!counts.replace(db, select_one(db, counts.class_name())))
				throw MyError("no " + std::string(counts.class_name()) + " in db");
			if (!parameters.replace(db, select_one(db, parameters.class_name())))
				throw MyError("no " + std::string(parameters.class_name()) + " in db");
		}
		process_stage(in, counts, parameters->hole_size(), egid);
		for (; *argv; ++argv)
		{
			Ifstream stage(*argv);
			if (!stage)
				throw MyError("could not open " + to_string(*argv) + ": " + strerror(errno));
			process_stage(&stage, counts, parameters->hole_size(), egid);
		}
	}
}

boost::shared_ptr<Ifstream> 
get_file(Ifstream & in)
{
	std::string fname;
	if (!(in >> fname))
		throw MyError("no file specified");
//	boost::shared_ptr<Ifstream> f = boost::shared_ptr<Ifstream>();
	return boost::shared_ptr<Ifstream>(new Ifstream(qualify(fname, in.fname())));
}

void
insert_matches(eyedb::Database * db, matches_t const & matches)
{
	using namespace boost;
	for_each
	(
		matches.begin(),
		matches.end(),
		bind
		(
			setup_insert_matches,
			db,
			bind(&matches_t::value_type::first, _1),
			bind(&matches_t::value_type::second_type::get, bind(&matches_t::value_type::second, _1))
		)
	);
}

void
insert_algorithms(eyedb::Database * db, algorithms_t const & algorithms)
{
	TransactionControl trans(db);
	using namespace boost;
	for_each
	(
		algorithms.begin(),
		algorithms.end(),
		bind
		(
			create_algorithm(),
			db,
			bind(&algorithms_t::value_type::first, _1),
			bind(&algorithms_t::value_type::second, _1)
		)
	);
	trans.ok();
}

int
main(int argc, char *argv[])
{
	gid_t const egid = getegid();
	if (setgid(getgid()) < 0)
		return fail(std::clog << myname << ": setgid: " << strerror(errno));

	if ((myname = strrchr(argv[0], '/')) == 0 || *++myname == 0)
		myname = argv[0];
	char const * core_directory = 0;
	char const * db_name = 0;
	bool stages = false;
	mekano::mekano initializer(argc, argv);
	
	eyedb::Database::OpenFlag mode = eyedb::Database::DBRWLocal;
	argv += options(argc, argv, &mode, &stages, &db_name, &core_directory);

	std::string const init_file(arg(argv++, "initialisation file"));
	Ifstream in;
	try
	{
		in.open(init_file);
	}
	catch (MyError & e)
	{
		return fail(std::clog << myname, e);
	}

	eyedb::Exception::setMode(eyedb::Exception::ExceptionMode);
	eyedb::Connection conn;
	try
	{
		conn.open();
	}
	catch (eyedb::Exception & e)
	{
		return fail(std::clog << myname << ": open connection", e);
	}
	
	try
	{
		Performance perf(std::string("import"));
		if (!stages)
			initialise(in, &conn, mode, egid, db_name, argv);
		else
		{
			eyedb::Database db(db_name);
			{
				GetEGid privilige(egid);
				db.open(&conn, mode);
			}
			do_stages(&in, &db, egid, argv);
		}
	}
	catch (eyedb::Exception & e)
	{
		std::clog << myname << ": error " << in.tag() << ": eyedb::Exception " << e << std::endl;
		return 1;
	}
	catch (MyError & e)
	{
		std::clog << myname << ": error " << in.tag() << ": " << e << std::endl;
		return 1;
	}
	catch (...)
	{
		return fail(std::clog << myname << ": unknown exception!: " << in.tag());
	}
	return 0;
}
