#include <time.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <list>
#include <map>
#include <string>
#include <math.h>
#include <limits.h>
#include <ctype.h>
#ifdef __linux__
#include <getopt.h>
#else
#include "getopt.h"
#endif
#ifdef __linux__
#include "tiffio.h"
#endif
#include <stdarg.h>
#include <string.h>
#include "pthread.h"

using namespace std;

#include "polygon.h"
#include "apertures.h"
#include "gerber.h"

unsigned char * DEGUB_bitmap_ptr_end;

unsigned char nbitsTable[256];


const char *help_message= 
"Gerber RS-274X file to raster graphics converter \n"
"\n"
"Usage: gerb2prn [OPTIONS] [file1] [file2]...\n"
"\n"
"Output control: \n"
"  -a, --area           Show total dark area of TIFF in square centimeters.\n"
"  -q, --quiet          Suppress warnings and non critical messages.\n"
"  -t                   Test only. Process Gerber file without writing TIFF.\n"
"  -o, --output=FILE    Set name of output TIFF to FILE. If gerber-file is\n"
"                       specified then default is <file1>.tiff\n"
"                       This option is required when no gerber-file specified.\n"
"  -v                   Verbose mode, display information while processing\n"
"                       multiple -v increases verbosity. Disables --quiet\n"
"  --help               This help screen\n"
"\n"
"Image options: \n"
"  --boarder-pixels=X   Add a boarder of X pixels around image. Default 0\n"
"  -b, --boarder-mm=X   same as --boarder except X is in millimeters\n"
"  -p, --dpi=X          Number of dots per inch X. Default 2400\n"
"  -n, --negative       Negate image polarity\n"
"  --grow-pixels=X      Expand perimeter of all aperture features by X pixels.\n"
"                       Negative values shrink. Fractional pixels allowed.\n"
"  --grow-mm=X          Same as --grow-pixels except X is in unit millimeters.\n"
"  --strip-rois=N       Specify N rows per strip in TIFF. Default 512\n"
"  --scale-y=FACTOR     Scale image in Y axis by FACTOR. Default 1\n"
"  --scale-x=FACTOR     Scale image in X axis by FACTOR. Default 1\n"
"\n"
"Where file1 file2... are gerber files rendered as overlays to a single bitmap.\n"
"Standard input is read if no gerber files specified and --output is specified.\n"
"Output bitmap is compressed monochrome TIFF.\n"
"\n";


void show_interval(const char * msg="")
{
	static clock_t  start_clock = clock();
    double cpu_time_used = ((double) (clock() - start_clock)) / CLOCKS_PER_SEC;
	printf("time: %.3f s (%s)\n",cpu_time_used,  msg);
	start_clock = clock();
}

void error(const string &message)
{
	cerr << "gerb2tiff: error: "<<message<<endl;
    exit(1);
}


//**********************************************************
// Optimised horizontal line drawing from x1,y to x2,y in the monochrome bitmap
// polarity specifies how pixels are changed.
// DRAW_ON = line is drawn bits set
// DRAW_OFF = line is drawn bits cleared
// DRAW_REVERSE  = line is drawn bits inverted
//
// global dependencies:	bytesPerScanline, bitmap
//**********************************************************
void horizontalLine( int x1, int x2, unsigned char *buffer, Polarity_t polarity)
{
	if (x1 > x2)
		swap(x1, x2);

	static char fillSingle[64] = {
			0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0xC0, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0xE0, 0x60, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00,
			0xF0, 0x70, 0x30, 0x10, 0x00, 0x00, 0x00, 0x00,
			0xF8, 0x78, 0x38, 0x18, 0x08, 0x00, 0x00, 0x00,
			0xFC, 0x7C, 0x3C, 0x1C, 0x0C, 0x04, 0x00, 0x00,
			0xFE, 0x7E, 0x3E, 0x1E, 0x0E, 0x06, 0x02, 0x00,
			0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01 };

	static char fillLast[8]  = {0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE, 0xFF};
	static char fillFirst[8] = {0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01};


	const char b1 = (x1 & 7);
	const char b2 = (x2 & 7);

	unsigned char *px1 = buffer + (x1 >> 3);
	unsigned char *px2 = buffer + (x2 >> 3);

    // left pixel = MSB
    // right pixel = LSB
	switch (polarity)
	{
	case DARK:	// plot line with set bits
		// fill in the pixels at the byte x1, and x2 occupy.
		if (px1 == px2)
		{ // x1 and x2 occupy the same  byte
			*px1 |= fillSingle[ b1 + (b2<<3) ];
		}
		else
		{ // x1 and x2 occupy different bytes
			*px1 |= fillFirst[ b1 ];
			*px2 |= fillLast[ b2 ];
			// fill only the whole bytes in buffer between x1 and x2
			px1++;
			memset(px1, 0xFF, (px2 - px1));
		}
		break;

	case CLEAR:		// plot line with cleared bits

		if (px1 == px2)	// fill in the pixels at the byte x1, and x2 occupy.
		{ // x1 and x2 occupy the same  byte
			*px1 &= ~fillSingle[ b1 + (b2<<3) ];
		}
		else
		{ // x1 and x2 occupy different bytes
			*px1 &= ~fillFirst[ b1 ];
			*px2 &= ~fillLast[ b2 ];
			// fill only the whole bytes in buffer between x1 and x2
			px1++;
			memset(px1, 0x0, (px2 - px1));
		}
		break;

	case XOR: // invert the pixels
		// fill in the pixels at the byte x1, and x2 occupy.
		if (px1 == px2)
		{ // x1 and x2 occupy the same  byte
			*px1 ^= fillSingle[ b1 + (b2<<3) ];
		}
		else
		{ // x1 and x2 occupy different bytes
			*px1 ^= fillFirst[ b1 ];
			*px2 ^= fillLast[ b2 ];
			// XOR only the whole bytes in buffer between x1 and x2 (exclusive)
			px1++;
			while (px1 < px2)
			{
				*px1 ^= 0xFF;
				px1++;
			}
		}
		break;
	}

} // end HorizontalLine()

list<Polygon> globalPolygons;	// Contains polygons created by the all gerbers.
pthread_mutex_t g_mutex;
FILE * fp = NULL;
#ifdef __linux__
	TIFF* tif = NULL;
#endif

struct thread_params {
	bool isPolarityDark;
	int ystart;
	int rowPerStrip;
	int bytesPerScanline;
	int xOffset;
	int miny;			// holds min and max dimentions of the occupied gerber images (superimposed)
	int minx;
	int maxy;
	int maxx;
	int index;
	int imageHeight;
	int stripCounter;
};

void * handlePoly(void* Param)
{
	thread_params * param = (thread_params *)Param;

	int ystart = param->ystart;
	int rowsPerStrip = param->rowPerStrip;
	int bytesPerScanline = param->bytesPerScanline;
	bool isPolarityDark = param->isPolarityDark;
	int xOffset = param->xOffset;
	int miny = param->miny;
	int minx = param->maxy;
	int maxy = param->maxy;
	int maxx = param->maxx;
	int index = param->index;
	int imageHeight = param->imageHeight;
	int stripCounter = param->stripCounter;

	/*std::list<Polygon> poly;
	pthread_mutex_lock(&g_mutex);
	std::copy(globalPolygons.begin(), globalPolygons.end(), std::back_inserter(poly));
	pthread_mutex_unlock(&g_mutex);*/

	list<PolygonReference >  activePolys;
	int bitmapBytes = bytesPerScanline * rowsPerStrip;
	unsigned char * bitmap = (unsigned char *)malloc(bitmapBytes);
	if (isPolarityDark)	memset(bitmap, 0x00, bitmapBytes);
	else				memset(bitmap, 0xff, bitmapBytes);
	unsigned char * bufferLine = bitmap;

	/*pthread_mutex_lock(&g_mutex);
	printf("globalPolygons size:%d poly size:%d\n", globalPolygons.size(), poly.size());
	pthread_mutex_unlock(&g_mutex);*/

	//jump to ystart position
	/*Polygon temp;
	temp.pixelMinY = ystart;
	list<Polygon>::iterator polyIterator = std::find(globalPolygons.begin(), globalPolygons.end(), temp);*/
	list<Polygon>::iterator polyIterator = globalPolygons.begin();
	while (polyIterator != globalPolygons.end()) {
		if ((ystart <= (polyIterator->pixelMinY)))
			break;
		polyIterator++;
	}

	printf("ystart:%d rowsPerStrip:%d bytesPerScanline:%d isPolarityDark:%d xOffset:%d index:%d miny:%d minx:%d maxy:%d maxx:%d imageHeight:%d stripCounter:%d\n",
		ystart, rowsPerStrip, bytesPerScanline, isPolarityDark, xOffset, index, miny, minx, maxy, maxx, imageHeight, stripCounter);

	// Loop over each row of the strip and fill with horizontal lines from the polygon raster data.
	// All polygon are sorted in the list globalPolygons. Iterating each polygon for raster data will guarantee no missing lines.
	for (int y = ystart; (y - ystart) < rowsPerStrip && (y <= maxy); y++, bufferLine += bytesPerScanline)
	{
		while (polyIterator != globalPolygons.end())
		{
			if (y == (polyIterator->pixelMinY)) {
				activePolys.push_back(PolygonReference());
				activePolys.back().polygon = &(*polyIterator);
				activePolys.sort();
				printf("added poly %d (y=%d)\n", activePolys.back().polygon->number, y);
				polyIterator++;
			}
			else
				break;
		}
		
		printf(" activePolys size:%d\n", activePolys.size());
		for (list<PolygonReference>::iterator it = activePolys.begin(); it != activePolys.end();)
		{
			if (y > it->polygon->pixelMaxY)
			{
			//						printf("erased poly %d (y=%d)\n", activePolys.back().polygon->number, y);
				it = activePolys.erase(it);
				continue;
			}
			int sliCount;
			int *sliTable;
			it->polygon->getNextLineX1X2Pairs(sliTable, sliCount);

			//				printf("p %2d y:%d (x cnt %d) |",it->polygon->number, y, sliCount); fflush(stdout);

			Polarity_t pol = it->polygon->polarity;
			if ((pol == DARK) && !isPolarityDark) pol = CLEAR;
			if ((pol == CLEAR) && isPolarityDark) pol = DARK;

			for (int i = 0; i < sliCount; i += 2)
			{
				//					printf(" %d~%d ",sliTable[i], sliTable[i+1] ); fflush(stdout);
				horizontalLine(xOffset + it->polygon->pixelOffsetX + sliTable[i], \
					xOffset + it->polygon->pixelOffsetX + sliTable[i + 1], \
					bufferLine, pol);

			}
					//		printf("\n");
			it++;
		}
	}

	unsigned lines = min(rowsPerStrip, imageHeight - rowsPerStrip * stripCounter);
	//printf("%d preentry\n", index);
	pthread_mutex_lock(&g_mutex);
	printf("%d entry bytesPerScanline:%d lines:%d\n", index, bytesPerScanline, lines);
	fseek(fp, bitmapBytes*index + 48, 0);
	fwrite(bitmap, 1, bytesPerScanline*lines, fp);
	fflush(fp);
#ifdef __linux__
	if (index > 0)
		memset(bitmap, 0x00, bytesPerScanline*lines);
	TIFFWriteEncodedStrip(tif, stripCounter, bitmap, bytesPerScanline*lines);
#endif
	pthread_mutex_unlock(&g_mutex);

	printf("%d leave\n", index);

	if (bitmap) free(bitmap);

	//printf("==time (sec):	%.2f\n", ((double)(clock() - temp_clock)) / CLOCKS_PER_SEC);
	
	return 0;
}

//---------------------------------------------------------------------------------
int main (int argc, char **argv)
{
	//**************************************************
	double imageDPI = 600;
	double optRotation;
	bool   optGrowUnitsMillimeters = false;
	bool   optBoarderUnitsMillimeters = false;
	double optBoarder = 0;
	bool  optInvertPolarity = false;
	bool  optTestOnly = false;
	int  optVerbose = 0;
	unsigned rowsPerStrip = 128;
	bool  optShowArea = false;
	bool  optQuiet = false;
	double total_area_cmsq = 0;
	double optGrowSize = 0;
	double optScaleX = 1;
	double optScaleY = 1;
	unsigned int bytesPerScanline;
	unsigned int bitmapBytes;
	bool debug = false;
	//***********************************************************

	time_t now;
	time(&now);
	printf("current time is:%lld\n", now);

	string inputfile;
	string outputFilename;

	// create 256 element look up table for fast retrieve of
	// number of bits set in numbers 0 to 255.
	// used for calculating positive pixels in the drawn bitmap

	for (int i=0; i < 256; i++)
	{
		nbitsTable[i]=0;
		if ((i&0x01)) nbitsTable[i]++;
		if ((i&0x02)) nbitsTable[i]++;
		if ((i&0x04)) nbitsTable[i]++;
		if ((i&0x08)) nbitsTable[i]++;
		if ((i&0x10)) nbitsTable[i]++;
		if ((i&0x20)) nbitsTable[i]++;
		if ((i&0x40)) nbitsTable[i]++;
		if ((i&0x80)) nbitsTable[i]++;
	}

	//
    // parse the command line
    //
	while (1)
	{
        static struct option long_options[] =
        {
			// These options don't set a flag.
			// We distinguish them by their indices.
			{"debug",   no_argument,	   0, 'd'},
			{"dpi",     required_argument, 0, 'p'},
			{"output",  required_argument, 0, 'o'},
			{"negative",no_argument, 	   0, 'n'},
			{"area",    no_argument, 	   0, 'a'},
			{"test",    no_argument, 	   0,  't'},
			{"quiet",   no_argument, 	   0,  'q'},
			{"verbose", no_argument,   	   0,  'v'},
			{"scale-x", required_argument, 0, 1},
			{"scale-y", required_argument, 0, 2},
			{"help",    no_argument, 	   0,  3},
			{"grow-pixels",	required_argument, 0, 4},
			{"grow-mm",	required_argument, 0, 5},
			{"strip-rows",	required_argument, 0, 6},
			{"boarder-mm", required_argument, 0, 'b'},
			{"boarder-pixels", required_argument, 0, 7},
			{"rotation", required_argument, 0, 8},
			{0, 0, 0, 0}
        };
        // getopt_long stores the option index here.
        int option_index = 0;

        int c = getopt_long (argc, argv, "G:b:o:p:atvnq",
							long_options, &option_index);

		if (c == EOF)		break;

		switch (c)
		{

		case 8:
			optRotation = atof(optarg);
		  break;
		case 7:
			optBoarder = atof(optarg);
			optBoarderUnitsMillimeters = false;
		  break;
		case 6:
			rowsPerStrip = atoi(optarg);
		  break;
		case 5:
		  optGrowSize = atof(optarg);
		  optGrowUnitsMillimeters = true;
		  break;
		case 4:
		  optGrowSize = atof(optarg);
		  optGrowUnitsMillimeters = false;
		  break;
		case 3:
			fprintf( stdout,"%s", help_message);
			exit(0);
		case 2:
		  optScaleY = atof(optarg);
		  break;
		case 1:
		  optScaleX = atof(optarg);
		  break;
		case 'p':
		  imageDPI = atof(optarg);
		  break;
		case 'b':
		  optBoarder = atof(optarg);
  		  optBoarderUnitsMillimeters = true;
		  break;
		case 'n':
		  optInvertPolarity = true;
		  break;
		case 't':
		  optTestOnly = true;
		  break;
		case 'a':
			optShowArea = true;
		  break;
		case 'v':
		  optVerbose++;
		  break;
		case 'q':
		  optQuiet = true;
		  break;
		case 'o':
			outputFilename = optarg;
		  break;
		case 'd':
			debug = true;
			break;
		case '?':
		case ':':
		  fprintf (stderr, "Try 'gerb2tiff --help' for more information.\n");
		  return 1;
		  break;
		}
	}

	if (optVerbose > 0 ) optQuiet = false;			// if user wants verbose, then cancel quiet option

	if (imageDPI < 1)		error(string("DPI setting must be >= 1"));
	if (optBoarder < 0)		error(string("boarder setting must be >= 0"));

	// correct the units for some options
	if ( optGrowUnitsMillimeters )
		optGrowSize *= imageDPI/25.4;
	if ( optBoarderUnitsMillimeters )
		optBoarder *= imageDPI/25.4;

    list<Gerber *> gerbers;			// pointer to the list of Gerber object

    bool isStandardInput = false;
	if (optind == argc)
		isStandardInput = true;


	int first_optind = optind;
	
	for(; optind < argc || isStandardInput; optind++)
	{
		FILE *file = 0;
		if ( isStandardInput )
		{
			file = stdin;
			if (!optTestOnly && outputFilename.empty())
			{
				cerr << "no output or input file specified.\n"
					    "Try 'gerb2tiff --help' for more information.\n";
				return 1;
			}
		}
		else
		{
			inputfile = argv[optind];
			if ( outputFilename.empty())
					outputFilename = inputfile + ".tiff";
#ifndef __linux__
			fopen_s(&file, argv[optind], "rb");
#else
			file = fopen( argv[optind], "rb");
#endif
			if (file == NULL)
					error( string("cannot open input file ")+inputfile );
			if (!optQuiet)
			{
				if (optind == first_optind)			cout << "gerb2tiff: ";
				if (!optQuiet && optind > first_optind)	cout << "+ ";
				cout << inputfile << " "<<flush;
			}
		}

		gerbers.push_back( new Gerber(file, imageDPI, optGrowSize, optScaleX, optScaleY) );

		if (! isStandardInput)
			fclose(file);

		// print all warning messages
		for (int i=0; i < gerbers.back()->messages.size() && !optQuiet; i++)
		{
			if (i==0) std::cout <<"\n";
			std::cout <<"("<<inputfile<<") "<<gerbers.back()->messages[i]<<endl;	    		// print messages if any
		}
		// print error messages and abort
		if (gerbers.back()->isError)
		{
			std::cout <<"\n("<<inputfile<<") "<<gerbers.back()->errorMessage.str() << endl;
			return 1;		// exit program
		}
		if ( isStandardInput )
			break;

	}

	if (!optTestOnly  && !optQuiet)		cout << "-> "<<outputFilename;
	if (!optQuiet)						cout << endl;

	int miny =  INT_MAX;			// holds min and max dimentions of the occupied gerber images (superimposed)
	int minx =  INT_MAX;
	int maxy =  INT_MIN;
	int maxx =  INT_MIN;

	// group all the polygons
    for (list<Gerber*>::iterator it = gerbers.begin(); it != gerbers.end();  it++)
    {
       globalPolygons.merge((*it)->polygons );
	}

	printf("globalPolygons size:%d, gerbers size:%d\n", globalPolygons.size(), gerbers.size());
//    for (int i=0; i < 100; i++)
//    {
//    	double radius = 50.0 * rand() / double(RAND_MAX);
//    	double x0  = 2000.0 * rand() / double(RAND_MAX);
//    	double y0  = 2000.0 * rand() / double(RAND_MAX);
//        globalPolygons.push_back(Polygon() );
//        globalPolygons.back().number = 1;
//        globalPolygons.back().vdata->add( 0,0);
//        globalPolygons.back().vdata->add(100,0);
//        globalPolygons.back().vdata->add(100,50);
//        globalPolygons.back().vdata->add( 0,50);
//        globalPolygons.back().vdata->rotate(222 *M_PI/180);
//        globalPolygons.back().vdata->initialise();
//        globalPolygons.back().initialise();
//    }

//    globalPolygons.sort();
//        globalPolygons.push_back(Polygon() );
//        globalPolygons.back().addArc(0, 2*M_PI, 10, 0, 0, false );
//        globalPolygons.back().number = 0;
//        globalPolygons.back().initialise();



	if (globalPolygons.size()==0 )// If nothing to draw then abort with error
		error("no image");


	// find extreme (x,y) coordinates for all polygons
	for (list<Polygon>::iterator it = globalPolygons.begin(); it != globalPolygons.end();  it++)
	{
		if (minx > it->pixelMinX) 	minx = it->pixelMinX;
		if (maxx < it->pixelMaxX) 	maxx = it->pixelMaxX;
		if (miny > it->pixelMinY) 	miny = it->pixelMinY;
		if (maxy < it->pixelMaxY)	maxy = it->pixelMaxY;
	}


	// use the world coordinate limits <maxx, minx, maxx, minx> to determine the
	// sized  of the bitmap buffer to allocate for drawing the image
	// always make image imageWidth multiple of 32. 4 byte align
    unsigned imageWidth 	= unsigned(ceil ( (maxx - minx) + 2*optBoarder + 1 ));
	imageWidth = (imageWidth + 31) / 32 * 32;
    unsigned imageHeight	= unsigned(ceil ( (maxy - miny) + 2*optBoarder + 1 ));
    int xOffset		= int(floor( optBoarder ));
    int yOffset		= xOffset;

    bool isPolarityDark = true;
    isPolarityDark = (optInvertPolarity ^ gerbers.front()->imagePolarityDark);	// polarity is relative to 1st gerber file
	//isPolarityDark = false;
    if ( rowsPerStrip > unsigned(imageHeight) || rowsPerStrip == 0)
    	rowsPerStrip = imageHeight;
	unsigned darkPixelsCount = 0;

    if (optVerbose >= 2)
    {
    	printf("polygon count:               %d\n",globalPolygons.size());
    	printf ("grow option:                 %.1f pixels , %.3f mm\n", optGrowSize, optGrowSize/imageDPI*25.4);
    }
    if (optVerbose >= 1)
    {
		printf ("Image data\n"
				"  origin (mm):               %.3f x %.3f\n"
				"  size (mm):                 %.3f x %.3f\n"
				"  size (pixels):             %u x %d\n"
				"  uncompressed size (MB):    %.1f\n"
				"  dots per inch:             %u\n"
				"  TIFF rows per strip        %u\n"
				,(-xOffset+minx)/imageDPI*25.4, (-yOffset+miny)/imageDPI*25.4
				,imageWidth/imageDPI*25.4, imageHeight/imageDPI*25.4
				,imageWidth, imageHeight
				,float( (((imageWidth+7) / 8) * imageHeight) / 0x100000)
				,int(imageDPI)
				,rowsPerStrip);

	}
    fflush(stdout);

	if (optTestOnly)	// stop here on testing or if no Polygons to draw
	{
		if (optVerbose) {
			time_t now;
			time(&now);
			printf("current time is:%lld\n", now);
		}
		return 0;
	}

	// Initialise TIFF with the libtiff library
	//
#ifdef __linux__
	tif = TIFFOpen(outputFilename.c_str(), "w");
    if	(tif==NULL)
    {
    	cout << "error creating output file '" << outputFilename << "\n";;
    	return 1;
    }

    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);		// avoid errors, dispite TIFF spec saying this tag not needed in monochrome images.
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISWHITE);		// white pixels are zero
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_CCITTRLE);		// use CCITT Group 3 1-Dimensional Modified Huffman run length encoding
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, imageHeight);
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, imageWidth);
    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, 2);					// Resulution unit in inches
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, imageDPI);
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, imageDPI);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, rowsPerStrip);
#endif
    
	//
    // Calculate size and allocate buffer for drawing. The image will be rendered sequential blocks of
    // imageWidth wide by rowsPerStrip high.
    //
	bytesPerScanline = (imageWidth >> 3);

	string prnName = inputfile;
	prnName  += ".prn";

#ifndef __linux__
	fopen_s(&fp, prnName.c_str(), "wb");
#else
	fp = fopen(prnName.c_str(), "wb");
#endif
	printf("imageHeight2:%d imageWidth:%d imageDPI:%f rowsPerStrip:%d\n", imageHeight, imageWidth, imageDPI, rowsPerStrip);
	char ripHeader[48];
	int width = bytesPerScanline;
	int xdpi = int(imageDPI);
	int ydpi = int(imageDPI);
	int colors = 1;
	int colorbit = 1;
	memset(ripHeader, 0x00, 48);
	memset(ripHeader, 0x55, 2);
	memcpy(ripHeader + 4, &xdpi, 4);
	memcpy(ripHeader + 8, &ydpi, 4);
	memcpy(ripHeader + 12, &width, 4);
	memcpy(ripHeader + 16, &imageHeight, 4);
	memcpy(ripHeader + 20, &imageWidth, 4);
	memcpy(ripHeader + 28, &colors, 4);
	memcpy(ripHeader + 32, &colorbit, 4);
	fwrite(ripHeader, 1, 48, fp);


    //-----------------------------------------------------------------------
    // Draw polygons
    //-----------------------------------------------------------------------
    xOffset -= minx;

	int stripCounter = 0;

	// The bitmap will be divided into strips, of height rowsPerStrip.
	// Polygons are plotted for each strip consecutively in a loop, where the strip y coordinate equals ystart
	int count = 0;
	if ((int(imageHeight)) % rowsPerStrip == 0)
		count = (int(imageHeight)) / rowsPerStrip;
	else
		count = (int(imageHeight)) / rowsPerStrip + 1;
	pthread_t * pid = new pthread_t[count];
	pthread_attr_t * attr = new pthread_attr_t[count];
	thread_params * params = new thread_params[count];
	pthread_mutex_init(&g_mutex, NULL);

    for (int ystart = miny - yOffset, i = 0; ystart < ( int(imageHeight) + miny - yOffset); ystart += rowsPerStrip, i++)
    {
		pthread_attr_init(&(attr[i]));
		pthread_attr_setscope(&(attr[i]), PTHREAD_SCOPE_SYSTEM);
		pthread_attr_setdetachstate(&(attr[i]), PTHREAD_CREATE_JOINABLE);
		params[i].ystart = ystart;
		params[i].rowPerStrip = rowsPerStrip;
		params[i].bytesPerScanline = bytesPerScanline;
		params[i].isPolarityDark = isPolarityDark;
		params[i].xOffset = xOffset;
		params[i].maxx = maxx;
		params[i].maxy = maxy;
		params[i].minx = minx;
		params[i].miny = miny;
		params[i].index = i;
		params[i].imageHeight = imageHeight;
		params[i].stripCounter = stripCounter;
		pthread_create(&(pid[i]), &(attr[i]), handlePoly, &(params[i]));
		pthread_join(pid[i], NULL);

        unsigned lines = min (rowsPerStrip, imageHeight - rowsPerStrip * stripCounter);
        int percentComplete = (100*(stripCounter*rowsPerStrip + lines))/imageHeight;
	    if (optVerbose)
		{
	    	static int last = percentComplete;
	    	/*if (percentComplete != last )
	    		cout << "Rendering "<< percentComplete <<"%  \n"<<flush;*/
	    	last = percentComplete;
		}

		stripCounter++;
    }

	//joinable thread.
	for(int i = 0; i < count; i++)
		pthread_join(pid[i], NULL);

	delete[] pid;
	delete[] attr;
	delete[] params;

#ifdef __linux__
    TIFFClose(tif);
#endif
    fclose(fp);

	/*delete pid;
	delete attr;
	delete params;*/
	pthread_mutex_destroy(&g_mutex);

    if (optVerbose)    	cout << "\n";

    if (optShowArea)
    {
    	printf("  dark  area (sq.cm):        %0.1f\n",darkPixelsCount*2.54*2.54/(imageDPI*imageDPI));
    	printf("  clear area (sq.cm):        %0.1f\n",((imageHeight*imageWidth) - darkPixelsCount*2.54*2.54)/(imageDPI*imageDPI));
    }

	if (optVerbose){
		time_t now;
		time(&now);
		printf("current time is:%lld\n", now);
	}

#ifndef __linux__
	system("pause");
#endif
}



