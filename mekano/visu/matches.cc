/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: matches.cc 771 2007-05-31 09:42:06Z stuart $
 */
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include "mekano.h"
#include <utils/sctg_data.h>
#include <utils/eyedb.h>
#include "utils.h"
#include "graphics.h"
#include <utils/utilities.h>

namespace
{
	void
	add_oids(SupercontigData::ConSeqContainer const & cseqs, std::string const & s, std::ostringstream * q)
	{
		*q << s << '(';
		if (!cseqs.empty())
		{
			SupercontigData::ConSeqContainer::const_iterator end = cseqs.end();
			using namespace boost::lambda;
			transform_if
			(
				cseqs.begin(),
				--end,
				std::ostream_iterator<eyedb::Oid>(*q, ","),
				bind(&SupercontigData::ConseqData::oid, _1),
				bind(&SupercontigData::ConseqData::is_conseq, _1)
			);
			*q << end->oid();
		}
		*q << ");\n";
	}
	
	void
	crossed_trapezium(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4)
	{
		// http://local.wasp.uwa.edu.au/~pbourke/geometry/lineline2d/
		double u = (x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3);
		u /= (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);
		double const x = x1 + u * (x2 - x1);
		double const y = y1 + u * (y2 - y1);
		glVertex2i(x1, y1);
		glVertex2i(x3, y3);
		glVertex2d(x, y);
		
		glVertex2d(x, y);
		glVertex2i(x2, y2);
		glVertex2i(x4, y4);
	}
	
	void
	simple_trapezium(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4)
	{
		glVertex2i(x4, y4);
		glVertex2i(x2, y2);
		glVertex2i(x1, y1);
		
		glVertex2i(x4, y4);
		glVertex2i(x3, y3);
		glVertex2i(x1, y1);
	}
	
	int
	correct_match_side
	(
		scaffolds_t::value_type const & scaf,
		int * x,
		eyedb::Oid const & cseq,
		unsigned const cseq_position,
		unsigned start,
		unsigned stop,
		bool const reverse_scaf
	)
	{
		assert(cseq_position < scaf->data().conseqs().size());
		SupercontigData::ConseqData const & conseq = scaf->data().conseqs()[cseq_position];
		assert(conseq.oid() == cseq);
		start = std::max(start, conseq.start());
		stop = std::min(stop, conseq.stop());
		int const direction = reverse_scaf ? -1 : 1;
		*x += direction* (start + scaf->continuous_sequence_positions()[cseq_position]);
		return *x + direction * (stop - start);
	}
	
	void
	draw_match
	(
		scaffolds_t::value_type const & scaf0,
		int x0,
		int const y0,
		eyedb::Oid const & cseq0,
		unsigned const cseq0_position,
		unsigned const start0,
		unsigned const stop0,
		scaffolds_t::value_type const & scaf1,
		int x1,
		int const y1,
		bool const reverse_scaf1,
		eyedb::Oid const & cseq1,
		unsigned const cseq1_position,
		unsigned const start1,
		unsigned const stop1
	)
	{
		int const x2 = correct_match_side(scaf0, &x0, cseq0, cseq0_position, start0, stop0, false);
		int const x3 = correct_match_side(scaf1, &x1, cseq1, cseq1_position, start1, stop1, reverse_scaf1);
		((x0 > x2) ^ (x1 > x3) ? crossed_trapezium : simple_trapezium)(x0, y0, x1, y1, x2, y0, x3, y1);
//		std::cout << "draw_match(" << x0 << ", " << y0 << ", " << x1 << ", " << y1 <<")" << std::endl;
	}
	
	std::string
	make_query(scaffolds_t::value_type const & smaller, scaffolds_t::value_type const & bigger)
	{
		std::ostringstream query;
		add_oids(smaller->data().conseqs(), "smaller:=array", &query);
		add_oids(bigger->data().conseqs(), "bigger:=set", &query);
		query << "r:=list();\n";
		query << "for (x in smaller) "
			"append (select struct("
					"a:m.sides[0].sequence, "
					"b:m.sides[1].sequence, "
					"c:m.sides[0].start, "
					"d:m.sides[0].stop, "
					"e:m.sides[1].start, "
					"f:m.sides[1].stop "
				")"
				"from Match m where "
				"m.sides[?].sequence = x and "
				"(m.sides[0].sequence = x and m.sides[1].sequence in bigger or "
					"m.sides[1].sequence = x and m.sides[0].sequence in bigger)) to r;\n";
		query << "r;";
		return query.str();
	}
}

void
matches
(
	scaffolds_t::value_type const & scaf0,
	int const x0,
	int const y0,
	scaffolds_t::value_type const & scaf1,
	int const x1,
	int const y1,
	bool const reverse_scaf1
)
{
	glutil::begin begin(GL_TRIANGLES);
	Colour(1, 1, 1).set();
	Perfs perfs("matches(" +  scaf0->sctg()->name() + ", " + scaf1->sctg()->name() + ")");
	typedef std::vector<std::pair<eyedb::Oid, unsigned> > mapping_t;
	mapping_t mapping;
	bool const scaf0_bigger = scaf0->data().conseqs().size() > scaf1->data().conseqs().size();
	scaffolds_t::value_type const & smaller = (scaf0_bigger ? scaf1 : scaf0);
	scaffolds_t::value_type const & bigger = (scaf0_bigger ? scaf0 : scaf1);

	mapping_t::value_type::second_type index = 0;
	using namespace boost::lambda;
	transform
	(
		bigger->data().conseqs().begin(),
		bigger->data().conseqs().end(),
		back_inserter(mapping),
		bind
		(
			std::make_pair<mapping_t::value_type::first_type, mapping_t::value_type::second_type>,
			bind(&SupercontigData::ConseqData::oid, _1),
			var(index)++
		)
	);
	std::sort(mapping.begin(), mapping.end());
#if 0
	std::for_each(mapping.begin(), mapping.end(),
		std::cout << lambda::bind(&mapping_t::value_type::first, lambda::_1) << ' ' << lambda::bind(&mapping_t::value_type::second, lambda::_1) << '\n'
	);
#endif

	eyedb::ValueArray results;
	eyedb::OQL(scaf1->sctg().database(), string2eyedb(make_query(smaller, bigger))).execute(results);

//	std::cout << "matches(" << scaf0->sctg()->name() << ", " << scaf1->sctg()->name() << ") -> " << results.getCount() << std::endl;

	SupercontigData::ConSeqContainer::const_iterator p = smaller->data().conseqs().begin();
	SupercontigData::ConSeqContainer::const_iterator pe = smaller->data().conseqs().end();
	
	for (unsigned i = 0; i < results.getCount(); )
	{
		eyedb::Oid const cseq0 = value_cast<eyedb::Oid>(results[i++]);
		eyedb::Oid const cseq1 = value_cast<eyedb::Oid>(results[i++]);
		unsigned const start0 = value_cast<long long>(results[i++]);
		unsigned const stop0 = value_cast<long long>(results[i++]);
		unsigned const start1 = value_cast<long long>(results[i++]);
		unsigned const stop1 = value_cast<long long>(results[i++]);
		assert(i <= results.getCount());

		assert(p != pe);
		bool cseq1_in_smallest;
		while ((cseq1_in_smallest = p->oid() != cseq0) && p->oid() != cseq1)	
			assert(++p != pe);
		mapping_t::const_iterator const q = lower_bound
		(
			mapping.begin(),
			mapping.end(),
			mapping_t::value_type(cseq1_in_smallest ? cseq0 : cseq1, 0),
			bind(&mapping_t::value_type::first, _1) < bind(&mapping_t::value_type::first, _2)
		);
		assert(q != mapping.end());
		int cseq0_position = q->second;
		int cseq1_position = p - smaller->data().conseqs().begin();
		if (!cseq1_in_smallest)
			std::swap(cseq0_position, cseq1_position);
		
		if (scaf0_bigger ^ cseq1_in_smallest)
			draw_match(scaf0, x0, y0, cseq1, cseq1_position, start1, stop1, scaf1, x1, y1, reverse_scaf1, cseq0, cseq0_position, start0, stop0);
		else
			draw_match(scaf0, x0, y0, cseq0, cseq0_position, start0, stop0, scaf1, x1, y1, reverse_scaf1, cseq1, cseq1_position, start1, stop1);
	}
}
