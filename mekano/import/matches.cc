/*	Stuart Pook, Genescope, 2006 (enscript -E $%)
 *	$Id: matches.cc 787 2007-06-04 12:59:21Z stuart $
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
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>

namespace
{
	void
	insert_matches(Ifstream * in, eyedb::Oid const & algorithm, TransactionControl * trans)
	{
		eyedb::Database * const db = trans->database();
		std::clog << myname << ": adding Matches from " << in->fname() << std::endl;
		ObjectHandle<mekano::Contig> contig;
		std::string sequence0;
		while (in->skip_white_space() >> sequence0 >> skipspaces)
		{
			unsigned start0;
			if (!(*in >> start0 >> skipspaces))
				throw MyError("missing first start");

			unsigned stop0;
			if (!(*in >> stop0 >> skipspaces))
				throw MyError("missing first end");

			std::string sequence1;
			if (!(*in >> sequence1 >> skipspaces))
				throw MyError("missing second sequence name");

			unsigned start1;
			if (!(*in >> start1 >> skipspaces))
				throw MyError("missing second start");

			unsigned stop1;
			if (!(*in >> stop1 >> skipspaces))
				throw MyError("missing second end");

			double identity;
			if (!(*in >> identity >> skipspaces))
				throw MyError("missing identity");
			double const max_identity = 100;
			if (identity < 0 || identity > max_identity)
				throw MyError("bad identity: " + to_string(identity));

			ObjectHandle<mekano::Match> match(db);
			match->algorithm_oid(algorithm);
			match->id_ratio(identity / max_identity);
			{
				ObjectHandle<mekano::MatchSide> side0(db);
				side0->sequence_oid(lookup(db, "Sequence", sequence0));
				side0->start(start0);
				side0->stop(stop0);
				match->sides(0, side0);
			}
			{
				ObjectHandle<mekano::MatchSide> side1(db);
				side1->sequence_oid(lookup(db, "Sequence", sequence1));
				side1->start(start1);
				side1->stop(stop1);
				match->sides(1, side1);
			}
			match.store();
			trans->checkpoint();

			if (in->get() != '\n')
				throw MyError("failed to read eol");
		}
		if (!in->eof())	
			throw MyError("count not read sequence name");
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
}

void
setup_insert_matches(eyedb::Database * db, std::string const & algo, Ifstream * in)
{
	TransactionControl transaction(db, 1500);
	eyedb::Oid algorithm = lookup(db, "Algorithm", algo);
	try
	{
		insert_matches(in, algorithm, &transaction);
	}
	catch (eyedb::Exception const & e)
	{
		throw MyError(in->tag() + ": eyedb::Exception inserting Matches: " + to_string(e));
	}
	catch (MyError & e)
	{
		throw MyError(in->tag() + ": error inserting Contigs: " + to_string(e));
	}
	transaction.ok();
}
