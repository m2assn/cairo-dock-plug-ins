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

#include "applet-struct.h"
#include "applet-config.h"
#include "applet-notifications.h"
#include "applet-load-icons.h"
#include "applet-read-data.h"
#include "applet-init.h"

CD_APPLET_DEFINE_BEGIN (N_("weather"),
	2, 0, 0,
	CAIRO_DOCK_CATEGORY_ACCESSORY,
	N_("This applet displays weather into your dock.\n"
	"Data are provided by www.weather.com, you can find your location in the config panel.\n"
	"It can detach itself to be a totally eye-candy 3D deskelt.\n"
	"Middle-click on the main icon to have current conditions information, left-click on a sub-icon to have forcast information.\n"),
	"Fabounet (Fabrice Rey)")
	CD_APPLET_DEFINE_COMMON_APPLET_INTERFACE
	pInterface->load_custom_widget = cd_weather_load_custom_widget;
CD_APPLET_DEFINE_END


CD_APPLET_INIT_BEGIN
	// On lance la mesure periodique.
	myData.pTask = cairo_dock_new_task (myConfig.iCheckInterval,
		(CairoDockGetDataAsyncFunc) cd_weather_get_distant_data,
		(CairoDockUpdateSyncFunc) cd_weather_update_from_data,
		myApplet);
	cairo_dock_launch_task (myData.pTask);
	
	// On s'abonne aux notifications.
	CD_APPLET_REGISTER_FOR_CLICK_EVENT;
	CD_APPLET_REGISTER_FOR_MIDDLE_CLICK_EVENT;
	CD_APPLET_REGISTER_FOR_BUILD_MENU_EVENT;
CD_APPLET_INIT_END


CD_APPLET_STOP_BEGIN
	//\_______________ On se desabonne de nos notifications.
	CD_APPLET_UNREGISTER_FOR_CLICK_EVENT;
	CD_APPLET_UNREGISTER_FOR_MIDDLE_CLICK_EVENT;
	CD_APPLET_UNREGISTER_FOR_BUILD_MENU_EVENT;
CD_APPLET_STOP_END


CD_APPLET_RELOAD_BEGIN
	//\_______________ On recharge les donnees qui ont pu changer.
	g_return_val_if_fail (myConfig.cLocationCode != NULL, FALSE);
	
	if (CD_APPLET_MY_CONFIG_CHANGED)
	{
		if (myConfig.bSetName)
		{
			CD_APPLET_DELETE_MY_ICONS_LIST;  // comme on va changer le nom, autant virer les icones du sous-dock des maintenant.
			g_free (myIcon->cName);
			myIcon->cName = NULL;
		}
		
		cd_weather_reset_all_datas (myApplet);  // on bourrine.
		
		myData.pTask = cairo_dock_new_task (myConfig.iCheckInterval,
			(CairoDockGetDataAsyncFunc) cd_weather_get_distant_data,
			(CairoDockUpdateSyncFunc) cd_weather_update_from_data,
			myApplet);
		cairo_dock_launch_task (myData.pTask);
	}
CD_APPLET_RELOAD_END

