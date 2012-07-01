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
#include <string.h>
#include <glib/gstdio.h>

#include "applet-struct.h"
#include "applet-load-icons.h"
#include "applet-read-data.h"
#include "applet-notifications.h"


CD_APPLET_ON_CLICK_BEGIN
	if (pClickedIcon == myIcon)  // en mode dock, on peut recevoir le clic sur l'icone principale si elle n'a pas de sous-dock, autrement dit si la connexion s'est mal passe, ce qui nous permet d'afficher le message d'erreur.
	{
		cd_weather_show_current_conditions_dialog (myApplet);
	}
	else if (pClickedIcon != NULL)  // clic sur une des sous-icones.
	{
		cd_weather_show_forecast_dialog (myApplet, pClickedIcon);
	}
CD_APPLET_ON_CLICK_END

static int _get_num_day_from_icon (CairoDockModuleInstance *myApplet, Icon *pIcon)
{
	/// TODO: determiner le jour exact...
	return (pIcon == myIcon ? 0 : pIcon->fOrder/2);  // la 1ere icone est le plus souvent celle d'aujourd'hui, toutefois cela peut ne pas etre vrai, surtout la nuit autour du changement de jour.
}
static inline void _go_to_site (CairoDockModuleInstance *myApplet, int iNumDay)
{
	gchar *cURI;
	if (iNumDay == 0)
		cURI = g_strdup_printf ("http://www.weather.com/weather/today/%s", myConfig.cLocationCode);
	else
		cURI = g_strdup_printf ("http://www.weather.com/weather/wxdetail/%s?dayNum=%d", myConfig.cLocationCode, iNumDay);
	cairo_dock_fm_launch_uri (cURI);
	g_free (cURI);
}

static inline void _reload (CairoDockModuleInstance *myApplet)
{
	if (cairo_dock_task_is_running (myData.pTask))
	{
		cairo_dock_show_temporary_dialog_with_icon (D_("Data are being retrieved, please wait a moment."), 
			myIcon,
			myContainer,
			3000,
			"same icon");
	}
	else
	{
		cairo_dock_stop_task (myData.pTask);  // not blocking since the task is not running.
		
		myData.bBusy = TRUE;
		cairo_dock_request_icon_animation (myIcon, myContainer, "busy", 999);
		cairo_dock_mark_icon_as_clicked (myIcon);  // prevent hovering the icon to overwrite the animation with another one.
		cairo_dock_launch_task (myData.pTask);
	}
}


static void _cd_weather_reload (GtkMenuItem *menu_item, CairoDockModuleInstance *myApplet)
{
	CD_APPLET_ENTER;
	_reload (myApplet);
	CD_APPLET_LEAVE ();
}
static void _cd_weather_show_site (GtkMenuItem *menu_item, CairoDockModuleInstance *myApplet)
{
	CD_APPLET_ENTER;
	_go_to_site (myApplet, myData.iClickedDay);
	CD_APPLET_LEAVE ();
}
static void _cd_weather_show_cc (GtkMenuItem *menu_item, CairoDockModuleInstance *myApplet)
{
	CD_APPLET_ENTER;
	cd_weather_show_current_conditions_dialog (myApplet);
	CD_APPLET_LEAVE ();
}
CD_APPLET_ON_BUILD_MENU_BEGIN
	if (pClickedIcon == myIcon)
	{
		CD_APPLET_ADD_IN_MENU_WITH_STOCK (D_("Show current conditions (middle-click)"), GTK_STOCK_DIALOG_INFO, _cd_weather_show_cc, CD_APPLET_MY_MENU);
	}
	if (pClickedIcon != NULL)
	{
		myData.iClickedDay = _get_num_day_from_icon (myApplet, pClickedIcon);
		CD_APPLET_ADD_IN_MENU_WITH_STOCK (D_("Open weather.com (double-click)"), GTK_STOCK_JUMP_TO, _cd_weather_show_site, CD_APPLET_MY_MENU);
	}
	CD_APPLET_ADD_IN_MENU_WITH_STOCK (D_("Reload now"), GTK_STOCK_REFRESH, _cd_weather_reload, CD_APPLET_MY_MENU);
CD_APPLET_ON_BUILD_MENU_END


CD_APPLET_ON_MIDDLE_CLICK_BEGIN
	if (pClickedIcon == myIcon)
	{
		cd_weather_show_current_conditions_dialog (myApplet);
	}
CD_APPLET_ON_MIDDLE_CLICK_END


CD_APPLET_ON_DOUBLE_CLICK_BEGIN
	if (pClickedIcon != NULL)
	{
		cairo_dock_remove_dialog_if_any (pClickedIcon);
		int iNumDay = _get_num_day_from_icon (myApplet, pClickedIcon);
		_go_to_site (myApplet, iNumDay);
	}
CD_APPLET_ON_DOUBLE_CLICK_END


void cd_weather_show_forecast_dialog (CairoDockModuleInstance *myApplet, Icon *pIcon)
{
	// remove any other forecast dialog.
	if (myDock != NULL)
		g_list_foreach (myIcon->pSubDock->icons, (GFunc) cairo_dock_remove_dialog_if_any_full, GINT_TO_POINTER (TRUE));
	else
		cairo_dock_remove_dialog_if_any (myIcon);
	
	// if we never got any result, show an error message. If we lost the connection, but could get some data beforehand, we'll just present the old data, since they are not likely to change very often.
	if (myData.wdata.cLocation == NULL)
	{
		cairo_dock_show_temporary_dialog_with_icon (D_("No data available\n is your connection alive?"), 
			(myDock ? pIcon : myIcon),
			(myDock ? CAIRO_CONTAINER (myIcon->pSubDock) : myContainer),
			myConfig.cDialogDuration,
			"same icon");
		return ;
	}
	
	// present the day's forecast.
	int iNumDay = ((int) pIcon->fOrder) / 2, iPart = ((int) pIcon->fOrder) - 2 * iNumDay;
	g_return_if_fail (iNumDay < myConfig.iNbDays && iPart < 2);
	
	Day *day = &myData.wdata.days[iNumDay];
	DayPart *part = &day->part[iPart];
	cairo_dock_show_temporary_dialog_with_icon_printf ("%s (%s) : %s\n %s : %s%s -> %s%s\n %s : %s%%\n %s : %s%s (%s)\n %s : %s%%\n %s : %s  %s %s",
		(myDock ? pIcon : myIcon),
		(myDock ? CAIRO_CONTAINER (myIcon->pSubDock) : myContainer),
		myConfig.cDialogDuration,
		"same icon",
		day->cName, day->cDate, part->cWeatherDescription,
		D_("Temperature"), _display (day->cTempMin), myData.wdata.units.cTemp, _display (day->cTempMax), myData.wdata.units.cTemp,
		D_("Precipitation probability"), _display (part->cPrecipitationProba),
		D_("Wind"), _display (part->cWindSpeed), myData.wdata.units.cSpeed, _display (part->cWindDirection),
		D_("Humidity"), _display (part->cHumidity),
		D_("Sunrise"), _display (day->cSunRise), _("Sunset"), _display (day->cSunSet));
}

void cd_weather_show_current_conditions_dialog (CairoDockModuleInstance *myApplet)
{
	cairo_dock_remove_dialog_if_any (myIcon);
	
	// if an error occured, the current conditions are no more valid.
	if (cairo_dock_task_is_running (myData.pTask))  // current conditions are outdated.
	{
		cairo_dock_show_temporary_dialog_with_icon (D_("Data are being fetched, please re-try in a few seconds."),
			myIcon,
			myContainer,
			3000,
			"same icon");
		return;
	}
	
	if (myData.bErrorRetrievingData)
	{
		cairo_dock_show_temporary_dialog_with_icon (D_("No data available\nRetrying now..."),
			myIcon,
			myContainer,
			3000,
			myIcon->cFileName);
		_reload (myApplet);
		return ;
	}
	
	// show a dialog with the current conditions.
	CurrentContitions *cc = &myData.wdata.currentConditions;
	cairo_dock_show_temporary_dialog_with_icon_printf ("%s (%s, %s)\n %s : %s%s (%s : %s%s)\n %s : %s%s (%s)\n %s : %s - %s : %s%s\n %s : %s  %s %s",
		myIcon, myContainer, myConfig.cDialogDuration, myIcon->cFileName,
		cc->cWeatherDescription, cc->cDataAcquisitionDate, cc->cObservatory,
		D_("Temperature"), _display (cc->cTemp), myData.wdata.units.cTemp, D_("Feels like"), _display (cc->cFeltTemp), myData.wdata.units.cTemp,
		D_("Wind"), _display (cc->cWindSpeed), myData.wdata.units.cSpeed, _display (cc->cWindDirection),
		D_("Humidity"), _display (cc->cHumidity), D_("Pressure"), _display (cc->cPressure), myData.wdata.units.cPressure,  // unite ?...
		D_("Sunrise"), _display (cc->cSunRise), D_("Sunset"), _display (cc->cSunSet));
}
