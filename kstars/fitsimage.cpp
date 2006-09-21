/***************************************************************************
                          FITSImage.cpp  -  FITS Image
                             -------------------
    begin                : Thu Jan 22 2004
    copyright            : (C) 2004 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Some code fragments were adapted from Peter Kirchgessner's FITS plugin*
 *   See http://members.aol.com/pkirchg for more details.                  *
 ***************************************************************************/


#include <klocale.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kaction.h>
#include <kactioncollection.h>
#include <kdebug.h>
#include <ktoolbar.h> 
#include <kapplication.h>

#include <ktempfile.h>
#include <kimageeffect.h> 
#include <kmenubar.h>
#include <kprogressdialog.h>
#include <kstatusbar.h>

#include <QPaintEvent>
#include <QScrollArea>

#include <qfile.h>
#include <qcursor.h>


#include <math.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>

#include "fitsimage.h"
#include "fitsviewer.h"
#include "ksutils.h"

#define INITIAL_W	640
#define INITIAL_H	480
#define ZOOM_DEFAULT 100.0
#define ZOOM_MIN	10
#define ZOOM_MAX	400
#define ZOOM_LOW_INCR	10
#define ZOOM_HIGH_INCR	50

FITSLabel::FITSLabel(FITSImage *img, QWidget *parent) : QLabel(parent)
{
  image = img;
}

FITSLabel::~FITSLabel() {}

void FITSLabel::mouseMoveEvent(QMouseEvent *e)
{
  double x,y, width, height;
  float *buffer = image->getImageBuffer();
 
  image->getFITSSize(&width, &height);

  if (buffer == NULL) return;
  
  x = round(e->x() / (image->getCurrentZoom() / ZOOM_DEFAULT));
  y = round(e->y() / (image->getCurrentZoom() / ZOOM_DEFAULT));

  if (x < 1)
	x = 1;
  else if (x > width)
	x = width;

 if (y < 1)
	y = 1;
 else if (y > height)
	y = height;
  
  image->getViewer()->statusBar()->changeItem(QString("%1 , %2").arg( (int) x).arg( (int) y), 0);

  // Range is 0 to dim -1 when accessing array
  x-=1;
  y-=1;

  image->getViewer()->statusBar()->changeItem( KGlobal::locale()->formatNumber( buffer[(int) (y * width + x)], 3 ), 1 );
  setCursor(Qt::CrossCursor);

  e->accept();

}

FITSImage::FITSImage(QWidget * parent, const char * name) : QScrollArea(parent) , zoomFactor(1.2)
{
  viewer = (FITSViewer *) parent;
  
   image_frame = new FITSLabel(this);
   image_buffer = NULL;
   displayImage = NULL;
   setBackgroundRole(QPalette::Dark);

   currentZoom = 0.0;

   image_frame->setMouseTracking(true);

   // Default size
   resize(INITIAL_W, INITIAL_H);
}

FITSImage::~FITSImage()
{
  delete(image_buffer);
  delete(displayImage);
}
	
void FITSImage::reLoadTemplateImage()
{
  /*displayImage = templateImage->copy(); */
}

void FITSImage::saveTemplateImage()
{
  //templateImage = new QImage(displayImage->copy());
}

void FITSImage::destroyTemplateImage()
{
  //delete (templateImage);
}

void FITSImage::clearMem()
{

 #if 0
  free(reducedImgBuffer);
  delete (displayImage);
  reducedImgBuffer = NULL;
  displayImage     = NULL;

 #endif

}


int FITSImage::loadFits (const char *filename)
{
 
  int status=0, nulval=0, anynull=0;
  long fpixel[2], nelements, naxes[2];

  if (fits_open_image(&fptr, filename, READWRITE, &status))
  {
	fits_report_error(stderr, status);
	return -1;
  }

  if (fits_get_img_param(fptr, 2, &(stats.bitpix), &(stats.ndim), naxes, &status))
  {
	fits_report_error(stderr, status);
	return -1;
  }

  stats.dim[0] = naxes[0];
  stats.dim[1] = naxes[1];

  kDebug() << "bitpix: " << stats.bitpix << " dim[0]: " << stats.dim[0] << " dim[1]: " << stats.dim[1] << " ndim: " << stats.ndim << endl;

  delete (image_buffer);
  delete (displayImage);

  image_buffer = new float[stats.dim[0] * stats.dim[1]];
  
  displayImage = new QImage(stats.dim[0], stats.dim[1], QImage::Format_Indexed8);

  displayImage->setNumColors(256);
 
  for (int i=0; i < 256; i++)
     displayImage->setColor(i, qRgb(i,i,i));

 nelements = stats.dim[0] * stats.dim[1];
 //fpixel = new long[2];
 fpixel[0] = 1;
 fpixel[1] = 1;

 if (fits_read_pix(fptr, TFLOAT, fpixel, nelements, &nulval, image_buffer, &anynull, &status))
 {
	fits_report_error(stderr, status);
	return -1;
 }

 // Rescale to fits window
 if (rescale(ZOOM_FIT_WINDOW))
	return -1;

  calculateStats();

  setAlignment(Qt::AlignCenter);

  return 0;
 
}

int FITSImage::calculateMinMax(bool refresh)
{
           /* pointer to the FITS file, defined in fitsio.h */
    int status,  anynull, nfound=0;
    long fpixel, nbuffer, npixels, ii;
    float nullval, buffer[1000];

  status = 0;

  if (!refresh)
  {
  	if (fits_read_key_dbl(fptr, "DATAMIN", &(stats.min), NULL, &status))
	  	fits_report_error(stderr, status);
  	else
		nfound++;

  	if (fits_read_key_dbl(fptr, "DATAMAX", &(stats.max), NULL, &status))
	  	fits_report_error(stderr, status);
  	else
		nfound++;
  
  	// If we found both keywords, no need to calculate them
  	if (nfound == 2)
		return 0;
  }

    npixels  = stats.dim[0] * stats.dim[1];         /* number of pixels in the image */
    fpixel   = 1;
    nullval  = 0;                /* don't check for null values in the image */
    status   = 0;
    stats.min= 1.0E30;
    stats.max= -1.0E30;

    for (int i=0; i < npixels; i++)
    {
	    if (image_buffer[i] < stats.min) stats.min = image_buffer[i];
	    else if (image_buffer[i] > stats.max) stats.max = image_buffer[i];
    }
    
    kDebug() << "DATAMIN: " << stats.min << " - DATAMAX: " << stats.max << endl;
    return 0;
}

int FITSImage::rescale(zoomType type)
{
  float val=0;
  double bscale, bzero;
 
  // Get Min Max failed, scaling is not possible
  if (type == ZOOM_KEEP_LEVEL)
  {
	  if (calculateMinMax(true))
		  return -1;
  }
  else
  {
	  if (calculateMinMax())
		  return -1;
  }

  bscale = 255. / (stats.max - stats.min);
  bzero  = (-stats.min) * (255. / (stats.max - stats.min));

  image_frame->setScaledContents(true);
  currentWidth  = displayImage->width();
  currentHeight = displayImage->height();

  /* Fill in pixel values using indexed map, linear scale */
    for (int j = 0; j < stats.dim[1]; j++)
        for (int i = 0; i < stats.dim[0]; i++)
	{
		val = image_buffer[j * stats.dim[0] + i];
		displayImage->setPixel(i, j, ((int) (val * bscale + bzero)));
	}

 switch (type)
 {
	 case ZOOM_FIT_WINDOW:
		 if ((displayImage->width() > width() || displayImage->height() > height()))
 		 {
			// Find the zoom level which will enclose the current FITS in the default window size (640x480)
        		currentZoom = floor( (INITIAL_W / currentWidth) * 10.) * 10.;

			currentWidth  = stats.dim[0] * (currentZoom / ZOOM_DEFAULT);
			currentHeight = stats.dim[1] * (currentZoom / ZOOM_DEFAULT);

			if (currentZoom <= ZOOM_MIN)
  			viewer->actionCollection()->action("view_zoom_out")->setEnabled (false);

			image_frame->setPixmap(QPixmap::fromImage(displayImage->scaled((int) currentWidth, (int) currentHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
 		}
		else
		{
			currentZoom   = 100;
			image_frame->setPixmap(QPixmap::fromImage(*displayImage));
		}
		break;
	
	 case ZOOM_KEEP_LEVEL:
		 currentWidth  = stats.dim[0] * (currentZoom / ZOOM_DEFAULT);
		 currentHeight = stats.dim[1] * (currentZoom / ZOOM_DEFAULT);

		 image_frame->setPixmap(QPixmap::fromImage(displayImage->scaled((int) currentWidth, (int) currentHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
		 break;
	
	 default:
	 	currentZoom   = 100;
 		image_frame->setPixmap(QPixmap::fromImage(*displayImage));
		break;
 }
 
 setWidget(image_frame);

 if (type != ZOOM_KEEP_LEVEL)
 	viewer->statusBar()->changeItem(QString("%1%").arg(currentZoom), 3);

 return 0;

}

void FITSImage::zoomToCurrent()
{

 #if 0
 double cwidth, cheight;
 
 if (currentZoom >= 0)
 {
   cwidth  = ((double) displayImage->width()) * pow(zoomFactor, currentZoom) ;
   cheight = ((double) displayImage->height()) * pow(zoomFactor, currentZoom);
 }
 else
 { 
   cwidth  = ((double) displayImage->width()) / pow(zoomFactor, abs((int) currentZoom)) ;
   cheight = ((double) displayImage->height()) / pow(zoomFactor, abs((int) currentZoom));
 }
  
 if (cwidth != displayImage->width() || cheight != displayImage->height())
         image_frame->setPixmap(QPixmap::fromImage(displayImage->scaled( (int) cwidth, (int) cheight, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
 else
   image_frame->setPixmap(QPixmap::fromImage(*displayImage));

 #endif
 
 currentWidth  = stats.dim[0] * (currentZoom / ZOOM_DEFAULT); //pow(zoomFactor, abs(currentZoom)) ;
 currentHeight = stats.dim[1] * (currentZoom / ZOOM_DEFAULT); //zoomFactor; //pow(zoomFactor, abs(currentZoom));

 image_frame->setPixmap(QPixmap::fromImage(displayImage->scaled( (int) currentWidth, (int) currentHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
 image_frame->resize( (int) currentWidth, (int) currentHeight);
}


void FITSImage::fitsZoomIn()
{
 
  if (currentZoom < ZOOM_DEFAULT)
	currentZoom += ZOOM_LOW_INCR;
  else
	currentZoom += ZOOM_HIGH_INCR;

   kDebug() << "in fitsZoomIn " << currentZoom << endl;
   //currentZoom++;
   viewer->actionCollection()->action("view_zoom_out")->setEnabled (true);
   if (currentZoom >= ZOOM_MAX)
     viewer->actionCollection()->action("view_zoom_in")->setEnabled (false);
   
   //currentWidth  = zoomFactor; //pow(zoomFactor, abs(currentZoom)) ;
   currentWidth  = stats.dim[0] * (currentZoom / ZOOM_DEFAULT); //pow(zoomFactor, abs(currentZoom)) ;
   currentHeight = stats.dim[1] * (currentZoom / ZOOM_DEFAULT); //zoomFactor; //pow(zoomFactor, abs(currentZoom));

   image_frame->setPixmap(QPixmap::fromImage(displayImage->scaled( (int) currentWidth, (int) currentHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
   image_frame->resize( (int) currentWidth, (int) currentHeight);

   viewer->statusBar()->changeItem(QString("%1%").arg(currentZoom), 3);
 
}

void FITSImage::fitsZoomOut()
{
  //currentZoom--;
  if (currentZoom <= ZOOM_DEFAULT)
	currentZoom -= ZOOM_LOW_INCR;
  else
	currentZoom -= ZOOM_HIGH_INCR;

  if (currentZoom <= ZOOM_MIN)
  	viewer->actionCollection()->action("view_zoom_out")->setEnabled (false);
  
   viewer->actionCollection()->action("view_zoom_in")->setEnabled (true);
  
  //currentWidth  /= zoomFactor; //pow(zoomFactor, abs(currentZoom));
  //currentHeight /= zoomFactor;//pow(zoomFactor, abs(currentZoom));
  currentWidth  = stats.dim[0] * (currentZoom / ZOOM_DEFAULT); //pow(zoomFactor, abs(currentZoom)) ;
  currentHeight = stats.dim[1] * (currentZoom / ZOOM_DEFAULT); //zoomFactor; //pow(zoomFactor, abs(currentZoom));

   image_frame->setPixmap(QPixmap::fromImage(displayImage->scaled( (int) currentWidth, (int) currentHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation)));

   image_frame->resize( (int) currentWidth, (int) currentHeight);

   viewer->statusBar()->changeItem(QString("%1%").arg(currentZoom), 3);
}

void FITSImage::fitsZoomDefault()
{
  viewer->actionCollection()->action("view_zoom_out")->setEnabled (true);
  viewer->actionCollection()->action("view_zoom_in")->setEnabled (true);
  
  currentZoom   = ZOOM_DEFAULT;
  currentWidth  = stats.dim[0];
  currentHeight = stats.dim[1];
  
  image_frame->setPixmap(QPixmap::fromImage(*displayImage));
  image_frame->resize( (int) currentWidth, (int) currentHeight);
  
  viewer->statusBar()->changeItem(QString("%1%").arg(currentZoom), 3);

  update();

}

void FITSImage::calculateStats()
{
  
  // #1 call average, average is used in std deviation
  stats.average = average();
  // #2 call std deviation
  stats.stddev  = stddev();

}

double FITSImage::average()
{
 
  double sum=0;
  int row=0;
  int width   = stats.dim[0];
  int height  = stats.dim[1];
  
  if (!image_buffer) return -1;

  for (int i= 0 ; i < height; i++)
  {
    row = (i * width);
    for (int j= 0; j < width; j++)
       sum += image_buffer[row+j];
  }

    return (sum / (width * height ));
 
}

double FITSImage::stddev()
{
 
  int row=0;
  double lsum=0;
  int width   = stats.dim[0]; 
  int height  = stats.dim[1];
  
  if (!image_buffer) return -1;

  for (int i= 0 ; i < height; i++)
  {
    row = (i * width);
    for (int j= 0; j < width; j++)
    {
       lsum += (image_buffer[row + j] - stats.average) * (image_buffer[row+j] - stats.average);
    }
  }

  return (sqrt(lsum/(width * height - 1)));
 

}

void FITSImage::setFITSMinMax(double newMin,  double newMax)
{
	stats.min = newMin;
	stats.max = newMax;	
	
}



#include "fitsimage.moc"
