/***************************************************************************
                          kswizard.h  -  description
                             -------------------
    begin                : Wed 28 Jan 2004
    copyright            : (C) 2004 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KSWIZARD_H_
#define KSWIZARD_H_

#include <Q3MemArray>

#include "ui_wizwelcome.h"
#include "ui_wizlocation.h"
#include "ui_wizdevices.h"
#include "ui_wizdownload.h"

class KDialog;
class GeoLocation;
class KStars;
class KPushButton;
class QStackedWidget;

class WizWelcomeUI : public QFrame, public Ui::WizWelcome {
	Q_OBJECT
	public:
		WizWelcomeUI( QWidget *parent=0 );
};

class WizLocationUI : public QFrame, public Ui::WizLocation {
	Q_OBJECT
	public:
		WizLocationUI( QWidget *parent=0 );
};

class WizDevicesUI : public QFrame, public Ui::WizDevices {
	Q_OBJECT
	public:
		WizDevicesUI( QWidget *parent=0 );
};

class WizDownloadUI : public QFrame, public Ui::WizDownload {
	Q_OBJECT
	public:
		WizDownloadUI( QWidget *parent=0 );
};

/**
	*@class KSWizard
	*The Setup Wizard will be automatically opened when KStars runs 
	*for the first time.  It allows the user to set up some basic parameters:
	*@li Geographic Location
	*@li Download extra data files
	*@author Jason Harris
	*@version 1.0
	*/
class KSWizard : public KDialog
{
Q_OBJECT
public:
	/**
		*Constructor
		*@p parent pointer to the parent widget
		*/
	KSWizard( QWidget *parent=0 );

	/**Destructor */
	~KSWizard();

	/**
		*@return pointer to the geographic location selected by the user
		*/
	GeoLocation* geo() const { return Geo; }

private slots:
	void slotNextPage();
	void slotPrevPage();

	/**
		*Set the geo pointer to the user's selected city, and display
		*its longitude and latitude in the window.
		*@note called when the highlighted city in the list box changes
		*/
	void slotChangeCity();

	/**
		*Display only those cities which meet the user's search criteria 
		*in the city list box.
		*@note called when one of the name filters is modified
		*/
	void slotFilterCities();

	void slotTelescopeSetup();

private:
	/**
		*@short Initialize the geographic location page.
		*Populate the city list box, and highlight the current location in the list.
		*/
	void initGeoPage();
	
	KPushButton* user1Button() { return actionButton( KDialog::User1 ); }
	KPushButton* user2Button() { return actionButton( KDialog::User2 ); }

	QStackedWidget *wizardStack;
	WizWelcomeUI *welcome;
	WizLocationUI *location;
	WizDevicesUI *devices;
	WizDownloadUI *download;

	KStars *ksw;
	Q3MemArray<int> GeoID;
	GeoLocation *Geo;
	QList<GeoLocation*> filteredCityList;
};

#endif
