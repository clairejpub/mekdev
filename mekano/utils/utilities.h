/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: utils.h 757 2007-05-24 16:21:10Z stuart $
 */
#ifndef MEKANO_UTILS_UTILITIES_H
#define MEKANO_UTILS_UTILITIES_H MEKANO_UTILS_UTILITIES_H

#include <sys/time.h>
#include <sys/resource.h>
#include <string>

class Performance
{
	struct timeval _start_time;
	struct rusage _start_usage;
	unsigned _reads;
	unsigned _reads_with_timeout;
	unsigned _writes;
	std::string const _what;
	double const _minimum_time;
	bool _done;
	void init();
	enum { minimum_time = 1, };
public:
	Performance(std::string const & what, double mt = minimum_time) :
		_what(what),
		_minimum_time(mt)
	{
		init();
	}
	Performance(Performance const & p, std::string const & what, double mt = minimum_time) :
		_what(p._what + " " + what),
		_minimum_time(mt)
	{
		init();
	}
	void done();
	~Performance() { if (!_done) done(); }
};
typedef Performance Perfs;

#endif
