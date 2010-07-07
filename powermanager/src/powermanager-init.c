/**
* This file is a part of the Cairo-Dock project
*
* Copyright : (C) see the 'copyright' file.
* E-mail    : see the 'copyright' file.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 3
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <cairo-dock.h>

#include "powermanager-draw.h"
#include "powermanager-config.h"
#include "powermanager-dbus.h"
#include "powermanager-menu-functions.h"
#include "powermanager-struct.h"
#include "powermanager-init.h"


CD_APPLET_DEFINITION ("PowerManager",
	2, 0, 6,
	CAIRO_DOCK_CATEGORY_ACCESSORY,
	N_("A power manager for laptop batteries. Works with ACPI and DBus."),
	"Necropotame (Adrien Pilleboue)")

static void _set_data_renderer (CairoDockModuleInstance *myApplet, gboolean bReload)
{
	CairoDataRendererAttribute *pRenderAttr = NULL;  // les attributs du data-renderer global.
	if (myConfig.iDisplayType == CD_POWERMANAGER_GAUGE)
	{
		CairoGaugeAttribute attr;  // les attributs de la jauge.
		memset (&attr, 0, sizeof (CairoGaugeAttribute));
		pRenderAttr = CAIRO_DATA_RENDERER_ATTRIBUTE (&attr);
		pRenderAttr->cModelName = "gauge";
		attr.cThemePath = myConfig.cGThemePath;
	}
	else if (myConfig.iDisplayType == CD_POWERMANAGER_GRAPH)
	{
		CairoGraphAttribute attr;  // les attributs du graphe.
		memset (&attr, 0, sizeof (CairoGraphAttribute));
		pRenderAttr = CAIRO_DATA_RENDERER_ATTRIBUTE (&attr);
		pRenderAttr->cModelName = "graph";
		pRenderAttr->iMemorySize = (myIcon->fWidth > 1 ? myIcon->fWidth : 32);  // fWidht peut etre <= 1 en mode desklet au chargement.
		attr.iType = myConfig.iGraphType;
		attr.iRadius = 10;
		attr.fHighColor = myConfig.fHigholor;
		attr.fLowColor = myConfig.fLowColor;
		memcpy (attr.fBackGroundColor, myConfig.fBgColor, 4*sizeof (double));
	}
	else if (myConfig.iDisplayType == CD_POWERMANAGER_ICONS)
	{
		
	}
	if (pRenderAttr != NULL)
	{
		//pRenderAttr->bWriteValues = TRUE;
		if (! bReload)
			CD_APPLET_ADD_DATA_RENDERER_ON_MY_ICON (pRenderAttr);
		else
			CD_APPLET_RELOAD_MY_DATA_RENDERER (pRenderAttr);
	}
}


CD_APPLET_INIT_BEGIN
	
	if (myDesklet)
	{
		CD_APPLET_SET_DESKLET_RENDERER ("Simple");
		CD_APPLET_ALLOW_NO_CLICKABLE_DESKLET;
	}
	
	// on ne charge pas toutes les surfaces, ce qui economise de la memoire, et du temps au chargement, car ce n'est pas necessaire. En effet, on ne redessine que si il y'a changement. Or la batterie se vide lentement, et la recharge n'est pas non plus fulgurante, donc au total on redesine reellement l'icone 1 fois toutes les 5 minutes peut-etre, ce qui ne justifie pas de pre-charger les surfaces.
	
	if (cd_powermanager_find_battery())
	{
		myData.dbus_enable = dbus_connect_to_bus ();  // pour le changement d'etat.
		///get_on_battery();
		
		// Initialisation du rendu.
		_set_data_renderer (myApplet, FALSE);
		
		myData.pEmblem = CD_APPLET_MAKE_EMBLEM (MY_APPLET_SHARE_DATA_DIR"/charge.svg");
		cairo_dock_set_emblem_position (myData.pEmblem, CAIRO_DOCK_EMBLEM_MIDDLE);
		
		if (myConfig.iDisplayType == CD_POWERMANAGER_GAUGE || myConfig.iDisplayType == CD_POWERMANAGER_GRAPH)
		{
			double x=0;
			CD_APPLET_RENDER_NEW_DATA_ON_MY_ICON (&x);
		}
		
		// initialisation des parametres pour le 1er redessin.
		myData.prev_battery_present = TRUE;
		myData.previous_battery_charge = -1;
		myData.previous_battery_time = -1;
		myData.alerted = TRUE;
		myData.bCritical = TRUE;
		update_stats();
		myData.checkLoop = g_timeout_add_seconds (myConfig.iCheckInterval, (GSourceFunc) update_stats, (gpointer) NULL);
	}
	else
	{
		CD_APPLET_SET_LOCAL_IMAGE_ON_MY_ICON ("sector.svg");
	}
	
	CD_APPLET_REGISTER_FOR_CLICK_EVENT;
	CD_APPLET_REGISTER_FOR_BUILD_MENU_EVENT;
CD_APPLET_INIT_END


CD_APPLET_STOP_BEGIN
	CD_APPLET_UNREGISTER_FOR_CLICK_EVENT;
	CD_APPLET_UNREGISTER_FOR_BUILD_MENU_EVENT;
	
	dbus_disconnect_from_bus ();
	
	if(myData.checkLoop != 0)
	{
		g_source_remove (myData.checkLoop);
		myData.checkLoop = 0;
	}
	
CD_APPLET_STOP_END


CD_APPLET_RELOAD_BEGIN
	cairo_dock_free_emblem (myData.pEmblem);
	myData.pEmblem = CD_APPLET_MAKE_EMBLEM (MY_APPLET_SHARE_DATA_DIR"/charge.svg");
	cairo_dock_set_emblem_position (myData.pEmblem, CAIRO_DOCK_EMBLEM_MIDDLE);
	
	if (CD_APPLET_MY_CONFIG_CHANGED)
	{
		if (myDesklet && CD_APPLET_MY_CONTAINER_TYPE_CHANGED)  // we are now in a desklet, set a renderer.
		{
			CD_APPLET_SET_DESKLET_RENDERER ("Simple");
			CD_APPLET_ALLOW_NO_CLICKABLE_DESKLET;
		}
		
		_set_data_renderer (myApplet, TRUE);
		
		if(myData.checkLoop != 0)  // la frequence peut avoir change.
			g_source_remove (myData.checkLoop);
		myData.checkLoop = g_timeout_add_seconds (myConfig.iCheckInterval, (GSourceFunc) update_stats, (gpointer) NULL);
	}
	else
	{
		CD_APPLET_RELOAD_MY_DATA_RENDERER (NULL);
		if (myConfig.iDisplayType == CD_POWERMANAGER_GRAPH)
			CD_APPLET_SET_MY_DATA_RENDERER_HISTORY_TO_MAX;
	}
	
	//\_______________ On redessine notre icone.
	if (myData.cBatteryStateFilePath)
	{
		if (myConfig.iDisplayType == CD_POWERMANAGER_GAUGE || myConfig.iDisplayType == CD_POWERMANAGER_GRAPH)  // On recharge la jauge.
		{
			double fPercent = (double) myData.battery_charge / 100.;
			CD_APPLET_RENDER_NEW_DATA_ON_MY_ICON (&fPercent);
			
			//Embleme sur notre icône
			//CD_APPLET_DRAW_EMBLEM ((myData.on_battery ? CAIRO_DOCK_EMBLEM_BLANK : CAIRO_DOCK_EMBLEM_CHARGE), CAIRO_DOCK_EMBLEM_MIDDLE);
			if (! myData.on_battery)
				CD_APPLET_DRAW_EMBLEM_ON_MY_ICON (myData.pEmblem);
		}
		else if (myConfig.iDisplayType == CD_POWERMANAGER_ICONS)
			cd_powermanager_draw_icon_with_effect (myData.on_battery);
		
		if (!myData.on_battery && myData.battery_charge < 100)
			myData.alerted = FALSE; //We will alert when battery charge reach 100%
		if (myData.on_battery)
		{
			if (myData.battery_charge > myConfig.lowBatteryValue)
				myData.alerted = FALSE; //We will alert when battery charge is under myConfig.lowBatteryValue
			
			if (myData.battery_charge > 4)
				myData.bCritical = FALSE; //We will alert when battery charge is critical (under 4%)
		}
		
		myData.previous_battery_charge = -1;
		myData.previous_battery_time = -1;
		update_icon();

	}
	else  // sinon on signale par l'icone appropriee que le bus n'est pas accessible.
		CD_APPLET_SET_LOCAL_IMAGE_ON_MY_ICON ("sector.svg");
	
CD_APPLET_RELOAD_END
