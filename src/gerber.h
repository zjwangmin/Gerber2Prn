/*
	copyright (c), 2001 Adam Seychell.


This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#ifndef GERBER_H_
#define GERBER_H_


#include <iostream>
#include <sstream>
#include <string>
#include <complex>
#include <vector>
#include <list>
#include <math.h>
#include <ctype.h>
#ifdef __linux__
#include <getopt.h>
#include <tiffio.h>
#endif
#include <string.h>

#include "polygon.h"
#include "apertures.h"
#ifndef __linux__
#include "corecrt_math_defines.h"
#endif
using namespace::std;

//#define DEBBUG

#ifdef DEBBUG
#define DEBUGPRINT(...) printf(__VA_ARGS__);
#else
#define DEBUGPRINT(...)
#endif




typedef struct T_Arc
{
	Point centre;
	Point stopped;
	double radius;
	double start;
	double end;
	bool isTooSmall;	// false if arc is too small to plot
} Arc;


extern char *yytext;
extern void yyrestart( FILE *new_file );


class Gerber {


	friend  int yylex(Gerber *gerb);
	friend  int yyparse(Gerber *gerb);
	private:
		enum APETURE_DRAWING_MODE {CIRCLE_CLOCKWISE, CIRCLE_ANTICLOCKWISE, LINEAR_10X, LINEAR_1X, LINEAR_01X, LINEAR_001X, CIRCULAR360, _INVALID_};
		typedef enum {MILLIMETER, INCH, UNDEFINED} Units_t ;

		ostringstream oss;
		const double optScaleY;
		const double optScaleX;
		const double growSize;
   		const double dotsPerInch;
		double coordPrevious[2];
		double scaleFactor[2];
		double imageOffsetPixels[2];		// offset of the image in pixels specified by %IO parameter
		bool layerPolarityClear;
		int coordsDecimals[2];
		int coordsInts[2];
		bool isAxisSwapped;
		bool isOmitLeadingZeroes;
        bool isCoordsAbsolute;
        Units_t units;
		bool isMirrorAaxis;				// negative all A axis coordinate data
		bool isMirrorBaxis;				// negative all B axis coordinate data
		int currentLine;
		enum APETURE_DRAWING_MODE 	drawingMode;
		bool isCircular360;
		bool isPolygonFill;
		bool isLampOn;
		std::list<Aperture>::iterator lastDrawnApertureSelect;
		double lastDrawnX;
		double lastDrawnY;
		double imageRotate;
		bool isDrawingEnabled;

		int warningCount;
		bool isWarnNoApertureSelect;

		std::list<Aperture>::iterator apertureSelect;

		std::vector< NodeT * > 	temporaryParameters;
		string 					temporaryNameMacro;

		std::vector<double>  variables;

		double X;
		double Y;
		double oldX;
		double oldY;
		double I;
		double J;
		double dotsPerUnit();
		const char * unitText()	{	switch (units) { case MILLIMETER: return "mm"; case INCH: return "\""; }	}
		double getCoordinate( char * const text, int axisNumber, bool alwaysAbsolute = false);
		void calculateArc(Arc &arc);
		void process_AD_block(int DCode);
		void process_D_command(int code);
		void process_G_command(int code);
		void processDataBlock();
		void flashAperture(double x, double y);
		void loadDefaults();
        std::list< Aperture > macro_apertures;
        std::list< Aperture > ad_apertures;

        struct StepRpeatBlock
        {
        	string buffer;
        	int X;
        	int Y;
        	double I;
        	double J;
        } repeat;


	public: //---------------------------
		void warning(const char * format, ...);
		bool imagePolarityDark;
		string layerName;
		string imageName;
		string imageFilm;
		std::vector< std::string > messages;
		std::ostringstream errorMessage;			// string of occurring error
		bool isError;

		list<Polygon> polygons;		// Contains a complete polygons list to build an image of this gerber file.
		list<VertexData *> vertexdata;	// Vertices information used by each new polygon requiring a new set of vertices.

		Gerber(FILE * fp_gerb, double ImageDPI, double GrowSize, double optScaleX, double optScaleY);
};


extern void yyerror( Gerber *g, const char * msg);











#endif // GERBER_H_
