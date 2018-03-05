/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: utilities.cc 785 2007-06-04 09:09:20Z stuart $
 */
#include <utils/utilities.h>
#include <utils/myerror.h>
#include <errno.h>
#include <eyedblib/rpc_lib.h>
#include <iostream>
#include <iomanip>

void
Performance::init()
{
	_done = false;
	if (gettimeofday(&_start_time, 0) != 0)
		throw MyError(std::string("gettimeofday failed ") + strerror(errno));
	if (getrusage(RUSAGE_SELF, &_start_usage) != 0)
		throw MyError(std::string("getrusage failed ") + strerror(errno));
	unsigned byte_read_cnt;
	unsigned byte_write_cnt;
	rpc_getStats(&_reads, &_reads_with_timeout, &_writes, &byte_read_cnt, &byte_write_cnt);
}

namespace
{
	double
	seconds(struct timeval const & tv)
	{
		return tv.tv_sec + tv.tv_usec / 1000000.0;
	}
}

void
Performance::done()
{
	if (_done)
		return;
	_done = true;
	struct timeval end_time;
	if (gettimeofday(&end_time, 0) != 0)
		throw MyError(std::string("gettimeofday failed ") + strerror(errno));
	double const real_time = seconds(end_time) - seconds(_start_time);
	if (real_time >= _minimum_time)
	{
		struct rusage end_usage;
		if (getrusage(RUSAGE_SELF, &end_usage) != 0)
			throw MyError(std::string("getrusage failed ") + strerror(errno));
		double cpu_time = seconds(end_usage.ru_stime) - seconds(_start_usage.ru_stime);
		cpu_time += seconds(end_usage.ru_utime) - seconds(_start_usage.ru_utime);
		double const cpu_percent = cpu_time * 100 / real_time;
		unsigned reads;
		unsigned reads_with_timeout;
		unsigned writes;
		unsigned byte_read_cnt;
		unsigned byte_write_cnt;
		rpc_getStats(&reads, &reads_with_timeout, &writes, &byte_read_cnt, &byte_write_cnt);

		std::cout << std::fixed;
		std::cout << _what << ": "
			<< std::setprecision(2) << real_time << "s cpu "
			<< std::setprecision(0) << cpu_percent << "%";
		if (unsigned round_trips = reads - _reads)
			std::cout << ' ' << round_trips << " r/t";
		std::cout << std::endl;
	}
}

