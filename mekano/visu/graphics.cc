/*	Stuart Pook, Genoscope 2007 (enscript -E $%) (Indent ON)
 *	$Id: graphics.cc 794 2007-06-04 17:19:02Z stuart $
 *	Requires fltk-2 (or greater?)
 */

#include <iostream>
using std::cerr;
#include <nurbs++/nurbsGL.h>

#include "graphics.h"
#include <fltk/gl.h>
#include <cmath>
#include <assert.h>
#include <ctype.h>
#include <GL/glu.h>
#include <fltk/glut.h>
#include <fltk/Font.h>
#include <fltk/draw.h>
#include <fltk/x.h>
// copied from OpenGL/GlChoice.h in fltk source
# define Window XWindow
# include <GL/glx.h>
# undef Window
#include <utils/templates.h>

#define CHECK(f)	(void(f), glutil::check_for_errors(#f, __FILE__, __LINE__))

class tracer
{
	enum {inc = 3};
	static int indent;
	std::string const file;
	int const line;
	std::string const function;
	void indenter()
	{
		for (int i = 0; i < indent; i++)
			std::clog << ' ';
	}
public:
	tracer(std::string const & f, int l, std::string const & fun) :
		file(f), line(l), function(fun)
	{
		indenter();
		std::clog << file << ':' << line << ' ' << function << std::endl;
		indent += inc;
	}
	~tracer()
	{
		indent -= inc;
		indenter();
		std::clog << "done " << file << ':' << line << ' ' << function << std::endl;
	}
};
int tracer::indent = 0;
#define TRACE() // tracer ttt_t(__FILE__, __LINE__, __FUNCTION__)

void
line(double x1, double y1, double x2, double y2, Colour const & c)
{
	TRACE();
	c.set();
	glVertex2d(x1, y1);
	CHECK(glVertex2d(x2, y2));
}

namespace
{
	void
	draw1line(double x1, double y1, double x2, double y2)
	{
		glColor3f(1, 0, 0);
		{
			glutil::begin begin(GL_LINES);
			glVertex2d(x1, y1);
			CHECK(glVertex2d(x2, y2));
		}
#if 0
		glColor3f(1, 1, 1);
		glPointSize(10);
		glutil::begin begin(GL_POINTS);
		glVertex2d(x1, y1);
#endif
	}

	inline void drawtext(char const * const text, int n)
	{
		glCallLists(n, GL_UNSIGNED_BYTE, text);
	}
	inline void drawtext(std::string const & s)
	{
		drawtext(s.c_str(), s.size());
	}
	inline void drawtext(char c)
	{
		drawtext(&c, 1);
	}
}

int GLWidget::ascent() const
{
	TRACE();
	assert(_xfont);
	return static_cast<XFontStruct *>(_xfont)->ascent;
}
int GLWidget::descent() const
{
	TRACE();
	assert(_xfont);
	return static_cast<XFontStruct *>(_xfont)->descent;
}
int
GLWidget::width(char const * s , int n) const
{
	TRACE();
	assert(_xfont);
	return XTextWidth(static_cast<XFontStruct *>(_xfont), s, n);
}

namespace
{

	// return dash number n, or pointer to ending \0 if none:
	const char *
	font_word(const char* p, int n)
	{
		for (; *p; p++)
			if (*p== '-' && --n == 0)
				return p;
		return p;
	}
	std::string
	scan_fonts(int const size, char * * xlist, int count)
	{
		if (!xlist || count <= 0)
			return "variable";

		char const encoding_[] = "iso10646-1"; // "iso8859-1"

		char const * best = xlist[0];	// best font found so far
		int const none = -1;
		int ptsize = none;	// size of best font found so far
		bool found_encoding = false;

		enum { pixels_position = 7, encoding_position = 13, };

		for (int i = 0; i < count; i++)
		{
			char * const thisname = xlist[i];
			char const * const this_encoding = font_word(thisname, encoding_position);
			if (*this_encoding && !strcmp(this_encoding + 1, encoding_))
			{
				if (!found_encoding) // forget any wrong encodings when we find the correct one
				{
					ptsize = none;
					found_encoding = true;
				}
			}
			else if (found_encoding)
				continue; // ignore later wrong encodings after we find a correct one
     
			char const * pixels = font_word(thisname, pixels_position);
			if (!*pixels || !isdigit(*++pixels))
				continue;
			int const this_size = atoi(pixels);
			if
			(
				this_size == size // exact match
				||
				this_size == 0 && ptsize != size // Use a scalable font if no exact match found
				||
				ptsize == none	// no fonts yet
				||
				ptsize != 0 && std::abs(this_size - size) < std::abs(ptsize - size) // nearest fixed size font
			)
			{
				best = thisname;
				ptsize = this_size;
			}
		}
		if (ptsize)
			return best;
		std::string result(best, font_word(best, pixels_position) - best);
		result.append(to_string(size));
		result.append(font_word(best, pixels_position + 1));
		return result;
	}
	XFontStruct *
	make_font(unsigned const size)
	{
		int count;
		char const system_name[] = "-*-helvetica-medium-r-normal--*";
		char * * xlist = XListFonts(fltk::xdisplay, system_name, 200, &count);
		std::string const xfontname = scan_fonts(size, xlist, count);
		XFreeFontNames(xlist);
//		std::clog << "XLoadQueryFont " << size << " -> " << xfontname << std::endl;
 		return XLoadQueryFont(fltk::xdisplay, xfontname.c_str());
	}
}
	
void * GLWidget::_xfont = 0;
//GLuint GLWidget::_forward_arrow = 0;

void
GLWidget::setfont()
{
	TRACE();
	if (!_xfont)
	{
		TRACE();
		_xfont = make_font(10);
	}
	if (!_listbase)
	{
		CHECK(_listbase = glGenLists(font_list_count));
		XFontStruct * const xfont = static_cast<XFontStruct *>(_xfont);
		int const base = xfont->min_char_or_byte2;
		int const last = std::min(255u, xfont->max_char_or_byte2);
		int const size = last - base + 1;
  		CHECK(glXUseXFont(xfont->fid, base, size, _listbase + base));
	}
	CHECK(glListBase(_listbase));
}

GLWidget::~GLWidget()
{
	if (_forward_arrow)
		CHECK(glDeleteTextures(1, &_forward_arrow));
	if (_listbase);
		CHECK(glDeleteLists(_listbase, font_list_count));
}

void
GLWidget::make_textures()
{
	enum { checkImageWidth = 64, checkImageHeight = checkImageWidth };
	GLubyte checkImage[checkImageHeight][checkImageWidth][4];

	for (int x = 0; x < checkImageHeight; x++)
		for (int y = 0; y < checkImageWidth; y++)
			checkImage[x][y][0] = checkImage[x][y][1] = checkImage[x][y][2] = checkImage[x][y][3] = 255;
	for (int y = 0; y < checkImageHeight; y++)
	{
		int x = (y < checkImageHeight / 2) ? y : (checkImageHeight - y - 1);
//		std::cout << "GLWidget::make_textures() " << y << " " << x << std::endl;
		for (int i = 0; i <= checkImageWidth / 2; i++)
			checkImage[x++][y][0] = 196;
		assert(x <= checkImageWidth);
	}

	glGenTextures(1, &_forward_arrow);
//	std::cout << "GLWidget::make_textures() "  << _forward_arrow << std::endl;
	glBindTexture(GL_TEXTURE_2D, _forward_arrow);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, checkImageWidth, checkImageHeight, 
                0, GL_RGBA, GL_UNSIGNED_BYTE, checkImage);
}

/*	must do
	glutil::enable do_texture(GL_TEXTURE_2D);
	glutil::begin poly(GL_QUADS); 
 */
double
texture_box(double x, double y, double length, double radius, Colour const & colour, GLuint texture, bool forward)
{
	assert(length > 0);
	assert(radius > 0);
	assert(texture);
	TRACE();
	colour.set();
//	std::clog << "texture_box "  << x << ' '  << y << ' ' << length << ' ' << height << std::endl;
	int const n_textures = 3 * (forward ? 1 : -1);
	glTexCoord2f(0, 0);
	glVertex2d(x, y - radius);
	glTexCoord2f(0, n_textures);
	glVertex2d(x + length, y - radius);
	glTexCoord2f(1, n_textures);
	glVertex2d(x + length, y + radius);
	glTexCoord2f(1, 0);
	CHECK(glVertex2d(x, y + radius));
	return length;
}

int
cylinder(GLint x, GLint y, GLint length, GLint radius, Colour const & colour)
{
	assert(length > 0);
	assert(radius > 0);
	TRACE();
	colour.set();
	glVertex2i(x, y - radius);
	glVertex2i(x + length, y - radius);
	glVertex2i(x + length, y + radius);
	CHECK(glVertex2i(x, y + radius));
	return length;
}

double
cylinder(double x, double y, double length, double radius, Colour const & colour)
{
	assert(length > 0);
	assert(radius > 0);
	TRACE();
	colour.set();
	glVertex2d(x, y - radius);
	glVertex2d(x + length, y - radius);
	glVertex2d(x + length, y + radius);
	CHECK(glVertex2d(x, y + radius));
	return length;
}

namespace {

 /*
 * Draws a solid torus
 */
void
solid_torus(double const iradius, double const oradius, double const start, double const fraction)
{
	TRACE();
	enum { rings_on_full_circle = 100, sides = 21 };
	int const rings = std::max(5, int(rings_on_full_circle * fraction + 0.999) + 1);

	double normal[3 * sides * rings];
	double vertex[3 * sides * rings];

	double const radians = 2 * M_PI * fraction;
	double const dpsi = radians / (rings - 1.0);
	double const dphi = -2 * M_PI / (sides - 1.0);

	double psi  = 2 * M_PI * start;

	for (int j=0; j<rings; j++ )
	{
		double const cpsi = cos(psi);
		double const spsi = sin(psi);
		double phi = 0.0;

		for (int i = 0; i < sides; i++)
		{
			int const offset = 3 * (j * sides + i);
			double const cphi = cos(phi);
			double const sphi = sin(phi);
			*(vertex + offset + 0) = cpsi * (oradius + cphi * iradius) ;
			*(vertex + offset + 1) = spsi * (oradius + cphi * iradius);
			*(vertex + offset + 2) = sphi * iradius	;
			*(normal + offset + 0) = cpsi * cphi ;
			*(normal + offset + 1) = spsi * cphi ;
			*(normal + offset + 2) = sphi ;
			phi += dphi;
		}
		psi += dpsi;
	}

	glutil::begin quads(GL_QUADS);

	for (int i = 0; i < sides - 1; i++)
		for (int j = 0; j < rings - 1; j++)
		{
			int const offset = 3 * ( j * sides + i ) ;
			glNormal3dv( normal + offset );
			glVertex3dv( vertex + offset );
			glNormal3dv( normal + offset + 3 );
			glVertex3dv( vertex + offset + 3 );
			glNormal3dv( normal + offset + 3 * sides + 3 );
			glVertex3dv( vertex + offset + 3 * sides + 3 );
			glNormal3dv( normal + offset + 3 * sides );
			glVertex3dv( vertex + offset + 3 * sides );
		}

	glNormal3f(0, 0, 1);
}
}

#define nelements(a)	(sizeof (a) / sizeof (a)[0])

namespace
{
	void nurbsError(GLenum errorCode)
	{
		std::clog << "Nurbs error: " << gluErrorString(errorCode) << std::endl;
	}
}

void circle()
{
	TRACE();
	glColor3f(1, 1, 1);
#if 1
int nCtrlPoints = 7;    
GLfloat ctrlPoints[7][4];
GLfloat pointsWeights[7][4]  = {    
#if 1
    {0,-.5, 0, 1},
    {.5,-.5, 0, 0.5},
    {0.25, -0.0669873, 0, 1},
    {0, 0.3880254, 0, 0.5},
    {-.25,  -0.0669873, 0, 1},
    {-.5,-.5, 0, 0.5},
    {0,-.5, 0, 1}};
#else
      {0, 0.5, 0, 1},
    {.5,-.5, 0, 0.5},
    {0.25, -0.0669873, 0, 1},
    {0, 0.3880254, 0, 0.5},
    {-.25,  -0.0669873, 0, 1},
    {-.5,-.5, 0, 0.5},
    {0,-.5, 0, 1}};
#endif
  
    
    // Knot vector
    int order = 3;
    int nKnots = 10;
    GLfloat Knots[10] = {0,0,0,1,1,2,2,3,3,3};
#define kStride 4
    
            // Setup the Nurbs object
            GLUnurbsObj * pNurb = gluNewNurbsRenderer();
            gluNurbsProperty(pNurb, GLU_SAMPLING_TOLERANCE, 5.0f);
            //gluNurbsProperty(pNurb, GLU_DISPLAY_MODE, GLU_OUTLINE_POLYGON);
            //gluNurbsProperty(pNurb, GLU_DISPLAY_MODE, (GLfloat)GLU_FILL);
            
            // make the NURBS control points from the points & weights
            for(int i = 0; i < nCtrlPoints; i++)
            {
                ctrlPoints[i][3] = pointsWeights[i][3];
                for (int j = 0; j < 3; j++)
                    ctrlPoints[i][j] = pointsWeights[i][j]*ctrlPoints[i][3];
            }

	
       gluBeginCurve(pNurb);
        
        // Send the Non Uniform Rational BSpline
        gluNurbsCurve(pNurb, nKnots, Knots, kStride, 
            &ctrlPoints[0][0], order, GL_MAP1_VERTEX_4);
        
        gluEndCurve(pNurb);
	gluDeleteNurbsRenderer(pNurb);
#else
        	GLfloat circleknots[] = {0, 0, 0, 1/4.0, 1/4.0, 2/4.0, 2/4.0, 3/4.0, 3/4.0, 1, 1, 1 };
	GLfloat const w = M_SQRT2 / 2;
	GLfloat circlepts[][3] =
	{
		{ 1, 0, 1 },
		{ 1, 1, w }, // 1
		{ 0, 1, 1 },
		{ -1, 1, w }, // 3
		{ -1, 0, 1 },
		{ -1, -1, w }, // 5
		{ 0, -1, 1 },
		{ 1, -1, w }, // 7
		{ 1, 0, 1 },
	};
            // make the NURBS control points from the points & weights
            for(unsigned i = 0; i < nelements(circlepts); i++)
            {
                for (int j = 0; j < 2; j++)
                {
                    circlepts[i][j] = circlepts[i][j]*circlepts[i][2];
                }
            }
	GLUnurbsObj * nobj = gluNewNurbsRenderer();
	gluNurbsCallback(nobj, GLU_ERROR, (GLvoid (*)())nurbsError);
	gluNurbsProperty(nobj, GLU_SAMPLING_TOLERANCE, 5);
	gluNurbsProperty(nobj, GLU_U_STEP, (10 - 1) * 4);
	gluNurbsProperty(nobj, GLU_DISPLAY_MODE, GLU_OUTLINE_POLYGON);
	gluBeginCurve(nobj);
	gluNurbsCurve(nobj, nelements(circleknots), circleknots, nelements(circlepts[0]), &circlepts[0][0],
		3, GL_MAP1_VERTEX_3);
	gluEndCurve(nobj);
	gluDeleteNurbsRenderer(nobj);
#endif
}

void
arc(double iradius, double oradius, double from, double length, Colour const & colour, GLuint)
{
	TRACE();
	colour.set();
#if 1
	solid_torus(iradius, oradius, from, length);
#else

glutil::enable mapping(GL_MAP2_VERTEX_4);
	glutil::enable normals(GL_AUTO_NORMAL);
//glFrontFace(GL_CCW);
GLUnurbsObj *nobj;
  nobj = gluNewNurbsRenderer();
  gluNurbsProperty(nobj, GLU_SAMPLING_METHOD, GLU_DOMAIN_DISTANCE);

	enum { usegments = 10, vsegments = usegments, };
	gluNurbsProperty(nobj, GLU_U_STEP, (usegments - 1) * 4);
	gluNurbsProperty(nobj, GLU_V_STEP, (vsegments - 1) * 4);

	gluNurbsProperty(nobj, GLU_DISPLAY_MODE, GLU_OUTLINE_POLYGON);
//	gluNurbsProperty(nobj, GLU_DISPLAY_MODE, GLU_FILL);
	gluBeginSurface(nobj);
/* Control points of a torus in NURBS form.  Can be rendered using
   the GLU NURBS routines. */
	GLfloat torusnurbpts[7][7][4] =
	{
#if 1
	{
		{4.0, 0.0, 0.0, 4.0}, {2.0, 0.0, 1.0, 2.0}, {4.0, 0.0, 1.0, 2.0},
		{8.0, 0.0, 0.0, 4.0}, { 4.0, 0.0,-1.0, 2.0}, { 2.0, 0.0,-1.0, 2.0},
		{4.0, 0.0, 0.0, 4.0},
	},
	{
		{ 2.0,-2.0, 0.0, 2.0}, { 1.0,-1.0, 0.5, 1.0}, {2.0,-2.0, 0.5, 1.0},
		{ 4.0,-4.0, 0.0, 2.0}, { 2.0,-2.0,-0.5, 1.0}, {1.0,-1.0,-0.5, 1.0},
		{ 2.0,-2.0, 0.0, 2.0},
	},
	{
		{-2.0,-2.0, 0.0, 2.0}, {-1.0,-1.0, 0.5, 1.0}, {-2.0,-2.0, 0.5, 1.0},
		{-4.0,-4.0, 0.0, 2.0}, {-2.0,-2.0,-0.5, 1.0}, {-1.0,-1.0,-0.5, 1.0},
		{-2.0,-2.0, 0.0, 2.0},
	},
	{
		{-4.0, 0.0, 0.0, 4.0}, {-2.0, 0.0, 1.0, 2.0}, {-4.0, 0.0, 1.0, 2.0},
		{-8.0, 0.0, 0.0, 4.0}, {-4.0, 0.0,-1.0, 2.0}, {-2.0, 0.0,-1.0, 2.0},
		{-4.0, 0.0, 0.0, 4.0},
	},
	{
		{-2.0, 2.0, 0.0, 2.0}, {-1.0, 1.0, 0.5, 1.0}, {-2.0, 2.0, 0.5, 1.0},
		{-4.0, 4.0, 0.0, 2.0}, {-2.0, 2.0,-0.5, 1.0}, {-1.0, 1.0,-0.5, 1.0},
		{-2.0, 2.0, 0.0, 2.0},
	},
	{
		{ 2.0, 2.0, 0.0, 2.0}, {1.0, 1.0, 0.5, 1.0}, { 2.0, 2.0, 0.5, 1.0},
		{ 4.0, 4.0, 0.0, 2.0}, {2.0, 2.0,-0.5, 1.0}, { 1.0, 1.0,-0.5, 1.0},
		{ 2.0, 2.0, 0.0, 2.0},
	},
	{
		{4.0, 0.0, 0.0, 4.0}, { 2.0, 0.0, 1.0, 2.0}, { 4.0, 0.0, 1.0, 2.0},
		{8.0, 0.0, 0.0, 4.0}, { 4.0, 0.0,-1.0, 2.0}, { 2.0, 0.0,-1.0, 2.0},
		{4.0, 0.0, 0.0, 4.0},
	}
#else
   4.0, 0.0, 0.0, 4.0, 2.0, 0.0, 1.0, 2.0, 4.0, 0.0, 1.0, 2.0,
   8.0, 0.0, 0.0, 4.0, 4.0, 0.0,-1.0, 2.0, 2.0, 0.0,-1.0, 2.0,
   4.0, 0.0, 0.0, 4.0, 2.0,-2.0, 0.0, 2.0, 1.0,-1.0, 0.5, 1.0,
   2.0,-2.0, 0.5, 1.0, 4.0,-4.0, 0.0, 2.0, 2.0,-2.0,-0.5, 1.0,
   1.0,-1.0,-0.5, 1.0, 2.0,-2.0, 0.0, 2.0,-2.0,-2.0, 0.0, 2.0,
  -1.0,-1.0, 0.5, 1.0,-2.0,-2.0, 0.5, 1.0,-4.0,-4.0, 0.0, 2.0,
  -2.0,-2.0,-0.5, 1.0,-1.0,-1.0,-0.5, 1.0,-2.0,-2.0, 0.0, 2.0,
  -4.0, 0.0, 0.0, 4.0,-2.0, 0.0, 1.0, 2.0,-4.0, 0.0, 1.0, 2.0,
  -8.0, 0.0, 0.0, 4.0,-4.0, 0.0,-1.0, 2.0,-2.0, 0.0,-1.0, 2.0,
  -4.0, 0.0, 0.0, 4.0,-2.0, 2.0, 0.0, 2.0,-1.0, 1.0, 0.5, 1.0,
  -2.0, 2.0, 0.5, 1.0,-4.0, 4.0, 0.0, 2.0,-2.0, 2.0,-0.5, 1.0,
  -1.0, 1.0,-0.5, 1.0,-2.0, 2.0, 0.0, 2.0, 2.0, 2.0, 0.0, 2.0,
   1.0, 1.0, 0.5, 1.0, 2.0, 2.0, 0.5, 1.0, 4.0, 4.0, 0.0, 2.0,
   2.0, 2.0,-0.5, 1.0, 1.0, 1.0,-0.5, 1.0, 2.0, 2.0, 0.0, 2.0,
   4.0, 0.0, 0.0, 4.0, 2.0, 0.0, 1.0, 2.0, 4.0, 0.0, 1.0, 2.0,
   8.0, 0.0, 0.0, 4.0, 4.0, 0.0,-1.0, 2.0, 2.0, 0.0,-1.0, 2.0,
   4.0, 0.0, 0.0, 4.0,
#endif
};
#if 0
	for (int i = 0; i < 7; i++)
		for (int j = 0; j < 7; j++)
		{
			GLfloat * p = torusnurbpts[i][j];
			p[0] /= p[3];
			p[1] /= p[3];
			p[2] /= p[3];
			p[3] /= p[3];
		}
#endif
//	http://www.opengl.org/resources/code/samples/glut_examples/examples/surfgrid.c
//	GLfloat circleknots[] = {0, 1,2,3,4,5,6,7,8,9,10,11};
	GLfloat circleknots[] = {0.0, 0.0, 0.0, 0.25, 0.50, 0.50, 0.75, 1.0, 1.0, 1.0};
	gluNurbsSurface(nobj, 10, circleknots, 10 - 4, circleknots,
		4, 28, &torusnurbpts[0][0][0], 3, 3, GL_MAP2_VERTEX_4);
	gluEndSurface(nobj);
#endif
}

namespace
{
	void reverse_draw_char(char c, GLWidget const * widget)
	{
		TRACE();
		float const w = widget->width(c);
		glBitmap(0, 0, 0, 0, -w, 0, 0);
		drawtext(&c, 1);
		glBitmap(0, 0, 0, 0, -w, 0, 0);
	}
}

class draw_string : public std::unary_function<ScaledText::Entry, void>
{
	double const xscale;
	double const yscale;
	double const xo;
	double const yo;
	float const height;
	float const yshift;
	GLWidget const * const widget;
	void draw(double x, GLfloat const y, std::string const & str)
	{
		TRACE();
		for (char const * s = str.c_str(); *s; s++)
		{
			glRasterPos3f(x, y, 10);
			drawtext(*s);
			x += widget->width(*s) * xscale;
		}
	}
public:
	draw_string(double x, double y, double x0, double y0, float h, float s, GLWidget const * w) :
		xscale(x), yscale(y), xo(x0), yo(y0), height(h), yshift(s), widget(w)
		{}
	result_type operator()(argument_type const & e)
	{
		TRACE();
		float const h = e.height / yscale - height;
		if (h >= 0)
		{
			double const width = widget->width(e.text) * xscale;
			if (int n = int(e.length / width))
			{
				if (n > 1)
					n /= 2;
				GLfloat y = e.ycentre + yscale * (h / 2 + yshift);
				GLfloat gap = (e.length - width * n) / (2 * n);
				for (int i = 0; i < n; i++)
					draw(e.x + (2 * i + 1) * gap + i * width, y, e.text);
			}
//			else std::clog << "draw_string::operator() " << e.text << " e.length " << e.length << " width " << width << " xscale " << xscale << std::endl;
		}
//		else std::clog << "draw_string::operator() " << e.text << " e.height " << e.height << " h " << h << " height " << height << " yscale " << yscale << std::endl;
	}
};

void
ScaledText::draw(double xscale, double yscale, double ox, double oy, GLWidget const * w) const
{
	TRACE();
	float const descent = w->descent();
	float const ascent = w->ascent();
	float const height = descent + ascent;
	float const yshift = (descent - ascent) / 2.0;

	Colour(100, 100, 100).set();
//	std::clog << "ScaledText::draw() " << xscale << ' ' << yscale << std::endl;
	for_each(_strings.begin(), _strings.end(), draw_string(xscale, yscale, ox, oy, height, yshift, w));
}

void
GLWidget::on_circle(double x, double y, std::string const & n)
{
	TRACE();
	enum vertical v;
	if (y == 0)
		v = middle;
	else if (y < 0)
		v = top;
	else
		v = bottom;
	enum horizontal h;
	if (x == 0)
		h = centre;
	else if (x < 0)
		h = right;
	else
		h = left;
	draw(x, y, n, h, v);
}

int
GLWidget::draw(double x, double y, std::string const & n, Colour const & colour, enum horizontal hpos, enum vertical vpos)
{
	TRACE();
//	std::clog << n << " at " << x << " " << y << std::endl;

	int const w = width(n);
	_text_width = std::max(_text_width, w);
	colour.set();
	glRasterPos3d(x, y, 3);
#if 0
	GLboolean valid;
	glGetBooleanv(GL_CURRENT_RASTER_POSITION_VALID, &valid);
	if (!valid)
	{	// see http://www.mesa3d.org/brianp/TR.html
		std::clog << n << " at " << x << " " << y << " not valid, skipping" << std::endl;
	}
	else
#endif
	{
		GLdouble position[4];
		glGetDoublev(GL_CURRENT_RASTER_POSITION, position);

		GLfloat yshift = 0;
		switch (vpos)
		{
		case middle:
			yshift = (descent() - ascent()) / 2.0;
			break;

		case top:
			yshift = -ascent();
			break;

		case bottom:
			yshift = descent();
		}
		glBitmap(0, 0, 0, 0, 0, yshift, 0);
		switch (hpos)
		{
		case right:
			std::for_each(n.rbegin(), n.rend(), std::bind2nd(std::ptr_fun(reverse_draw_char), this));
			break;

		case left:
			drawtext(n.c_str());
			break;

		default:
			std::clog << n << " at " << x << " " << y << " bad hpos " << hpos << std::endl;
		}
	}
	return w;
}

namespace
{
	void
	line_position_on_circle
	(
		double p1, double p2,
		double * x1, double * y1,
		double * x2, double * y2,
		double * x0, double * y0, double const r
	)
	{
		assert(p1 <= p2);
		
		double d = p2 - p1;
		double a0 = p1 + p2;
		if (d > 0.5)
		{
			d = 1 - d;
			a0 = p1 - 1 + p2;
		}
		d = 2 * (0.5 - d);
		
		assert(d <= 1 && d >= 0);
		
		a0 *= M_PI;
		
		p1 *= M_PI * 2;
		p2 *= M_PI * 2;
		*x1 = r * cos(p1);
		*y1 = r * sin(p1);
		*x2 = r * cos(p2);
		*y2 = r * sin(p2);
		*x0 = d * r * cos(a0);
		*y0 = d * r * sin(a0);
	}
}

inline double
atan3(double x, double y)
{
	return atan2(x, y);
//	return (r < 0) ? M_PI + r : r;
};

void
bezier(double p1, double p2, double const circle_radius, double const, Colour const & colour)
{
	TRACE();
	double x1, x2, y1, y2, x0, y0;
	line_position_on_circle(p1, p2, &x1, &y1, &x2, &y2, &x0, &y0, circle_radius);

	colour.set();
	enum { u1 = 0, u2 = 1, count = 30, vertexes = 3, order = 3, };
	glutil::enable mapping(GL_MAP1_VERTEX_3); // 3 == vertexes
	GLdouble control_points[][vertexes] = { { x1, y1, 0 }, { x0, y0, 0 },  { x2, y2, 0 } };
	assert(order == sizeof control_points / sizeof control_points[0]);
	glMap1d(mapping.mode, u1, u2, vertexes, order, &control_points[0][0]);
	glMapGrid1f(count, u1, u2);
	glEvalMesh1(GL_LINE, 0, count);
}

#if 0
static
void error(char const * s, NurbsError & e)
{
	std::clog << s << std::endl;
	e.print();
	exit(1);
}
#endif

void
spline2
(
	double p1, double p2,
	double const circle_radius,
	double const tube_radius,
	Colour const & colour
)
{
	TRACE();
//	glPointSize(10);
	double x1, x2, y1, y2, x0, y0;
	line_position_on_circle(p1, p2, &x1, &y1, &x2, &y2, &x0, &y0, circle_radius);

	colour.set();
#if 1
	GLUnurbsObj * const nurbs_renderer = gluNewNurbsRenderer();
	gluNurbsProperty(nurbs_renderer, GLU_SAMPLING_TOLERANCE, 5);
	gluNurbsCallback(nurbs_renderer, GLU_ERROR, (GLvoid (*)())nurbsError);

//	PLib::NurbsDisplayMode = PLib::NURBS_DISPLAY_SHADED;
	Vector_Point3Df tp;
	tp.resize(3);
	tp[0] = PLib::Point3Df(x1, y1, 0);
	tp[1] = PLib::Point3Df(x0, y0, 0);
	tp[2] = PLib::Point3Df(x2, y2, 0);
	PLib::NurbsCurveGL T;
	T.globalInterp(tp, 2);
	PLib::NurbsCurveGL C;
	C.makeCircle(PLib::Point3Df(0, 0, 0), PLib::Point3Df(1, 0, 0),PLib::Point3Df(0, 0, 1), tube_radius, 0, 2 * M_PI);
	PLib::NurbsSurfaceGL surface;
	enum { quality = 12 };
	surface.sweep(T, C, quality);
	glutil::enable normals(GL_AUTO_NORMAL);
	glutil::enable normalize(GL_NORMALIZE);
	gluNurbsSurface(nurbs_renderer, surface.U.n(), surface.U.memory(), surface.V.n(), surface.V.memory(),4,4*surface.P.rows(), surface.P[0]->data, surface.degU+1, surface.degV+1, GL_MAP2_VERTEX_4);
//	surface.gluNurbs();
	gluDeleteNurbsRenderer(nurbs_renderer);
#else
	enum { u1 = 0, u2 = 1, count = 30, vertexes = 3, };
	{
		double const t1 = atan3(y1 - y0, x1 - x0);
		double const t2 = atan3(y2 - y0, x2 - x0);
		enum { v1 = u1, v2 = u2, uorder = 4, vorder = 3 };
		double const d = tube_radius;
		double const dx1 = d * sin(t1);
		double const dy1 = d * cos(t1);
		double const dx2 = d * sin(t2);
		double const dy2 = d * cos(t2);
		double const t = (t2 + t1) / 2;
		double dx0 = d * cos(t);
		double dy0 = d * sin(t);
		if (t2 < t1)
		{
			dx0 *= -1;
			dy0 *= -1;
		}
#if 0	
		enum { m = 3 };
		draw1line(x1 + m * dx1, y1 - m * dy1, x1 - m * dx1, y1 + m * dy1);
		draw1line(x0 - m * dx0, y0 - m * dy0, x0 + m * dx0, y0 + m * dy0);
		draw1line(x2 - m * dx2, y2 + m * dy2, x2 + m * dx2, y2 - m * dy2);
#endif
		GLdouble control_points[][uorder][vertexes] = {
		{
			{ x1 + dx1, y1 - dy1, 0 },
			{ x1, y1, -d },
			{ x1 - dx1, y1 + dy1, 0 },
			{ x1, y1, +d },
//			{ x1 + dx1, y1 - dy1, 0 },
		},
		{
			{ x0 - dx0, y0 - dy0, 0 },
			{ x0, y0, -d },
			{ x0 + dx0, y0 + dy0, 0},
			{ x0, y0, +d },
//			{ x0 - dx0, y0 - dy0, 0 },
		},
		{
			{ x2 - dx2, y2 + dy2, 0 },
			{ x2, y2, -d },
			{ x2 + dx2, y2 - dy2, 0 },
			{ x2, y2, +d },
//			{ x2 - dx2, y2 + dy2, 0 },
		}
		};
		assert(vorder == sizeof control_points / sizeof control_points[0]);
		assert(uorder == sizeof control_points[0] / sizeof control_points[0][0]);
		glutil::enable mapping(GL_MAP2_VERTEX_3); // 3 == vertexes
		glMap2d
		(
			mapping.mode,
			u1, u2, vertexes, uorder, // u1, u2, ustride, uorder
			v1, v2, vertexes * uorder, vorder, // v1, v2, vstride, vorder
			&control_points[0][0][0]
		);
		glutil::enable normals(GL_AUTO_NORMAL);
		glMapGrid2f(count, u1, u2, count, v1, v2);
		glEvalMesh2(GL_FILL, 0, count, 0, count);
	}
#endif
}

void
spline
(
	double x1, double y1,
	double x2, double y2,
	double x0, double y0,
	double radius,
	Colour const & colour,
	bool tube
)
{
	TRACE();
	std::swap(x1, x2);
	std::swap(y1, y2);
	colour.set();
	enum { u1 = 0, u2 = 1, count = 30, vertexes = 3, };
	if (tube)
	{
		enum { v1 = u1, v2 = u2, uorder = 4, vorder = 3 };
		double const d = radius;
		GLdouble control_points[][uorder][vertexes] = {
#if 0
		{
			{ x1 + d, y1 + d, 0 },
			{ x1 + d, y1 - d, 0 },
			{ x1 - d, y1 - d, 0 },
			{ x1 - d, y1 + d, 0 },
		},
		{
			{ x0 + d, y0 + d, 0 },
			{ x0 + d, y0 - d, 0 },
			{ x0 - d, y0 - d, 0 },
			{ x0 - d, y0 + d, 0 },
		},
		{
			{ x2 + d, y2 + d, 0 },
			{ x2 + d, y2 - d, 0 },
			{ x2 - d, y2 - d, 0 },
			{ x2 - d, y2 + d, 0 },
		}
#else
		{
			{ x1 + d, y1,  +d },
			{ x1 + d, y1,  -d },
			{ x1 - d, y1,  -d },
			{ x1 - d, y1,  +d },
		},
		{
			{ x0 + d, y0,  +d },
			{ x0 + d, y0,  -d },
			{ x0 - d, y0,  -d },
			{ x0 - d, y0,  +d },
		},
		{
			{ x2 + d, y2,  + d },
			{ x2 + d, y2,  - d },
			{ x2 - d, y2,  - d },
			{ x2 - d, y2,  + d },
		}
#endif
		};
		assert(vorder == sizeof control_points / sizeof control_points[0]);
		assert(uorder == sizeof control_points[0] / sizeof control_points[0][0]);
		glMap2d
		(
			GL_MAP2_VERTEX_3, //mapping.mode,
			u1, u2, vertexes, uorder, // u1, u2, ustride, uorder
			v1, v2, vertexes * uorder, vorder, // v1, v2, vstride, vorder
			&control_points[0][0][0]
		);
		glutil::enable mapping(GL_MAP2_VERTEX_3); // 3 == vertexes
		glutil::enable normals(GL_AUTO_NORMAL);
		glMapGrid2f(count, u1, u2, count, v1, v2);
		glEvalMesh2(GL_FILL, 0, count, 0, count);
	}
	else
	{
		enum { order = 3, };
		glutil::enable mapping(GL_MAP1_VERTEX_3); // 3 == vertexes
		GLdouble control_points[][vertexes] = { { x1, y1, 0 }, { x0, y0, 0 },  { x2, y2, 0 } };
		assert(order == sizeof control_points / sizeof control_points[0]);
		glMap1d(mapping.mode, u1, u2, vertexes, order, &control_points[0][0]);
		glMapGrid1f(count, u1, u2);
		glEvalMesh1(GL_LINE, 0, count);
	}
}

// http://en.wikipedia.org/wiki/HSV_color_space
Colour
Colour::hsv(double hue, double saturation, double value)
{
	if (saturation == 0)
		return Colour(value, value, value);
	if (hue >= 360)
		hue = 0;
	int ihue = int(hue / 60) % 6;
	double f = hue / 60 - ihue;
	double p = value * (1 - saturation);
	double q = value * (1 - f * saturation);
	double t = value * (1 - (1 - f) * saturation);
	switch (ihue)
	{
	case 0:
		return Colour(value, t, p);
	case 1:
		return Colour(q, value, p);
	case 2:
		return Colour(p, value, t);
	case 3:
		return Colour(p, q, value);
	case 4:
		return Colour(t, p, value);
	}
	return Colour(value, p, q);
}

namespace glutil
{
	void check_for_errors::error(GLenum e, char const * after, char const * file, unsigned line)
	{
		TRACE();
		if (file)
			std::clog << file << ":" << line << ": ";
		std::clog << "GL error";
		if (after)
			std::clog << " after \"" << after << '"';
		std::cerr << ": ";
		if (GLubyte const * s = gluErrorString(e))
			std::clog << s;
		else
			std::clog << "unknown";
		std::clog << std::endl;
		int z = 0;
		assert(0/z);
	}
}

void
GLWidget::draw()
{
	if (already_drawing("draw", true))
		return;
	CHECK("GLWidget::draw()");
	TRACE();

	if (!valid())
	{
		CHECK("GLWidget::draw() ... valid");
		TRACE();
		CHECK(mode(fltk::DEPTH_BUFFER | fltk::DOUBLE_BUFFER | fltk::RGB_COLOR));
		assert(w() > 0 && h() > 0);
		CHECK(glViewport(0, 0, w(), h()));
		CHECK(ortho());
		setfont();	
		CHECK(initialize());
	}

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	if (!display_list.compiled())
	{
		TRACE();
		ortho();
		ShowWaitCursor w(_expensive ? this : 0, "GLWidget::draw()");
		make_lists();
		glutil::display_list::compiler compiling = display_list.compile();
		assert(compiling);
		draw_all();
	}
	glMatrixMode(GL_PROJECTION);
	CHECK(glLoadIdentity());
	projection();

	glMatrixMode(GL_MODELVIEW);
	CHECK(glLoadIdentity());
//	std::clog << "GLWidget::draw() calling " << display_list << std::endl;
	display_list.call();
	
	_drawing = 0;
}

int
label_width(fltk::Widget const * widget)
{
	int w = 0;
	int h;
	if (widget->label())
		widget->measure_label(w, h);
	return w;
}

int
width_of_widest_child(fltk::Group const * g)
{
	int w = 0;
	for (int i = g->children(); --i >= 0; )
		w = std::max(w, label_width(g->child(i)));
	return w;
}

GLWidget * GLWidget::_drawing = 0;

bool
GLWidget::already_drawing(std::string const & who, bool will_draw)
{
	if (_drawing)
	{
		std::clog << "GLWidget::" << who << ": already drawing " << ((_drawing == this) ? "myself" : "other") << std::endl;
		return true;
	}
	if (will_draw)
		_drawing = this;
	return false;
}

void
GLWidget::pick(int x, int y)
{
	if (already_drawing("pick"))
		return;

	make_current();

	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	glSelectBuffer(sizeof _selected / sizeof _selected[0], _selected);
	glRenderMode(GL_SELECT);
	
	glInitNames();

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	gluPickMatrix(x, viewport[3] - y, 1, 1, viewport);
	projection();
	if (display_list.compiled())
		display_list.call();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	_hits = glRenderMode(GL_RENDER);
}

bool
GLWidget::unproject(int * xp, int * yp)
{
	make_current();
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	GLdouble mvmatrix[16];
	glGetDoublev(GL_MODELVIEW_MATRIX, mvmatrix);
	GLdouble projmatrix[16];
	glGetDoublev(GL_PROJECTION_MATRIX, projmatrix);
	GLint const real_y = viewport[3] - GLint(*yp) - 1;
	GLdouble wx, wy, wz;
	if (gluUnProject(*xp, real_y, 0, mvmatrix, projmatrix, viewport, &wx, &wy, &wz) != GL_TRUE)
		return false;
	*xp = int(wx + 0.5);
	*yp = int(wy + 0.5);
	return true;
}

#include <cmath>
int
GLWidget::handle(int event)
{
	int const nop = -1;
	int const done = -2;
	switch(event)
	{
	case fltk::PUSH:
		//    ... mouse down event ...
		_operation = nop;
		_old_x = fltk::event_x();
		_old_y = fltk::event_y();
		_dragged = false;
		pick(_old_x, _old_y);
		return 1;

	case fltk::DRAG:
		//   ... mouse moved while down event ...
		if (_operation == nop)
		{
			double const x = fltk::event_x() - _old_x;
			double const y = fltk::event_y() - _old_y;
			if (x * x + y * y > 20 * 20)
			{
				double angle = atan2(x, y) / M_PI_2;
				_operation = int(angle + 2.5);
				if (_operation == 4)
					_operation = 0;
//				std::cout << "GLWidget::handle angle " << angle <<  " _operation " << _operation << std::endl;
				assert(_operation >= 0 && _operation < 4);
			}
		}
		if (_operation != done && (fltk::event_x() != _old_x || fltk::event_y() != _old_y))
		{
			_dragged = true;
			if (_operation != nop)
			{
				if (dragged(_operation, _hits, _selected, _old_x, _old_y, fltk::event_x(), fltk::event_y()))
					_operation = done;
				_old_x = fltk::event_x();
				_old_y = fltk::event_y();
			}
		}
		return 1;

	case fltk::RELEASE: // mouse up event
		if (!_dragged)
			picked(_hits, _selected, fltk::event_x(),  fltk::event_y());
    		return 1;
		
	default:
		// let the base class handle all other events:
		return GlWindow::handle(event);
	}
}

ShowWaitCursor::ShowWaitCursor(fltk::Widget * widget, std::string const &)
{
	if (false || !widget)
		_what = 0;
	else
	{
//		std::clog << "ShowWaitCursor: " << s << std::endl;
		while (fltk::Widget * window = widget->window())
			widget = window;
		_what = widget;
		_what->cursor(fltk::CURSOR_WAIT);
		fltk::flush();
	}
}

ShowWaitCursor::~ShowWaitCursor()
{
	if (_what)
		_what->cursor(fltk::CURSOR_DEFAULT);
}
