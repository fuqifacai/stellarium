/*
 * Stellarium
 * Copyright (C) 2015 Marcos Cardinot
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

#include "LandscapeMgr.hpp"
#include "Meteor.hpp"
#include "SolarSystem.hpp"
#include "SporadicMeteorMgr.hpp"
#include "StelApp.hpp"
#include "StelCore.hpp"
#include "StelFileMgr.hpp"
#include "StelModuleMgr.hpp"
#include "StelPainter.hpp"
#include "StelTextureMgr.hpp"

#include <QSettings>

SporadicMeteorMgr::SporadicMeteorMgr(int zhr, int maxv)
	: ZHR(zhr)
	, maxVelocity(maxv)
	, flagShow(true)
{
	setObjectName("SporadicMeteorMgr");
	qsrand(QDateTime::currentMSecsSinceEpoch());
}

SporadicMeteorMgr::~SporadicMeteorMgr()
{
	qDeleteAll(activeMeteors);
	activeMeteors.clear();
	m_bolideTexture.clear();
}

void SporadicMeteorMgr::init()
{
	m_bolideTexture = StelApp::getInstance().getTextureManager().createTextureThread(
				StelFileMgr::getInstallationDir() + "/textures/cometComa.png",
				StelTexture::StelTextureParams(true, GL_LINEAR, GL_CLAMP_TO_EDGE));

	setZHR(StelApp::getInstance().getSettings()->value("astro/meteor_rate", 10).toInt());
}

/*************************************************************************
 Reimplementation of the getCallOrder method
*************************************************************************/
double SporadicMeteorMgr::getCallOrder(StelModuleActionName actionName) const
{
	if (actionName==StelModule::ActionDraw)
	{
		return GETSTELMODULE(SolarSystem)->getCallOrder(actionName) + 10.;
	}
	return 0;
}

void SporadicMeteorMgr::setZHR(int zhr)
{
	ZHR = zhr;
	emit zhrChanged(zhr);
}

int SporadicMeteorMgr::getZHR()
{
	return ZHR;
}

void SporadicMeteorMgr::setMaxVelocity(int maxv)
{
	maxVelocity = maxv;
}

void SporadicMeteorMgr::update(double deltaTime)
{
	if (!flagShow)
	{
		return;
	}

	StelCore* core = StelApp::getInstance().getCore();

	double tspeed = core->getTimeRate() * 86400;  // sky seconds per actual second
	if (!tspeed)
	{ // is paused?
		return; // freeze meteors at the current position
	}

	deltaTime*=1000;
	// if stellarium has been suspended, don't create
	// huge number of meteors to make up for lost time!
	if (deltaTime > 500)
	{
		deltaTime = 500;
	}

	// step through and update all active meteors
	foreach (SporadicMeteor* m, activeMeteors)
	{
		if (!m->update(deltaTime))
		{
			activeMeteors.removeOne(m);
		}
	}

	// only makes sense given lifetimes of meteors to draw when timeSpeed is realtime
	// otherwise high overhead of large numbers of meteors
	if (tspeed<0 || fabs(tspeed)>1.)
	{
		// don't start any more meteors
		return;
	}

	// determine average meteors per frame needing to be created
	int mpf = (int) ((double) ZHR * ZHR_TO_WSR * deltaTime / 1000.0 + 0.5);
	if (mpf < 1)
	{
		mpf = 1;
	}

	for (int i = 0; i < mpf; ++i)
	{
		// start new meteor based on ZHR time probability
		double prob = ((double) qrand()) / RAND_MAX;
		if (ZHR > 0 && prob < ((double) ZHR * ZHR_TO_WSR * deltaTime / 1000.0 / (double) mpf))
		{
			SporadicMeteor* m = new SporadicMeteor(core, maxVelocity, m_bolideTexture);
			activeMeteors.append(m);
		}
	}
}

void SporadicMeteorMgr::draw(StelCore* core)
{
	if (!flagShow || !core->getSkyDrawer()->getFlagHasAtmosphere())
	{
		return;
	}

	LandscapeMgr* landmgr = GETSTELMODULE(LandscapeMgr);
	if (landmgr->getFlagAtmosphere() && landmgr->getLuminance()>5)
	{
		return;
	}

	// step through and draw all active meteors
	StelPainter sPainter(core->getProjection(StelCore::FrameAltAz));
	foreach (SporadicMeteor* m, activeMeteors)
	{
		m->draw(core, sPainter);
	}
}
