/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: etape.cc 787 2007-06-04 12:59:21Z stuart $
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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "mekano.h"
#include "common.h"
#include <utils/templates.h>
#include <utils/iterators.h>
#include <utils/eyedb.h>
#include <utils/sctg_data.h>
#include <boost/tuple/tuple.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/bind.hpp>

#include "common.h"

namespace
{
	typedef std::vector<boost::tuple<boost::shared_ptr<Ifstream>, std::string, std::string> > moves_t;

	struct change_group_helper
	{
		/*	The following line is ambiguous with gcc 4.1.1 & boost 1.33.1 so I had to add
		*	this helper. Yuck.
		*	boost::bind(&moves_t::value_type::get<0>, _1)(moves.front());
		*/
		typedef void result_type;
		result_type operator()(ObjectHandle<mekano::Stage> const & stage, moves_t::value_type const & v)
		{
			change_group(stage, v.get<0>().get(), v.get<1>(), v.get<2>());
		}
	};
		
	void
	real_process_stage
	(
		Ifstream * in,
		ObjectHandle<mekano::Calculated> counts,
		int hole_size,
		gid_t const egid
	)
	{
		eyedb::Database * const db = counts.database();
		std::string type;
		
		typedef std::vector<std::pair<std::string, std::string> > groups_t;
		groups_t groups;
		
		typedef std::vector<boost::shared_ptr<Ifstream> > mctgs_t;
		mctgs_t mctgs;
		
		typedef std::vector<std::pair<boost::shared_ptr<Ifstream>,  std::string> > sctgs_t;
		sctgs_t sctgs;

		typedef std::vector<std::pair<std::string, std::string> > algorithms_t;
		algorithms_t algorithms;

		matches_t matches;
		
		moves_t moves;
		std::string tag;
		while (in->skip_white_space() >> tag >> skipspaces)
		{
			if (tag == "type")
			{
				if (!type.empty())
					throw MyError("can only specify the type once");
				if (!(*in >> type >> skipspaces))
					throw MyError("no type specified");
			}
			else if (tag == "group")
			{
				std::string n;
				if (!(*in >> n >> skipspaces))
					throw MyError("no group type specified");
				std::string comment;
				if (!getline(*in, comment))
					throw MyError("failed to read the group's comment");
				groups.push_back(std::make_pair(n, comment));
			}
			else if (tag == "mctg")
			{
				std::string fname;
				if (!(*in >> fname >> skipspaces))
					throw MyError("no file specified");
				mctgs.push_back(boost::shared_ptr<Ifstream>(new Ifstream(qualify(fname, in->fname()))));
			}
			else if (tag == "sctg")
			{
				std::string fname;
				std::string group;
				if (!(*in >> fname >> skipspaces >> group >> skipspaces))
					throw MyError("no file specified");
				boost::shared_ptr<Ifstream> f(new Ifstream(qualify(fname, in->fname())));
				sctgs.push_back(std::make_pair(f, group));
			}
			else if (tag == "move")
			{
				std::string fname;
				std::string group1;
				std::string group2;
				if (!(*in >> fname >> skipspaces >> group1 >> skipspaces >> group2 >> skipspaces))
					throw MyError("no file specified");
				boost::shared_ptr<Ifstream> f(new Ifstream(qualify(fname, in->fname())));
				moves.push_back(boost::make_tuple(f, group1, group2));
			}
			else if (tag == "algo")
			{
				std::string algo;
				if (!(*in >> algo >> skipspaces))
					throw MyError("no algorithm specified");
				std::string comment;
				if (!getline(*in, comment))
					throw MyError("failed to read the algorithm's comment");
				algorithms.push_back(std::make_pair(algo, comment));
			}
			else if (tag == "match")
			{
				std::string algo;
				std::string fname;
				if (!(*in >> algo >> skipspaces))
					throw MyError("no algorithm specified");
				matches.push_back(std::make_pair(algo, get_file(*in)));
			}
			else
				throw MyError("unknown tag " + tag);
		}
		if (!in->eof())
			throw MyError("I/O error");
		
		if (type.empty())
			throw MyError("must specify a type");
		ObjectHandle<mekano::Stage> stage;
		GetEGid privilige(egid);
		{
			TransactionControl trans(db);
			stage.replace(db, copy_stage(db, type));
			for_each
			(
				groups.begin(),
				groups.end(),
				boost::bind
				(
					add_group,
					stage,
					boost::bind(&groups_t::value_type::first, _1),
					boost::bind(&groups_t::value_type::second, _1)
				)
			);
			trans.validate();
		}
		for_each(mctgs.begin(), mctgs.end(), boost::bind(insert_meta_contigs, boost::bind(&mctgs_t::value_type::get, _1), stage));
		insert_matches(db, matches);
		for_each
		(
			sctgs.begin(),
			sctgs.end(),
			boost::bind
			(
				joins,
				stage,
				counts,
				hole_size,
				boost::bind(&sctgs_t::value_type::first_type::get, boost::bind(&sctgs_t::value_type::first, _1)),
				boost::bind(&sctgs_t::value_type::second, _1)
			)
		);
		for_each
		(
			moves.begin(),
			moves.end(),
			boost::bind(change_group_helper(), stage, _1)
		);
	}
}

void
process_stage
(
	Ifstream * in,
	ObjectHandle<mekano::Calculated> counts,
	int hole_size,
	gid_t const egid
)
{
	std::string const what("new stage");
	try
	{
		real_process_stage(in, counts, hole_size, egid);
	}
	catch (eyedb::Exception const & e)
	{
		throw MyError(in->tag() + ": eyedb::Exception " + what + ": " + to_string(e));
	}
	catch (MyError & e)
	{
		throw MyError(in->tag() + ": " + what + ": " + to_string(e));
	}
}
