/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: eyedb.cc 771 2007-05-31 09:42:06Z stuart $
 */
#include <utils/eyedb.h>
#include <utils/myerror.h>
#include <errno.h>
#include <iomanip>

eyedb::Oid
select_one(eyedb::Database * db, std::string const & class_name)
{
	std::string query = "select one " + class_name;
	eyedb::OidArray oids;
	eyedb::OQL(db, query.c_str()).execute(oids);
	if (oids.getCount() > 1)
		throw MyError("multiple objects from: " + query);
	if (oids.getCount() == 0)
		throw MyError("zero objects from: " + query);
	return oids[0];
}

eyedb::Oid
query(eyedb::Database * const db, std::string const & q)
{
	std::string query = "select " + q;
	eyedb::OidArray oids;
	eyedb::OQL(db, query.c_str()).execute(oids);
	if (oids.getCount() != 1)
		return eyedb::Oid::nullOid;
	return oids[0];
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
TransactionControl::init()
{
	if (gettimeofday(&_start_time, 0) != 0)
		throw MyError(std::string("gettimeofday failed ") + strerror(errno));
	if (getrusage(RUSAGE_SELF, &_start_usage) != 0)
		throw MyError(std::string("getrusage failed ") + strerror(errno));
}

TransactionControl::~TransactionControl()
{
	if (!_db)
		return;
	if (_ok)
		_db->transactionCommit();
	else
		_db->transactionAbort();

	if (_checkpoint && (_checkpointed || n))
	{
		if (_ok)
		{
			if (!_checkpointed)
				std::clog << ' ';
			std::clog << count();
		}
		struct timeval end_time;
		if (gettimeofday(&end_time, 0) != 0)
			throw MyError(std::string("gettimeofday failed ") + strerror(errno));
		double const real_time = seconds(end_time) - seconds(_start_time);
		struct rusage end_usage;
		if (getrusage(RUSAGE_SELF, &end_usage) != 0)
			throw MyError(std::string("getrusage failed ") + strerror(errno));
		double cpu_time = seconds(end_usage.ru_stime) - seconds(_start_usage.ru_stime);
		cpu_time += seconds(end_usage.ru_utime) - seconds(_start_usage.ru_utime);
		double const cpu_percent = cpu_time * 100.0 / real_time;
		std::clog << ' ' << std::fixed
			<< std::setprecision(2) << real_time << "s "
			<< std::setprecision(0) << cpu_percent << "% "
			<< std::endl;
	}
}

void
TransactionControl::checkpoint(eyedb::Object * o, eyedb::RecMode const * r)
{
	if (_offset == 0 && _checkpoint)
	{
		std::string count(to_string(_checkpoint));
		std::clog << count << std::flush;
		_offset = count.length();
	}

	if (++n == _checkpoint)
	{
		if (o)
			o->store(r);
		_db->transactionCommit();
		std::clog << (((++_checkpointed + _offset) % 80) ? "." : ".\n") << std::flush;
		_db->transactionBegin();
		n = 0;
	}
}
