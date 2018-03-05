/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: bcm.h 742 2007-05-14 13:38:08Z stuart $
 */
#include "mekano.h"
#include <utils/eyedb.h>
#include "graphics.h"

#ifndef MEKANO_VISU_BCM_H
#define MEKANO_VISU_BCM_H MEKANO_VISU_BCM_H
struct BankColourMap
{
	struct OidCmp : public std::binary_function<ObjectHandle<mekano::Bank>, ObjectHandle<mekano::Bank>, bool>
	{
		result_type operator()(first_argument_type const & x, second_argument_type const & y) const
		{
			return x.oid() < y.oid();
		}
	};
	typedef unsigned short opaque_t;
	typedef std::map<ObjectHandle<mekano::Bank>, opaque_t, OidCmp> map_t;
private:
	map_t _map_by_length;
	map_t _map_by_bank;
	unsigned _n_different_lengths;
	static opaque_t find(ObjectHandle<mekano::Bank> const & b, map_t const & map)
	{
		map_t::const_iterator i = map.find(b);
		if (i == map.end())
			throw MyError(std::string("unknown Bank in BankColourMap: ") + b->name());
		return i->second;
	}
public:
	opaque_t opaque(ObjectHandle<mekano::Bank> const & b, bool by_bank) const
	{
		return find(b, by_bank ? _map_by_bank : _map_by_length);
	}
	BankColourMap(eyedb::Database *); // BankColourMap::BankColourMap
	double value(opaque_t i, bool by_bank) const
	{
		return i / double(by_bank ? _map_by_bank.size() : _n_different_lengths);
	}
	double value(ObjectHandle<mekano::Bank> const & o, bool by_bank) const
	{
		return value(opaque(o, by_bank), by_bank);
	}
	Colour colour(opaque_t i, bool by_bank, bool reversed) const
	{
		return Colour::hsv(value(i, by_bank) * 360, (reversed) ? 1 : 0.3, 1);
	}
	Colour colour(ObjectHandle<mekano::Bank> const & o, bool by_bank, bool reversed) const
	{
		return colour(opaque(o, by_bank), by_bank, reversed);
	}
};
#endif
