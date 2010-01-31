/***************************************************************************
               constellationboundary.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2007-07-10
    copyright            : (C) 2007 James B. Bowlin
    email                : bowlin@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef CONSTELLATION_BOUNDARY_H
#define CONSTELLATION_BOUNDARY_H

#include "polylistindex.h"

#include <QHash>
#include <QPolygonF>

class PolyList;

typedef QVector<PolyList*> PolyListList;


/* @class ConstellationBoundaryPoly
 * Holds the outlines of every constellation boundary in PolygonF's.
 *
 * @author James B. Bowlin
 * @version 0.1
*/

class ConstellationBoundary : public PolyListIndex
{
public:
    explicit ConstellationBoundary( SkyComponent *parent );

    QString constellationName( SkyPoint *p );

    const QPolygonF* constellationPoly( SkyPoint *p );

    const QPolygonF* constellationPoly( const QString& name );

    bool inConstellation( const QString &name, SkyPoint *p );
};


#endif
