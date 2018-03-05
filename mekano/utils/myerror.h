/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: myerror.h 788 2007-06-04 12:59:44Z stuart $
 */
#ifndef MEKANO_UTILS_MYERROR_H
#define MEKANO_UTILS_MYERROR_H MEKANO_UTILS_MYERROR_H
class MyError
{
	std::string const _message;
public:
	MyError(std::string const & m) : _message(m) {}
	std::string const & toString() const
	{
		return _message;
	}
};

inline std::ostream & operator<<(std::ostream & s, MyError const & e)
{
	return s << e.toString(); 
}
#endif
