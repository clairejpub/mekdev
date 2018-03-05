/*	Stuart Pook, Genoscope 2007 (Indent ON)
 *	$Id: visu.h 764 2007-05-25 15:41:23Z stuart $
 *	uses fltk-2.0.x-r???? from http://www.fltk.org/
 */
#include <boost/function.hpp>
#include "bcm.h"

#ifndef MENAKO_VISU_VISU_H
#define MENAKO_VISU_VISU_H MENAKO_VISU_VISU_H
enum { button_height = 25, };

class Legend : public GLWidget
{
 	typedef std::map<BankColourMap::opaque_t, ObjectHandle<mekano::Bank> > used_t;
	glutil::display_list contents;
 	BankColourMap * const _bcm;
	boost::function<void()> const _hidden_changed;
	enum { space = 2 };
	void picked(GLint, GLuint const *, int, int);
	void label_bank(used_t::value_type const & v, int * const x, int *);
	Overlap<int> _overlaps;
	std::set<int> _hidden;
	used_t _used[2];
	boost::function<bool()> _banks_by_bank;
public:
	void reset_hidden()
	{
		_hidden.clear();
//		recalculate(); // called by sctg_drawn()
	}
	BankColourMap * bcm() const { return _bcm; }
	Legend(int x, int y, int w, int h, BankColourMap * bcm, boost::function<void()> hidden_changed, boost::function<bool()> by_bank) :
		GLWidget(x, y, w, h),
		_bcm(bcm),
		_hidden_changed(hidden_changed),
		_banks_by_bank(by_bank)
	{
	}
	void sctg_drawn()
	{
//		std::clog << "Legend::sctg_drawn " << used.size() << std::endl;
		recalculate();
	}
	void projection()
	{
		glOrtho(0, w(), -h() / 2.0, h() / 2.0, -10, 10);
	}
	void draw_all();
//	bool all_visible() const { return _hidden.empty(); }
	bool visible(ObjectHandle<mekano::Bank> const & b) const // ::visible
	{
		return _hidden.empty() || _hidden.find(_bcm->opaque(b, _banks_by_bank())) == _hidden.end();
	}
	Colour colour(ObjectHandle<mekano::Bank> const & b, bool reversed)
	{
		bool const by_bank = _banks_by_bank();
		BankColourMap::opaque_t v = _bcm->opaque(b, by_bank);
		_used[by_bank].insert(std::make_pair(v, b));
		return _bcm->colour(v, by_bank, reversed);
	}
	Colour colour(mekano::ReadPosition * rp1, mekano::ReadPosition * rp2, bool reversed)
	{
		if (rp1->read()->clone_oid() != rp2->read()->clone_oid())
			throw MyError("unequal clones");
		reversed ^= !(rp1->forward() ^ rp2->forward());
		return colour(rp1->read()->clone()->bank(), reversed);
	}
	Colour colour(SupercontigData::PositionnedRead const * pr1, SupercontigData::PositionnedRead const * pr2, bool reversed)
	{
		return colour(pr1->read_position(), pr2->read_position(), reversed);
	}
};
typedef Legend BankInfo;

template<class Drawer>
class DrawConSeq : public std::unary_function<SupercontigData::ConSeqData, void>
{
	Drawer * _drawer;
	unsigned _pos;
public:
	DrawConSeq(Drawer * drawer) : _drawer(drawer), _pos(0) {}
	result_type operator()(argument_type const & ci)
	{
		if (!ci.is_gap())
		{
			_drawer->contig(_pos, ci);
			_pos += ci.length();
		}
		else if (ci.gap_unknown())
			_pos += _drawer->gap(_pos);
		else
		{
			_drawer->gap(_pos, ci.gap_size());
			_pos += ci.gap_size();
		}
	}
};
template<class Drawer>
class DrawConSeq<Drawer>
draw_con_seq(Drawer * drawer)
{
	return DrawConSeq<Drawer>(drawer);
}

void common_clones(scaffolds_t::value_type const &, scaffolds_t::value_type const &, BankInfo *, clones_t *, clones_t *);

void open_sctg_window(scaffolds_t::value_type const &, BankColourMap *, ObjectHandle<mekano::Parameters> const &);
void open_sctg_window(scaffolds_t::value_type const &, scaffolds_t::value_type const &, BankColourMap *, ObjectHandle<mekano::Parameters> const &);

#endif
