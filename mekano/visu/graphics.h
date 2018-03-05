/*	Stuart Pook, Genoscope 2006 (enscript -E $%) (Indent ON)
 *	$Id: graphics.h 776 2007-06-01 10:16:26Z stuart $
 *	uses fltk-2.0.x-r5497 from http://www.fltk.org/
 */
#ifndef MENAKO_VISU_GRAPHICS_H
#define MENAKO_VISU_GRAPHICS_H MENAKO_VISU_GRAPHICS_H
#include <string>
#include <iostream>
#include <vector>
#include <fltk/gl.h>
#include <fltk/GlWindow.h>
#include <fltk/Font.h>
#include <fltk/InvisibleBox.h>
#include <assert.h>
#include <fltk/draw.h>
#include <boost/utility.hpp>
#include <fltk/glut.h>
#include "utils.h"

int label_width(fltk::Widget const *);
inline int text_width(fltk::Widget const * widget, char const * s)
{
	int w;
	int h;
	fltk::setfont(widget->textfont(), widget->textsize());
	fltk::measure(s, w, h, 0);
	return w;
}
int width_of_widest_child(fltk::Group const *);

class Label : public fltk::InvisibleBox
{
	void init(int extra = 0)
	{
		w(label_width(this) + extra + fudge);
		flags(fltk::ALIGN_INSIDE_RIGHT);
	}
public:
	enum { fudge = 15 };
	Label(int x, int y, int h, char const * label, int extra) :
		fltk::InvisibleBox(x, y, 10, h, label)
	{
		init(extra);
	}
	Label(int x, int y, int h, char const * label) :
		fltk::InvisibleBox(x, y, 10, h, label)
	{
		init();
	}
	Label(int x, int y, int h, std::string const & label) :
		fltk::InvisibleBox(x, y, 10, h)
	{
		copy_label(label.c_str());
		init();
	}
};

class LabelSpace : public fltk::InvisibleBox
{
	std::string const _label;
public:
	enum { fudge = Label::fudge };
	LabelSpace(int x, int y, int h, std::string const & label, int extra) :
		fltk::InvisibleBox(x, y, 10, h),
		_label(label)
	{
		w(text_width(this, clabel()) + extra + fudge);
	}
	LabelSpace(int x, int y, int h, std::string const & label) :
		fltk::InvisibleBox(x, y, 10, h),
		_label(label)
	{
		w(text_width(this, clabel()) + fudge);
	}
	std::string label() const { return _label; }
	char const * clabel() const { return _label.c_str(); }
};

struct TextBase
{
	enum horizontal { right, left, centre = left };
	enum vertical { middle, vcentre = middle, top, bottom };
};

#define CHECK(f)	((f), glutil::check_for_errors(#f, __FILE__, __LINE__))

namespace glutil
{
	class check_for_errors
	{
		void error(GLenum, char const *, char const *, unsigned);
	public:
		check_for_errors(char const * after = 0, char const * file = 0, unsigned line = 0)
		{
			GLenum e = glGetError();
			if (e != GL_NO_ERROR)
				error(e, after, file, line);
		}
	};
	inline void flush()
	{
		CHECK(glFlush());
	}
	class display_list : private boost::noncopyable
	{
	public:
		class compiler
		{
			display_list * _owner;
			friend class glutil::display_list;
			compiler & _copy(compiler const & o)
			{
				if (_owner = o._owner)
					_owner->_current = this;
				return *this;
			}
			compiler & operator=(compiler const & o)
			{
				std::clog << "display_list::compiler::compiler assign " << o._owner << std::endl;
				return _copy(o);
			}
			explicit compiler(display_list * o) : _owner(o) { if (o) o->_current = this; }
		public:
			compiler(compiler const & o) // copy constructor
			{
				std::clog << "display_list::compiler::compiler copy " << o._owner << std::endl;
				_copy(o);
			}
			~compiler()
			{
#if 0
				std::clog << "display_list::compiler::~compiler " << _owner;
				if (_owner)
					std::clog << " " << _owner->_current;
				std::clog << " " << this << std::endl;
#endif
				if (_owner && _owner->_current == this)
				{
//					std::clog << "display_list::compiler::~compiler glEndList " << _owner->_id << std::endl;
					CHECK(glEndList());
					_owner->_current = 0;
				}
			}
			operator bool() const { return _owner; }
		};
		void do_clear()
		{
			if (_id)
			{
//				std::clog << "display_list::do_clear " << _id << std::endl;
				CHECK(glDeleteLists(_id, 1));
				_id = 0;
			}
		}
		void check_must_clear()
		{
			if (_must_clear)
			{
				_must_clear = false;
				do_clear();
			}
		}
	private:
 		GLuint _id;
		bool _must_clear;
		compiler * _current;
		friend class glutil::display_list::compiler;
	public:
		display_list() : _id(0), _must_clear(false), _current(0) {}
		compiler compile()
		{
			check_must_clear();
			if (_current)
			{
				std::clog << "display_list::compile already compiling " << _id << std::endl;
				assert(0);
			}
			if (compiled())
				return compiler(0);
			CHECK(_id = glGenLists(1));
//			std::clog << "display_list::compile " << _id << std::endl;
			CHECK(glNewList(_id, GL_COMPILE));
			return compiler(this);
		}
		void call() const
		{
			assert(!_current);
			assert(!_must_clear);
			assert(compiled());
//			std::clog << "display_list::call " << _id << std::endl;
			CHECK(glCallList(_id));
		}
		~display_list()
		{
			assert(!_current);
			do_clear();
			std::clog << "display_list::~display_list " << _id << std::endl;
		}
		void clear()
		{
			assert(!_current);
			_must_clear = true;
		}
		bool compiled() const
		{
			assert(!_current);
			return _id && !_must_clear;
		}
		friend std::ostream & operator<<(std::ostream & s, display_list const & l)
		{
			return s << l._id;
		}
	};
	struct translate : private boost::noncopyable
	{
		translate(double  x, double y)
		{
			glPushMatrix();
			CHECK(glTranslated(x, y, 0));
		}
		~translate() { CHECK(glPopMatrix()); }
	};
	struct push_name : private boost::noncopyable
	{
		push_name() { CHECK(glPushName(42)); }
		push_name(GLuint name) { CHECK(glPushName(name)); }
		~push_name() { CHECK(glPopName()); }
		void change(GLuint name) { CHECK(glLoadName(name)); };
	};
#if 0
	struct compile_list : private boost::noncopyable
	{
		compile_list(GLuint list)
		{
			std::clog << "compile_list::compile_list " << list << std::endl;
			CHECK(glNewList(list, GL_COMPILE));
		}
		~compile_list() {
			std::clog << "compile_list::~compile_list" << std::endl;
			CHECK(glEndList());
		}
	};
#endif
	struct begin : private boost::noncopyable
	{
		begin(GLenum mode) { CHECK(glBegin(mode)); }
		~begin() { CHECK(glEnd()); }
	};
	template <GLenum mode>
	struct enablet : private boost::noncopyable
	{
		enablet() { CHECK(glEnable(mode)); }
		~enablet() { CHECK(glDisable(mode)); }
	};
	struct enable : private boost::noncopyable
	{
		GLenum const mode;
		enable(GLenum m) : mode(m) { if (mode) CHECK(glEnable(mode)); }
		~enable() { if (mode) CHECK(glDisable(mode)); }
	};
	class tuber
	{
		class Colour;
		class GLUnurbsObj;
		GLUnurbsObj * const renderer;
	public:
		tuber();
		~tuber();
		void operator()(double, double, double, double, Colour const &);
	};
	class mkarray
	{
		GLfloat a[4];
		typedef GLfloat const * p;
	public:
		mkarray(GLfloat a0, GLfloat a1, GLfloat a2, GLfloat a3)
		{
			a[0] = a0; a[1] = a1; a[2] = a2; a[3] = a3;
		}
		operator p() { return a; }
	};
	class lighting
	{
		enablet<GL_COLOR_MATERIAL> color_material;
		enablet<GL_LIGHTING> lighting_;
		enablet<GL_LIGHT0> lighting0;
		enablet<GL_DEPTH_TEST> depth_test;
	public:
		lighting()
		{
			CHECK(glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE));
			CHECK(glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, mkarray(0.1, 0.1, 0.1, 1)));
			CHECK(glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 100));
		}
	};
}

class Colour
{
	GLfloat _c[4];
	static unsigned char uchar(GLfloat f) { return (unsigned char)(f * 255); }
public:
	static Colour i(int r, int g, int b) { double v = 255; return Colour(r / v, g / v,  b / v); }
	static Colour gray(int g) { double v = 255; return Colour(g / v, g / v,  g / v); }
	unsigned char ured() const { return uchar(_c[0]); }
	unsigned char ugreen() const { return uchar(_c[1]); }
	unsigned char ublue() const { return uchar(_c[2]); }
	double red() const { return _c[0]; }
	double green() const { return _c[1]; }
	double blue() const { return _c[2]; }
	Colour(double r, double g, double b) { _c[0] = r; _c[1] = g; _c[2] = b; _c[3] = 1; }
	static Colour hsv(double h, double s, double v);
	Colour() { _c[0] =_c[1] = _c[2] = 0; _c[3] = 1; }
	void set() const {
#if 0
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, _c);
#else
		CHECK(glColor3fv(_c));
#endif
	}
	GLfloat const * array() const { return _c; }
};

class GLWidget : public fltk::GlWindow, public TextBase
{
	enum { font_list_count = 256 };
	int _text_width;
	static void * _xfont;
	GLuint _listbase;
	GLuint _forward_arrow;
	void make_textures();
	void draw();
	virtual bool dragged(short, GLint, GLuint const *, int, int, int, int) { return true; }
	int _old_x;
	int _old_y;
	bool _dragged;
	GLuint _selected[512];
	GLint _hits;
	short _operation;
	void pick(int, int);
	glutil::display_list display_list;
	static GLWidget * _drawing;
	bool const _expensive;
	bool already_drawing(std::string const & who, bool will_draw = false);
protected:
	bool unproject(int *, int *);
	enum { op_s = 2, op_south = op_s, op_info = op_s, op_north = 0, op_pan_vertical = op_north, };
 	virtual void picked(GLint, GLuint const *, int, int) {}
	int handle(int event);
	virtual void draw_all() = 0;
	virtual void projection() = 0;
	virtual void initialize() {}
	virtual void make_lists() {}
	void setfont();
	void clear() { display_list.clear(); }
public:
	virtual void recalculate() { clear(); redraw(); }
	void set_colour(Colour const & c)
	{
		glColor3fv(c.array());
	}
	int ascent() const;
	int descent() const;
	int width(char const *, int) const;
	int width(char c) const { return width(&c, 1); }
	int width(std::string const & s) const { return width(s.c_str(), s.size()); }
	int widest() const { return _text_width; }
	int draw(double x, double y, std::string const & s, enum horizontal h = left, enum vertical v = middle)
	{
		return draw(x, y, s, Colour(100, 100, 100), h, v);
	}
	int draw(double, double, std::string const &, Colour const &, enum horizontal = left, enum vertical = middle);
	void on_circle(double, double, std::string const &);
	GLuint arrow_texture() { if (_forward_arrow == 0) make_textures(); return _forward_arrow; }

	GLWidget(int x, int y, int w, int h, bool expensive = false) :
		fltk::GlWindow(x, y, w,h),
		_text_width(0),
		_listbase(0),
		_forward_arrow(0),
		_expensive(expensive)
		{}
	~GLWidget();
};

struct ScaledText : public TextBase
{
	struct Entry
	{
		double x;
		double ycentre;
		float length;
		float height;
		std::string text;
//		float width;
		Entry(double xi, double y, double l, double h, std::string const & s) :
			x(xi), ycentre(y), length(l), height(h), text(s) {}
	};
private:
	std::vector<Entry> _strings;
public:
	void scaled(double x, double ycentre, double length, double height, std::string const & s)
	{
		_strings.push_back(Entry(x, ycentre, length, height, s));
	}
	void draw(double, double, double, double, GLWidget const *) const;
	void clear() { _strings.clear(); }
};

inline std::ostream &
operator<<(std::ostream & o, Colour const & c)
{
	return o << "(" << c.red() << "," << c.green() << ',' << c.blue() << ')';
}

void arc(double, double, double, double, Colour const &, GLuint name = 0);

#if 0
void
spline
(
	double x1, double y1,
	double x2, double y2,
	double x0, double y0,
	double radius,
	Colour const & colour,
	bool tube = false
);
#endif
void spline2(double, double, double, double, Colour const &);
void bezier(double, double, double, double, Colour const &);
void line(double, double, double, double, Colour const &);

double texture_box(double, double, double, double, Colour const &, GLuint, bool);
int inline
texture_box(double x, double y, int length, double radius, Colour const & c, GLuint t, bool forward)
{
	texture_box(x, y, double(length), radius, c, t, forward);
	return length;
}

double cylinder(double, double, double, double, Colour const &);
int inline
cylinder(double x, double y, int length, double radius, Colour const & c)
{
	cylinder(x, y, double(length), radius, c);
	return length;
}
int cylinder(GLint, GLint, GLint, GLint, Colour const &);

class ShowWaitCursor : private boost::noncopyable
{
	fltk::Widget * _what;
public:
	ShowWaitCursor(fltk::Widget * widget, std::string const & s = "");
	~ShowWaitCursor();
};
#undef CHECK
#endif
