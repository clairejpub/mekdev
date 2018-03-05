/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: bcm.cc 717 2007-05-04 14:51:40Z stuart $
 */

#include "mekano.h"
#include <utils/templates.h>
#include <utils/iterators.h>
#include "graphics.h"
#include "utils.h"
#include "bcm.h"
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/any.hpp>

#include <utils/eyedb.h>

template <class C>
struct ReadObjectOid : std::binary_function<eyedb::Database *, eyedb::Oid, ObjectHandle<C> >
{
	ObjectHandle<C> operator()(eyedb::Database * db, eyedb::Oid oid) const
	{
		return ObjectHandle<C>(db, oid);
	}
};

struct get_length
{
	typedef int result_type;
	result_type operator()(mekano::Bank const * b) { return b->length(); }
};

struct mapit
{
	typedef BankColourMap::map_t::value_type result_type;
	result_type operator()(ObjectHandle<mekano::Bank> const & b, BankColourMap::opaque_t * pos, int * length)
	{
		if (*length != b->length())
		{
			if (*length >= 0)
				++*pos;
			*length = b->length();
		}
		return std::make_pair(b, *pos);
	}
};

namespace
{
	template<typename T>
	struct increment
	{
		typedef T result_type;
		result_type operator()() { return i++; }
		increment() : i(0) {}
	private:
		T i;
	};
}

BankColourMap::BankColourMap(eyedb::Database * db) // class BankColourMap
{
	TransactionControl trans(db);

	eyedb::OidArray oids;
	eyedb::OQL(db, "select Bank").execute(oids);
	if (unsigned count = oids.getCount())
	{
		typedef std::vector<ObjectHandle<mekano::Bank> > banks_t;
		banks_t banks;
		std::transform(&oids[0], &oids[count], std::back_inserter(banks), std::bind1st(ReadObjectOid<mekano::Bank>(), db));

		transform
		(
			banks.begin(),
			banks.end(),
			inserter(_map_by_bank, _map_by_bank.begin()),
			boost::bind
			(
				std::make_pair<map_t::key_type, map_t::value_type::second_type>,
				_1,
				boost::bind(increment<opaque_t>())
			)
		);
#if 0
		transform(_map_by_bank.begin(), _map_by_bank.end(), std::ostream_iterator<opaque_t>(std::clog, " "), boost::bind(&map_t::value_type::second, _1)); std::clog << std::endl;
		std::clog << "size= " << _map_by_bank.size() << std::endl;
#endif

		std::sort
		(
			banks.begin(),
			banks.end(),
			boost::bind
			(
				std::less<int>(),
				boost::bind(get_length(), boost::bind(&ObjectHandle<mekano::Bank>::rep, _1)),
				boost::bind(get_length(), boost::bind(&ObjectHandle<mekano::Bank>::rep, _2))
			)
		);
		BankColourMap::opaque_t pos = 0;
		int length = -1;
		transform(banks.begin(), banks.end(), inserter(_map_by_length, _map_by_length.begin()), boost::bind(mapit(), _1, &pos, &length));
		_n_different_lengths = unique
		(
			banks.begin(), banks.end(),
			boost::bind
			(
				std::equal_to<int>(),
				boost::bind(get_length(), boost::bind(&ObjectHandle<mekano::Bank>::rep, _1)),
				boost::bind(get_length(), boost::bind(&ObjectHandle<mekano::Bank>::rep, _2))
			)
		) - banks.begin();
	}
}
