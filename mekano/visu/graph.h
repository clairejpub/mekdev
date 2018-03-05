/*	Stuart Pook, Genoscope 2007 (enscript -E $%) (Indent ON)
 *	$Id: graph.h 764 2007-05-25 15:41:23Z stuart $
 */

typedef std::list<std::pair<unsigned, unsigned> > index_t;

class Graph : public GLWidget, private boost::noncopyable
{
	void draw_all();
	void projection();
	void make_lists();
	void initialize();
	index_t const * const _table;
	index_t::value_type::first_type _start;
	index_t::value_type::first_type _stop;
public:
	Graph(int x, int y, int w, int h, index_t const * table) :
		GLWidget(x, y, w, h),
		_table(table),
		_start(0),
		_stop(0)
	{
	}
	void start(index_t::value_type::first_type v) { _start = v; refill(); }
	void stop(index_t::value_type::first_type v) { _stop = v; refill(); }
	void refill() { clear(); redraw(); }
	void text(char const *) const {}
};
