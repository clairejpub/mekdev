/*	$Id: eyedb.h 771 2007-05-31 09:42:06Z stuart $
 *	Stuart Pook, Genescope, 2006
 */
#ifndef MEKANO_utils_eyedb_h
#define MEKANO_utils_eyedb_h MEKANO_utils_eyedb_h
#include <eyedb/eyedb.h>
#include <sstream>
#include <utils/templates.h>
#include <sys/time.h>
#include <sys/resource.h>

class bad_cast {};

template<typename ValueType>
inline ValueType value_cast(eyedb::Value const &)
{
	throw bad_cast();
}

template<>
inline int value_cast(eyedb::Value const & o)
{
	if (o.getType() == eyedb::Value::tInt)
		return o.i;
	throw bad_cast();
}

template<>
inline long long value_cast(eyedb::Value const & o)
{
	if (o.getType() == eyedb::Value::tLong)
		return o.l;
	throw bad_cast();
}

template<>
inline eyedb::Oid value_cast(eyedb::Value const & o)
{
	if (o.getType() == eyedb::Value::tOid)
		return *o.oid;
	throw bad_cast();
}

template<typename ValueType>
inline ValueType const * value_cast(eyedb::Value const *)
{
	return 0;
}

template<>
inline double const * value_cast(eyedb::Value const * o)
{
	return o && o->getType() == eyedb::Value::tDouble ? &o->d : 0;
}

template<>
inline int const * value_cast(eyedb::Value const * o)
{
	return o && o->getType() == eyedb::Value::tInt ? &o->i : 0;
}

template<>
inline long long const * value_cast(eyedb::Value const * o)
{
	return o && o->getType() == eyedb::Value::tLong ? &o->l : 0;
}

template<>
inline unsigned char const * value_cast(eyedb::Value const * o)
{
	return o && o->getType() == eyedb::Value::tByte ? &o->by : 0;
}

template<>
inline eyedb::Oid const * value_cast(eyedb::Value const * o)
{
	return o && o->getType() == eyedb::Value::tOid ? o->oid : 0;
}

inline char const * string2eyedb(std::string const & s) { return s.c_str(); }

template <typename C>
inline void
query(eyedb::Database * db, C result, std::string const & q)
{
	typedef typename C::container_type::value_type::object_type object_t;
	std::string query = std::string("select ") + object_t::_classname();
	if (!q.empty())
		query += "." + q;
	eyedb::ObjectArray returned;
	eyedb::OQL(db, string2eyedb(query)).execute(returned);
	for (unsigned i = 0; i < returned.getCount(); i++)
		*result++ = typename C::container_type::value_type(object_t::_convert(returned[i]));
}

eyedb::Oid select_one(eyedb::Database *, std::string const &);
eyedb::Oid query(eyedb::Database *, std::string const &);

class TransactionControl
{
	struct timeval _start_time;
	struct rusage _start_usage;
	eyedb::Database * _db;
	unsigned const _checkpoint;
	bool _ok;
	TransactionControl(TransactionControl const &);
	TransactionControl & operator=(TransactionControl const &);
	unsigned n;
	unsigned _checkpointed;
	unsigned _offset;
	void init();
public:
	enum type { exclusive, };
	TransactionControl(eyedb::Database * const db, unsigned checkpoint = 0) :
		_db(db),
		_checkpoint(checkpoint),
		_ok(false),
		n(0),
		_checkpointed(0),
		_offset(0)
	{
		_db->transactionBegin();
		init();
	}
	TransactionControl(eyedb::Database * const db, enum type) :
		_db(db),
		_checkpoint(0),
		_ok(false),
		n(0),
		_checkpointed(0),
		_offset(0)
	{
		_db->transactionBeginExclusive();
		init();
	}
	eyedb::Database * database() const { return _db; }
	void validate() { _ok = true; }
	void ok() { validate(); }
	unsigned count() const { return _checkpointed * _checkpoint + n; }
	~TransactionControl();
	void checkpoint(eyedb::Object * o = 0, eyedb::RecMode const * r = eyedb::RecMode::NoRecurs);
};

template <class EyedbClass>
struct ObjectHandle
{
	typedef EyedbClass object_type;
	typedef object_type * pointer_type;

	~ObjectHandle() { release(); }
	ObjectHandle() : _o(0) {}

	explicit ObjectHandle(eyedb::Database * db) : _o(new object_type(db)) {}
	ObjectHandle(object_type * o) : _o(o) { reserve(); }

	ObjectHandle(eyedb::Database * db, eyedb::Oid const & oid) : _o(load(db, oid)) {}
	ObjectHandle(eyedb::Database * db, int id) : _o(lookup(db, id)) {}
	ObjectHandle(eyedb::Database * db, std::string const & n) : _o(lookup(db, n)) {}
	ObjectHandle(eyedb::Database * db, char const * n) : _o(lookup(db, n)) {}

	ObjectHandle(eyedb::Database * db, eyedb::Class * c, eyedb::Bool isref = eyedb::True) : _o(new object_type(db, "", c, isref)) {}

	ObjectHandle(ObjectHandle const & r) : _o(r._o) { reserve(); } // copy constructor
	ObjectHandle & operator=(ObjectHandle const & r) // copy assignment
	{
		if (this != &r)
		{
			release();
			_o = r._o;
			reserve();
		}
		return *this;
	}

	void replace(eyedb::Database * db) { *this = ObjectHandle(db); }
	bool replace(eyedb::Database * db, int id) { return *this = ObjectHandle(db, id); }
	bool replace(eyedb::Database * db, std::string const & n) { return *this = ObjectHandle(db, n); }
	bool replace(eyedb::Database * db, eyedb::Oid const & oid) { return *this = ObjectHandle(db, oid); }
	bool replace(eyedb::Database * db, eyedb::Class * c, eyedb::Bool isref = eyedb::True) { return *this = ObjectHandle(db, c, isref); }

	void store() const { Instance()->store(); }
	void store(eyedb::RecMode const * r) const { Instance()->store(r); }
	eyedb::Database * database() const { return Struct()->getDatabase(); }
	eyedb::Oid const & oid() const { return Struct()->getOid(); }
	static char const * class_name() { return object_type::_classname(); }
	eyedb::Class const * get_class() const { return Instance()->getClass(); }
	eyedb::Class * get_class() { return Instance()->getClass(); }

	operator bool() const { return _o; }
	operator pointer_type() const { return _o; }
	pointer_type operator->() const { return _o; }
	pointer_type const & rep() const { return _o; }
private:
	eyedb::Struct * Struct() const { return _o; }
	eyedb::Instance * Instance() const { return _o; }
	pointer_type _o;
	void reserve() const { if (_o) _o->incrRefCount(); }
	void release() const { if (_o) _o->release(); }

	static pointer_type load(eyedb::Database * db, eyedb::Oid const & oid)
	{
		eyedb::Object * o = 0;
		if (oid != eyedb::Oid::nullOid)
			db->loadObject(oid, o);
		return object_type::_convert(o);
	}
	static pointer_type lookup(eyedb::Database * const db, std::string const & n)
	{
		std::string query = std::string("select ") + class_name() + ".name==\""+ n + "\"";
		eyedb::OidArray oids;
		eyedb::OQL(db, query.c_str()).execute(oids);
		if (oids.getCount() != 1)
			return 0;
		return load(db, oids[0]);
	}
	static pointer_type lookup(eyedb::Database * const db, int id)
	{
		std::ostringstream query;
		query << "select " << class_name() << ".id==" << id;
		eyedb::OidArray oids;
		eyedb::OQL(db, query.str().c_str()).execute(oids);
		if (oids.getCount() != 1)
			return 0;
		return load(db, oids[0]);
	}
};
template <class EyedbClass>
bool operator==(ObjectHandle<EyedbClass> const & o1, ObjectHandle<EyedbClass> const & o2)
{
	return o1.rep() == o2.rep();
}
template <class EyedbClass>
bool operator<(ObjectHandle<EyedbClass> const & o1, ObjectHandle<EyedbClass> const & o2)
{
	return o1.oid() < o2.oid();
}

#define MakeIterator(Parent, Type, Field, Const) \
struct Parent##Type##Proxy##Const \
{ \
	typedef mekano::Parent Const * parent_type; \
	typedef mekano::Type Const * field_type; \
private:\
	parent_type parent; \
	unsigned pos; \
public: \
	Parent##Type##Proxy##Const(parent_type p, unsigned d) : parent(p),  pos(d) {} \
	operator field_type() const { return parent->Field(pos); } \
};\
inline IteratorEyeDB<class Parent##Type##Proxy##Const> \
begin_##Field(mekano::Parent Const * p) { return IteratorEyeDB<Parent##Type##Proxy##Const>(p); } \
inline IteratorEyeDB<class Parent##Type##Proxy##Const> \
end_##Field(mekano::Parent Const * p) { return IteratorEyeDB<Parent##Type##Proxy##Const>(p, p->Field##_cnt()); }\
inline std::reverse_iterator<IteratorEyeDB<class Parent##Type##Proxy##Const> >\
rbegin_##Field(mekano::Parent Const * p) { return std::reverse_iterator<IteratorEyeDB<class Parent##Type##Proxy##Const> >(IteratorEyeDB<Parent##Type##Proxy##Const>(p, p->Field##_cnt())); } \
inline std::reverse_iterator<IteratorEyeDB<class Parent##Type##Proxy##Const> >\
rend_##Field(mekano::Parent Const * p) \
{ \
	return std::reverse_iterator<IteratorEyeDB<class Parent##Type##Proxy##Const> >(IteratorEyeDB<Parent##Type##Proxy##Const>(p)); \
 }

template <class Proxy>
struct IteratorEyeDB : public std::iterator
<
	std::random_access_iterator_tag,
	typename Proxy::field_type,
	unsigned,
	typename Proxy::field_type const *,
	Proxy
>
{
	typedef typename Proxy::parent_type parent_t;
	typedef Proxy reference;
	typedef unsigned difference_type;
private:
	parent_t parent;
	difference_type pos;
public:
	reference operator*() const { return reference(parent, pos); }
	reference operator[](difference_type d) const { return reference(parent, pos + d); }
//	pointer operator->() const { return ?
	IteratorEyeDB operator+(difference_type d) const { return IteratorEyeDB(parent, pos + d); }
	IteratorEyeDB operator-(difference_type d) const { return IteratorEyeDB(parent, pos - d); }
	difference_type operator-(IteratorEyeDB const & v) const { return pos - v.pos; }
	IteratorEyeDB(parent_t s) : parent(s), pos(0) {}
	IteratorEyeDB(parent_t s, difference_type p) : parent(s), pos(p) {}
	friend bool operator==(IteratorEyeDB const & s, IteratorEyeDB const & t)
	{
		return s.parent == t.parent && s.pos == t.pos;
	}
	friend bool operator!=(IteratorEyeDB const & s, IteratorEyeDB const & t) { return !(s == t); }
	IteratorEyeDB & operator++() { ++pos; return *this; }
	IteratorEyeDB operator++(int) { IteratorEyeDB tmp = *this; ++*this; return tmp; }
	IteratorEyeDB & operator--() { --pos; return *this; }
	IteratorEyeDB operator--(int) { IteratorEyeDB tmp = *this; --*this; return tmp; }
	IteratorEyeDB & operator+=(difference_type d) { pos += d; return *this; }
	IteratorEyeDB & operator-=(difference_type d) { pos -= d; return *this; }
	friend bool operator<(IteratorEyeDB const & s, IteratorEyeDB const & t)
	{
		return s.parent == t.parent && s.pos < t.pos;
	}
	friend bool operator>(IteratorEyeDB const & s, IteratorEyeDB const & t)
	{
		return s.parent == t.parent && s.pos > t.pos;
	}
	friend bool operator>=(IteratorEyeDB const & s, IteratorEyeDB const & t)
	{
		return s.parent == t.parent && s.pos >= t.pos;
	}
	friend bool operator<=(IteratorEyeDB const & s, IteratorEyeDB const & t)
	{
		return s.parent == t.parent && s.pos <= t.pos;
	}
};

#endif
