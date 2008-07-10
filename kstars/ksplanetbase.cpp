/***************************************************************************
                          ksplanetbase.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Jul 22 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ksplanetbase.h"

#include <math.h>

#include <QFile>
#include <QPoint>
#include <QMatrix>

#include "kstarsdata.h"
#include "ksutils.h"
#include "ksnumbers.h"
#include "Options.h"
#include "skymap.h"
#include "ksasteroid.h"
#include "kspluto.h"
#include "ksplanet.h"
#include "kssun.h"
#include "ksmoon.h"

KSPlanetBase::KSPlanetBase( KStarsData *kd, const QString &s, const QString &image_file, const QColor &c, double pSize )
    : TrailObject( 2, 0.0, 0.0, 0.0, s ), Rearth(0.0), Image(), data(kd) {
    init( s, image_file, c, pSize );
}

void KSPlanetBase::init( const QString &s, const QString &image_file, const QColor &c, double pSize ) {
    if (! image_file.isEmpty()) {
        QFile imFile;

        if ( KSUtils::openDataFile( imFile, image_file ) ) {
            imFile.close();
            Image0.load( imFile.fileName() );
            Image = Image0.convertToFormat( QImage::Format_ARGB32 );
            Image0 = Image;
        }
    }
    PositionAngle = 0.0;
    ImageAngle = 0.0;
    PhysicalSize = pSize;
    m_Color = c;
    setName( s );
}

KSPlanetBase* KSPlanetBase::createPlanet( int n ) {
    KStarsData *kd = KStarsData::Instance();

    switch ( n ) {
        case MERCURY:
        case VENUS:
        case MARS:
        case JUPITER:
        case SATURN:
        case URANUS:
        case NEPTUNE:
            return new KSPlanet( kd, n );
            break;

        case PLUTO:
            return new KSPluto(kd);
            break;
        case SUN:
            return new KSSun(kd);
            break;
        case MOON:
            return new KSMoon(kd);
            break;
    }

    return 0;
}

void KSPlanetBase::EquatorialToEcliptic( const dms *Obliquity ) {
    findEcliptic( Obliquity, ep.longitude, ep.latitude );
}

void KSPlanetBase::EclipticToEquatorial( const dms *Obliquity ) {
    setFromEcliptic( Obliquity, &ep.longitude, &ep.latitude );
}

void KSPlanetBase::updateCoords( KSNumbers *num, bool includePlanets, const dms *lat, const dms *LST ){
    if ( includePlanets ) {
        data->skyComposite()->earth()->findPosition( num ); //since we don't pass lat & LST, localizeCoords will be skipped

        if ( lat && LST ) {
            findPosition( num, lat, LST, data->skyComposite()->earth() );
            //Don't add to the trail this time
            if ( hasTrail() ) Trail.takeLast();
        } else {
            findGeocentricPosition( num, data->skyComposite()->earth() );
        }
    }
}

void KSPlanetBase::findPosition( const KSNumbers *num, const dms *lat, const dms *LST, const KSPlanetBase *Earth ) {
    findGeocentricPosition( num, Earth );  //private function, reimplemented in each subclass
    setAngularSize( asin(physicalSize()/Rearth/AU_KM)*60.*180./dms::PI ); //angular size in arcmin

    if ( lat && LST )
        localizeCoords( num, lat, LST ); //correct for figure-of-the-Earth

    if ( hasTrail() ) {
        addToTrail();
        if ( Trail.size() > MAXTRAIL ) Trail.takeFirst();
    }

    if ( isMajorPlanet() || type() == SkyObject::ASTEROID )
        findMagnitude(num);
}

bool KSPlanetBase::isMajorPlanet() const {
    if ( name() == "Mercury" || name() == "Venus" || name() == "Mars" ||
            name() == "Jupiter" || name() == "Saturn" || name() == "Uranus" ||
            name() == "Neptune" )
        return true;
    return false;
}

void KSPlanetBase::localizeCoords( const KSNumbers *num, const dms *lat, const dms *LST ) {
    //convert geocentric coordinates to local apparent coordinates (topocentric coordinates)
    dms HA, HA2; //Hour Angle, before and after correction
    double rsinp, rcosp, u, sinHA, cosHA, sinDec, cosDec, D;
    double cosHA2;
    double r = Rearth * AU_KM; //distance from Earth, in km
    u = atan( 0.996647*tan( lat->radians() ) );
    rsinp = 0.996647*sin( u );
    rcosp = cos( u );
    HA.setD( LST->Degrees() - ra()->Degrees() );
    HA.SinCos( sinHA, cosHA );
    dec()->SinCos( sinDec, cosDec );

    D = atan2( rcosp*sinHA, r*cosDec/6378.14 - rcosp*cosHA );
    dms temp;
    temp.setRadians( ra()->radians() - D );
    setRA( temp );

    HA2.setD( LST->Degrees() - ra()->Degrees() );
    cosHA2 = cos( HA2.radians() );

    //temp.setRadians( atan2( cosHA2*( r*sinDec/6378.14 - rsinp ), r*cosDec*cosHA/6378.14 - rcosp ) );
    // The atan2() version above makes the planets move crazy in the htm branch -jbb
    temp.setRadians( atan( cosHA2*( r*sinDec/6378.14 - rsinp )/( r*cosDec*cosHA/6378.14 - rcosp ) ) );

    setDec( temp );

    //Make sure Dec is between -90 and +90
    if ( dec()->Degrees() > 90.0 ) {
        setDec( 180.0 - dec()->Degrees() );
        setRA( ra()->Hours() + 12.0 );
        ra()->reduce();
    }
    if ( dec()->Degrees() < -90.0 ) {
        setDec( 180.0 + dec()->Degrees() );
        setRA( ra()->Hours() + 12.0 );
        ra()->reduce();
    }

    EquatorialToEcliptic( num->obliquity() );
}

void KSPlanetBase::setRearth( const KSPlanetBase *Earth ) {
    double sinL, sinB, sinL0, sinB0;
    double cosL, cosB, cosL0, cosB0;
    double x,y,z;

    //The Moon's Rearth is set in its findGeocentricPosition()...
    if ( name() == "Moon" ) {
        return;
    }

    if ( name() == "Earth" ) {
        Rearth = 0.0;
        return;
    }

    if ( ! Earth  ) {
        kDebug() << i18n( "KSPlanetBase::setRearth():  Error: Need an Earth pointer.  (" ) << name() << ")";
        Rearth = 1.0;
        return;
    }

    Earth->ecLong()->SinCos( sinL0, cosL0 );
    Earth->ecLat()->SinCos( sinB0, cosB0 );
    double eX = Earth->rsun()*cosB0*cosL0;
    double eY = Earth->rsun()*cosB0*sinL0;
    double eZ = Earth->rsun()*sinB0;

    helEcLong()->SinCos( sinL, cosL );
    helEcLat()->SinCos( sinB, cosB );
    x = rsun()*cosB*cosL - eX;
    y = rsun()*cosB*sinL - eY;
    z = rsun()*sinB - eZ;

    Rearth = sqrt(x*x + y*y + z*z);

    //Set angular size, in arcmin
    AngularSize = asin(PhysicalSize/Rearth/AU_KM)*60.*180./dms::PI;
}

void KSPlanetBase::findPA( const KSNumbers *num ) {
    //Determine position angle of planet (assuming that it is aligned with
    //the Ecliptic, which is only roughly correct).
    //Displace a point along +Ecliptic Latitude by 1 degree
    SkyPoint test;
    dms newELat( ecLat()->Degrees() + 1.0 );
    test.setFromEcliptic( num->obliquity(), ecLong(), &newELat );
    double dx = ra()->Degrees() - test.ra()->Degrees(); 
    double dy = test.dec()->Degrees() - dec()->Degrees();
    double pa;
    if ( dy ) {
        pa = atan2( dx, dy )*180.0/dms::PI;
    } else {
		    pa = 90.0;
		    if ( dx > 0 ) pa = -90.0;
    }
    setPA( pa );

}

double KSPlanetBase::labelOffset() const {
    double scale = SkyMap::Instance()->scale();
    double size = angSize() * scale * dms::PI * Options::zoomFactor()/10800.0;

    //Determine minimum size for offset
    double minsize = 4.;
    if ( type() == SkyObject::ASTEROID || type() == SkyObject::COMET )
        minsize = 2.;
    if ( name() == "Sun" || name() == "Moon" )
        minsize = 8.;
    if ( size < minsize )
        size = minsize;

    //Inflate offset for Saturn
    if ( name() == "Saturn" )
        size = int(2.5*size);

    return 0.5*size + 4.;
}

void KSPlanetBase::rotateImage( double imAngle ) {
    ImageAngle = imAngle;
    QMatrix m;
    m.rotate( ImageAngle );
    Image = Image0.transformed( m );
}

void KSPlanetBase::scaleRotateImage( float size, double imAngle ) {
    ImageAngle = imAngle;
    QMatrix m;
    m.rotate( ImageAngle );
    Image = Image0.transformed( m ).scaledToWidth( int(size) );
}

void KSPlanetBase::findMagnitude(const KSNumbers *num) {
    double cosDec, sinDec;
    dec()->SinCos(cosDec, sinDec);

    /* Phase of the planet in degrees */
    double earthSun = data->skyComposite()->earth()->rsun();
    double cosPhase = (rsun()*rsun() + rearth()*rearth() - earthSun*earthSun)
                      / (2 * rsun() * rearth() );
    double phase_rad = acos ( cosPhase ); // Phase in radian - used for asteroid magnitudes
    double phase = phase_rad * 180.0 / dms::PI;

    /* Computation of the visual magnitude (V band) of the planet.
    * Algorithm provided by Pere Planesas (Observatorio Astronomico Nacional)
    * It has some simmilarity to J. Meeus algorithm in Astronomical Algorithms, Chapter 40.
    * */

    // Initialized to the faintest magnitude observable with the HST
    float magnitude = 30;

    double param = 5 * log10(rsun() * rearth() );
    double f1 = phase/100.;

    if ( name() == "Mercury" ) {
        if ( phase > 150. ) f1 = 1.5;
        magnitude = -0.36 + param + 3.8*f1 - 2.73*f1*f1 + 2*f1*f1*f1;
    }
    if ( name() =="Venus")
        magnitude = -4.29 + param + 0.09*f1 + 2.39*f1*f1 - 0.65*f1*f1*f1;
    if( name() == "Mars")
        magnitude = -1.52 + param + 0.016*phase;
    if( name() == "Jupiter")
        magnitude = -9.25 + param + 0.005*phase;

    if( name() == "Saturn") {
        double T = num->julianCenturies();
        double a0 = (40.66-4.695*T)* dms::PI / 180.;
        double d0 = (83.52+0.403*T)* dms::PI / 180.;
        double sinx = -cos(d0)*cosDec*cos(a0 - ra()->radians());
        sinx = fabs(sinx-sin(d0)*sinDec);
        double rings = -2.6*sinx + 1.25*sinx*sinx;
        magnitude = -8.88 + param + 0.044*phase + rings;
    }

    if( name() == "Uranus")
        magnitude = -7.19 + param + 0.0028*phase;
    if( name() == "Neptune")
        magnitude = -6.87 + param;
    if( name() == "Pluto" )
        magnitude = -1.01 + param + 0.041*phase;

    if( type() == SkyObject::ASTEROID ) {
        // Asteroid
        KSAsteroid *me = (KSAsteroid *)this;
        double phi1 = exp( -3.33 * pow(tan(phase_rad/2), 0.63) );
        double phi2 = exp( -1.87 * pow(tan(phase_rad/2), 1.22) );
        double H, G;
        H = me -> getAbsoluteMagnitude();
        G = me -> getSlopeParameter();
        magnitude = H + param - 2.5 * log10( (1 - G) * phi1 + G * phi2 );
    }
    
    setMag(magnitude);
}

