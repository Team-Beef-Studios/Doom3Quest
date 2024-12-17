/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/


#include "sys/platform.h"
#include "idlib/LangDict.h"
#include "framework/async/NetworkSystem.h"
#include "framework/DeclEntityDef.h"
#include "renderer/RenderSystem.h"
#include "renderer/ModelManager.h"

#include "gamesys/SysCvar.h"
#include "script/Script_Thread.h"
#include "ai/AI.h"
#include "WorldSpawn.h"
#include "Player.h"
#include "../idlib/geometry/JointTransform.h"
#include "Camera.h"
#include "Fx.h"
#include "Misc.h"
#include "Vr.h"
#include "idlib/Lib.h"
#include "Mover.h"
#include "sys/sys_public.h"
#include "framework/DeclSkin.h"


const int ASYNC_PLAYER_INV_AMMO_BITS = idMath::BitsForInteger( 999 );	// 9 bits to cover the range [0, 999]
const int ASYNC_PLAYER_INV_CLIP_BITS = -7;								// -7 bits to cover the range [-1, 60]

//#define	ANGLE2SHORT(x)			( idMath::Ftoi( (x) * 65536.0f / 360.0f ) & 65535 )

idCVar flashlight_batteryDrainTimeMS( "flashlight_batteryDrainTimeMS", "30000", CVAR_INTEGER, "amount of time (in MS) it takes for full battery to drain (-1 == no battery drain)" );
idCVar flashlight_batteryChargeTimeMS( "flashlight_batteryChargeTimeMS", "3000", CVAR_INTEGER, "amount of time (in MS) it takes to fully recharge battery" );
idCVar flashlight_minActivatePercent( "flashlight_minActivatePercent", ".25", CVAR_FLOAT, "( 0.0 - 1.0 ) minimum amount of battery (%) needed to turn on flashlight" );
idCVar flashlight_batteryFlickerPercent( "flashlight_batteryFlickerPercent", ".1", CVAR_FLOAT, "chance of flickering when battery is low" );

// Client-authoritative stuff
idCVar pm_clientAuthoritative_debug( "pm_clientAuthoritative_debug", "0", CVAR_BOOL, "" );
idCVar pm_controllerShake_damageMaxMag( "pm_controllerShake_damageMaxMag", "60.0f", CVAR_FLOAT, "" );
idCVar pm_controllerShake_damageMaxDur( "pm_controllerShake_damageMaxDur", "60.0f", CVAR_FLOAT, "" );

idCVar pm_clientAuthoritative_warnDist( "pm_clientAuthoritative_warnDist", "100.0f", CVAR_FLOAT, "" );
idCVar pm_clientAuthoritative_minDistZ( "pm_clientAuthoritative_minDistZ", "1.0f", CVAR_FLOAT, "" );
idCVar pm_clientAuthoritative_minDist( "pm_clientAuthoritative_minDist", "-1.0f", CVAR_FLOAT, "" );
idCVar pm_clientAuthoritative_Lerp( "pm_clientAuthoritative_Lerp", "0.9f", CVAR_FLOAT, "" );

idCVar pm_clientAuthoritative_Divergence( "pm_clientAuthoritative_Divergence", "200.0f", CVAR_FLOAT, "" );
idCVar pm_clientInterpolation_Divergence( "pm_clientInterpolation_Divergence", "5000.0f", CVAR_FLOAT, "" );

idCVar pm_clientAuthoritative_minSpeedSquared( "pm_clientAuthoritative_minSpeedSquared", "1000.0f", CVAR_FLOAT, "" );

idCVar vr_wipScale( "vr_wipScale", "1.0", CVAR_FLOAT | CVAR_ARCHIVE, "" );

idCVar vr_debugGui( "vr_debugGui", "1", CVAR_BOOL, "" );
idCVar vr_guiFocusPitchAdj( "vr_guiFocusPitchAdj", "7", CVAR_FLOAT | CVAR_ARCHIVE, "View pitch adjust to help activate in game touch screens" );

idCVar vr_bx1( "vr_bx1", "5", CVAR_FLOAT, "");
idCVar vr_bx2( "vr_bx2", "5", CVAR_FLOAT, "" );
idCVar vr_by1( "vr_by1", "1", CVAR_FLOAT, "" );
idCVar vr_by2( "vr_by2", "0", CVAR_FLOAT, "" );
idCVar vr_bz1( "vr_bz1", "0", CVAR_FLOAT, "" );
idCVar vr_bz2( "vr_bz2", "0", CVAR_FLOAT, "" );

idCVar vr_teleportVel( "vr_teleportVel", "650", CVAR_FLOAT,"" );
idCVar vr_teleportDist( "vr_teleportDist", "60", CVAR_FLOAT,"" );
idCVar vr_teleportMaxPoints( "vr_teleportMaxPoints", "24", CVAR_FLOAT, "" );
idCVar vr_teleportMaxDrop( "vr_teleportMaxDrop", "360", CVAR_FLOAT, "" );

idCVar vr_laserSightUseOffset( "vr_laserSightUseOffset", "1", CVAR_BOOL | CVAR_ARCHIVE, " 0 = lasersight emits straight from barrel.\n 1 = use offsets from weapon def" );

idCVar vr_weaponWheel( "vr_weaponWheel", "0", CVAR_BOOL, "Information if weapon wheel is shown right now" );
idCVar vr_weaponWheelCurrent( "vr_weaponWheelCurrent", "0", CVAR_INTEGER, "Current weapon in the weapon wheel" );
vec3_t vr_weaponWheelDir;

// for testing
idCVar ftx( "ftx", "0", CVAR_FLOAT, "" );
idCVar fty( "fty", "0", CVAR_FLOAT, "" );
idCVar ftz( "ftz", "0", CVAR_FLOAT, "" );

extern idCVar g_demoMode;

const idVec3 neckOffset( -3, 0, -6 );
const int waistZ = -22.f;

idCVar vr_slotDebug( "vr_slotDebug", "0", CVAR_BOOL, "slot debug visualation" );
idCVar vr_slotMag( "vr_slotMag", "0.1", CVAR_FLOAT | CVAR_ARCHIVE, "slot vibration magnitude (0 is off)" );
idCVar vr_slotDur( "vr_slotDur", "18", CVAR_INTEGER | CVAR_ARCHIVE, "slot vibration duration in milliseconds" );
idCVar vr_slotDisable( "vr_slotDisable", "0", CVAR_BOOL | CVAR_ARCHIVE, "slot disable" );

slot_t slots[ SLOT_COUNT ] = {
        { idVec3( 0, 10, -4 ), 9.0f * 9.0f },
        { idVec3( 0, -10, -4 ), 9.0f * 9.0f },
        { idVec3( -9, -4, 4 ), 9.0f * 9.0f },
        { idVec3( -9, -4,-waistZ - neckOffset.z ), 9.0f * 9.0f },
        { idVec3( 4, 8, -waistZ + 2 ), 9.0f * 9.0f },
        { idVec3( -neckOffset.x, 0, -waistZ - neckOffset.z + 7 ), 9.0f * 9.0f },
};

idAngles pdaAngle1( 0, -90, 0);
idAngles pdaAngle2( 0, 0, 76.5);
idAngles pdaAngle3( 0, 0, 0);

extern idCVar g_useWeaponDepthHack;

/*
===============================================================================

	Player control of the Doom Marine.
	This object handles all player movement and world interaction.

===============================================================================
*/

// distance between ladder rungs (actually is half that distance, but this sounds better)
const int LADDER_RUNG_DISTANCE = 32;

// amount of health per dose from the health station
const int HEALTH_PER_DOSE = 10;

// time before a weapon dropped to the floor disappears
const int WEAPON_DROP_TIME = 20 * 1000;

// time before a next or prev weapon switch happens
const int WEAPON_SWITCH_DELAY = 150;

// how many units to raise spectator above default view height so it's in the head of someone
const int SPECTATE_RAISE = 25;

const int HEALTHPULSE_TIME = 333;

// minimum speed to bob and play run/walk animations at
const float MIN_BOB_SPEED = 5.0f;

const idEventDef EV_Player_GetButtons( "getButtons", NULL, 'd' );
const idEventDef EV_Player_GetMove( "getMove", NULL, 'v' );
const idEventDef EV_Player_GetViewAngles( "getViewAngles", NULL, 'v' );
const idEventDef EV_Player_StopFxFov( "stopFxFov" );
const idEventDef EV_Player_EnableWeapon( "enableWeapon" );
const idEventDef EV_Player_DisableWeapon( "disableWeapon" );
const idEventDef EV_Player_GetCurrentWeapon( "getCurrentWeapon", NULL, 's' );
const idEventDef EV_Player_GetPreviousWeapon( "getPreviousWeapon", NULL, 's' );
const idEventDef EV_Player_SelectWeapon( "selectWeapon", "s" );
const idEventDef EV_Player_GetWeaponEntity( "getWeaponEntity", NULL, 'e' );
const idEventDef EV_Player_OpenPDA( "openPDA" );
const idEventDef EV_Player_InPDA( "inPDA", NULL, 'd' );
const idEventDef EV_Player_ExitTeleporter( "exitTeleporter" );
const idEventDef EV_Player_StopAudioLog( "stopAudioLog" );
const idEventDef EV_Player_HideTip( "hideTip" );
const idEventDef EV_Player_LevelTrigger( "levelTrigger" );
const idEventDef EV_SpectatorTouch( "spectatorTouch", "et" );
const idEventDef EV_Player_GiveInventoryItem("giveInventoryItem", "s");
const idEventDef EV_Player_RemoveInventoryItem("removeInventoryItem", "s");
const idEventDef EV_Player_GetIdealWeapon( "getIdealWeapon", NULL, 's' );
const idEventDef EV_Player_SetPowerupTime("setPowerupTime", "dd");
const idEventDef EV_Player_IsPowerupActive("isPowerupActive", "d", 'd');
const idEventDef EV_Player_WeaponAvailable("weaponAvailable", "s", 'd');
const idEventDef EV_Player_StartWarp("startWarp");
const idEventDef EV_Player_StopHelltime("stopHelltime", "d");
const idEventDef EV_Player_ToggleBloom("toggleBloom", "d");
const idEventDef EV_Player_SetBloomParms("setBloomParms", "ff");
const idEventDef EV_Player_ShowConsequences("showConsequences", NULL);
const idEventDef EV_Player_SetViewAngles("setViewAngles", "v");
const idEventDef EV_Player_GetEyeHeight("getEyeHeight", NULL, 'f');
// Koz begin - let scripts query which hand does what when using motion controls
const idEventDef EV_Player_GetWeaponHand( "getWeaponHand", NULL, 'd' );
const idEventDef EV_Player_GetFlashHand( "getFlashHand", NULL, 'd' ); // get flashlight hand
const idEventDef EV_Player_GetWeaponHandState( "getWeaponHandState", NULL, 'd' );
const idEventDef EV_Player_GetFlashHandState( "getFlashHandState", NULL, 'd' ); // get flashlight hand state
const idEventDef EV_Player_GetFlashState( "getFlashState", NULL, 'd' ); // get flashlight state

// Koz end

CLASS_DECLARATION( idActor, idPlayer )
	EVENT( EV_Player_GetButtons,			idPlayer::Event_GetButtons )
	EVENT( EV_Player_GetMove,				idPlayer::Event_GetMove )
	EVENT( EV_Player_GetViewAngles,			idPlayer::Event_GetViewAngles )
	EVENT( EV_Player_StopFxFov,				idPlayer::Event_StopFxFov )
	EVENT( EV_Player_EnableWeapon,			idPlayer::Event_EnableWeapon )
	EVENT( EV_Player_DisableWeapon,			idPlayer::Event_DisableWeapon )
	EVENT( EV_Player_GetCurrentWeapon,		idPlayer::Event_GetCurrentWeapon )
	EVENT( EV_Player_GetPreviousWeapon,		idPlayer::Event_GetPreviousWeapon )
	EVENT( EV_Player_SelectWeapon,			idPlayer::Event_SelectWeapon )
	EVENT( EV_Player_GetWeaponEntity,		idPlayer::Event_GetWeaponEntity )
	EVENT( EV_Player_OpenPDA,				idPlayer::Event_OpenPDA )
	EVENT( EV_Player_InPDA,					idPlayer::Event_InPDA )
	EVENT( EV_Player_ExitTeleporter,		idPlayer::Event_ExitTeleporter )
	EVENT( EV_Player_StopAudioLog,			idPlayer::Event_StopAudioLog )
	EVENT( EV_Player_HideTip,				idPlayer::Event_HideTip )
	EVENT( EV_Player_LevelTrigger,			idPlayer::Event_LevelTrigger )
	EVENT( EV_Gibbed,						idPlayer::Event_Gibbed )
	EVENT(EV_Player_GiveInventoryItem,		idPlayer::Event_GiveInventoryItem)
	EVENT(EV_Player_RemoveInventoryItem,	idPlayer::Event_RemoveInventoryItem)
	EVENT( EV_Player_GetIdealWeapon,		idPlayer::Event_GetIdealWeapon )
	EVENT(EV_Player_WeaponAvailable,		idPlayer::Event_WeaponAvailable)
	EVENT(EV_Player_SetPowerupTime,		idPlayer::Event_SetPowerupTime)
	EVENT(EV_Player_IsPowerupActive,		idPlayer::Event_IsPowerupActive)
	EVENT(EV_Player_StartWarp,				idPlayer::Event_StartWarp)
	EVENT(EV_Player_StopHelltime,			idPlayer::Event_StopHelltime)
	EVENT(EV_Player_ToggleBloom,			idPlayer::Event_ToggleBloom)
	EVENT(EV_Player_SetBloomParms,			idPlayer::Event_SetBloomParms)
	EVENT(EV_Player_ShowConsequences,		idPlayer::Event_ShowConsequences)
	EVENT(EV_Player_SetViewAngles,			idPlayer::Event_SetViewAngles)
	EVENT(EV_Player_GetEyeHeight,			idPlayer::Event_GetEyeHeight)
    // Koz begin
    EVENT( EV_Player_GetWeaponHand, 		idPlayer::Event_GetWeaponHand )
    EVENT( EV_Player_GetFlashHand,			idPlayer::Event_GetFlashHand ) // get flashlight hand
    EVENT( EV_Player_GetWeaponHandState,	idPlayer::Event_GetWeaponHandState )
    EVENT( EV_Player_GetFlashHandState,		idPlayer::Event_GetFlashHandState ) // get flashlight hand state
    EVENT( EV_Player_GetFlashState,			idPlayer::Event_GetFlashState ) // get flashlight state
    // Koz end
END_CLASS

const int MAX_RESPAWN_TIME_MP = 2000; //Lubos: in multiplayer is for some reason the timing different
const int MAX_RESPAWN_TIME = 10000;
const int RAGDOLL_DEATH_TIME = 3000;
const int MAX_PDAS = 64;
const int MAX_PDA_ITEMS = 128;
const int STEPUP_TIME = 200;
const int MAX_INVENTORY_ITEMS = 20;

idVec3 idPlayer::colorBarTable[ 8 ] = {
	idVec3( 0.25f, 0.25f, 0.25f ),
	idVec3( 1.00f, 0.00f, 0.00f ),
	idVec3( 0.00f, 0.80f, 0.10f ),
	idVec3( 0.20f, 0.50f, 0.80f ),
	idVec3( 1.00f, 0.80f, 0.10f ),
	idVec3(0.425f, 0.484f, 0.445f),
	idVec3(0.39f, 0.199f, 0.3f),
	idVec3(0.484f, 0.312f, 0.074f)
};

/* Carl: Teleport
================
idPlayer::CanReachPosition
================
*/
bool idPlayer::CanReachPosition( const idVec3& pos, idVec3& betterPos )
{
    aasPath_t	path;
    int			toAreaNum;
    int			areaNum;
    idVec3 origin;

    toAreaNum = PointReachableAreaNum(pos);
    betterPos = pos;
    if (aas)
        aas->PushPointIntoAreaNum( toAreaNum, betterPos );

    idVec3 floorPos = betterPos;

    origin = physicsObj.GetOrigin();
    areaNum = PointReachableAreaNum(origin);

    // check relative to the AAS area's official floor
    if (aas)
    {
        floorPos.z -= 1000;
        aas->PushPointIntoAreaNum(toAreaNum, floorPos);
        // sloped floors will change x or y, not just z, wrecking our algorithm
        if (floorPos.x != betterPos.x || floorPos.y != betterPos.y)
            floorPos = betterPos;
        // AAS areas have a valid floor (except for stairs), but not a valid ceiling
        // if it's stairs, or our point is higher, then use our point
        if (floorPos.z - pos.z < pm_stepsize.GetFloat() + 2 )
            betterPos.z = pos.z;
        // but if our point is too much lower than the AAS floor, use the AAS floor
    }
    // if in the same area, check relative to our feet
    if (toAreaNum == areaNum)
    {
        floorPos.z = origin.z;
    }
    float height = pos.z - floorPos.z;

    // if it's higher off the floor than we can jump, or lower than we can fall, then give up now
    if (height > pm_jumpheight.GetFloat() + 2 || height < -140)
        return false;

    // if there's no AAS, we can teleport anywhere horizontal we can see, as long as it's height is within jumping or falling height
    if (!aas)
        return true;

    if (ai_debugMove.GetBool())
    {
        aas->DrawArea(areaNum);
        aas->DrawArea(toAreaNum);
    }
    if (!toAreaNum)
        return false;
    if (ai_debugMove.GetBool())
        aas->ShowWalkPath(origin, toAreaNum, betterPos, travelFlags);
    if (areaNum == toAreaNum)
        return true;

    aas->PushPointIntoAreaNum(areaNum, origin);
    idReachability* reach = NULL;
    int travelTime;

    bool result = aas->RouteToGoalArea(areaNum, origin, toAreaNum, travelFlags, travelTime, &reach) && reach && (travelTime <= vr_teleportMaxTravel.GetInteger())
                  && CheckTeleportPath(betterPos, toAreaNum);
    return result;
}

/*
==============
idInventory::Clear
==============
*/
void idInventory::Clear( void ) {
	maxHealth		= 0;
	weapons			= 0;
	powerups		= 0;
	armor			= 0;
	maxarmor		= 0;
	deplete_armor	= 0;
	deplete_rate	= 0.0f;
	deplete_ammount	= 0;
	nextArmorDepleteTime = 0;

	memset( ammo, 0, sizeof( ammo ) );

	ClearPowerUps();

	// set to -1 so that the gun knows to have a full clip the first time we get it and at the start of the level
	memset( clip, -1, sizeof( clip ) );

	items.DeleteContents( true );
	memset(pdasViewed, 0, 4 * sizeof( pdasViewed[0] ) );
	pdas.Clear();
	videos.Clear();
	emails.Clear();
	selVideo = 0;
	selEMail = 0;
	selPDA = 0;
	selAudio = 0;
	pdaOpened = false;
	turkeyScore = false;

	levelTriggers.Clear();

	nextItemPickup = 0;
	nextItemNum = 1;
	onePickupTime = 0;
	pickupItemNames.Clear();
	objectiveNames.Clear();

	ammoPredictTime = 0;

	lastGiveTime = 0;

	ammoPulse	= false;
	weaponPulse	= false;
	armorPulse	= false;
}

/*
==============
idInventory::GivePowerUp
==============
*/
void idInventory::GivePowerUp( idPlayer *player, int powerup, int msec ) {
	if ( !msec ) {
		// get the duration from the .def files
		const idDeclEntityDef *def = NULL;
		switch ( powerup ) {
			case BERSERK:
				def = gameLocal.FindEntityDef( "powerup_berserk", false );
				break;
			case INVISIBILITY:
				def = gameLocal.FindEntityDef( "powerup_invisibility", false );
				break;
			case MEGAHEALTH:
				def = gameLocal.FindEntityDef( "powerup_megahealth", false );
				break;
			case ADRENALINE:
				def = gameLocal.FindEntityDef( "powerup_adrenaline", false );
				break;
			case INVULNERABILITY:
				def = gameLocal.FindEntityDef("powerup_invulnerability", false);
				break;
			/*case HASTE:
				def = gameLocal.FindEntityDef( "powerup_haste", false );
				break;*/
		}
		assert( def );
		msec = def->dict.GetInt( "time" ) * 1000;
	}
	powerups |= 1 << powerup;
	powerupEndTime[ powerup ] = gameLocal.time + msec;
}

/*
==============
idInventory::ClearPowerUps
==============
*/
void idInventory::ClearPowerUps( void ) {
	int i;
	for ( i = 0; i < MAX_POWERUPS; i++ ) {
		powerupEndTime[ i ] = 0;
	}
	powerups = 0;
}

/*
==============
idInventory::GetPersistantData
==============
*/
void idInventory::GetPersistantData( idDict &dict ) {
	int		i;
	int		num;
	idDict	*item;
	idStr	key;
	const idKeyValue *kv;
	const char *name;

	// armor
	dict.SetInt( "armor", armor );

	// don't bother with powerups, maxhealth, maxarmor, or the clip

	// ammo
	for( i = 0; i < AMMO_NUMTYPES; i++ ) {
		name = idWeapon::GetAmmoNameForNum( ( ammo_t )i );
		if ( name ) {
			dict.SetInt( name, ammo[ i ] );
		}
	}

	//Save the clip data
	for (i = 0; i < MAX_WEAPONS; i++) {
		dict.SetInt(va("clip%i", i), clip[ i ]);
	}

	// items
	num = 0;
	for( i = 0; i < items.Num(); i++ ) {
		item = items[ i ];

		// copy all keys with "inv_"
		kv = item->MatchPrefix( "inv_" );
		if ( kv ) {
			while( kv ) {
				sprintf( key, "item_%i %s", num, kv->GetKey().c_str() );
				dict.Set( key, kv->GetValue() );
				kv = item->MatchPrefix( "inv_", kv );
			}
			num++;
		}
	}
	dict.SetInt( "items", num );

	// pdas viewed
	for ( i = 0; i < 4; i++ ) {
		dict.SetInt( va("pdasViewed_%i", i), pdasViewed[i] );
	}

	dict.SetInt( "selPDA", selPDA );
	dict.SetInt( "selVideo", selVideo );
	dict.SetInt( "selEmail", selEMail );
	dict.SetInt( "selAudio", selAudio );
	dict.SetInt( "pdaOpened", pdaOpened );
	dict.SetInt( "turkeyScore", turkeyScore );

	// pdas
	for ( i = 0; i < pdas.Num(); i++ ) {
		sprintf( key, "pda_%i", i );
		dict.Set( key, pdas[ i ] );
	}
	dict.SetInt( "pdas", pdas.Num() );

	// video cds
	for ( i = 0; i < videos.Num(); i++ ) {
		sprintf( key, "video_%i", i );
		dict.Set( key, videos[ i ].c_str() );
	}
	dict.SetInt( "videos", videos.Num() );

	// emails
	for ( i = 0; i < emails.Num(); i++ ) {
		sprintf( key, "email_%i", i );
		dict.Set( key, emails[ i ].c_str() );
	}
	dict.SetInt( "emails", emails.Num() );

	// weapons
	dict.SetInt( "weapon_bits", weapons );

	dict.SetInt( "levelTriggers", levelTriggers.Num() );
	for ( i = 0; i < levelTriggers.Num(); i++ ) {
		sprintf( key, "levelTrigger_Level_%i", i );
		dict.Set( key, levelTriggers[i].levelName );
		sprintf( key, "levelTrigger_Trigger_%i", i );
		dict.Set( key, levelTriggers[i].triggerName );
	}
}

/*
==============
idInventory::RestoreInventory
==============
*/
void idInventory::RestoreInventory( idPlayer *owner, const idDict &dict ) {
	int			i;
	int			num;
	idDict		*item;
	idStr		key;
	idStr		itemname;
	const idKeyValue *kv;
	const char	*name;

	Clear();

	// health/armor
	maxHealth		= dict.GetInt( "maxhealth", "100" );
	armor			= dict.GetInt( "armor", "50" );
	maxarmor		= dict.GetInt( "maxarmor", "100" );
	deplete_armor	= dict.GetInt( "deplete_armor", "0" );
	deplete_rate	= dict.GetFloat( "deplete_rate", "2.0" );
	deplete_ammount	= dict.GetInt( "deplete_ammount", "1" );

	// the clip and powerups aren't restored

	// ammo
	for( i = 0; i < AMMO_NUMTYPES; i++ ) {
		name = idWeapon::GetAmmoNameForNum( ( ammo_t )i );
		if ( name ) {
			ammo[ i ] = dict.GetInt( name );
		}
	}

	//Restore the clip data
	for( i = 0; i < MAX_WEAPONS; i++ )
	{
		clip[i] = dict.GetInt( va( "clip%i", i ), "-1" );
		clipDuplicate[i] = dict.GetInt( va( "clipDuplicate%i", i ), "-1" );
	}

	// items
	num = dict.GetInt( "items" );
	items.SetNum( num );
	for( i = 0; i < num; i++ ) {
		item = new idDict();
		items[ i ] = item;
		sprintf( itemname, "item_%i ", i );
		kv = dict.MatchPrefix( itemname );
		while( kv ) {
			key = kv->GetKey();
			key.Strip( itemname );
			item->Set( key, kv->GetValue() );
			kv = dict.MatchPrefix( itemname, kv );
		}
	}

	// pdas viewed
	for ( i = 0; i < 4; i++ ) {
		pdasViewed[i] = dict.GetInt(va("pdasViewed_%i", i));
	}

	selPDA = dict.GetInt( "selPDA" );
	selEMail = dict.GetInt( "selEmail" );
	selVideo = dict.GetInt( "selVideo" );
	selAudio = dict.GetInt( "selAudio" );
	pdaOpened = dict.GetBool( "pdaOpened" );
	turkeyScore = dict.GetBool( "turkeyScore" );

	// pdas
	num = dict.GetInt( "pdas" );
	pdas.SetNum( num );
	for ( i = 0; i < num; i++ ) {
		sprintf( itemname, "pda_%i", i );
		pdas[i] = dict.GetString( itemname, "default" );
	}

	// videos
	num = dict.GetInt( "videos" );
	videos.SetNum( num );
	for ( i = 0; i < num; i++ ) {
		sprintf( itemname, "video_%i", i );
		videos[i] = dict.GetString( itemname, "default" );
	}

	// emails
	num = dict.GetInt( "emails" );
	emails.SetNum( num );
	for ( i = 0; i < num; i++ ) {
		sprintf( itemname, "email_%i", i );
		emails[i] = dict.GetString( itemname, "default" );
	}

	// weapons are stored as a number for persistant data, but as strings in the entityDef
	weapons	= dict.GetInt( "weapon_bits", "0" );

	if ( g_skill.GetInteger() >= 3 ) {
		Give( owner, dict, "weapon", dict.GetString( "weapon_nightmare" ), NULL, false );
	} else {
		Give( owner, dict, "weapon", dict.GetString( "weapon" ), NULL, false );
	}

	num = dict.GetInt( "levelTriggers" );
	for ( i = 0; i < num; i++ ) {
		sprintf( itemname, "levelTrigger_Level_%i", i );
		idLevelTriggerInfo lti;
		lti.levelName = dict.GetString( itemname );
		sprintf( itemname, "levelTrigger_Trigger_%i", i );
		lti.triggerName = dict.GetString( itemname );
		levelTriggers.Append( lti );
	}

}

/*
==============
idInventory::Save
==============
*/
void idInventory::Save( idSaveGame *savefile ) const {
	int i;

	savefile->WriteInt( maxHealth );
	savefile->WriteInt( weapons );
	savefile->WriteInt( powerups );
	savefile->WriteInt( armor );
	savefile->WriteInt( maxarmor );
	savefile->WriteInt( ammoPredictTime );
	savefile->WriteInt( deplete_armor );
	savefile->WriteFloat( deplete_rate );
	savefile->WriteInt( deplete_ammount );
	savefile->WriteInt( nextArmorDepleteTime );

	for( i = 0; i < AMMO_NUMTYPES; i++ ) {
		savefile->WriteInt( ammo[ i ] );
	}
	for( i = 0; i < MAX_WEAPONS; i++ ) {
		savefile->WriteInt( clip[ i ] );
		//savefile->WriteInt( clip[i] + clipDuplicate[ i ]);
	}
	for( i = 0; i < MAX_POWERUPS; i++ ) {
		savefile->WriteInt( powerupEndTime[ i ] );
	}

	savefile->WriteInt( items.Num() );
	for( i = 0; i < items.Num(); i++ ) {
		savefile->WriteDict( items[ i ] );
	}

	savefile->WriteInt( pdasViewed[0] );
	savefile->WriteInt( pdasViewed[1] );
	savefile->WriteInt( pdasViewed[2] );
	savefile->WriteInt( pdasViewed[3] );

	savefile->WriteInt( selPDA );
	savefile->WriteInt( selVideo );
	savefile->WriteInt( selEMail );
	savefile->WriteInt( selAudio );
	savefile->WriteBool( pdaOpened );
	savefile->WriteBool( turkeyScore );

	savefile->WriteInt( pdas.Num() );
	for( i = 0; i < pdas.Num(); i++ ) {
		savefile->WriteString( pdas[ i ] );
	}

	savefile->WriteInt( pdaSecurity.Num() );
	for( i=0; i < pdaSecurity.Num(); i++ ) {
		savefile->WriteString( pdaSecurity[ i ] );
	}

	savefile->WriteInt( videos.Num() );
	for( i = 0; i < videos.Num(); i++ ) {
		savefile->WriteString( videos[ i ] );
	}

	savefile->WriteInt( emails.Num() );
	for ( i = 0; i < emails.Num(); i++ ) {
		savefile->WriteString( emails[ i ] );
	}

	savefile->WriteInt( nextItemPickup );
	savefile->WriteInt( nextItemNum );
	savefile->WriteInt( onePickupTime );

	savefile->WriteInt( pickupItemNames.Num() );
	for( i = 0; i < pickupItemNames.Num(); i++ ) {
		savefile->WriteString( pickupItemNames[i].icon );
		savefile->WriteString( pickupItemNames[i].name );
	}

	savefile->WriteInt( objectiveNames.Num() );
	for( i = 0; i < objectiveNames.Num(); i++ ) {
		savefile->WriteString( objectiveNames[i].screenshot );
		savefile->WriteString( objectiveNames[i].text );
		savefile->WriteString( objectiveNames[i].title );
	}

	savefile->WriteInt( levelTriggers.Num() );
	for ( i = 0; i < levelTriggers.Num(); i++ ) {
		savefile->WriteString( levelTriggers[i].levelName );
		savefile->WriteString( levelTriggers[i].triggerName );
	}

	savefile->WriteBool( ammoPulse );
	savefile->WriteBool( weaponPulse );
	savefile->WriteBool( armorPulse );

	savefile->WriteInt( lastGiveTime );

	for (i = 0; i < AMMO_NUMTYPES; i++) {
		savefile->WriteInt(rechargeAmmo[i].ammo);
		savefile->WriteInt(rechargeAmmo[i].rechargeTime);
		savefile->WriteString(rechargeAmmo[i].ammoName);
	}
}

/*
==============
idInventory::Restore
==============
*/
void idInventory::Restore( idRestoreGame *savefile ) {
	int i, num;

	savefile->ReadInt( maxHealth );
	savefile->ReadInt( weapons );
	savefile->ReadInt( powerups );
	savefile->ReadInt( armor );
	savefile->ReadInt( maxarmor );
	savefile->ReadInt( ammoPredictTime );
	savefile->ReadInt( deplete_armor );
	savefile->ReadFloat( deplete_rate );
	savefile->ReadInt( deplete_ammount );
	savefile->ReadInt( nextArmorDepleteTime );

	for( i = 0; i < AMMO_NUMTYPES; i++ ) {
		savefile->ReadInt( ammo[ i ] );
	}
	for( i = 0; i < MAX_WEAPONS; i++ ) {
		savefile->ReadInt( clip[ i ] );
	}
	for( i = 0; i < MAX_POWERUPS; i++ ) {
		savefile->ReadInt( powerupEndTime[ i ] );
	}

	savefile->ReadInt( num );
	for( i = 0; i < num; i++ ) {
		idDict *itemdict = new idDict;

		savefile->ReadDict( itemdict );
		items.Append( itemdict );
	}

	// pdas
	savefile->ReadInt( pdasViewed[0] );
	savefile->ReadInt( pdasViewed[1] );
	savefile->ReadInt( pdasViewed[2] );
	savefile->ReadInt( pdasViewed[3] );

	savefile->ReadInt( selPDA );
	savefile->ReadInt( selVideo );
	savefile->ReadInt( selEMail );
	savefile->ReadInt( selAudio );
	savefile->ReadBool( pdaOpened );
	savefile->ReadBool( turkeyScore );

	savefile->ReadInt( num );
	for( i = 0; i < num; i++ ) {
		idStr strPda;
		savefile->ReadString( strPda );
		pdas.Append( strPda );
	}

	// pda security clearances
	savefile->ReadInt( num );
	for ( i = 0; i < num; i++ ) {
		idStr invName;
		savefile->ReadString( invName );
		pdaSecurity.Append( invName );
	}

	// videos
	savefile->ReadInt( num );
	for( i = 0; i < num; i++ ) {
		idStr strVideo;
		savefile->ReadString( strVideo );
		videos.Append( strVideo );
	}

	// email
	savefile->ReadInt( num );
	for( i = 0; i < num; i++ ) {
		idStr strEmail;
		savefile->ReadString( strEmail );
		emails.Append( strEmail );
	}

	savefile->ReadInt( nextItemPickup );
	savefile->ReadInt( nextItemNum );
	savefile->ReadInt( onePickupTime );
	savefile->ReadInt( num );
	for( i = 0; i < num; i++ ) {
		idItemInfo info;

		savefile->ReadString( info.icon );
		savefile->ReadString( info.name );

		pickupItemNames.Append( info );
	}

	savefile->ReadInt( num );
	for( i = 0; i < num; i++ ) {
		idObjectiveInfo obj;

		savefile->ReadString( obj.screenshot );
		savefile->ReadString( obj.text );
		savefile->ReadString( obj.title );

		objectiveNames.Append( obj );
	}

	savefile->ReadInt( num );
	for ( i = 0; i < num; i++ ) {
		idLevelTriggerInfo lti;
		savefile->ReadString( lti.levelName );
		savefile->ReadString( lti.triggerName );
		levelTriggers.Append( lti );
	}

	savefile->ReadBool( ammoPulse );
	savefile->ReadBool( weaponPulse );
	savefile->ReadBool( armorPulse );

	savefile->ReadInt( lastGiveTime );

	if (!oldSaveVersion) {
		for (i = 0; i < AMMO_NUMTYPES; i++) {
			savefile->ReadInt(rechargeAmmo[i].ammo);
			savefile->ReadInt(rechargeAmmo[i].rechargeTime);

			idStr name;
			savefile->ReadString(name);
			strcpy(rechargeAmmo[i].ammoName, name);
		}
	}
}

/*
==============
idInventory::AmmoIndexForAmmoClass
==============
*/
ammo_t idInventory::AmmoIndexForAmmoClass( const char *ammo_classname ) const {
	return idWeapon::GetAmmoNumForName( ammo_classname );
}

/*
==============
idInventory::AmmoIndexForAmmoClass
==============
*/
int idInventory::MaxAmmoForAmmoClass( idPlayer *owner, const char *ammo_classname ) const {
	return owner->spawnArgs.GetInt( va( "max_%s", ammo_classname ), "0" );
}

/*
==============
idInventory::AmmoPickupNameForIndex
==============
*/
const char *idInventory::AmmoPickupNameForIndex( ammo_t ammonum ) const {
	return idWeapon::GetAmmoPickupNameForNum( ammonum );
}

/*
===============
idInventory::CanGive
===============
*/
bool idInventory::CanGive(idPlayer *owner, const idDict &spawnArgs, const char *statname, const char *value, int *idealWeapon) {

	if (!idStr::Icmp(statname, "ammo_bloodstone")) {
		int max = MaxAmmoForAmmoClass(owner, statname);
		int i = AmmoIndexForAmmoClass(statname);

		if (max <= 0) {
			//No Max
			return true;
		} else {
			//Already at or above the max so don't allow the give
			if (ammo[ i ] >= max) {
				ammo[ i ] = max;
				return false;
			}

			return true;
		}
	} else if (!idStr::Icmp(statname, "item") || !idStr::Icmp(statname, "icon") || !idStr::Icmp(statname, "name")) {
		// ignore these as they're handled elsewhere
		//These items should not be considered as succesful gives because it messes up the max ammo items
		return false;
	}

	return true;
}

/*
==============
idInventory::WeaponIndexForAmmoClass
mapping could be prepared in the constructor
==============
*/
int idInventory::WeaponIndexForAmmoClass( const idDict & spawnArgs, const char *ammo_classname ) const {
	int i;
	const char *weapon_classname;
	for( i = 0; i < MAX_WEAPONS; i++ ) {
		weapon_classname = spawnArgs.GetString( va( "def_weapon%d", i ) );
		if ( !weapon_classname ) {
			continue;
		}
		const idDeclEntityDef *decl = gameLocal.FindEntityDef( weapon_classname, false );
		if ( !decl ) {
			continue;
		}
		if ( !idStr::Icmp( ammo_classname, decl->dict.GetString( "ammoType" ) ) ) {
			return i;
		}
	}
	return -1;
}

/*
==============
idInventory::AmmoIndexForWeaponClass
==============
*/
ammo_t idInventory::AmmoIndexForWeaponClass( const char *weapon_classname, int *ammoRequired ) {
	const idDeclEntityDef *decl = gameLocal.FindEntityDef( weapon_classname, false );
	if ( !decl ) {
	    return 0;
		//gameLocal.Error( "Unknown weapon in decl '%s'", weapon_classname );
	}
	if ( ammoRequired ) {
		*ammoRequired = decl->dict.GetInt( "ammoRequired" );
	}
	ammo_t ammo_i = AmmoIndexForAmmoClass( decl->dict.GetString( "ammoType" ) );
	return ammo_i;
}

/*
==============
idInventory::AddPickupName
==============
*/
void idInventory::AddPickupName(const char *name, const char *icon, idPlayer *owner) {    //_D3XP
	int num;

	num = pickupItemNames.Num();
	if ( ( num == 0 ) || ( pickupItemNames[ num - 1 ].name.Icmp( name ) != 0 ) ) {
		idItemInfo &info = pickupItemNames.Alloc();

		if ( idStr::Cmpn( name, STRTABLE_ID, STRTABLE_ID_LENGTH ) == 0 ) {
			info.name = common->GetLanguageDict()->GetString( name );
		} else {
			info.name = name;
		}
		info.icon = icon;

		if (gameLocal.isServer) {
			idBitMsg	msg;
			byte		msgBuf[MAX_EVENT_PARAM_SIZE];

			msg.Init(msgBuf, sizeof(msgBuf));
			msg.WriteString(name, MAX_EVENT_PARAM_SIZE);
			owner->ServerSendEvent(idPlayer::EVENT_PICKUPNAME, &msg, false, -1);
		}
	}
}

/*
==============
idInventory::Give
==============
*/
bool idInventory::Give( idPlayer *owner, const idDict &spawnArgs, const char *statname, const char *value, int *idealWeapon, bool updateHud ) {
	int						i;
	const char				*pos;
	const char				*end;
	int						len;
	idStr					weaponString;
	int						max;
	const idDeclEntityDef	*weaponDecl;
	bool					tookWeapon;
	int						amount;
	idItemInfo				info;
	const char				*name;

	if (!idStr::Icmp(statname, "ammo_bloodstone")) {
		i = AmmoIndexForAmmoClass(statname);
		max = MaxAmmoForAmmoClass(owner, statname);

		if (max <= 0) {
			//No Max
			ammo[ i ] += atoi(value);
		} else {
			//Already at or above the max so don't allow the give
			if (ammo[ i ] >= max) {
				ammo[ i ] = max;
				return false;
			}

			//We were below the max so accept the give but cap it at the max
			ammo[ i ] += atoi(value);

			if (ammo[ i ] > max) {
				ammo[ i ] = max;
			}
		}
	} else if ( !idStr::Icmpn( statname, "ammo_", 5 ) ) {
		i = AmmoIndexForAmmoClass( statname );
		max = MaxAmmoForAmmoClass( owner, statname );
		if ( ammo[ i ] >= max ) {
			return false;
		}
		amount = atoi( value );
		if ( amount ) {
			ammo[ i ] += amount;
			if ( ( max > 0 ) && ( ammo[ i ] > max ) ) {
				ammo[ i ] = max;
			}
			ammoPulse = true;

			name = AmmoPickupNameForIndex( i );
			if ( idStr::Length( name ) ) {
				AddPickupName( name, "", owner );
			}
		}
	} else if ( !idStr::Icmp( statname, "armor" ) ) {
		if ( armor >= maxarmor ) {
			return false;	// can't hold any more, so leave the item
		}
		amount = atoi( value );
		if ( amount ) {
			armor += amount;
			if ( armor > maxarmor ) {
				armor = maxarmor;
			}

			common->HapticEvent("pickup_shield", 0, 0, amount * 5, 0, 0);

			nextArmorDepleteTime = 0;
			armorPulse = true;
		}
	} else if ( idStr::FindText( statname, "inclip_" ) == 0 ) {
		idStr temp = statname;
		i = atoi(temp.Mid(7, 2));
		if ( i != -1 ) {
			common->HapticEvent("pickup_ammo", 0, 0, 100, 0, 0);
		}
	} else if (!idStr::Icmp(statname, "invulnerability")) {
		owner->GivePowerUp(INVULNERABILITY, SEC2MS(atof(value)));
	} else if (!idStr::Icmp(statname, "helltime")) {
		owner->GivePowerUp(HELLTIME, SEC2MS(atof(value)));
	} else if (!idStr::Icmp(statname, "envirosuit")) {
		owner->GivePowerUp(ENVIROSUIT, SEC2MS(atof(value)));
		owner->GivePowerUp(ENVIROTIME, SEC2MS(atof(value)));
	} else if (!idStr::Icmp(statname, "berserk")) {
		owner->GivePowerUp(BERSERK, SEC2MS(atof(value)));
	//} else if ( !idStr::Icmp( statname, "haste" ) ) {
	//	owner->GivePowerUp( HASTE, SEC2MS( atof( value ) ) );
	} else if ( !idStr::Icmp( statname, "mega" ) ) {
		GivePowerUp( owner, MEGAHEALTH, SEC2MS( atof( value ) ) );
	} else if ( !idStr::Icmp( statname, "weapon" ) ) {
		tookWeapon = false;
		for( pos = value; pos != NULL; pos = end ) {
			end = strchr( pos, ',' );
			if ( end ) {
				len = end - pos;
				end++;
			} else {
				len = strlen( pos );
			}

			idStr weaponName( pos, 0, len );

			// find the number of the matching weapon name
			for( i = 0; i < MAX_WEAPONS; i++ ) {
				if ( weaponName == spawnArgs.GetString( va( "def_weapon%d", i ) ) ) {
					break;
				}
			}

			if ( i >= MAX_WEAPONS ) {
				//Loop through and print out spawnArgs
				spawnArgs.Print();
				gameLocal.Warning("Unknown weapon '%s'", weaponName.c_str());
				continue;
			}

			// cache the media for this weapon
			weaponDecl = gameLocal.FindEntityDef( weaponName, false );

			// don't pickup "no ammo" weapon types twice
			// not for D3 SP .. there is only one case in the game where you can get a no ammo
			// weapon when you might already have it, in that case it is more conistent to pick it up
            if( gameLocal.isMultiplayer && ( weapons & ( 1 << i ) ) && ( duplicateWeapons & ( 1 << i ) ) && ( weaponDecl != NULL ) && !weaponDecl->dict.GetInt( "ammoRequired" ) )
			{
				continue;
			}

			if ( !gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) || ( weaponName == "weapon_fists" ) || ( weaponName == "weapon_soulcube" ) ) {
				if ( ( weapons & ( 1 << i ) ) == 0 || ( duplicateWeapons & ( 1 << i ) ) == 0 || gameLocal.isMultiplayer ) {
					tookWeapon = true;

					common->HapticEvent("pickup_weapon", 0, 0, 100, 0, 0);

					if ( owner->GetUserInfo()->GetBool( "ui_autoSwitch" ) && idealWeapon ) {
						assert( !gameLocal.isClient );
						*idealWeapon = i;
					}
					if ( owner->hud && updateHud && lastGiveTime + 1000 < gameLocal.time ) {
						owner->hud->SetStateInt( "newWeapon", owner->MapWeaponHudId(i) );
						owner->hud->HandleNamedEvent( "newWeapon" );
						lastGiveTime = gameLocal.time;
					}
					weaponPulse = true;
					if( ( weapons & ( 1 << i ) ) == 0 )
						weapons |= ( 1 << i );
                    else
                        duplicateWeapons |= ( 1 << i );
					foundWeapons |= weapons;

					if( weaponName != "weapon_pda" )
					{
						for( int index = 0; index < NUM_QUICK_SLOTS; ++index )
						{
							if( owner->GetQuickSlot( index ) == -1 )
							{
								owner->SetQuickSlot( index, i );
								break;
							}
						}
					}
				}
			}
		}
		return tookWeapon;
	} else if ( !idStr::Icmp( statname, "item" ) || !idStr::Icmp( statname, "icon" ) || !idStr::Icmp( statname, "name" ) ) {
		// ignore these as they're handled elsewhere
		return false;
	} else {
		// unknown item
		gameLocal.Warning( "Unknown stat '%s' added to player's inventory", statname );
		return false;
	}

	return true;
}

/*
===============
idInventoy::Drop
===============
*/
void idInventory::Drop( const idDict &spawnArgs, const char *weapon_classname, int weapon_index ) {
	// remove the weapon bit
	// also remove the ammo associated with the weapon as we pushed it in the item
	assert( weapon_index != -1 || weapon_classname );
	if ( weapon_index == -1 ) {
		for( weapon_index = 0; weapon_index < MAX_WEAPONS; weapon_index++ ) {
		    const char * spawnWeaponarg = spawnArgs.GetString( va( "def_weapon%d", weapon_index ) );
			if ( !idStr::Icmp( weapon_classname, spawnWeaponarg ) ) {
				break;
			}
		}
		if ( weapon_index >= MAX_WEAPONS ) {
			gameLocal.Error( "Unknown weapon '%s'", weapon_classname );
		}
	} else if ( !weapon_classname ) {
		weapon_classname = spawnArgs.GetString( va( "def_weapon%d", weapon_index ) );
	}
	weapons &= ( 0xffffffff ^ ( 1 << weapon_index ) );
	ammo_t ammo_i = AmmoIndexForWeaponClass( weapon_classname, NULL );
	if ( ammo_i ) {
		clip[ weapon_index ] = -1;
		ammo[ ammo_i ] = 0;
	}
}

/*
===============
idInventory::HasAmmo
===============
*/
int idInventory::HasAmmo( ammo_t type, int amount ) {
	if ( ( type == 0 ) || !amount ) {
		// always allow weapons that don't use ammo to fire
		return -1;
	}

	// check if we have infinite ammo
	if ( ammo[ type ] < 0 ) {
		return -1;
	}

	// return how many shots we can fire
	return ammo[ type ] / amount;
}

bool idPlayer::HasHoldableFlashlight()
{
    int fm = commonVr->currentFlashlightMode;
    if( vr_flashlightStrict.GetBool() )
    {
        if( fm != FLASHLIGHT_HAND && fm != FLASHLIGHT_INVENTORY )
            return false;
    }
    else
    {
        // In non-strict mode you can move the flashlight from your armour or helmet to your hand,
        // but you can't move it from your weapon.
        if( fm == FLASHLIGHT_GUN || fm == FLASHLIGHT_PISTOL || fm == FLASHLIGHT_NONE )
            return false;
    }
    // Carl: I'm not checking the weapons in the inventory. Maybe I should check that too? But I don't trust those values for the flashlight.
    return flashlight && flashlight->IsLinked() && !spectating && weaponEnabled && !hiddenWeapon && !gameLocal.world->spawnArgs.GetBool( "no_Weapons" );
}

/*
===============
idInventory::HasAmmo
===============
*/
int idInventory::HasAmmo(const char *weapon_classname, bool includeClip, idPlayer *owner) { 		//_D3XP
	int ammoRequired;
	ammo_t ammo_i = AmmoIndexForWeaponClass( weapon_classname, &ammoRequired );
	int ammoCount = HasAmmo(ammo_i, ammoRequired);

	if (includeClip && owner) {
		ammoCount += clip[owner->SlotForWeapon(weapon_classname)];
	}

	return ammoCount;
}

/*
===============
idInventory::HasEmptyClipCannotRefill
===============
*/
bool idInventory::HasEmptyClipCannotRefill(const char *weapon_classname, idPlayer *owner) {

    int clipSize = clip[owner->SlotForWeapon(weapon_classname)];

    if (clipSize) {
        return false;
    }

    const idDeclEntityDef *decl = gameLocal.FindEntityDef(weapon_classname, false);

    if (!decl) {
        gameLocal.Error("Unknown weapon in decl '%s'", weapon_classname);
    }

    int minclip = decl->dict.GetInt("minclipsize");

    if (!minclip) {
        return false;
    }

    ammo_t ammo_i = AmmoIndexForAmmoClass(decl->dict.GetString("ammoType"));
    int ammoRequired = decl->dict.GetInt("ammoRequired");
    int ammoCount = HasAmmo(ammo_i, ammoRequired);

    if (ammoCount < minclip) {
        return true;
    }

    return false;
}

/*
===============
idInventory::UseAmmo
===============
*/
bool idInventory::UseAmmo( ammo_t type, int amount ) {
	if ( !HasAmmo( type, amount ) ) {
		return false;
	}

	// take an ammo away if not infinite
	if ( ammo[ type ] >= 0 ) {
		ammo[ type ] -= amount;
		ammoPredictTime = gameLocal.time; // mp client: we predict this. mark time so we're not confused by snapshots
	}

	return true;
}

/*
===============
idInventory::UpdateArmor
===============
*/
void idInventory::UpdateArmor( void ) {
	if ( deplete_armor != 0.0f && deplete_armor < armor ) {
		if ( !nextArmorDepleteTime ) {
			nextArmorDepleteTime = gameLocal.time + deplete_rate * 1000;
		} else if ( gameLocal.time > nextArmorDepleteTime ) {
			armor -= deplete_ammount;
			if ( armor < deplete_armor ) {
				armor = deplete_armor;
			}
			nextArmorDepleteTime = gameLocal.time + deplete_rate * 1000;
		}
	}
}

/*
===============
idInventory::InitRechargeAmmo
===============
* Loads any recharge ammo definitions from the ammo_types entity definitions.
*/
void idInventory::InitRechargeAmmo(idPlayer *owner) {

    memset(rechargeAmmo, 0, sizeof(rechargeAmmo));

    const idKeyValue *kv = owner->spawnArgs.MatchPrefix("ammorecharge_");

    while (kv) {
        idStr key = kv->GetKey();
        idStr ammoname = key.Right(key.Length()- strlen("ammorecharge_"));
        int ammoType = AmmoIndexForAmmoClass(ammoname);
        rechargeAmmo[ammoType].ammo = (atof(kv->GetValue().c_str())*1000);
        strcpy(rechargeAmmo[ammoType].ammoName, ammoname);
        kv = owner->spawnArgs.MatchPrefix("ammorecharge_", kv);
    }
}

/*
===============
idInventory::RechargeAmmo
===============
* Called once per frame to update any ammo amount for ammo types that recharge.
*/
void idInventory::RechargeAmmo(idPlayer *owner) {
    for (int i = 0; i < AMMO_NUMTYPES; i++) {
        if (rechargeAmmo[i].ammo > 0) {
            if (!rechargeAmmo[i].rechargeTime)  {
                //Initialize the recharge timer.
                rechargeAmmo[i].rechargeTime = gameLocal.time;
            }

            int elapsed = gameLocal.time - rechargeAmmo[i].rechargeTime;

            if (elapsed >= rechargeAmmo[i].ammo) {
                int intervals = (gameLocal.time - rechargeAmmo[i].rechargeTime)/rechargeAmmo[i].ammo;
                ammo[i] += intervals;

                int max = MaxAmmoForAmmoClass(owner, rechargeAmmo[i].ammoName);

                if (max > 0) {
                    if (ammo[i] > max) {
                        ammo[i] = max;
                    }
                }

                rechargeAmmo[i].rechargeTime += intervals*rechargeAmmo[i].ammo;
            }
        }
    }
}

/*
==============
idPlayer::idPlayer
==============
*/
idPlayer::idPlayer() {
	memset( &usercmd, 0, sizeof( usercmd ) );

    aas = NULL;
    travelFlags = TFL_WALK | TFL_AIR | TFL_CROUCH | TFL_WALKOFFLEDGE | TFL_BARRIERJUMP | TFL_JUMP | TFL_LADDER | TFL_WATERJUMP | TFL_ELEVATOR | TFL_SPECIAL;
    aimValidForTeleport = false;

    teleportAimPoint = vec3_zero;
    teleportPoint = vec3_zero;
    teleportAimPointPitch = 0.0f;

	flashlightPreviouslyInHand = false;
    warpMove				= false;
    warpAim					= false;
    warpVel					= vec3_zero;

	noclip					= false;
	godmode					= false;

	spawnAnglesSet			= false;
	spawnAngles				= ang_zero;
	viewAngles				= ang_zero;
	cmdAngles				= ang_zero;

    // Koz begin : for independent weapon aiming in VR.
    commonVr->independentWeaponYaw = 0.0f;
    commonVr->independentWeaponPitch = 0.0f;
    // Koz end

	oldButtons				= 0;
	buttonMask				= 0;
	oldFlags				= 0;

	lastHitTime				= 0;
	lastSndHitTime			= 0;
	lastSavingThrowTime		= 0;

	pdaModelDefHandle = -1;
	memset( &pdaRenderEntity, 0, sizeof( pdaRenderEntity ) );

	holsterModelDefHandle = -1;
	memset( &holsterRenderEntity, 0, sizeof( holsterRenderEntity ) );

	hud						= NULL;
	objectiveSystem			= NULL;
	objectiveSystemOpen		= false;

	mountedObject			= NULL;
	enviroSuitLight			= NULL;

	heartRate				= BASE_HEARTRATE;
	heartInfo.Init( 0, 0, 0, 0 );
	lastHeartAdjust			= 0;
	lastHeartBeat			= 0;
	lastDmgTime				= 0;
	deathClearContentsTime	= 0;
	lastArmorPulse			= -10000;
	stamina					= 0.0f;
	healthPool				= 0.0f;
	nextHealthPulse			= 0;
	healthPulse				= false;
	nextHealthTake			= 0;
	healthTake				= false;

	scoreBoardOpen			= false;
	forceScoreBoard			= false;
	forceRespawn			= false;
	spectating				= false;
	spectator				= 0;
	colorBar				= vec3_zero;
	colorBarIndex			= 0;
	forcedReady				= false;
	wantSpectate			= false;

	lastHitToggle			= false;

	minRespawnTime			= 0;
	maxRespawnTime			= 0;

	firstPersonViewOrigin	= vec3_zero;
	firstPersonViewAxis		= mat3_identity;

	hipJoint				= INVALID_JOINT;
	chestJoint				= INVALID_JOINT;
	headJoint				= INVALID_JOINT;

	bobFoot					= 0;
	bobFrac					= 0.0f;
	bobfracsin				= 0.0f;
	bobCycle				= 0;
	xyspeed					= 0.0f;
	stepUpTime				= 0;
	stepUpDelta				= 0.0f;
	idealLegsYaw			= 0.0f;
	legsYaw					= 0.0f;
	legsForward				= true;
	oldViewYaw				= 0.0f;
	viewBobAngles			= ang_zero;
	viewBob					= vec3_zero;
	landChange				= 0;
	landTime				= 0;

    weaponEnabled			= true;
    risingWeaponHand		= -1;
    weapon_soulcube			= -1;
    weapon_pda				= -1;
    weapon_fists			= -1;
    weapon_chainsaw			= -1;
	weapon_none 			= -1;
	weapon_flashlight		= -1;
	weapon_bloodstone 		= -1;
	weapon_bloodstone_active1 = -1;
	weapon_bloodstone_active2 = -1;
	weapon_bloodstone_active3 = -1;
	harvest_lock			= false;

	hudPowerup				= -1;
	lastHudPowerup			= -1;
	hudPowerupDuration		= 0;
	// Koz begin
	weapon_pistol 			= -1;
	weapon_shotgun			= -1;
	weapon_shotgun_double	= -1;
	weapon_machinegun		= -1;
	weapon_chaingun			= -1;
	weapon_handgrenade		= -1;
	weapon_plasmagun		= -1;
	weapon_rocketlauncher	= -1;
	weapon_bfg				= -1;
	weapon_flashlight_new	= -1;
	weapon_grabber			= -1;

	showWeaponViewModel		= true;

    flashlightModelDefHandle = -1;

    hudHandle = -1;
    memset( &hudEntity, 0, sizeof( hudEntity ) );

    skin					= NULL;
    powerUpSkin				= NULL;
	baseSkinName			= "";

    numProjectilesFired		= 0;
	numProjectileHits		= 0;

	airless					= false;
	airTics					= 0;
	lastAirDamage			= 0;

	gibDeath				= false;
	gibsLaunched			= false;
	gibsDir					= vec3_zero;

	zoomFov.Init( 0, 0, 0, 0 );
	centerView.Init( 0, 0, 0, 0 );
	fxFov					= false;

	influenceFov			= 0;
	influenceActive			= 0;
	influenceRadius			= 0.0f;
	influenceEntity			= NULL;
	influenceMaterial		= NULL;
	influenceSkin			= NULL;

	privateCameraView		= NULL;

	memset( loggedViewAngles, 0, sizeof( loggedViewAngles ) );
	memset( loggedAccel, 0, sizeof( loggedAccel ) );
	currentLoggedAccel	= 0;

	focusTime				= 0;
	focusGUIent				= NULL;
	focusUI					= NULL;
	focusCharacter			= NULL;
	talkCursor				= 0;
	focusVehicle			= NULL;
	cursor					= NULL;

	oldMouseX				= 0;
	oldMouseY				= 0;

	pdaAudio				= "";
	pdaVideo				= "";
	pdaVideoWave			= "";

	lastDamageDef			= 0;
	lastDamageDir			= vec3_zero;
	lastDamageLocation		= 0;
	smoothedFrame			= 0;
	smoothedOriginUpdated	= false;
	smoothedOrigin			= vec3_zero;
	smoothedAngles			= ang_zero;

	fl.networkSync			= true;

	latchedTeam				= -1;
	doingDeathSkin			= false;
	weaponGone				= false;
	useInitialSpawns		= false;
	tourneyRank				= 0;
	lastSpectateTeleport	= 0;
	tourneyLine				= 0;
	hiddenWeapon			= false;
	tipUp					= false;
	objectiveUp				= false;
	teleportEntity			= NULL;
	teleportKiller			= -1;
	respawning				= false;
	ready					= false;
	leader					= false;
	lastSpectateChange		= 0;
	lastTeleFX				= -9999;
	weaponCatchup			= false;
	lastSnapshotSequence	= 0;

	MPAim					= -1;
	lastMPAim				= -1;
	lastMPAimTime			= 0;
	MPAimFadeTime			= 0;
	MPAimHighlight			= false;

	spawnedTime				= 0;
	lastManOver				= false;
	lastManPlayAgain		= false;
	lastManPresent			= false;

	isTelefragged			= false;

	isLagged				= false;
	isChatting				= false;

	selfSmooth				= false;

	itemSystemCallExit		= "";
	nextTriggerTime			= 0;

    ResetControllerShake();
    blink = false;
}

/*
==============
idPlayer::LinkScriptVariables

set up conditions for animation
==============
*/
void idPlayer::LinkScriptVariables( void ) {
	AI_FORWARD.LinkTo(			scriptObject, "AI_FORWARD" );
	AI_BACKWARD.LinkTo(			scriptObject, "AI_BACKWARD" );
	AI_STRAFE_LEFT.LinkTo(		scriptObject, "AI_STRAFE_LEFT" );
	AI_STRAFE_RIGHT.LinkTo(		scriptObject, "AI_STRAFE_RIGHT" );
	AI_ATTACK_HELD.LinkTo(		scriptObject, "AI_ATTACK_HELD" );
	AI_WEAPON_FIRED.LinkTo(		scriptObject, "AI_WEAPON_FIRED" );
	AI_JUMP.LinkTo(				scriptObject, "AI_JUMP" );
	AI_DEAD.LinkTo(				scriptObject, "AI_DEAD" );
	AI_CROUCH.LinkTo(			scriptObject, "AI_CROUCH" );
	AI_ONGROUND.LinkTo(			scriptObject, "AI_ONGROUND" );
	AI_ONLADDER.LinkTo(			scriptObject, "AI_ONLADDER" );
	AI_HARDLANDING.LinkTo(		scriptObject, "AI_HARDLANDING" );
	AI_SOFTLANDING.LinkTo(		scriptObject, "AI_SOFTLANDING" );
	AI_RUN.LinkTo(				scriptObject, "AI_RUN" );
	AI_PAIN.LinkTo(				scriptObject, "AI_PAIN" );
	AI_RELOAD.LinkTo(			scriptObject, "AI_RELOAD" );
	AI_TELEPORT.LinkTo(			scriptObject, "AI_TELEPORT" );
	AI_TURN_LEFT.LinkTo(		scriptObject, "AI_TURN_LEFT" );
	AI_TURN_RIGHT.LinkTo(		scriptObject, "AI_TURN_RIGHT" );
}

/*
==============
idPlayer::SetupWeaponEntity
==============
*/
void idPlayer::SetupWeaponEntity( void ) {
	int w;
	const char *weap;

    // Carl: dual wielding
    for( int h = 0; h < 2; h++ )
    {
        if( hands[h].weapon )
        {
            // get rid of old weapon
            hands[h].weapon->Clear();
            hands[h].currentWeapon = -1; // carl: todo dual wielding
        }
        else if( !gameLocal.isClient )
        {
            hands[h].weapon = static_cast< idWeapon* >( gameLocal.SpawnEntityType( idWeapon::Type, NULL ) );
            hands[h].weapon->SetOwner( this, h );
            hands[h].currentWeapon = -1; // carl: todo dual wielding
        }
    }
    if( flashlight )
    {
    }
    else if( !gameLocal.isClient )
    {
        // flashlight
        flashlight = static_cast<idWeapon*>( gameLocal.SpawnEntityType( idWeapon::Type, NULL ) );
        flashlight->SetFlashlightOwner( this );
        //FlashlightOff();

    }


	for( w = 0; w < MAX_WEAPONS; w++ ) {
		weap = spawnArgs.GetString( va( "def_weapon%d", w ) );
		if ( weap && *weap ) {
			idWeapon::CacheWeapon( weap );
		}
	}
}

/*
==================
idPlayer::ShouldBlink

Returns true if the view needs to be darkened
==================
*/
bool idPlayer::ShouldBlink()
{
    return ( blink || commonVr->leanBlank );
}

/*
==============
idWeaponHolder::idWeaponHolder
==============
*/
idWeaponHolder::idWeaponHolder()
{
    owner = NULL;
    currentWeapon = 0;
    isTheDuplicate = false;
}

/*
==============
idWeaponHolder::~idWeaponHolder
==============
*/
idWeaponHolder::~idWeaponHolder()
{
}

/*
==============
idWeaponHolder::Init
==============
*/
void idWeaponHolder::Init( idPlayer* player )
{
    owner = player;
    currentWeapon = owner->weapon_fists;
    isTheDuplicate = false;
}

/*
==============
idWeaponHolder::isEmpty
==============
*/
bool idWeaponHolder::isEmpty()
{
    if( currentWeapon < 0 || !owner )
        return true;
    return currentWeapon == owner->weapon_fists;
}

/*
==============
idPlayer::Init
==============
*/
void idPlayer::Init( void ) {
	const char			*value;
	const idKeyValue	*kv;

	flashlightPreviouslyInHand = false;

	//GB
    velocityPunched         = false;

	noclip					= false;
	godmode					= false;

	oldButtons				= 0;
	oldFlags				= 0;

	weaponEnabled			= true;
	risingWeaponHand		= -1;
	weapon_none 			= 0;
	weapon_soulcube			= SlotForWeapon( "weapon_soulcube" );
	weapon_pda				= SlotForWeapon( "weapon_pda" );
	weapon_fists			= SlotForWeapon( "weapon_fists" );
	weapon_flashlight		= SlotForWeapon( "weapon_flashlight" );
	weapon_chainsaw			= SlotForWeapon( "weapon_chainsaw" );
	weapon_bloodstone		= SlotForWeapon( "weapon_bloodstone_passive" );
	weapon_bloodstone_active1 = SlotForWeapon( "weapon_bloodstone_active1" );
	weapon_bloodstone_active2 = SlotForWeapon( "weapon_bloodstone_active2" );
	weapon_bloodstone_active3 = SlotForWeapon( "weapon_bloodstone_active3" );
	harvest_lock			= false;

	// Koz begin;
	weapon_pistol			= SlotForWeapon( "weapon_pistol" );
	weapon_shotgun			= SlotForWeapon( "weapon_shotgun" );
	weapon_shotgun_double	= SlotForWeapon( "weapon_shotgun_double" );
	weapon_machinegun		= SlotForWeapon( "weapon_machinegun" );
	weapon_chaingun			= SlotForWeapon( "weapon_chaingun" );
	weapon_handgrenade		= SlotForWeapon( "weapon_handgrenade" );
	weapon_plasmagun		= SlotForWeapon( "weapon_plasmagun" );
	weapon_rocketlauncher	= SlotForWeapon( "weapon_rocketlauncher" );
	weapon_bfg				= SlotForWeapon( "weapon_bfg" );
	weapon_flashlight_new	= SlotForWeapon( "weapon_flashlight_new" );
	weapon_grabber			= SlotForWeapon( "weapon_grabber" );
	// Koz end


	lastDmgTime				= 0;
	lastArmorPulse			= -10000;
	lastHeartAdjust			= 0;
	lastHeartBeat			= 0;
	heartInfo.Init( 0, 0, 0, 0 );

	bobCycle				= 0;
	bobFrac					= 0.0f;
	landChange				= 0;
	landTime				= 0;
	zoomFov.Init( 0, 0, 0, 0 );
	centerView.Init( 0, 0, 0, 0 );
	fxFov					= false;

	influenceFov			= 0;
	influenceActive			= 0;
	influenceRadius			= 0.0f;
	influenceEntity			= NULL;
	influenceMaterial		= NULL;
	influenceSkin			= NULL;

	mountedObject			= NULL;
	if (enviroSuitLight.IsValid()) {
		enviroSuitLight.GetEntity()->PostEventMS(&EV_Remove, 0);
	}

	enviroSuitLight			= NULL;
	healthRecharge			= false;
	lastHealthRechargeTime	= 0;
	rechargeSpeed			= 500;
	new_g_damageScale		= 1.f;
	bloomEnabled			= false;
	bloomSpeed				= 1.f;
	bloomIntensity			= -0.01f;
	inventory.InitRechargeAmmo(this);
	hudPowerup				= -1;
	lastHudPowerup			= -1;
	hudPowerupDuration		= 0;
	currentLoggedAccel		= 0;

	handRaised = false;
	handLowered = false;

	focusTime				= 0;
	focusGUIent				= NULL;
	focusUI					= NULL;
	focusCharacter			= NULL;
	focusClickable			= NULL;
	talkCursor				= 0;
	focusVehicle			= NULL;

	pVRClientInfo			= NULL;

	// remove any damage effects
	playerView.ClearEffects();

	// damage values
	fl.takedamage			= true;
	ClearPain();

	// Koz reset holster slots

	holsteredWeapon = weapon_fists;
	extraHolsteredWeapon = weapon_fists;
	extraHolsteredWeaponModel = NULL;

	// restore persistent data
	RestorePersistantInfo();

	bobCycle		= 0;
	stamina			= 0.0f;
	healthPool		= 0.0f;
	nextHealthPulse = 0;
	healthPulse		= false;
	nextHealthTake	= 0;
	healthTake		= false;

	for( int hand = 0; hand < 2; hand++ )
		hands[ hand ].Init( this, hand );
	SetupWeaponEntity();

	heartRate = BASE_HEARTRATE;
	AdjustHeartRate( BASE_HEARTRATE, 0.0f, 0.0f, true );

	idealLegsYaw = 0.0f;
	legsYaw = 0.0f;
	legsForward	= true;
	oldViewYaw = 0.0f;

	// set the pm_ cvars
	if ( !gameLocal.isMultiplayer || gameLocal.isServer ) {
		kv = spawnArgs.MatchPrefix( "pm_", NULL );
		while( kv ) {
			cvarSystem->SetCVarString( kv->GetKey(), kv->GetValue() );
			kv = spawnArgs.MatchPrefix( "pm_", kv );
		}
	}

	// disable stamina on hell levels
	if ( gameLocal.world && gameLocal.world->spawnArgs.GetBool( "no_stamina" ) ) {
		pm_stamina.SetFloat( 0.0f );
	}

	// stamina always initialized to maximum
	stamina = pm_stamina.GetFloat();

	// air always initialized to maximum too
	airTics = pm_airTics.GetFloat();
	airless = false;

	gibDeath = false;
	gibsLaunched = false;
	gibsDir.Zero();

	// set the gravity
	physicsObj.SetGravity( gameLocal.GetGravity() );

	// start out standing
	SetEyeHeight( pm_normalviewheight.GetFloat() );

	stepUpTime = 0;
	stepUpDelta = 0.0f;
	viewBobAngles.Zero();
	viewBob.Zero();

	value = spawnArgs.GetString( "model" );
	if ( value && ( *value != 0 ) ) {
		SetModel( value );
	}

	if ( cursor ) {
		cursor->SetStateInt( "talkcursor", 0 );
		cursor->SetStateString( "combatcursor", "1" );
		cursor->SetStateString( "itemcursor", "0" );
		cursor->SetStateString( "guicursor", "0" );
		cursor->SetStateString("grabbercursor", "0");
	}

	if ( ( gameLocal.isMultiplayer || g_testDeath.GetBool() ) && skin ) {
		SetSkin( skin );
		renderEntity.shaderParms[6] = 0.0f;
	} else if ( spawnArgs.GetString( "spawn_skin", NULL, &value ) ) {
		skin = declManager->FindSkin( value );
		SetSkin( skin );
		renderEntity.shaderParms[6] = 0.0f;
	}

	InitPlayerBones();

	commonVr->currentFlashlightMode = vr_flashlightMode.GetInteger();
	commonVr->restoreFlashlightMode = false;

	commonVr->thirdPersonMovement = false;
	commonVr->thirdPersonDelta = 0.0f;
	commonVr->thirdPersonHudPos = vec3_zero;
	commonVr->thirdPersonHudAxis = mat3_identity;

	// initialize the script variables
	AI_FORWARD		= false;
	AI_BACKWARD		= false;
	AI_STRAFE_LEFT	= false;
	AI_STRAFE_RIGHT	= false;
	AI_ATTACK_HELD	= false;
	AI_WEAPON_FIRED	= false;
	AI_JUMP			= false;
	AI_DEAD			= false;
	AI_CROUCH		= false;
	AI_ONGROUND		= true;
	AI_ONLADDER		= false;
	AI_HARDLANDING	= false;
	AI_SOFTLANDING	= false;
	AI_RUN			= false;
	AI_PAIN			= false;
	AI_RELOAD		= false;
	AI_TELEPORT		= false;
	AI_TURN_LEFT	= false;
	AI_TURN_RIGHT	= false;

	// reset the script object
	ConstructScriptObject();

	// execute the script so the script object's constructor takes effect immediately
	scriptThread->Execute();

	forceScoreBoard		= false;
	forcedReady			= false;

	privateCameraView	= NULL;

	lastSpectateChange	= 0;
	lastTeleFX			= -9999;

	hiddenWeapon		= false;
	tipUp				= false;
	objectiveUp			= false;
	teleportEntity		= NULL;
	teleportKiller		= -1;
	leader				= false;

	SetPrivateCameraView( NULL );

	lastSnapshotSequence	= 0;

	MPAim				= -1;
	lastMPAim			= -1;
	lastMPAimTime		= 0;
	MPAimFadeTime		= 0;
	MPAimHighlight		= false;

	flashlightBattery = flashlight_batteryDrainTimeMS.GetInteger();		// fully charged

	if ( hud ) {
		hud->HandleNamedEvent( "aim_clear" );
	}

	cvarSystem->SetCVarBool( "ui_chat", false );

	SetupPDASlot( true );

	//Dr Beef - Implementation
	//SetupFlashlightHolster();
	//SetupLaserSight();

	// Koz begin

	// model to place hud in 3d space
	memset( &hudEntity, 0, sizeof( hudEntity ) );
	hudEntity.hModel = renderModelManager->FindModel( "/models/mapobjects/hud.lwo" );
	hudEntity.customShader = declManager->FindMaterial( "vr/hud" );
	hudEntity.weaponDepthHack = vr_hudOcclusion.GetBool();

    skinCrosshairDot = declManager->FindSkin( "skins/vr/crosshairDot" );
	skinCrosshairCircleDot = declManager->FindSkin( "skins/vr/crosshairCircleDot" );
	skinCrosshairCross = declManager->FindSkin( "skins/vr/crosshairCross" );

	//GBFix Not in Original
	hudActive = true;
	PDAorigin = vec3_zero;
	PDAaxis = mat3_identity;
	InitTeleportTarget();

	aasState = 0;
	// Koz end
}

/*
==============
idPlayer::Spawn

Prepare any resources used by the player.
==============
*/
void idPlayer::Spawn( void ) {
	idStr		temp;
	idBounds	bounds;

	if ( entityNumber >= MAX_CLIENTS ) {
		gameLocal.Error( "entityNum > MAX_CLIENTS for player.  Player may only be spawned with a client." );
	}

	// allow thinking during cinematics
	cinematic = true;

	if ( gameLocal.isMultiplayer ) {
		// always start in spectating state waiting to be spawned in
		// do this before SetClipModel to get the right bounding box
		spectating = true;
	}

	// set our collision model
	physicsObj.SetSelf( this );
	SetClipModel();
	physicsObj.SetMass( spawnArgs.GetFloat( "mass", "100" ) );
	physicsObj.SetContents( CONTENTS_BODY );
	physicsObj.SetClipMask( MASK_PLAYERSOLID );
	SetPhysics( &physicsObj );

    SetAAS();
	InitAASLocation();

	skin = renderEntity.customSkin;

	// only the local player needs guis
	if ( !gameLocal.isMultiplayer || entityNumber == gameLocal.localClientNum ) {

		// load HUD
		if ( gameLocal.isMultiplayer ) {
			hud = uiManager->FindGui( "guis/mphud.gui", true, false, true );
		} else if ( spawnArgs.GetString( "hud", "", temp ) ) {
			hud = uiManager->FindGui( temp, true, false, true );
		}
		if ( hud ) {
			hud->Activate( true, gameLocal.time );
		}

		// load cursor
		if ( spawnArgs.GetString( "cursor", "", temp ) ) {
			cursor = uiManager->FindGui( temp, true, gameLocal.isMultiplayer, gameLocal.isMultiplayer );
		}
		if ( cursor ) {
			// DG: make it scale to 4:3 so crosshair looks properly round
			//     yes, like so many scaling-related things this is a bit hacky
			//     and note that this is special cased in StateChanged and you
			//     can *not* generally set windowDef properties like this.
			cursor->SetStateBool("scaleto43", true);
			cursor->StateChanged(gameLocal.time); // DG end

			cursor->Activate( true, gameLocal.time );
		}

		objectiveSystem = uiManager->FindGui( "guis/pda.gui", true, false, true );
		objectiveSystemOpen = false;
		objectiveSystemOpenTime = 0;

		itemSystem = fileSystem->RunningPhobos() ? uiManager->FindGui("guis/itemsystem.gui", true, false, true) : NULL;
		itemSystemOpen = false;

		textMessageSystem = fileSystem->RunningPhobos() ? uiManager->FindGui("guis/textmessagesystem.gui", true, false, true) : NULL;
		textMessageSystemOpen = false;

		subtitleSystem = fileSystem->RunningPhobos() ? uiManager->FindGui("guis/subtitles.gui", true, false, true) : NULL;
		subtitleSystemOpen = false;

		cameraGuiSystem = NULL;
		cameraGuiSystemOpen = false;
		cameraGuiCamName = "";
	}

	SetLastHitTime( 0 );

	// load the armor sound feedback
	declManager->FindSound( "player_sounds_hitArmor" );

	// set up conditions for animation
	LinkScriptVariables();

	animator.RemoveOriginOffset( true );

	// initialize user info related settings
	// on server, we wait for the userinfo broadcast, as this controls when the player is initially spawned in game
	if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum ) {
		UserInfoChanged( false );
	}

	// create combat collision hull for exact collision detection
	SetCombatModel();

	// init the damage effects
	playerView.SetPlayerEntity( this );

	// supress model in non-player views, but allow it in mirrors and remote views
	//renderEntity.suppressSurfaceInViewID = entityNumber+1;
    //Carl: don't suppress drawing the player's body in 1st person if we want to see it (in VR)
    renderEntity.suppressSurfaceInViewID = 0;

	// don't project shadow on self or weapon
	renderEntity.noSelfShadow = true;

	idAFAttachment *headEnt = head;
	if ( headEnt ) {
		// supress head in non-player views, but allow it in mirrors and remote views
		headEnt->GetRenderEntity()->suppressSurfaceInViewID = entityNumber+1;
		headEnt->GetRenderEntity()->noSelfShadow = true;
	}

	if ( gameLocal.isMultiplayer ) {
		Init();
		Hide();	// properly hidden if starting as a spectator
		if ( !gameLocal.isClient ) {
			// set yourself ready to spawn. idMultiplayerGame will decide when/if appropriate and call SpawnFromSpawnSpot
			SetupWeaponEntity();
			SpawnFromSpawnSpot();
			forceRespawn = true;
			assert( spectating );
		}
	} else {
		SetupWeaponEntity();
		SpawnFromSpawnSpot();
	}

	// trigger playtesting item gives, if we didn't get here from a previous level
	// the devmap key will be set on the first devmap, but cleared on any level
	// transitions
	if ( !gameLocal.isMultiplayer && gameLocal.serverInfo.FindKey( "devmap" ) ) {
		// fire a trigger with the name "devmap"
		idEntity *ent = gameLocal.FindEntity( "devmap" );
		if ( ent ) {
			ent->ActivateTargets( this );
		}
	}
	if ( hud ) {
		//We can spawn with a full bloodstone, so make sure the hud knows
		if (fileSystem->RunningD3XP()) {
			if (weapon_bloodstone > 0 && (inventory.weapons & (1 << weapon_bloodstone))) {
				hud->HandleNamedEvent("bloodstoneReady");
			}
		} else {
			if ( weapon_soulcube > 0 && ( inventory.weapons & ( 1 << weapon_soulcube ) ) ) {
				int max_souls = inventory.MaxAmmoForAmmoClass( this, "ammo_souls" );
				if ( inventory.ammo[idWeapon::GetAmmoNumForName( "ammo_souls" )] >= max_souls ) {
					hud->HandleNamedEvent( "soulCubeReady" );
				}
			}
		}
		hud->HandleNamedEvent( "itemPickup" );
	}

	if ( GetPDA() ) {
		// Add any emails from the inventory
		for ( int i = 0; i < inventory.emails.Num(); i++ ) {
			GetPDA()->AddEmail( inventory.emails[i] );
		}
		GetPDA()->SetSecurity( common->GetLanguageDict()->GetString( "#str_00066" ) );
	}

	if ( gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) ) {
		hiddenWeapon = true;
        // Carl: dual wielding
        for( int h = 0; h < 2; h++ )
        {
            if( hands[h].weapon )
            {
                if( !game->isVR && pVRClientInfo ) hands[ h ].weapon->LowerWeapon(); // Koz
            }
            hands[ h ].idealWeapon = weapon_fists;
        }
	} else {
		hiddenWeapon = false;
	}

	if ( hud ) {
		UpdateHudWeapon(vr_weaponHand.GetInteger());
		hud->StateChanged( gameLocal.time );
	}

	tipUp = false;
	objectiveUp = false;

	if ( inventory.levelTriggers.Num() ) {
		PostEventMS( &EV_Player_LevelTrigger, 0 );
	}

	inventory.pdaOpened = false;
	inventory.selPDA = 0;

	if ( !gameLocal.isMultiplayer ) {
		if ( g_skill.GetInteger() < 2 ) {
			if ( health < 25 ) {
				health = 25;
			}
			if ( g_useDynamicProtection.GetBool() ) {
				new_g_damageScale = 1.0f;
			}
		} else {
			new_g_damageScale = 1.0f;
			g_armorProtection.SetFloat( ( g_skill.GetInteger() < 2 ) ? 0.4f : 0.2f );

			if ( g_skill.GetInteger() == 3 ) {
				healthTake = true;
				nextHealthTake = gameLocal.time + g_healthTakeTime.GetInteger() * 1000;
			}
		}
	}

    //Setup the weapon toggle lists
    const idKeyValue* kv;
    kv = spawnArgs.MatchPrefix( "weapontoggle", NULL );
    while( kv )
    {
        WeaponToggle_t newToggle;
        strcpy( newToggle.name, kv->GetKey().c_str() );

        idStr toggleData = kv->GetValue();

        idLexer src;
        idToken token;
        src.LoadMemory( toggleData, toggleData.Length(), "toggleData" );
        while( 1 )
        {
            if( !src.ReadToken( &token ) )
            {
                break;
            }
            int index = atoi( token.c_str() );
            newToggle.toggleList.Append( index );

            //Skip the ,
            src.ReadToken( &token );
        }
        newToggle.lastUsed = 0;
        weaponToggles.Set( newToggle.name, newToggle );

        kv = spawnArgs.MatchPrefix( "weapontoggle", kv );
    }

    if (g_skill.GetInteger() >= 3) {
        if (!WeaponAvailable("weapon_bloodstone_passive")) {
           GiveInventoryItem("weapon_bloodstone_passive");
        }

        if (!WeaponAvailable("weapon_bloodstone_active1")) {
            GiveInventoryItem("weapon_bloodstone_active1");
        }

        if (!WeaponAvailable("weapon_bloodstone_active2")) {
            GiveInventoryItem("weapon_bloodstone_active2");
        }

        if (!WeaponAvailable("weapon_bloodstone_active3")) {
            GiveInventoryItem("weapon_bloodstone_active3");
        }
    }

    bloomEnabled			= false;
    bloomSpeed				= 1;
    bloomIntensity			= -0.01f;
    itemSystemCallExit		= "";
    nextTriggerTime			= 0;

    OrientHMDBody();
}

/*
==============
idPlayer::~idPlayer()

Release any resources used by the player.
==============
*/
idPlayer::~idPlayer() {
	FreePDASlot();
	FreeHolsterSlot();

	delete flashlight.GetEntity();
	flashlight = NULL;

	//GBFIX Not in New
	/*delete weapon;
	weapon = NULL;*/
}

/*
==============
idPlayerHand::~idPlayerHand()

Release any resources used by the player's hand.
==============
*/
idPlayerHand::~idPlayerHand()
{
    delete weapon;
    weapon = NULL;
}

/*
==============
idPlayerHand::idPlayerHand
==============
*/
idPlayerHand::idPlayerHand():
        idealWeapon( -1 )
{
    weapon = NULL;
    weaponGone = false;
    playerPdaPos = vec3_zero;
    currentWeapon = -1;
    previousWeapon = -1;
    isTheDuplicate = false;
    weaponSwitchTime = 0;

    laserSightHandle = -1;
    memset( &laserSightRenderEntity, 0, sizeof( laserSightRenderEntity ) );

    crosshairHandle = -1;
    memset( &crosshairEntity, 0, sizeof( crosshairEntity ) );

    lastCrosshairMode = -1;

    throwDirection = vec3_zero;
    throwVelocity = 0.0f;
    frameTime[10] = { 0 };
    position[10] = { vec3_zero };
    frameNum = -1;
    curTime = 0;
    timeDelta = 0;
    startFrameNum = 0;

    grabbingWorld = false;
    triggerDown = false;
    thumbDown = false;
    oldGrabbingWorld = false;
    oldTriggerDown = false;
    oldFlashlightTriggerDown = false;
    oldThumbDown = false;

    // Koz begin
    //laserSightActive = vr_weaponSight.GetInteger() == 0;
    // Koz end

}


/*
===========
idPlayer::Save
===========
*/
void idPlayer::Save( idSaveGame *savefile ) const {
	int i;

	savefile->WriteUsercmd( usercmd );
	playerView.Save( savefile );

	savefile->WriteBool( noclip );
	savefile->WriteBool( godmode );

	// don't save spawnAnglesSet, since we'll have to reset them after loading the savegame
	savefile->WriteAngles( spawnAngles );
	savefile->WriteAngles( viewAngles );
	savefile->WriteAngles( cmdAngles );

	savefile->WriteInt( buttonMask );
	savefile->WriteInt( oldButtons );
	savefile->WriteInt( oldFlags );

	savefile->WriteInt( lastHitTime );
	savefile->WriteInt( lastSndHitTime );
	savefile->WriteInt( lastSavingThrowTime );

	// idBoolFields don't need to be saved, just re-linked in Restore
	inventory.Save( savefile );

    // Carl: todo dual wielding
    hands[vr_weaponHand.GetInteger()].weapon.Save( savefile ); // Carl: Don't make saves incompatible. Note, we are saving the entity number here, not the object
	hands[1 - vr_weaponHand.GetInteger()].weapon.Save( savefile ); // GB: Do makes saves incompatible, because I am just so very tired

    for( int i = 0; i < NUM_QUICK_SLOTS; ++i )
    {
        savefile->WriteInt( quickSlot[ i ] );
    }

	savefile->WriteUserInterface( hud, false );
	savefile->WriteUserInterface( objectiveSystem, false );
	savefile->WriteBool( objectiveSystemOpen );
	if (fileSystem->RunningPhobos()) {
		savefile->WriteInt(objectiveSystemOpenTime);
		savefile->WriteUserInterface(itemSystem, false);
		savefile->WriteBool(itemSystemOpen);
		savefile->WriteUserInterface(textMessageSystem, false);
		savefile->WriteBool(textMessageSystemOpen);
		savefile->WriteUserInterface(subtitleSystem, false);
		savefile->WriteBool(subtitleSystemOpen);
		savefile->WriteUserInterface(cameraGuiSystem, false);
		savefile->WriteBool(cameraGuiSystemOpen);
		savefile->WriteString(cameraGuiCamName);
	}

	savefile->WriteInt( weapon_soulcube );
	savefile->WriteInt( weapon_pda );
	savefile->WriteInt( weapon_fists );
	savefile->WriteInt( weapon_flashlight );
	savefile->WriteInt( weapon_chainsaw );
	savefile->WriteInt( weapon_bloodstone );
	savefile->WriteInt( weapon_bloodstone_active1 );
	savefile->WriteInt( weapon_bloodstone_active2 );
	savefile->WriteInt( weapon_bloodstone_active3 );
	savefile->WriteInt(hudPowerup);
	savefile->WriteInt(lastHudPowerup);
	savefile->WriteInt(hudPowerupDuration);
	// Koz
	savefile->WriteInt( weapon_pistol );
	savefile->WriteInt( weapon_shotgun );
	savefile->WriteInt( weapon_shotgun_double );
	savefile->WriteInt( weapon_machinegun );
	savefile->WriteInt( weapon_chaingun );
	savefile->WriteInt( weapon_handgrenade );
	savefile->WriteInt( weapon_plasmagun );
	savefile->WriteInt( weapon_rocketlauncher );
	savefile->WriteInt( weapon_bfg );
	savefile->WriteInt( weapon_flashlight_new );
	savefile->WriteInt( weapon_grabber );
	// Koz end
    savefile->WriteBool( harvest_lock );
	savefile->WriteInt( heartRate );

	savefile->WriteFloat( heartInfo.GetStartTime() );
	savefile->WriteFloat( heartInfo.GetDuration() );
	savefile->WriteFloat( heartInfo.GetStartValue() );
	savefile->WriteFloat( heartInfo.GetEndValue() );

	savefile->WriteInt( lastHeartAdjust );
	savefile->WriteInt( lastHeartBeat );
	savefile->WriteInt( lastDmgTime );
	savefile->WriteInt( deathClearContentsTime );
	savefile->WriteBool( doingDeathSkin );
	savefile->WriteInt( lastArmorPulse );
	savefile->WriteFloat( stamina );
	savefile->WriteFloat( healthPool );
	savefile->WriteInt( nextHealthPulse );
	savefile->WriteBool( healthPulse );
	savefile->WriteInt( nextHealthTake );
	savefile->WriteBool( healthTake );

	savefile->WriteBool( hiddenWeapon );
	soulCubeProjectile.Save( savefile );

	savefile->WriteInt( spectator );
	savefile->WriteVec3( colorBar );
	savefile->WriteInt( colorBarIndex );
	savefile->WriteBool( scoreBoardOpen );
	savefile->WriteBool( forceScoreBoard );
	savefile->WriteBool( forceRespawn );
	savefile->WriteBool( spectating );
	savefile->WriteInt( lastSpectateTeleport );
	savefile->WriteBool( lastHitToggle );
	savefile->WriteBool( forcedReady );
	savefile->WriteBool( wantSpectate );

	savefile->WriteBool( weaponGone );
	savefile->WriteBool( useInitialSpawns );
	savefile->WriteInt( latchedTeam );
	savefile->WriteInt( tourneyRank );
	savefile->WriteInt( tourneyLine );

	teleportEntity.Save( savefile );
	savefile->WriteInt( teleportKiller );

	savefile->WriteInt( minRespawnTime );
	savefile->WriteInt( maxRespawnTime );

	savefile->WriteVec3( firstPersonViewOrigin );
	savefile->WriteMat3( firstPersonViewAxis );

	// don't bother saving dragEntity since it's a dev tool

	savefile->WriteJoint( hipJoint );
	savefile->WriteJoint( chestJoint );
	savefile->WriteJoint( headJoint );
    // Koz begin
    savefile->WriteJoint( neckJoint );
    savefile->WriteJoint( chestPivotJoint );

    for ( i = 0; i < 2; i++ )
    {
        savefile->WriteJoint( ik_hand[i] );
        savefile->WriteJoint( ik_elbow[i] );
        savefile->WriteJoint( ik_shoulder[i] );
        savefile->WriteJoint( ik_handAttacher[i] );
    }

    for ( i = 0; i < 2; i++ )
    {
        for ( int j = 0; j < 32; j++ )
        {
            savefile->WriteMat3( ik_handCorrectAxis[i][j] );
            savefile->WriteVec3( handWeaponAttachertoWristJointOffset[i][j] );
            savefile->WriteVec3( handWeaponAttacherToDefaultOffset[i][j] );
        }
    }

    savefile->WriteBool( handLowered );
    savefile->WriteBool( handRaised );
    savefile->WriteBool( commonVr->handInGui );
    // Koz end

	savefile->WriteStaticObject( physicsObj );

	savefile->WriteInt( aasLocation.Num() );
	for( i = 0; i < aasLocation.Num(); i++ ) {
		savefile->WriteInt( aasLocation[ i ].areaNum );
		savefile->WriteVec3( aasLocation[ i ].pos );
	}

	savefile->WriteInt( bobFoot );
	savefile->WriteFloat( bobFrac );
	savefile->WriteFloat( bobfracsin );
	savefile->WriteInt( bobCycle );
	savefile->WriteFloat( xyspeed );
	savefile->WriteInt( stepUpTime );
	savefile->WriteFloat( stepUpDelta );
	savefile->WriteFloat( idealLegsYaw );
	savefile->WriteFloat( legsYaw );
	savefile->WriteBool( legsForward );
	savefile->WriteFloat( oldViewYaw );
	savefile->WriteAngles( viewBobAngles );
	savefile->WriteVec3( viewBob );
	savefile->WriteInt( landChange );
	savefile->WriteInt( landTime );

    savefile->WriteInt( hands[ vr_weaponHand.GetInteger() ].currentWeapon );
    savefile->WriteInt( hands[ vr_weaponHand.GetInteger() ].idealWeapon );
    savefile->WriteInt( hands[ vr_weaponHand.GetInteger() ].previousWeapon );
    savefile->WriteInt( hands[ vr_weaponHand.GetInteger() ].weaponSwitchTime );

	savefile->WriteInt( hands[ 1 - vr_weaponHand.GetInteger() ].currentWeapon );
	savefile->WriteInt( hands[ 1 - vr_weaponHand.GetInteger() ].idealWeapon );
	savefile->WriteInt( hands[ 1 - vr_weaponHand.GetInteger() ].previousWeapon );
	savefile->WriteInt( hands[ 1 - vr_weaponHand.GetInteger() ].weaponSwitchTime );

    savefile->WriteBool( weaponEnabled );
	savefile->WriteBool( showWeaponViewModel );

	savefile->WriteSkin( skin );
	savefile->WriteSkin( powerUpSkin );
	savefile->WriteString( baseSkinName );

	savefile->WriteInt( numProjectilesFired );
	savefile->WriteInt( numProjectileHits );

	savefile->WriteBool( airless );
	savefile->WriteInt( airTics );
	savefile->WriteInt( lastAirDamage );

	savefile->WriteBool( gibDeath );
	savefile->WriteBool( gibsLaunched );
	savefile->WriteVec3( gibsDir );

	savefile->WriteFloat( zoomFov.GetStartTime() );
	savefile->WriteFloat( zoomFov.GetDuration() );
	savefile->WriteFloat( zoomFov.GetStartValue() );
	savefile->WriteFloat( zoomFov.GetEndValue() );

	savefile->WriteFloat( centerView.GetStartTime() );
	savefile->WriteFloat( centerView.GetDuration() );
	savefile->WriteFloat( centerView.GetStartValue() );
	savefile->WriteFloat( centerView.GetEndValue() );

	savefile->WriteBool( fxFov );

	savefile->WriteFloat( influenceFov );
	savefile->WriteInt( influenceActive );
	savefile->WriteFloat( influenceRadius );
	savefile->WriteObject( influenceEntity );
	savefile->WriteMaterial( influenceMaterial );
	savefile->WriteSkin( influenceSkin );

	savefile->WriteObject( privateCameraView );

	for( i = 0; i < NUM_LOGGED_VIEW_ANGLES; i++ ) {
		savefile->WriteAngles( loggedViewAngles[ i ] );
	}
	for( i = 0; i < NUM_LOGGED_ACCELS; i++ ) {
		savefile->WriteInt( loggedAccel[ i ].time );
		savefile->WriteVec3( loggedAccel[ i ].dir );
	}
	savefile->WriteInt( currentLoggedAccel );

	savefile->WriteObject( focusGUIent );
	// can't save focusUI
	savefile->WriteObject( focusCharacter );
	if (fileSystem->RunningPhobos()) {
		savefile->WriteObject(focusClickable);
	}
	savefile->WriteInt( talkCursor );
	savefile->WriteInt( focusTime );
	savefile->WriteObject( focusVehicle );
	savefile->WriteUserInterface( cursor, false );

	savefile->WriteInt( oldMouseX );
	savefile->WriteInt( oldMouseY );

	savefile->WriteString( pdaAudio );
	savefile->WriteString( pdaVideo );
	savefile->WriteString( pdaVideoWave );

	savefile->WriteBool( tipUp );
	savefile->WriteBool( objectiveUp );

	savefile->WriteInt( lastDamageDef );
	savefile->WriteVec3( lastDamageDir );
	savefile->WriteInt( lastDamageLocation );
	savefile->WriteInt( smoothedFrame );
	savefile->WriteBool( smoothedOriginUpdated );
	savefile->WriteVec3( smoothedOrigin );
	savefile->WriteAngles( smoothedAngles );

	savefile->WriteBool( ready );
	savefile->WriteBool( respawning );
	savefile->WriteBool( leader );
	savefile->WriteInt( lastSpectateChange );
	savefile->WriteInt( lastTeleFX );

	savefile->WriteFloat( pm_stamina.GetFloat() );

	savefile->WriteInt( weaponToggles.Num() );
    for( i = 0; i < weaponToggles.Num(); i++ )
    {
        WeaponToggle_t* weaponToggle = weaponToggles.GetIndex( i );
        savefile->WriteString( weaponToggle->name );
        savefile->WriteInt( weaponToggle->toggleList.Num() );
        for( int j = 0; j < weaponToggle->toggleList.Num(); j++ )
        {
            savefile->WriteInt( weaponToggle->toggleList[j] );
        }
    }

    savefile->WriteObject(mountedObject);
    enviroSuitLight.Save(savefile);
    savefile->WriteBool(healthRecharge);
    savefile->WriteInt(lastHealthRechargeTime);
    savefile->WriteInt(rechargeSpeed);
    savefile->WriteFloat(new_g_damageScale);

    savefile->WriteBool(bloomEnabled);
    savefile->WriteFloat(bloomSpeed);
    savefile->WriteFloat(bloomIntensity);

    if (fileSystem->RunningPhobos()) {
        savefile->WriteString(itemSystemCallExit);
        savefile->WriteInt(nextTriggerTime);
    }

    savefile->WriteObject( flashlight );
    savefile->WriteInt( flashlightBattery );

    // Koz begin
    savefile->WriteBool( hands[0].laserSightActive || hands[1].laserSightActive );
    savefile->WriteBool( hudActive );

    savefile->WriteInt( commonVr->currentFlashlightMode );
    savefile->WriteSkin( hands[vr_weaponHand.GetInteger()].crosshairEntity.customSkin );

    savefile->WriteBool( hands[0].PDAfixed || hands[1].PDAfixed ); // Carl: Dual wielding, don't make saves incompatible
    savefile->WriteVec3( PDAorigin );
    savefile->WriteMat3( PDAaxis );


    //blech.  Im going to pad the savegame file with a few diff var types,
    // so if more changes are needed in the future, maybe save game compat can be preserved.

    //padded ints have been used now.
    savefile->WriteInt( 666 ); // flag that holster has been saved.
    savefile->WriteInt( holsteredWeapon );
    savefile->WriteInt( extraHolsteredWeapon );
    savefile->WriteInt( (int) holsterModelDefHandle );


    savefile->WriteFloat( 0 );
    savefile->WriteFloat( 0 );
    savefile->WriteFloat( 0 );
    savefile->WriteFloat( 0 );
    savefile->WriteBool( false );
    savefile->WriteBool( false );
    savefile->WriteBool( false );
    savefile->WriteBool( false );
    savefile->WriteVec3( vec3_zero );
    savefile->WriteVec3( vec3_zero );
    savefile->WriteVec3( vec3_zero );
    savefile->WriteVec3( vec3_zero );

    savefile->WriteMat3( holsterAxis );
    savefile->WriteMat3( mat3_identity );
    savefile->WriteMat3( mat3_identity );
    savefile->WriteMat3( mat3_identity );

    // end padding
	//GBFIX
    //savefile->WriteRenderEntity( holsterRenderEntity ); // have to check if this has been saved
    //savefile->WriteString( extraHolsteredWeaponModel );

    // Koz end

	if ( hud ) {
		hud->SetStateString( "message", common->GetLanguageDict()->GetString( "#str_02916" ) );
		hud->HandleNamedEvent( "Message" );
	}
}

/*
===========
idPlayer::Restore
===========
*/
void idPlayer::Restore( idRestoreGame *savefile ) {
	int i;
	int num;
	float set;

	savefile->ReadUsercmd(usercmd);
	playerView.Restore(savefile);

	savefile->ReadBool(noclip);
	savefile->ReadBool(godmode);

	savefile->ReadAngles(spawnAngles);
	savefile->ReadAngles(viewAngles);
	savefile->ReadAngles(cmdAngles);

	memset(usercmd.angles, 0, sizeof(usercmd.angles));
	SetViewAngles(viewAngles);
	spawnAnglesSet = true;

	savefile->ReadInt(buttonMask);
	savefile->ReadInt(oldButtons);
	savefile->ReadInt(oldFlags);

	usercmd.flags = 0;
	oldFlags = 0;

	savefile->ReadInt(lastHitTime);
	savefile->ReadInt(lastSndHitTime);
	savefile->ReadInt(lastSavingThrowTime);

	// Re-link idBoolFields to the scriptObject, values will be restored in scriptObject's restore
	LinkScriptVariables();

	// re-init the hand's laser model (and other things)
    for( int hand = 0; hand < 2; hand++ )
        hands[hand].Init( this, hand );

    inventory.Restore(savefile);
	idEntityPtr<idWeapon> weapon;
	weapon.Restore(savefile);
    // Carl: if it's the PDA, then we saved the PDA hand, otherwise we saved the weapon hand.
    int savedWeaponHand;
    if( weapon && weapon->IdentifyWeapon() == WEAPON_PDA )
        savedWeaponHand = 1 - vr_weaponHand.GetInteger();
    else
        savedWeaponHand = vr_weaponHand.GetInteger();
    hands[savedWeaponHand].weapon = weapon;

	weapon.Restore(savefile);
	hands[1 - savedWeaponHand].weapon = weapon;

    // Carl: other hand's weapon isn't saved yet, so recreate it
    //hands[ 1 - savedWeaponHand ].weapon = static_cast< idWeapon* >( gameLocal.SpawnEntityType( idWeapon::Type, NULL ) );
    //hands[ 1 - savedWeaponHand ].weapon->SetOwner( this, 1 - savedWeaponHand );

	for (i = 0; i < inventory.emails.Num(); i++) {
		GetPDA()->AddEmail(inventory.emails[i]);
	}

	for( int i = 0; i < NUM_QUICK_SLOTS; ++i )
	{
		savefile->ReadInt( quickSlot[ i ] );
	}

	savefile->ReadUserInterface(hud);
	savefile->ReadUserInterface(objectiveSystem);
	savefile->ReadBool(objectiveSystemOpen);
	if (fileSystem->RunningPhobos()) {
		savefile->ReadInt(objectiveSystemOpenTime);
		savefile->ReadUserInterface(itemSystem);
		savefile->ReadBool(itemSystemOpen);
		savefile->ReadUserInterface(textMessageSystem);
		savefile->ReadBool(textMessageSystemOpen);
		savefile->ReadUserInterface(subtitleSystem);
		savefile->ReadBool(subtitleSystemOpen);
		savefile->ReadUserInterface(cameraGuiSystem);
		savefile->ReadBool(cameraGuiSystemOpen);
		savefile->ReadString(cameraGuiCamName);
	}

	weapon_none = 0;
	savefile->ReadInt( weapon_soulcube );
	savefile->ReadInt( weapon_pda );
	savefile->ReadInt( weapon_fists );
	savefile->ReadInt( weapon_flashlight );
	savefile->ReadInt( weapon_chainsaw );
	savefile->ReadInt( weapon_bloodstone );
	savefile->ReadInt( weapon_bloodstone_active1 );
	savefile->ReadInt( weapon_bloodstone_active2 );
	savefile->ReadInt( weapon_bloodstone_active3 );

	if (!oldSaveVersion) {
		savefile->ReadInt(hudPowerup);
		savefile->ReadInt(lastHudPowerup);
		savefile->ReadInt(hudPowerupDuration);
	}

	// Koz
	savefile->ReadInt( weapon_pistol );
	savefile->ReadInt( weapon_shotgun );
	savefile->ReadInt( weapon_shotgun_double );
	savefile->ReadInt( weapon_machinegun );
	savefile->ReadInt( weapon_chaingun );
	savefile->ReadInt( weapon_handgrenade );
	savefile->ReadInt( weapon_plasmagun );
	savefile->ReadInt( weapon_rocketlauncher );
	savefile->ReadInt( weapon_bfg );
	savefile->ReadInt( weapon_flashlight_new );
	savefile->ReadInt( weapon_grabber );
	// Koz end

    savefile->ReadBool( harvest_lock );
    savefile->ReadInt(heartRate);

	savefile->ReadFloat(set);
	heartInfo.SetStartTime(set);
	savefile->ReadFloat(set);
	heartInfo.SetDuration(set);
	savefile->ReadFloat(set);
	heartInfo.SetStartValue(set);
	savefile->ReadFloat(set);
	heartInfo.SetEndValue(set);

	savefile->ReadInt(lastHeartAdjust);
	savefile->ReadInt(lastHeartBeat);
	savefile->ReadInt(lastDmgTime);
	savefile->ReadInt(deathClearContentsTime);
	savefile->ReadBool(doingDeathSkin);
	savefile->ReadInt(lastArmorPulse);
	savefile->ReadFloat(stamina);
	savefile->ReadFloat(healthPool);
	savefile->ReadInt(nextHealthPulse);
	savefile->ReadBool(healthPulse);
	savefile->ReadInt(nextHealthTake);
	savefile->ReadBool(healthTake);

	savefile->ReadBool(hiddenWeapon);
	soulCubeProjectile.Restore(savefile);

	savefile->ReadInt(spectator);
	savefile->ReadVec3(colorBar);
	savefile->ReadInt(colorBarIndex);
	savefile->ReadBool(scoreBoardOpen);
	savefile->ReadBool(forceScoreBoard);
	savefile->ReadBool(forceRespawn);
	savefile->ReadBool(spectating);
	savefile->ReadInt(lastSpectateTeleport);
	savefile->ReadBool(lastHitToggle);
	savefile->ReadBool(forcedReady);
	savefile->ReadBool(wantSpectate);
    // Carl: Don't change save format
    bool gone = false;
    savefile->ReadBool( gone );
    hands[ 0 ].weaponGone = gone;
    hands[ 1 ].weaponGone = gone;
	savefile->ReadBool(useInitialSpawns);
	savefile->ReadInt(latchedTeam);
	savefile->ReadInt(tourneyRank);
	savefile->ReadInt(tourneyLine);

	teleportEntity.Restore(savefile);
	savefile->ReadInt(teleportKiller);

	savefile->ReadInt(minRespawnTime);
	savefile->ReadInt(maxRespawnTime);

	savefile->ReadVec3(firstPersonViewOrigin);
	savefile->ReadMat3(firstPersonViewAxis);

	// don't bother saving dragEntity since it's a dev tool
	dragEntity.Clear();

	savefile->ReadJoint(hipJoint);
	savefile->ReadJoint(chestJoint);
	savefile->ReadJoint(headJoint);
    savefile->ReadJoint( neckJoint );
    savefile->ReadJoint( chestPivotJoint );

    for ( i = 0; i < 2; i++ )
    {
        savefile->ReadJoint( ik_hand[i] );
        savefile->ReadJoint( ik_elbow[i] );
        savefile->ReadJoint( ik_shoulder[i] );
        savefile->ReadJoint( ik_handAttacher[i] );
    }

    for ( i = 0; i < 2; i++ )
    {
        for ( int j = 0; j < 32; j++ )
        {
            savefile->ReadMat3( ik_handCorrectAxis[i][j] );
            savefile->ReadVec3( handWeaponAttachertoWristJointOffset[i][j] );
            savefile->ReadVec3( handWeaponAttacherToDefaultOffset[i][j] );
        }
    }

    savefile->ReadBool( handLowered );
    savefile->ReadBool( handRaised );
    savefile->ReadBool( commonVr->handInGui );

	savefile->ReadStaticObject(physicsObj);
	RestorePhysics(&physicsObj);

	savefile->ReadInt(num);
	aasLocation.SetGranularity(1);
	aasLocation.SetNum(num);
	for (i = 0; i < num; i++) {
		savefile->ReadInt(aasLocation[i].areaNum);
		savefile->ReadVec3(aasLocation[i].pos);
	}

	savefile->ReadInt(bobFoot);
	savefile->ReadFloat(bobFrac);
	savefile->ReadFloat(bobfracsin);
	savefile->ReadInt(bobCycle);
	savefile->ReadFloat(xyspeed);
	savefile->ReadInt(stepUpTime);
	savefile->ReadFloat(stepUpDelta);
	savefile->ReadFloat(idealLegsYaw);
	savefile->ReadFloat(legsYaw);
	savefile->ReadBool(legsForward);
	savefile->ReadFloat(oldViewYaw);
	savefile->ReadAngles(viewBobAngles);
	savefile->ReadVec3(viewBob);
	savefile->ReadInt(landChange);
	savefile->ReadInt(landTime);

    // carl: dual wielding, don't change save format
    savefile->ReadInt( hands[ vr_weaponHand.GetInteger() ].currentWeapon );
    int savedIdealWeapon = -1;
    savefile->ReadInt( savedIdealWeapon );
    hands[ vr_weaponHand.GetInteger() ].idealWeapon = savedIdealWeapon;
    savefile->ReadInt( hands[ vr_weaponHand.GetInteger() ].previousWeapon );
    savefile->ReadInt( hands[ vr_weaponHand.GetInteger() ].weaponSwitchTime );

	// GB Change save format
    savefile->ReadInt( hands[ 1 - vr_weaponHand.GetInteger() ].currentWeapon );
	int savedIdealWeaponOff = -1;
	savefile->ReadInt( savedIdealWeaponOff );
	hands[ 1 - vr_weaponHand.GetInteger() ].idealWeapon = savedIdealWeaponOff;
	savefile->ReadInt( hands[ 1 - vr_weaponHand.GetInteger() ].previousWeapon );
	savefile->ReadInt( hands[ 1 - vr_weaponHand.GetInteger() ].weaponSwitchTime );

	savefile->ReadBool(weaponEnabled);
	risingWeaponHand = -1;

	savefile->ReadBool(showWeaponViewModel);

	savefile->ReadSkin(skin);
	savefile->ReadSkin(powerUpSkin);
	savefile->ReadString(baseSkinName);

	savefile->ReadInt(numProjectilesFired);
	savefile->ReadInt(numProjectileHits);

	savefile->ReadBool(airless);
	savefile->ReadInt(airTics);
	savefile->ReadInt(lastAirDamage);

	savefile->ReadBool(gibDeath);
	savefile->ReadBool(gibsLaunched);
	savefile->ReadVec3(gibsDir);

	savefile->ReadFloat(set);
	zoomFov.SetStartTime(set);
	savefile->ReadFloat(set);
	zoomFov.SetDuration(set);
	savefile->ReadFloat(set);
	zoomFov.SetStartValue(set);
	savefile->ReadFloat(set);
	zoomFov.SetEndValue(set);

	savefile->ReadFloat(set);
	centerView.SetStartTime(set);
	savefile->ReadFloat(set);
	centerView.SetDuration(set);
	savefile->ReadFloat(set);
	centerView.SetStartValue(set);
	savefile->ReadFloat(set);
	centerView.SetEndValue(set);

	savefile->ReadBool(fxFov);

	savefile->ReadFloat(influenceFov);
	savefile->ReadInt(influenceActive);
	savefile->ReadFloat(influenceRadius);
	savefile->ReadObject(reinterpret_cast<idClass *&>( influenceEntity ));
	savefile->ReadMaterial(influenceMaterial);
	savefile->ReadSkin(influenceSkin);

	savefile->ReadObject(reinterpret_cast<idClass *&>( privateCameraView ));

	for (i = 0; i < NUM_LOGGED_VIEW_ANGLES; i++) {
		savefile->ReadAngles(loggedViewAngles[i]);
	}
	for (i = 0; i < NUM_LOGGED_ACCELS; i++) {
		savefile->ReadInt(loggedAccel[i].time);
		savefile->ReadVec3(loggedAccel[i].dir);
	}
	savefile->ReadInt(currentLoggedAccel);

	savefile->ReadObject(reinterpret_cast<idClass *&>( focusGUIent ));
	// can't save focusUI
	focusUI = NULL;
	savefile->ReadObject(reinterpret_cast<idClass *&>( focusCharacter ));
	if (fileSystem->RunningPhobos()) {
		savefile->ReadObject(reinterpret_cast<idClass *&>(focusClickable));
	}
	savefile->ReadInt(talkCursor);
	savefile->ReadInt(focusTime);
	savefile->ReadObject(reinterpret_cast<idClass *&>( focusVehicle ));
	savefile->ReadUserInterface(cursor);

	// DG: make it scale to 4:3 so crosshair looks properly round
	//     yes, like so many scaling-related things this is a bit hacky
	//     and note that this is special cased in StateChanged and you
	//     can *not* generally set windowDef properties like this.
	cursor->SetStateBool("scaleto43", true);
	cursor->StateChanged(gameLocal.time); // DG end

	savefile->ReadInt(oldMouseX);
	savefile->ReadInt(oldMouseY);

	savefile->ReadString(pdaAudio);
	savefile->ReadString(pdaVideo);
	savefile->ReadString(pdaVideoWave);

	savefile->ReadBool(tipUp);
	savefile->ReadBool(objectiveUp);

	savefile->ReadInt(lastDamageDef);
	savefile->ReadVec3(lastDamageDir);
	savefile->ReadInt(lastDamageLocation);
	savefile->ReadInt(smoothedFrame);
	savefile->ReadBool(smoothedOriginUpdated);
	savefile->ReadVec3(smoothedOrigin);
	savefile->ReadAngles(smoothedAngles);

	savefile->ReadBool(ready);
	savefile->ReadBool(respawning);
	savefile->ReadBool(leader);
	savefile->ReadInt(lastSpectateChange);
	savefile->ReadInt(lastTeleFX);

	// set the pm_ cvars
	const idKeyValue *kv;
	kv = spawnArgs.MatchPrefix("pm_", NULL);
	while (kv) {
		cvarSystem->SetCVarString(kv->GetKey(), kv->GetValue());
		kv = spawnArgs.MatchPrefix("pm_", kv);
	}
	savefile->ReadFloat(set);
	pm_stamina.SetFloat(set);

	// create combat collision hull for exact collision detection
	SetCombatModel();

    int weaponToggleCount;
    savefile->ReadInt( weaponToggleCount );
    for( i = 0; i < weaponToggleCount; i++ )
    {
        WeaponToggle_t newToggle;
        memset( &newToggle, 0, sizeof( newToggle ) );

        idStr name;
        savefile->ReadString( name );
        strcpy( newToggle.name, name.c_str() );

        int indexCount;
        savefile->ReadInt( indexCount );
        for( int j = 0; j < indexCount; j++ )
        {
            int temp;
            savefile->ReadInt( temp );
            newToggle.toggleList.Append( temp );
        }
        newToggle.lastUsed = 0;
        weaponToggles.Set( newToggle.name, newToggle );
    }

    if (!oldSaveVersion) {
        savefile->ReadObject(reinterpret_cast<idClass * &>(mountedObject));
        enviroSuitLight.Restore(savefile);
        savefile->ReadBool(healthRecharge);
        savefile->ReadInt(lastHealthRechargeTime);
        savefile->ReadInt(rechargeSpeed);
        savefile->ReadFloat(new_g_damageScale);

        savefile->ReadBool(bloomEnabled);
        savefile->ReadFloat(bloomSpeed);
        savefile->ReadFloat(bloomIntensity);
    }

    if (fileSystem->RunningPhobos()) {
        savefile->ReadString(itemSystemCallExit);
        savefile->ReadInt(nextTriggerTime);

        // DG: workaround for lingering messages that are shown forever after loading a savegame
        //     (one way to get them is saving again, while the message from first save is still
        //      shown, and then load)
        if (hud) {
            hud->SetStateString("message", "");
        }
    }

    // flashlight
    idWeapon* tempWeapon;
    savefile->ReadObject( reinterpret_cast<idClass*&>( tempWeapon ) );
    tempWeapon->SetIsPlayerFlashlight( true );
    flashlight = tempWeapon;
    savefile->ReadInt( flashlightBattery );

    holsteredWeapon = weapon_fists;
    extraHolsteredWeapon = weapon_fists;
    extraHolsteredWeaponModel = NULL;
    //GB
    velocityPunched = false;


    //------------------------------------------------------------
    // Koz begin - VR specific initialization

    // Koz fixme ovr_RecenterTrackingOrigin( commonVr->hmdSession ); // Koz reset hmd orientation  Koz fixme check if still appropriate here.

    // make sure the clipmodels for the body and head are re-initialized.
    SetClipModel();

    // re-init the player render model if we're loading this savegame from a different mod
    memset(&renderEntity, 0, sizeof(renderEntity));
    renderEntity.numJoints = animator.NumJoints();
    animator.GetJoints(&renderEntity.numJoints, &renderEntity.joints);
    renderEntity.hModel = animator.ModelHandle();
    if (renderEntity.hModel)
    {
        //renderEntity.hModel->Reset();

        renderEntity.hModel->InitFromFile( renderEntity.hModel->Name() );
        animator.ClearAllJoints();

        renderEntity.bounds = renderEntity.hModel->Bounds(&renderEntity);
    }
    renderEntity.shaderParms[SHADERPARM_RED] = 1.0f;
    renderEntity.shaderParms[SHADERPARM_GREEN] = 1.0f;
    renderEntity.shaderParms[SHADERPARM_BLUE] = 1.0f;
    renderEntity.shaderParms[3] = 1.0f;
    renderEntity.shaderParms[SHADERPARM_TIMEOFFSET] = 0.0f;
    renderEntity.shaderParms[5] = 0.0f;
    renderEntity.shaderParms[6] = 0.0f;
    renderEntity.shaderParms[7] = 0.0f;

    // Koz re-init the player model bones. Changes to player model require this to allow compatability with older savegames.
    InitPlayerBones();

    //re-init the VR ui models
    hudHandle = -1;

    // re-init hud model
    memset( &hudEntity, 0, sizeof( hudEntity ) );
    hudEntity.hModel = renderModelManager->FindModel( "/models/mapobjects/hud.lwo" );
    hudEntity.customShader = declManager->FindMaterial( "vr/hud" );
    hudEntity.weaponDepthHack = vr_hudOcclusion.GetBool();

    skinCrosshairDot = declManager->FindSkin( "skins/vr/crosshairDot" );
    skinCrosshairCircleDot = declManager->FindSkin( "skins/vr/crosshairCircleDot" );
    skinCrosshairCross = declManager->FindSkin( "skins/vr/crosshairCross" );
    skinTelepadCrouch = declManager->FindSkin( "skins/vr/padcrouch" );

    const idDeclSkin* blag;
    // Koz begin

    bool laserSightActive;
    savefile->ReadBool( laserSightActive );
    hands[ 0 ].laserSightActive = laserSightActive;
    hands[ 1 ].laserSightActive = laserSightActive;
    savefile->ReadBool( hudActive );

    savefile->ReadInt( commonVr->currentFlashlightMode );
    //	savefile->ReadSkin( hands[0].crosshairEntity.customSkin );
    savefile->ReadSkin( blag );

    bool PDAfixed = false;
    savefile->ReadBool( PDAfixed );
    hands[ 1 - vr_weaponHand.GetInteger() ].PDAfixed = PDAfixed;
    hands[ vr_weaponHand.GetInteger() ].PDAfixed = false;
    savefile->ReadVec3( PDAorigin );
    savefile->ReadMat3( PDAaxis );

    int tempInt;
    float tempFloat;
    bool tempBool;
    idVec3 tempVec3;
    idMat3 tempMat3;

    int holsterFlag;

    //blech.  Im going to pad the savegame file with a few diff var types,
    // so if more changes are needed in the future, maybe save game compat can be preserved.

    //ints used saving weapon holsters.
    savefile->ReadInt( holsterFlag );
    if ( holsterFlag == 666 )
    {
        savefile->ReadInt( holsteredWeapon );
        savefile->ReadInt( extraHolsteredWeapon );
        savefile->ReadInt( tempInt );
        holsterModelDefHandle = tempInt;
    }
    else
    {
        savefile->ReadInt( tempInt );
        savefile->ReadInt( tempInt );
        savefile->ReadInt( tempInt );
    }

    savefile->ReadFloat( tempFloat );
    savefile->ReadFloat( tempFloat );
    savefile->ReadFloat( tempFloat );
    savefile->ReadFloat( tempFloat );
    savefile->ReadBool( tempBool );
    savefile->ReadBool( tempBool );
    savefile->ReadBool( tempBool );
    savefile->ReadBool( tempBool );
    savefile->ReadVec3( tempVec3 );
    savefile->ReadVec3( tempVec3 );
    savefile->ReadVec3( tempVec3 );
    savefile->ReadVec3( tempVec3 );

    savefile->ReadMat3( tempMat3 );
    if ( holsterFlag == 666 ) holsterAxis = tempMat3;

    savefile->ReadMat3( tempMat3 );
    savefile->ReadMat3( tempMat3 );
    savefile->ReadMat3( tempMat3 );

    /*if ( holsterFlag == 666 )
    {

        idStr	ehwm;
        savefile->ReadRenderEntity( holsterRenderEntity );
        savefile->ReadString( ehwm );
        extraHolsteredWeaponModel = ehwm.c_str();

        if ( extraHolsteredWeapon != weapon_fists )
        {

            /*
            If the game was autosaved, the holster and holster model will be correct,
            but if the game was saved through the pause menu, the active weapon was pushed to the holster,
            and the holstered weapon was pushed to extraHolsteredWeapon. This is the only time extraholstered
            will not hold weapon_fists.

            Check if the holstered weapon was pushed to the extraholster, and switch it back on load.


            holsteredWeapon = extraHolsteredWeapon;
            extraHolsteredWeapon = weapon_fists;
            //common->Printf( "Loading holster model %s\n", extraHolsteredWeaponModel );
            holsterRenderEntity.hModel = renderModelManager->FindModel( extraHolsteredWeaponModel );

            if ( strcmp( extraHolsteredWeaponModel, "models/weapons/pistol/w_pistol.lwo" ) == 0 )
            {
                holsterAxis = idAngles( 90, 0, 0 ).ToMat3() * 0.75f;
            }
            else if ( strcmp( extraHolsteredWeaponModel, "models/weapons/shotgun/w_shotgun2.lwo" ) == 0 ||
                      strcmp( extraHolsteredWeaponModel, "models/weapons/bfg/bfg_world.lwo" ) == 0 )
            {
                holsterAxis = idAngles( 0, -90, -90 ).ToMat3();
            }
            else if ( strcmp( extraHolsteredWeaponModel, "models/weapons/machinegun/w_machinegun.lwo" ) == 0 )
            {
                holsterAxis = idAngles( 0, 90, 90 ).ToMat3() * 0.75f;
            }
            else if ( strcmp( extraHolsteredWeaponModel, "models/weapons/plasmagun/plasmagun_world.lwo" ) == 0 )
            {
                holsterAxis = idAngles( 0, 90, 90 ).ToMat3() * 0.75f;
            }
            else if ( strcmp( extraHolsteredWeaponModel, "models/weapons/chainsaw/w_chainsaw.lwo" ) == 0 )
            {
                holsterAxis = idAngles( 0, 90, 90 ).ToMat3() * 0.9f;
            }
            else if ( strcmp( extraHolsteredWeaponModel, "models/weapons/chaingun/w_chaingun.lwo" ) == 0 )
            {
                holsterAxis = idAngles( 0, 90, 90 ).ToMat3() * 0.9f;
            }
            else
            {
                holsterAxis = idAngles( 0, 90, 90 ).ToMat3();
            }
        }
    }*/
    armIK.Init( this, IK_ANIM, modelOffset );

    if( vr_debugHands.GetBool() )
    {
        common->Printf( "\nRestored Hands:\n" );
        hands[HAND_LEFT].debugPrint();
        hands[HAND_RIGHT].debugPrint();
    }

    vr_weaponSight.SetModified(); // make sure these get initialized properly
    aasState = 0;

    // Koz end



	// DG: workaround for lingering messages that are shown forever after loading a savegame
	//     (one way to get them is saving again, while the message from first save is still
	//      shown, and then load)
	if (hud) {
		hud->SetStateString("message", "");
	}

	//Have to do this for loaded games
	//GB - I think this is Dr Beef Code
	//weapon_flashlight = SlotForWeapon("weapon_flashlight");
	//SetupFlashlightHolster();
	//SetupLaserSight();
}

void idPlayer::SetupLaserSight()
{
	laserSightHandle = -1;
	memset( &laserSightRenderEntity, 0, sizeof( laserSightRenderEntity ) );
	laserSightRenderEntity.hModel = renderModelManager->FindModel( "_BEAM" );
	laserSightRenderEntity.customShader = declManager->FindMaterial( "_white" );
	laserSightRenderEntity.shaderParms[ SHADERPARM_RED ]	= 1.0f;
	laserSightRenderEntity.shaderParms[ SHADERPARM_GREEN ] = 0.0f;
	laserSightRenderEntity.shaderParms[ SHADERPARM_BLUE ]	= 0.0f;
	laserSightRenderEntity.shaderParms[ SHADERPARM_ALPHA ] = 0.4f;
	laserSightRenderEntity.shaderParms[ SHADERPARM_TIMEOFFSET ] = 0.0f;
	laserSightRenderEntity.shaderParms[5] = 0.0f;
	laserSightRenderEntity.shaderParms[6] = 0.0f;
	laserSightRenderEntity.shaderParms[7] = 0.0f;

}

/*
===============
idPlayer::PrepareForRestart
================
*/
void idPlayer::PrepareForRestart( void ) {
	ClearPowerUps();
	Spectate( true );
	forceRespawn = true;

	// we will be restarting program, clear the client entities from program-related things first
	ShutdownThreads();

	// the sound world is going to be cleared, don't keep references to emitters
	FreeSoundEmitter( false );
}

/*
===============
idPlayer::Restart
================
*/
void idPlayer::Restart( void ) {
	idActor::Restart();

	// client needs to setup the animation script object again
	if ( gameLocal.isClient ) {
        // Make sure the weapon spawnId gets re-linked on the next snapshot.
        // Otherwise, its owner might not be set after the map restart, which causes asserts and crashes.
        for( int h = 0; h < 2; h++ )
            hands[ h ].weapon = NULL;
        flashlight = NULL;
        Init();
	} else {
		// choose a random spot and prepare the point of view in case player is left spectating
		assert( spectating );
		SpawnFromSpawnSpot();
	}

	useInitialSpawns = true;
	UpdateSkinSetup();
}

/*
===============
idPlayer::ServerSpectate
================
*/
void idPlayer::ServerSpectate( bool spectate ) {
	assert( !gameLocal.isClient );

	if ( spectating != spectate ) {
		Spectate( spectate );
		if ( spectate ) {
			SetSpectateOrigin();
		} else {
			if ( gameLocal.gameType == GAME_DM ) {
				// make sure the scores are reset so you can't exploit by spectating and entering the game back
				// other game types don't matter, as you either can't join back, or it's team scores
				gameLocal.mpGame.ClearFrags( entityNumber );
			}
		}
	}
	if ( !spectate ) {
		SpawnFromSpawnSpot();
	}
}

/*
===========
idPlayer::SelectInitialSpawnPoint

Try to find a spawn point marked 'initial', otherwise
use normal spawn selection.
============
*/
void idPlayer::SelectInitialSpawnPoint( idVec3 &origin, idAngles &angles ) {
	idEntity *spot;
	idStr skin;

	spot = gameLocal.SelectInitialSpawnPoint( this );

	// set the player skin from the spawn location
	if ( spot->spawnArgs.GetString( "skin", NULL, skin ) ) {
		spawnArgs.Set( "spawn_skin", skin );
	}

	// activate the spawn locations targets
	spot->PostEventMS( &EV_ActivateTargets, 0, this );

	origin = spot->GetPhysics()->GetOrigin();
	origin[2] += 4.0f + CM_BOX_EPSILON;		// move up to make sure the player is at least an epsilon above the floor
	angles = spot->GetPhysics()->GetAxis().ToAngles();
}

/*
===========
idPlayer::SpawnFromSpawnSpot

Chooses a spawn location and spawns the player
============
*/
void idPlayer::SpawnFromSpawnSpot( void ) {
	idVec3		spawn_origin;
	idAngles	spawn_angles;

	SelectInitialSpawnPoint( spawn_origin, spawn_angles );
	SpawnToPoint( spawn_origin, spawn_angles );
}

/*
===========
idPlayer::SpawnToPoint

Called every time a client is placed fresh in the world:
after the first ClientBegin, and after each respawn
Initializes all non-persistant parts of playerState

when called here with spectating set to true, just place yourself and init
============
*/
void idPlayer::SpawnToPoint( const idVec3 &spawn_origin, const idAngles &spawn_angles ) {
	idVec3 spec_origin;

	assert( !gameLocal.isClient );

	respawning = true;

	Init();

	fl.noknockback = false;

	// stop any ragdolls being used
	StopRagdoll();

	// set back the player physics
	SetPhysics( &physicsObj );

	physicsObj.SetClipModelAxis();
	physicsObj.EnableClip();

	if ( !spectating ) {
		SetCombatContents( true );
	}

	physicsObj.SetLinearVelocity( vec3_origin );

	// setup our initial view
	if ( !spectating ) {
		SetOrigin( spawn_origin );
	} else {
		spec_origin = spawn_origin;
		spec_origin[ 2 ] += pm_normalheight.GetFloat();
		spec_origin[ 2 ] += SPECTATE_RAISE;
		SetOrigin( spec_origin );
	}

	// if this is the first spawn of the map, we don't have a usercmd yet,
	// so the delta angles won't be correct.  This will be fixed on the first think.
	viewAngles = ang_zero;
	SetDeltaViewAngles( ang_zero );
	SetViewAngles( spawn_angles );
	spawnAngles = spawn_angles;
	spawnAnglesSet = false;

	legsForward = true;
	legsYaw = 0.0f;
	idealLegsYaw = 0.0f;
	oldViewYaw = viewAngles.yaw;

	if ( spectating ) {
		Hide();
	} else {
		Show();
	}

	if ( gameLocal.isMultiplayer ) {
		if ( !spectating ) {
			// we may be called twice in a row in some situations. avoid a double fx and 'fly to the roof'
			if ( lastTeleFX < gameLocal.time - 1000 ) {
				idEntityFx::StartFx( spawnArgs.GetString( "fx_spawn" ), &spawn_origin, NULL, this, true );
				lastTeleFX = gameLocal.time;
			}
		}
		AI_TELEPORT = true;
	} else {
		AI_TELEPORT = false;
	}

	// kill anything at the new position
	if ( !spectating ) {
		physicsObj.SetClipMask( MASK_PLAYERSOLID ); // the clip mask is usually maintained in Move(), but KillBox requires it
		gameLocal.KillBox( this );
	}

	// don't allow full run speed for a bit
	physicsObj.SetKnockBack( 100 );

	// set our respawn time and buttons so that if we're killed we don't respawn immediately
	minRespawnTime = gameLocal.time;
	maxRespawnTime = gameLocal.time;
	if ( !spectating ) {
		forceRespawn = false;
	}

	privateCameraView = NULL;

	BecomeActive( TH_THINK );

	// run a client frame to drop exactly to the floor,
	// initialize animations and other things
	Think();

	respawning			= false;
	lastManOver			= false;
	lastManPlayAgain	= false;
	isTelefragged		= false;
}

/*
===============
idPlayer::SavePersistantInfo

Saves any inventory and player stats when changing levels.
===============
*/
void idPlayer::SavePersistantInfo( void ) {
	idDict &playerInfo = gameLocal.persistentPlayerInfo[entityNumber];

	playerInfo.Clear();
	inventory.GetPersistantData( playerInfo );
	playerInfo.SetInt( "health", health );
    playerInfo.SetInt( "current_weapon", hands[ GetBestWeaponHand() ].currentWeapon );
    // Koz begin
    playerInfo.SetBool( "laserSightActive", hands[0].laserSightActive || hands[1].laserSightActive ); // Carl: don't change save format
    playerInfo.SetBool( "hudActive", hudActive );
    playerInfo.SetInt( "currentFlashMode", commonVr->currentFlashlightMode );

    playerInfo.SetInt( "holsteredWeapon", holsteredWeapon );
    playerInfo.SetInt( "extraHolsteredWeapon", extraHolsteredWeapon );
    if ( holsterRenderEntity.hModel )
    {
        playerInfo.Set( "holsteredWeaponModel", holsterRenderEntity.hModel->Name() );
    }
    playerInfo.SetMatrix( "holsterAxis", holsterAxis );
    // Koz end
}

/*
===============
idPlayer::RestorePersistantInfo

Restores any inventory and player stats when changing levels.
===============
*/
void idPlayer::RestorePersistantInfo( void ) {
	if ( gameLocal.isMultiplayer ) {
		gameLocal.persistentPlayerInfo[entityNumber].Clear();
	}

	spawnArgs.Copy( gameLocal.persistentPlayerInfo[entityNumber] );

	inventory.RestoreInventory( this, spawnArgs );
	health = spawnArgs.GetInt( "health", "100" );
    hands[vr_weaponHand.GetInteger()].idealWeapon = spawnArgs.GetInt( "current_weapon", "1" );
    hands[ 1 - vr_weaponHand.GetInteger() ].idealWeapon = weapon_fists;
    // Koz begin
    bool laserSightActive = spawnArgs.GetBool( "laserSightActive", "1" );
    hands[ 0 ].laserSightActive = laserSightActive;
    hands[ 1 ].laserSightActive = laserSightActive;
    commonVr->currentFlashlightMode = spawnArgs.GetInt( "currentFlashMode", "3" );
    hudActive = spawnArgs.GetBool( "hudActive", "1" );

    holsteredWeapon = spawnArgs.GetInt( "holsteredWeapon", "-1" );
    extraHolsteredWeapon = spawnArgs.GetInt( "extraHolsteredWeapon", "-1" );

    idStr hwm;

    //playerInfo.Get( "holsteredWeaponModel", holsterRenderEntity.hModel->Name() );
    hwm = spawnArgs.GetString( "holsteredWeaponModel", "" );
    holsterAxis = spawnArgs.GetMatrix( "holsterAxis", "" );


    holsterRenderEntity.hModel = renderModelManager->FindModel( hwm.c_str() );


    common->Printf( "Restored holsteredWeapon %d\n", holsteredWeapon );
    common->Printf( "Restored extraHolsteredWeapon %d\n", extraHolsteredWeapon );


    //playerInfo.SetInt( "holsteredWeapon", holsteredWeapon );
    //playerInfo.SetInt( "extraHolsteredWeapon", extraHolsteredWeapon );
    //playerInfo.Set( "extraHolsteredWeaponModel", extraHolsteredWeaponModel );
    // Koz end
}

/*
================
idPlayer::GetUserInfo
================
*/
idDict *idPlayer::GetUserInfo( void ) {
	return &gameLocal.userInfo[ entityNumber ];
}

/*
==============
idPlayer::UpdateSkinSetup
==============
*/
/*
==============
idPlayer::UpdateSkinSetup
==============
*/
void idPlayer::UpdateSkinSetup()
{
    const char* handsOnly = "/vrHandsOnly";
    const char* weaponOnly = "/vrWeaponsOnly";
    const char* body = "/vrBody";

    gameExpansionType_t gameType;

    if ( game->isVR )
    {
        idStr skinN = skin->GetName();
        //GB Force skin
        //idStr skinN = "skins/characters/player/greenmarine_arm2";

        if ( strstr( skinN.c_str(), "skins/characters/player/tshirt_mp" ) )
        {
            skinN = "skins/characters/player/tshirt_mp";
        }
        else if ( strstr( skinN.c_str(), "skins/characters/player/greenmarine_arm2" ) )
        {
            skinN = "skins/characters/player/greenmarine_arm2";
        }
        else if ( strstr( skinN.c_str(), "skins/characters/player/d3xp_sp_vrik" ) )
        {
            skinN = "skins/characters/player/d3xp_sp_vrik";
        }
        else
        {
            gameType = GetExpansionType();

            if ( gameType == GAME_D3XP || gameType == GAME_D3LE )
            {
                skinN = "skins/characters/player/d3xp_sp_vrik";
            }
            else
            {
                skinN = "skins/characters/player/greenmarine_arm2";
            }
        }

        if ( commonVr->thirdPersonMovement )
        {
            skinN += body;
        }
        else if ( vr_playerBodyMode.GetInteger() == 1 || ( vr_playerBodyMode.GetInteger() == 2 && (hands[ 0 ].currentWeapon == weapon_fists || hands[ 1 ].currentWeapon == weapon_fists || commonVr->handInGui) ) )
        {
            skinN += handsOnly;
        }
        else if ( vr_playerBodyMode.GetInteger() == 2 )
        {
            skinN += weaponOnly;
        }
        else
        {
            // if crouched more than 16 inches hide the body if enabled.
            if ( (commonVr->headHeightDiff < -16.0f || IsCrouching()) && vr_crouchHideBody.GetBool() )
            {
                skinN += handsOnly;
            }
            else
            {
                skinN += body;
            }
        }

        skin = declManager->FindSkin( skinN.c_str(), false );
        //	common->Printf( "UpdateSkinSetup returning player skin %s\n", skinN.c_str() );
        return;
    }


    if ( !gameLocal.isMultiplayer )
    {
        return;
    }

    /*if( gameLocal.mpGame.IsGametypeTeamBased() )    // CTF
    {
        skinIndex = team + 1;
    }
    else
    {
        // Each player will now have their Skin Index Reflect their entity number  ( host = 0, client 1 = 1, client 2 = 2 etc )
        skinIndex = entityNumber; // session->GetActingGameStateLobbyBase().GetLobbyUserSkinIndex( gameLocal.lobbyUserIDs[entityNumber] );
    }
    const char* baseSkinName = gameLocal.mpGame.GetSkinName( skinIndex );
    skin = declManager->FindSkin( baseSkinName, false );
    if( PowerUpActive( BERSERK ) )
    {
        idStr powerSkinName = baseSkinName;
        powerSkinName.Append( "_berserk" );
        powerUpSkin = declManager->FindSkin( powerSkinName );
    }
    else if( PowerUpActive( INVULNERABILITY ) )
    {
        idStr powerSkinName = baseSkinName;
        powerSkinName.Append( "_invuln" );
        powerUpSkin = declManager->FindSkin( powerSkinName );
    }
    else if( PowerUpActive( INVISIBILITY ) )
    {
        const char* invisibleSkin = "";
        spawnArgs.GetString( "skin_invisibility", "", &invisibleSkin );
        powerUpSkin = declManager->FindSkin( invisibleSkin );
    }*/
}

/*
==============
idPlayer::BalanceTDM
==============
*/
bool idPlayer::BalanceTDM( void ) {
	int			i, balanceTeam, teamCount[2];
	idEntity	*ent;

	teamCount[ 0 ] = teamCount[ 1 ] = 0;
	for( i = 0; i < gameLocal.numClients; i++ ) {
		ent = gameLocal.entities[ i ];
		if ( ent && ent->IsType( idPlayer::Type ) ) {
			teamCount[ static_cast< idPlayer * >( ent )->team ]++;
		}
	}
	balanceTeam = -1;
	if ( teamCount[ 0 ] < teamCount[ 1 ] ) {
		balanceTeam = 0;
	} else if ( teamCount[ 0 ] > teamCount[ 1 ] ) {
		balanceTeam = 1;
	}
	if ( balanceTeam != -1 && team != balanceTeam ) {
		common->DPrintf( "team balance: forcing player %d to %s team\n", entityNumber, balanceTeam ? "blue" : "red" );
		team = balanceTeam;
		GetUserInfo()->Set( "ui_team", team ? "Blue" : "Red" );
		return true;
	}
	return false;
}

/*
==============
idPlayer::UserInfoChanged
==============
*/
bool idPlayer::UserInfoChanged( bool canModify ) {
	idDict	*userInfo;
	bool	modifiedInfo;
	bool	spec;
	bool	newready;

	userInfo = GetUserInfo();
	showWeaponViewModel = userInfo->GetBool( "ui_showGun" );

	if ( !gameLocal.isMultiplayer ) {
		return false;
	}

	modifiedInfo = false;

	spec = ( idStr::Icmp( userInfo->GetString( "ui_spectate" ), "Spectate" ) == 0 );
	if ( gameLocal.serverInfo.GetBool( "si_spectators" ) ) {
		// never let spectators go back to game while sudden death is on
		if ( canModify && gameLocal.mpGame.GetGameState() == idMultiplayerGame::SUDDENDEATH && !spec && wantSpectate == true ) {
			userInfo->Set( "ui_spectate", "Spectate" );
			modifiedInfo |= true;
		} else {
			if ( spec != wantSpectate && !spec ) {
				// returning from spectate, set forceRespawn so we don't get stuck in spectate forever
				forceRespawn = true;
			}
			wantSpectate = spec;
		}
	} else {
		if ( canModify && spec ) {
			userInfo->Set( "ui_spectate", "Play" );
			modifiedInfo |= true;
		} else if ( spectating ) {
			// allow player to leaving spectator mode if they were in it when si_spectators got turned off
			forceRespawn = true;
		}
		wantSpectate = false;
	}

	newready = ( idStr::Icmp( userInfo->GetString( "ui_ready" ), "Ready" ) == 0 );
	if ( ready != newready && gameLocal.mpGame.GetGameState() == idMultiplayerGame::WARMUP && !wantSpectate ) {
		gameLocal.mpGame.AddChatLine( common->GetLanguageDict()->GetString( "#str_07180" ), userInfo->GetString( "ui_name" ), newready ? common->GetLanguageDict()->GetString( "#str_04300" ) : common->GetLanguageDict()->GetString( "#str_04301" ) );
	}
	ready = newready;
	team = ( idStr::Icmp( userInfo->GetString( "ui_team" ), "Blue" ) == 0 );
	// server maintains TDM balance
	if ( canModify && gameLocal.gameType == GAME_TDM && !gameLocal.mpGame.IsInGame( entityNumber ) && g_balanceTDM.GetBool() ) {
		modifiedInfo |= BalanceTDM( );
	}
	UpdateSkinSetup( );

	isChatting = userInfo->GetBool( "ui_chat", "0" );
	if ( canModify && isChatting && AI_DEAD ) {
		// if dead, always force chat icon off.
		isChatting = false;
		userInfo->SetBool( "ui_chat", false );
		modifiedInfo |= true;
	}

	return modifiedInfo;
}

/*
===============
idPlayer::UpdateHudAmmo
===============
*/
void idPlayer::UpdateHudAmmo( idUserInterface *_hud, int hand ) {
	int inclip;
	int ammoamount;

	assert( hands[hand].weapon );
	assert( _hud );

	inclip		= hands[hand].weapon->AmmoInClip();
	ammoamount	= hands[hand].weapon->AmmoAvailable();
	//Hack to stop the bloodstone ammo to display when it is being activated
	if (ammoamount < 0 || !hands[hand].weapon->IsReady() || hands[hand].currentWeapon == weapon_bloodstone) {
		// show infinite ammo
		_hud->SetStateString( "player_ammo", "" );
		_hud->SetStateString( "player_totalammo", "" );
	} else {
		// show remaining ammo
		_hud->SetStateString("player_totalammo", va("%i", ammoamount));
		_hud->SetStateString( "player_ammo", hands[hand].weapon->ClipSize() ? va( "%i", inclip ) : "--" );		// how much in the current clip
		_hud->SetStateString( "player_clips", hands[hand].weapon->ClipSize() ? va( "%i", ammoamount / hands[hand].weapon->ClipSize() ) : "--" );
		_hud->SetStateString("player_allammo", va("%i/%i", inclip, ammoamount));
	}

	_hud->SetStateBool( "player_ammo_empty", ( ammoamount == 0 ) );
	_hud->SetStateBool( "player_clip_empty", ( hands[hand].weapon->ClipSize() ? inclip == 0 : false ) );
	_hud->SetStateBool( "player_clip_low", ( hands[hand].weapon->ClipSize() ? inclip <= hands[hand].weapon->LowAmmo() : false ) );

	//Hack to stop the bloodstone ammo to display when it is being activated
	if (hands[hand].currentWeapon == weapon_bloodstone) {
		_hud->SetStateBool("player_ammo_empty", false);
		_hud->SetStateBool("player_clip_empty", false);
		_hud->SetStateBool("player_clip_low", false);
	}

	//Let the HUD know the total amount of ammo regardless of the ammo required value
	_hud->SetStateString("player_ammo_count", va("%i", hands[hand].weapon->AmmoCount()));

	//Make sure the hud always knows how many bloodstone charges there are
	int ammoRequired;
	ammo_t ammo_i = inventory.AmmoIndexForWeaponClass("weapon_bloodstone_passive", &ammoRequired);
	int bloodstoneAmmo = inventory.HasAmmo(ammo_i, ammoRequired);
	_hud->SetStateString("player_bloodstone_ammo", va("%i", bloodstoneAmmo));
	_hud->HandleNamedEvent("bloodstoneAmmoUpdate");

	if (fileSystem->RunningPhobos()) {
		idWeapon* weapon = GetWeaponInHand( vr_weaponHand.GetInteger() );
		_hud->SetStateString("player_clip_size", va("%i", weapon->ClipSize() ? weapon->ClipSize() : 1));
	}

	_hud->HandleNamedEvent( "updateAmmo" );
}

/*
===============
idPlayer::UpdateHudStats
===============
*/
void idPlayer::UpdateHudStats( idUserInterface *_hud ) {
	int staminapercentage;
	float max_stamina;

	assert( _hud );

	max_stamina = pm_stamina.GetFloat();
	if ( !max_stamina ) {
		// stamina disabled, so show full stamina bar
		staminapercentage = 100.0f;
	} else {
		staminapercentage = idMath::FtoiFast( 100.0f * stamina / max_stamina );
	}

	_hud->SetStateInt( "player_health", health );
	_hud->SetStateInt( "player_stamina", staminapercentage );
	_hud->SetStateInt( "player_armor", inventory.armor );
	_hud->SetStateInt( "player_hr", heartRate );
	_hud->SetStateInt( "player_nostamina", ( max_stamina == 0 ) ? 1 : 0 );

	_hud->HandleNamedEvent( "updateArmorHealthAir" );

	_hud->HandleNamedEvent("updatePowerup");

	if ( healthPulse ) {
		_hud->HandleNamedEvent( "healthPulse" );
		StartSound( "snd_healthpulse", SND_CHANNEL_ITEM, 0, false, NULL );
		healthPulse = false;
	}

	if ( healthTake ) {
		_hud->HandleNamedEvent( "healthPulse" );
		StartSound( "snd_healthtake", SND_CHANNEL_ITEM, 0, false, NULL );
		healthTake = false;
	}

	if ( inventory.ammoPulse ) {
		_hud->HandleNamedEvent( "ammoPulse" );
		inventory.ammoPulse = false;
	}
	if ( inventory.weaponPulse ) {
		// We need to update the weapon hud manually, but not
		// the armor/ammo/health because they are updated every
		// frame no matter what
		UpdateHudWeapon( vr_weaponHand.GetInteger() );
		_hud->HandleNamedEvent( "weaponPulse" );
		inventory.weaponPulse = false;
	}
	if ( inventory.armorPulse ) {
		_hud->HandleNamedEvent( "armorPulse" );
		inventory.armorPulse = false;
	}

	UpdateHudAmmo( _hud, vr_weaponHand.GetInteger());
}

/*
===============
idPlayer::UpdateHudWeapon
===============
*/
void idPlayer::UpdateHudWeapon( int flashWeaponHand ) {
	idUserInterface *hud = idPlayer::hud;

	// if updating the hud of a followed client
	if ( gameLocal.localClientNum >= 0 && gameLocal.entities[ gameLocal.localClientNum ] && gameLocal.entities[ gameLocal.localClientNum ]->IsType( idPlayer::Type ) ) {
		idPlayer *p = static_cast< idPlayer * >( gameLocal.entities[ gameLocal.localClientNum ] );
		if ( p->spectating && p->spectator == entityNumber ) {
			assert( p->hud );
			hud = p->hud;
		}
	}

	if ( !hud ) {
		return;
	}

	for ( int i = 0; i < MAX_WEAPONS; i++ ) {
		const char *weapnum = va( "def_weapon%d", i );
		const char *hudWeap = va( "weapon%d", i );
		int weapstate = 0;
		if ( inventory.weapons & ( 1 << i ) ) {
			const char *weap = spawnArgs.GetString( weapnum );
			if ( weap && *weap ) {
				weapstate++;
			}
			if ( hands[ vr_weaponHand.GetInteger() ].idealWeapon == i ) {
				weapstate++;
			}
		}
		hud->SetStateInt( hudWeap, weapstate );
	}
	//GBFIX Doesnt ever seemed to be called
	/*if ( flashWeapon ) {
		hud->HandleNamedEvent( "weaponChange" );
	}*/
}

/*
===============
idPlayer::DrawHUD
===============
*/
void idPlayer::DrawHUD( idUserInterface *_hud ) {
    DrawWeaponWheel( _hud );

    // Koz begin
    if ( game->isVR && vr_hudType.GetInteger() == VR_HUD_NONE && !vr_weaponWheel.GetBool())
    {
        return;
    }
    // Koz end

    if (fileSystem->RunningPhobos()) {
        idWeapon* weapon = GetWeaponInHand( vr_weaponHand.GetInteger() );
        if (!weapon || (influenceActive != INFLUENCE_NONE && influenceActive <= INFLUENCE_LEVEL3) ||
            privateCameraView || (gameLocal.GetCamera() && !gameLocal.GetCamera()->spawnArgs.GetInt("AllowClickers")) ||
            !_hud || !g_showHud.GetBool()) {
            return;
        }
        _hud->SetStateString("influenceVisible", va("%i", influenceActive == INFLUENCE_NONE));
        _hud->SetStateString("pickupsVisible", va("%i", influenceActive == INFLUENCE_NONE));
    } else if ( !hands[vr_weaponHand.GetInteger()].weapon || influenceActive != INFLUENCE_NONE || privateCameraView || gameLocal.GetCamera() || !_hud || !g_showHud.GetBool() ) {
		return;
	}

	UpdateHudStats( _hud );

	if ( !gameLocal.isMultiplayer )
		_hud->SetStateString( "weapicon",  hands[ vr_weaponHand.GetInteger() ].weapon->Icon() );

	// FIXME: this is temp to allow the sound meter to show up in the hud
	// it should be commented out before shipping but the code can remain
	// for mod developers to enable for the same functionality
	_hud->SetStateInt( "s_debug", cvarSystem->GetCVarInteger( "s_showLevelMeter" ) );

	hands[ vr_weaponHand.GetInteger() ].weapon->UpdateGUI();

	_hud->Redraw( gameLocal.realClientTime );

	// weapon targeting crosshair
	if ( !GuiActive() ) {
		if ( cursor && hands[ vr_weaponHand.GetInteger() ].weapon->ShowCrosshair() ) {
			//GB Fix - we may need this later
			/*if (hands[ vr_weaponHand.GetInteger() ].weapon->GetGrabberState() == 1 || hands[ vr_weaponHand.GetInteger() ].weapon->GetGrabberState() == 2) {
				cursor->SetStateString("grabbercursor", "1");
				cursor->SetStateString("combatcursor", "0");
			} else {
				cursor->SetStateString("grabbercursor", "0");
				cursor->SetStateString("combatcursor", "1");
			}
			cursor->Redraw( gameLocal.realClientTime );*/
		}
	}
}

void idPlayer::DrawWeaponWheel(idUserInterface *_hud) {
    //Update visibility
    _hud->SetStateInt("weapon_wheel", vr_weaponWheel.GetBool() ? 1 : 0);
    if (!vr_weaponWheel.GetBool()) {
        return;
    }

    //Calculate hand movement diff
    vec3_t diff;
    diff[PITCH] = vr_weaponWheelDir[PITCH] - pVRClientInfo->weaponangles_temp[PITCH] + pVRClientInfo->hmdorientation_temp[PITCH];
    diff[YAW] = vr_weaponWheelDir[YAW] - pVRClientInfo->weaponangles_temp[YAW] + pVRClientInfo->hmdorientation_temp[YAW];
    diff[ROLL] = vr_weaponWheelDir[ROLL] - pVRClientInfo->weaponangles_temp[ROLL] + pVRClientInfo->hmdorientation_temp[ROLL];
    while (diff[YAW] > 180) diff[YAW] -= 360;
    while (diff[YAW] < -180) diff[YAW] += 360;
    float dir = RAD2DEG(atan2(diff[PITCH], diff[YAW]));
    //gameLocal.Warning("Weapon wheel hand dir %d", (int)dir);

    //Selecting weapons
    float dst = sqrt(diff[PITCH] * diff[PITCH] + diff[YAW] * diff[YAW]);
    if (dst > 15) {
        if (dir < -160) vr_weaponWheelCurrent.SetInteger(weapon_machinegun);
        else if (dir < -130) vr_weaponWheelCurrent.SetInteger(weapon_plasmagun);
        else if (dir > 170) vr_weaponWheelCurrent.SetInteger(weapon_machinegun);
        else if (dir > 140) vr_weaponWheelCurrent.SetInteger(weapon_chaingun);
        else if (dir > 120) vr_weaponWheelCurrent.SetInteger(weapon_pistol);
        else if (dir > 80) vr_weaponWheelCurrent.SetInteger(weapon_shotgun);
        else if (dir > 45) vr_weaponWheelCurrent.SetInteger(weapon_handgrenade);
        else if (dir > 10) vr_weaponWheelCurrent.SetInteger(weapon_rocketlauncher);
        else if (dir > -20) vr_weaponWheelCurrent.SetInteger(weapon_bfg);
        else if (dir > -60) vr_weaponWheelCurrent.SetInteger(fileSystem->RunningD3XP() ? weapon_shotgun_double : weapon_chainsaw);
        else vr_weaponWheelCurrent.SetInteger(fileSystem->RunningD3XP() ? weapon_grabber : weapon_fists);
    }

    //Define alpha values
    int currentWeapon = vr_weaponWheelCurrent.GetInteger();
    float alphaMax = IsWeaponReady(currentWeapon) ? 0.6f : 0.0f;
    float alphaEasy = 0.4f;
    float alphaBase = 0.1f;

    //Default value based on the weapon availability
    static float alpha[MAX_WEAPONS] = {};
    alpha[0] = alphaEasy;
    alpha[1] = IsWeaponReady(weapon_pistol) ? alphaEasy : alphaBase;
    alpha[2] = IsWeaponReady(weapon_shotgun) ? alphaEasy : alphaBase;
    alpha[3] = IsWeaponReady(weapon_machinegun) ? alphaEasy : alphaBase;
    alpha[4] = IsWeaponReady(weapon_chaingun) ? alphaEasy : alphaBase;
    alpha[5] = IsWeaponReady(weapon_handgrenade) ? alphaEasy : alphaBase;
    alpha[6] = IsWeaponReady(weapon_plasmagun) ? alphaEasy : alphaBase;
    alpha[7] = IsWeaponReady(weapon_rocketlauncher) ? alphaEasy : alphaBase;
    alpha[8] = IsWeaponReady(weapon_bfg) ? alphaEasy : alphaBase;
    alpha[10] = IsWeaponReady(weapon_chainsaw) ? alphaEasy : alphaBase;
    alpha[12] = IsWeaponReady(weapon_grabber) ? alphaEasy : alphaBase;
    alpha[13] = IsWeaponReady(weapon_shotgun_double) ? alphaEasy : alphaBase;

    //Highlight selected weapon
    alpha[0] += currentWeapon == weapon_fists ? alphaMax : 0;
    alpha[1] += currentWeapon == weapon_pistol ? alphaMax : 0;
    alpha[2] += currentWeapon == weapon_shotgun ? alphaMax : 0;
    alpha[3] += currentWeapon == weapon_machinegun ? alphaMax : 0;
    alpha[4] += currentWeapon == weapon_chaingun ? alphaMax : 0;
    alpha[5] += currentWeapon == weapon_handgrenade ? alphaMax : 0;
    alpha[6] += currentWeapon == weapon_plasmagun ? alphaMax : 0;
    alpha[7] += currentWeapon == weapon_rocketlauncher ? alphaMax : 0;
    alpha[8] += currentWeapon == weapon_bfg ? alphaMax : 0;
    alpha[10] += currentWeapon == weapon_chainsaw ? alphaMax : 0;
    alpha[12] += currentWeapon == weapon_grabber ? alphaMax : 0;
    alpha[13] += currentWeapon == weapon_shotgun_double ? alphaMax : 0;

    //Cross-fading
    static float finalAlpha[MAX_WEAPONS] = {};
    for (int i = 0; i < MAX_WEAPONS; i++) {
        finalAlpha[i] = finalAlpha[i] * 0.8f + alpha[i] * 0.2f;
    }

    //Calculate cursor
    float x = 100 + 100 * diff[YAW] / 30.0f;
    float y = 80 - 80 * diff[PITCH] / 30.0f;
    float glow = fmax(0.5f - dst / 50.0f, 0.0f);

    //Apply values
    _hud->SetStateFloat("weapon_wheel_glow_x", x);
    _hud->SetStateFloat("weapon_wheel_glow_y", y);
    _hud->SetStateFloat("weapon_wheel_glow_alpha", glow);
    _hud->SetStateFloat("weapon_wheel0", finalAlpha[0]);
    _hud->SetStateFloat("weapon_wheel1", finalAlpha[1]);
    _hud->SetStateFloat("weapon_wheel2", finalAlpha[2]);
    _hud->SetStateFloat("weapon_wheel3", finalAlpha[3]);
    _hud->SetStateFloat("weapon_wheel4", finalAlpha[4]);
    _hud->SetStateFloat("weapon_wheel5", finalAlpha[5]);
    _hud->SetStateFloat("weapon_wheel6", finalAlpha[6]);
    _hud->SetStateFloat("weapon_wheel7", finalAlpha[7]);
    _hud->SetStateFloat("weapon_wheel8", finalAlpha[8]);
    _hud->SetStateFloat("weapon_wheel10", finalAlpha[10]);
    _hud->SetStateFloat("weapon_wheel12", finalAlpha[12]);
    _hud->SetStateFloat("weapon_wheel13", finalAlpha[13]);
}


bool idPlayer::IsWeaponReady( int weapon )
{
    if ((weapon > 0) && (inventory.weapons & (1 << weapon))) {
        const char* weap = spawnArgs.GetString(va( "def_weapon%d", weapon));
        return inventory.HasAmmo(weap, true, this);
    }
    return false;
}

/*
===============
idPlayer::GetHudAlpha
===============
*/
float idPlayer::GetHudAlpha()
{
	static int lastFrame = 0;
	static float currentAlpha = 0.0f;
	static float delta = 0.0f;
	int currentFrameNumber = common->GetFrameNumber();

	delta = vr_hudTransparency.GetFloat() / (125 / (1000 / commonVr->hmdHz));

	if ( vr_hudType.GetInteger() != VR_HUD_LOOK_DOWN )
	{
		return hudActive ? vr_hudTransparency.GetFloat() : 0;
	}

	if ( lastFrame == currentFrameNumber ) return currentAlpha;

	lastFrame = currentFrameNumber;

	bool force = false;

	if ( vr_hudLowHealth.GetInteger() >= health && health >= 0 ) force = true;

	if ( commonVr->lastHMDPitch >= vr_hudRevealAngle.GetFloat() || force ) // fade stats in
	{
		currentAlpha += delta;
		float targetTransparency = vr_weaponWheel.GetBool() ? 1 : vr_hudTransparency.GetFloat();
		if ( currentAlpha > targetTransparency ) currentAlpha = targetTransparency;
	}
	else
	{
		currentAlpha -= delta;
		if ( currentAlpha < 0.0f ) currentAlpha = 0.0f;
	}

	return currentAlpha;
}

/*
==============
Koz
idPlayer::UpdateVrHud
==============
*/
void idPlayer::UpdateVrHud()
{
    static idVec3 hudOrigin;
    static idMat3 hudAxis;
    float hudPitch;

    //GBFIX - This is DrBeefs code not FP
    // update the hud model
    /*
    if ( (pVRClientInfo == nullptr) || !pVRClientInfo->visible_hud || gameLocal.inCinematic)
    {
        // hide it
        hudEntity.allowSurfaceInViewID = -1;
    }
    else
    {
        hudEntity.allowSurfaceInViewID = entityNumber + 1;

        {

            if (vr_hudmode.GetInteger() == 0)
            {
                // CalculateRenderView must have been called first
                const idVec3 &viewOrigin = firstPersonViewOrigin;
                const idMat3 &viewAxis = firstPersonViewAxis;

                if (pVRClientInfo)
                {
                    float yaw;
                    idAngles angles;

                    angles.pitch = pVRClientInfo->offhandangles[PITCH];
                    angles.yaw = viewAngles.yaw +
                                 (pVRClientInfo->offhandangles[YAW] - pVRClientInfo->hmdorientation[YAW]);
                    angles.roll = pVRClientInfo->offhandangles[ROLL];

                    hudAxis = angles.ToMat3();

                    idVec3	offpos( -pVRClientInfo->offhandoffset[2],
                                      -pVRClientInfo->offhandoffset[0],
                                      pVRClientInfo->offhandoffset[1]);

                    idAngles a(0, viewAngles.yaw - pVRClientInfo->hmdorientation[YAW], 0);
                    offpos *= a.ToMat3();
                    offpos *= ((100.0f / 2.54f) * vr_scale.GetFloat());

                    {
                        hudOrigin = viewOrigin + offpos;
                    }
                }
                hudOrigin += hudAxis[2] * 16.0f;
                hudOrigin += hudAxis[1] * -8.5f;
                hudAxis *= 0.7; // scale
            }
            else {
                //Fixed HUD, but make it take a short time to catch up with the player's yaw
                static float yaw_x = 0.0f;
                static float yaw_y = 1.0f;
                yaw_x = 0.97f * yaw_x + 0.03f * cosf(DEG2RAD(viewAngles.yaw));
                yaw_y = 0.97f * yaw_y + 0.03f * sinf(DEG2RAD(viewAngles.yaw));

                GetViewPos( hudOrigin, hudAxis );
                hudAxis = idAngles( 10.0f, RAD2DEG(atan2(yaw_y, yaw_x)), 0.0f ).ToMat3();
                hudOrigin += hudAxis[0] * 24.0f;
                hudOrigin.z += 4.0f;
                hudOrigin += hudAxis[1] * -8.5f;
            }
        }

        hudEntity.axis = hudAxis;
        hudEntity.origin = hudOrigin;
        hudEntity.weaponDepthHack = true;

    }*/

    // update the hud model
    if (gameLocal.inCinematic)
    //if ( (pVRClientInfo == nullptr) || ( !hudActive && ( vr_hudLowHealth.GetInteger() == 0 ) ) || commonVr->PDAforced || game->IsPDAOpen() || gameLocal.inCinematic)
    {
        // hide it
        hudEntity.allowSurfaceInViewID = -1;
    }
    else
    {
        // always show HUD if in flicksync
        hudEntity.allowSurfaceInViewID = entityNumber + 1;

        if ( vr_hudPosLock.GetInteger() == 1 && !commonVr->thirdPersonMovement && !vr_weaponWheel.GetBool() ) // hud in fixed position in space, except if running in third person, then attach to face.
        {
            hudPitch = vr_hudType.GetInteger() == VR_HUD_LOOK_DOWN ? vr_hudPosAngle.GetFloat() : 10.0f;

            //Fixed HUD, but make it take a short time to catch up with the player's yaw
            if( gameLocal.inCinematic )
            {
                hud_yaw_x = cosf(DEG2RAD(commonVr->lastHMDViewAxis.ToAngles().yaw));
                hud_yaw_y = sinf(DEG2RAD(commonVr->lastHMDViewAxis.ToAngles().yaw));
                hudOrigin = commonVr->lastHMDViewOrigin;
            }
            else if (resetHUDYaw)
            {
                hud_yaw_x = cosf(DEG2RAD(viewAngles.yaw));
                hud_yaw_y = sinf(DEG2RAD(viewAngles.yaw));
                resetHUDYaw = false;
                GetViewPos( hudOrigin, hudAxis );
            }
            else
            {
                hud_yaw_x = 0.97f * hud_yaw_x + 0.03f * cosf(DEG2RAD(viewAngles.yaw));
                hud_yaw_y = 0.97f * hud_yaw_y + 0.03f * sinf(DEG2RAD(viewAngles.yaw));
                GetViewPos( hudOrigin, hudAxis );
            }
            hudAxis = idAngles( 10.0f, RAD2DEG(atan2(hud_yaw_y, hud_yaw_x)), 0.0f ).ToMat3();
			hudOrigin += hudAxis[0] * vr_hudPosDist.GetFloat();
			hudOrigin += hudAxis[1] * vr_hudPosHorz.GetFloat();
			hudOrigin.z += vr_hudPosVert.GetFloat();

            /*hudOrigin += hudAxis[0] * vr_hudPosDist.GetFloat();
            hudOrigin += hudAxis[1] * vr_hudPosHorz.GetFloat();
            hudOrigin.z += vr_hudPosVert.GetFloat();*/

            /*float yaw;
            if( gameLocal.inCinematic )
            {
                yaw = commonVr->lastHMDViewAxis.ToAngles().yaw;
                hudOrigin = commonVr->lastHMDViewOrigin;
            }
            else
            {
                GetViewPos( hudOrigin, hudAxis );
                yaw = viewAngles.yaw;
            }
            hudAxis = idAngles( hudPitch, yaw, 0.0f ).ToMat3();

            hudOrigin += hudAxis[0] * vr_hudPosDist.GetFloat();
            hudOrigin += hudAxis[1] * vr_hudPosHorz.GetFloat();
            hudOrigin.z += vr_hudPosVert.GetFloat();*/
        }
        else if (vr_weaponWheel.GetBool())
        {
            hudAxis = commonVr->lastHMDViewAxis;
            hudOrigin = hands[vr_weaponHand.GetInteger()].weapon->viewWeaponOrigin;
            hudOrigin += hudAxis[0] * 16;
            hudOrigin -= hudAxis[2] * 24;
        }
        else // hud locked to face
        {
            if ( commonVr->thirdPersonMovement )
            {
                hudAxis = commonVr->thirdPersonHudAxis;
                hudOrigin = commonVr->thirdPersonHudPos;
            }
            else
            {
                hudAxis = commonVr->lastHMDViewAxis;
                hudOrigin = commonVr->lastHMDViewOrigin;
            }
            hudOrigin += hudAxis[0] * vr_hudPosDist.GetFloat();
            hudOrigin += hudAxis[1] * vr_hudPosHorz.GetFloat();
            hudOrigin += hudAxis[2] * vr_hudPosVert.GetFloat();

        }

        hudAxis *= vr_weaponWheel.GetBool() ? 1.5f : vr_hudScale.GetFloat();

        hudEntity.axis = hudAxis;
        hudEntity.origin = hudOrigin;
        hudEntity.weaponDepthHack = vr_hudOcclusion.GetBool();

    }

    if ( hudHandle == -1 )
    {
        hudHandle = gameRenderWorld->AddEntityDef( &hudEntity );
    }
    else
    {
        gameRenderWorld->UpdateEntityDef( hudHandle, &hudEntity );
    }
}

/*
===============
idPlayer::EnterCinematic
===============
*/
void idPlayer::EnterCinematic( void ) {
	if (PowerUpActive(HELLTIME)) {
		StopHelltime();
	}

	Hide();
	StopAudioLog();
	StopSound( SND_CHANNEL_PDA, false );
	if ( hud ) {
		hud->HandleNamedEvent( "radioChatterDown" );
	}

	physicsObj.SetLinearVelocity( vec3_origin );

	SetState( "EnterCinematic" );
	UpdateScript();

	if ( weaponEnabled ) {
        for( int h = 0; h < 2; h++ )
        {
            idWeapon* weapon = GetWeaponInHand( h );
            if ( weapon )
                weapon->EnterCinematic();
        }
	}
    if( flashlight )
    {
        flashlight->EnterCinematic();
    }

	AI_FORWARD		= false;
	AI_BACKWARD		= false;
	AI_STRAFE_LEFT	= false;
	AI_STRAFE_RIGHT	= false;
	AI_RUN			= false;
	AI_ATTACK_HELD	= false;
	AI_WEAPON_FIRED	= false;
	AI_JUMP			= false;
	AI_CROUCH		= false;
	AI_ONGROUND		= true;
	AI_ONLADDER		= false;
	AI_DEAD			= ( health <= 0 );
	AI_RUN			= false;
	AI_PAIN			= false;
	AI_HARDLANDING	= false;
	AI_SOFTLANDING	= false;
	AI_RELOAD		= false;
	AI_TELEPORT		= false;
	AI_TURN_LEFT	= false;
	AI_TURN_RIGHT	= false;
}

/*
===============
idPlayer::ExitCinematic
===============
*/
void idPlayer::ExitCinematic( void ) {
	Show();

    if( weaponEnabled )
    {
        for( int h = 0; h < 2; h++ )
        {
            idWeapon* weapon = GetWeaponInHand( h );
            if( weapon )
                weapon->ExitCinematic();
        }
    }
    if( flashlight )
    {
        flashlight->ExitCinematic();
    }
    // long cinematics would have surpassed the healthTakeTime, causing the player to take damage
    // immediately after the cinematic ends.  Instead we start the healthTake cooldown again once
    // the cinematic ends.
    if( g_skill.GetInteger() == 3 )
    {
        nextHealthTake = gameLocal.time + g_healthTakeTime.GetInteger() * 1000;
    }

	SetState( "ExitCinematic" );
	UpdateScript();
}

/*
=====================
idPlayer::UpdateConditions
=====================
*/
void idPlayer::UpdateConditions( void ) {
	idVec3	velocity;
	float	forwardspeed;
	float	sidespeed;

	// minus the push velocity to avoid playing the walking animation and sounds when riding a mover
	velocity = physicsObj.GetLinearVelocity() - physicsObj.GetPushedLinearVelocity();

	if ( fileSystem->RunningPhobos() ? influenceActive == INFLUENCE_LEVEL2 : influenceActive ) {
		AI_FORWARD		= false;
		AI_BACKWARD		= false;
		AI_STRAFE_LEFT	= false;
		AI_STRAFE_RIGHT	= false;
	} else if ( gameLocal.time - lastDmgTime < 500 ) {
		forwardspeed = velocity * viewAxis[ 0 ];
		sidespeed = velocity * viewAxis[ 1 ];
		AI_FORWARD		= AI_ONGROUND && ( forwardspeed > 20.01f );
		AI_BACKWARD		= AI_ONGROUND && ( forwardspeed < -20.01f );
		AI_STRAFE_LEFT	= AI_ONGROUND && ( sidespeed > 20.01f );
		AI_STRAFE_RIGHT	= AI_ONGROUND && ( sidespeed < -20.01f );
	} else if ( xyspeed > MIN_BOB_SPEED ) {
		AI_FORWARD		= AI_ONGROUND && ( usercmd.forwardmove > 0 );
		AI_BACKWARD		= AI_ONGROUND && ( usercmd.forwardmove < 0 );
		AI_STRAFE_LEFT	= AI_ONGROUND && ( usercmd.rightmove < 0 );
		AI_STRAFE_RIGHT	= AI_ONGROUND && ( usercmd.rightmove > 0 );
	} else {
		AI_FORWARD		= false;
		AI_BACKWARD		= false;
		AI_STRAFE_LEFT	= false;
		AI_STRAFE_RIGHT	= false;
	}

	AI_RUN			= ( usercmd.buttons & BUTTON_RUN ) && ( ( !pm_stamina.GetFloat() ) || ( stamina > pm_staminathreshold.GetFloat() ) );
	AI_DEAD			= ( health <= 0 );
}

/*
==================
WeaponFireFeedback

Called when a weapon fires, generates head twitches, etc
==================
*/
void idPlayer::WeaponFireFeedback( int hand, const idDict *weaponDef ) {
	// force a blink
	blink_time = 0;

	// play the fire animation
	AI_WEAPON_FIRED = true;

	// shake controller
	float highMagnitude = weaponDef->GetFloat( "controllerShakeHighMag" );
	int highDuration = weaponDef->GetInt( "controllerShakeHighTime" );
	float lowMagnitude = weaponDef->GetFloat( "controllerShakeLowMag" );
	int lowDuration = weaponDef->GetInt( "controllerShakeLowTime" );

	// update view feedback
	playerView.WeaponFireFeedback( weaponDef );

	if( IsLocallyControlled() )
	{
		hands[hand].SetControllerShake( highMagnitude, highDuration, lowMagnitude, lowDuration );
	}
}

bool idPlayer::WeaponHandImpulseSlot()
{
	int hand = vr_weaponHand.GetInteger();
	slotIndex_t weaponHandSlot = hands[ hand ].handSlot;
	if( weaponHandSlot == SLOT_WEAPON_HIP )
	{
		if ( objectiveSystemOpen ) // for now this means our weapon hand was empty
		{
			if ( hands[ hand ].previousWeapon == weapon_fists )
			{
				hands[ hand ].previousWeapon = holsteredWeapon;
			}
			TogglePDA( hand );
		}
		else
		{
			// if our hand is too full, we can't pick up the weapon
			if( hands[ hand ].tooFullToInteract() && holsteredWeapon != weapon_fists )
				return false;
			SetupHolsterSlot( hand );
		}
		return true;
	}
	if( weaponHandSlot == SLOT_WEAPON_BACK_BOTTOM )
	{
		if ( objectiveSystemOpen )
		{
			TogglePDA( hand );
		}
		hands[ hand ].PrevWeapon();
		return true;
	}
	if( weaponHandSlot == SLOT_WEAPON_BACK_TOP )
	{
		if ( objectiveSystemOpen )
		{
			TogglePDA( hand );
		}
		hands[ hand ].NextWeapon();
		return true;
	}
	if ( weaponHandSlot == SLOT_PDA_HIP )
	{
		// if we're holding a gun ( not a pointer finger or fist ) then holster the gun
		//if ( !commonVr->PDAforced && !objectiveSystemOpen && currentWeapon != weapon_fists )
		//	SetupHolsterSlot( hand );
		// pick up PDA in our weapon hand, or pick up the torch if our hand is a pointer finger
		if ( !gameLocal.isMultiplayer )
		{
			// we don't have a PDA, so toggle the menu instead
			if ( commonVr->PDAforced || inventory.pdas.Num() == 0 )
			{
				if( !commonVr->PDAforced && hands[ vr_weaponHand.GetInteger() ].tooFullToInteract() )
					return false;
				SwapWeaponHand();
				PerformImpulse( 40 );
			}
			else if( objectiveSystemOpen )
			{
				SwapWeaponHand();
				TogglePDA( hand );
			}
			else if( weapon_pda >= 0 )
			{
				if( hands[ hand ].tooFullToInteract() )
					return false;
				SwapWeaponHand();
				SetupPDASlot( false );
				SetupHolsterSlot( 1 - hand, false );
				hands[ hand ].SelectWeapon( weapon_pda, true, false );
			}
		}
		return true;
	}
	if( weaponHandSlot == SLOT_FLASHLIGHT_HEAD && !commonVr->PDAforced && !objectiveSystemOpen
		&& flashlight.IsValid() && !spectating && weaponEnabled && !hiddenWeapon && !gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) && !vr_flashlightStrict.GetBool() )
	{
		// if the flashlight is mounted on our head, and we're unambiguously trying to grab it with our weapon hand
		if( commonVr->currentFlashlightPosition == FLASHLIGHT_HEAD && (hands[ hand ].currentWeapon == weapon_fists || vr_gripMode.GetInteger() == VR_GRIP_TOGGLE_WITH_DROP ) && HasHoldableFlashlight() )
		{
			// move weapon from hand to inventory
			hands[hand].idealWeapon = weapon_fists;
			SwapWeaponHand();
			// swap flashlight between head and hand
			vr_flashlightMode.SetInteger( FLASHLIGHT_HAND );
			vr_flashlightMode.SetModified();
			return true;
		}
			// if we're unambiguously trying to grab it, but there's nothing there, process and ignore the grip
		else if( vr_gripMode.GetInteger() == VR_GRIP_TOGGLE_WITH_DROP )
		{
			return true;
		}
	}
	if( weaponHandSlot == SLOT_FLASHLIGHT_SHOULDER && !commonVr->PDAforced && !objectiveSystemOpen
		&& flashlight.IsValid() && !spectating && weaponEnabled && !hiddenWeapon && !gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) && !vr_flashlightStrict.GetBool() )
	{
		// if the flashlight is mounted on our shoulder, and we're unambiguously trying to grab it with our weapon hand
		if( commonVr->currentFlashlightPosition == FLASHLIGHT_BODY && ( hands[ hand ].currentWeapon == weapon_fists || vr_gripMode.GetInteger() == VR_GRIP_TOGGLE_WITH_DROP ) && HasHoldableFlashlight() )
		{
			// move weapon from hand to inventory
			hands[hand].idealWeapon = weapon_fists;
			// swap flashlight from shoulder to hand
			SwapWeaponHand();
			vr_flashlightMode.SetInteger( FLASHLIGHT_HAND );
			vr_flashlightMode.SetModified();
			return true;
		}
			// if we're unambiguously trying to grab it, but there's nothing there, process and ignore the grip
		else if( vr_gripMode.GetInteger() == VR_GRIP_TOGGLE_WITH_DROP )
		{
			return true;
		}
	}
	return false;
}

/*
===============
idPlayer::StopFiring
===============
*/
void idPlayer::StopFiring( void ) {
	AI_ATTACK_HELD	= false;
	AI_WEAPON_FIRED = false;
	AI_RELOAD		= false;
    for( int h = 0; h < 2; h++ )
    {
        if( hands[h].weapon )
        {
            hands[h].weapon->EndAttack();
        }
    }
}

/*
===============
idPlayer::FireWeapon
===============
*/
void idPlayer::FireWeapon( int hand, idWeapon *weap ) {
    if( vr_debugHands.GetBool() )
    {
        common->Printf( "Before FireWeapon(%d):\n", hand );
        hands[hand].debugPrint();
    }
    idMat3 axis;
	idVec3 muzzle;

	if ( privateCameraView ) {
		return;
	}

	if ( g_editEntityMode.GetInteger() ) {
		GetViewPos( muzzle, axis );
		if ( gameLocal.editEntities->SelectEntity( muzzle, axis[0], this ) ) {
			return;
		}
	}

	if ( !hiddenWeapon && weap->IsReady() ) {
        if( g_infiniteAmmo.GetBool() || weap->AmmoInClip() || weap->AmmoAvailable() )
        {
            weapon_t w = weap->IdentifyWeapon();
			// Koz grabber doesn't fire projectiles, so player script won't trigger fire anim for hand if we dont do this
			if( w == WEAPON_GRABBER ) AI_WEAPON_FIRED = true;

			AI_ATTACK_HELD = true;
			weap->BeginAttack();

			if ( ( weapon_soulcube >= 0 ) && ( w == WEAPON_SOULCUBE ) ) {
				if ( hud ) {
					hud->HandleNamedEvent( "soulCubeNotReady" );
				}
                hands[hand].SelectWeapon( hands[hand].previousWeapon, false, false );

                if ((weapon_bloodstone >= 0) && (hands[hand].currentWeapon == weapon_bloodstone) && inventory.weapons & (1 << weapon_bloodstone_active1) && hands[hand].weapon.GetEntity()->GetStatus() == WP_READY) {
                    // tell it to switch to the previous weapon. Only do this once to prevent
                    // weapon toggling messing up the previous weapon
                    if (hands[hand].idealWeapon == weapon_bloodstone) {
                        if (hands[hand].previousWeapon == weapon_bloodstone || hands[hand].previousWeapon == -1) {
                            NextBestWeapon();
                        } else {
                            //Since this is a toggle weapon just select itself and it will toggle to the last weapon
                            SelectWeapon(weapon_bloodstone, false);
                        }
                    }
                }
			}
		} else {
			NextBestWeapon();
		}
	}

	if ( hud ) {
		if ( tipUp ) {
			HideTip();
		}
		// may want to track with with a bool as well
		// keep from looking up named events so often
		if ( objectiveUp ) {
			HideObjective();
		}
	}
}

/*
===============
idPlayer::FlashlightOff
===============
*/
void idPlayer::FlashlightOff()
{
    if( !flashlight.IsValid() )
    {
        return;
    }
    if( !flashlight->lightOn )
    {
        return;
    }
    //flashlight->FlashlightOff();
    flashlight->FlashlightOff();


    // Koz
    const function_t* func;
    func = scriptObject.GetFunction( "SetFlashHandPose" ); // Set flashlight hand pose
    if ( func )
    {
        // use the frameCommandThread since it's safe to use outside of framecommands
        // Koz debug common->Printf( "Calling SetFlashHandPose\n" ); // Set flashlight hand pose
        gameLocal.frameCommandThread->CallFunction( this, func, true );
        gameLocal.frameCommandThread->Execute();

    }
    else
    {
        common->Warning( "Can't find function 'SetFlashHandPose' in object '%s'", scriptObject.GetTypeName() ); // Set flashlight hand pose
        return;
    }
    // Koz
}

/*
===============
idPlayer::FlashlightOn
===============
*/
void idPlayer::FlashlightOn()
{
	if( commonVr->currentFlashlightPosition == FLASHLIGHT_NONE || commonVr->currentFlashlightPosition == FLASHLIGHT_INVENTORY )
		return;

	if( !flashlight.IsValid() )
	{
		return;
	}
	if( flashlightBattery < idMath::Ftoi( flashlight_minActivatePercent.GetFloat() * flashlight_batteryDrainTimeMS.GetFloat() ) )
	{
		return;
	}
	if( gameLocal.inCinematic)
	{
		return;
	}
	if( flashlight->lightOn )
	{
		return;
	}
	if( health <= 0 )
	{
		return;
	}
	if( spectating )
	{
		return;
	}

	flashlight->FlashlightOn();

	// Koz pose the hand
	const function_t* func;

	func = scriptObject.GetFunction( "SetFlashHandPose" ); // Set flashlight hand pose
	if ( func )
	{
		// use the frameCommandThread since it's safe to use outside of framecommands
		// Koz debug common->Printf( "Calling SetFlashHandPose\n" ); // Set flashlight hand pose
		gameLocal.frameCommandThread->CallFunction( this, func, true );
		gameLocal.frameCommandThread->Execute();

	}
	else
	{
		common->Warning( "Can't find function 'SetFlashHandPose' in object '%s'", scriptObject.GetTypeName() ); // Set flashlight hand pose
		return;
	}
	// Koz end


}

void idPlayer::FreeModelDef()
{
	idAFEntity_Base::FreeModelDef();
}

/*
==============
idPlayer::FreePDASlot
==============
*/
void idPlayer::FreePDASlot()
{
    if( pdaModelDefHandle != -1 )
    {
        gameRenderWorld->FreeEntityDef( pdaModelDefHandle );
        pdaModelDefHandle = -1;
    }
}

/*
==============
idPlayer::FreeHolsterSlot
==============
*/
void idPlayer::FreeHolsterSlot()
{
	if( holsterModelDefHandle != -1 )
	{
		gameRenderWorld->FreeEntityDef( holsterModelDefHandle );
		holsterModelDefHandle = -1;
	}
}

/*
===============
idPlayer::CacheWeapons
===============
*/
void idPlayer::CacheWeapons( void ) {
	idStr	weap;
	int		w;

	// check if we have any weapons
	if ( !inventory.weapons ) {
		return;
	}

	for( w = 0; w < MAX_WEAPONS; w++ ) {
		if ( inventory.weapons & ( 1 << w ) ) {
			weap = spawnArgs.GetString( va( "def_weapon%d", w ) );
			if ( weap != "" ) {
				idWeapon::CacheWeapon( weap );
			} else {
				inventory.weapons &= ~( 1 << w );
                inventory.foundWeapons &= ~( 1 << w );
                inventory.duplicateWeapons &= ~( 1 << w );
			}
		}
	}
}

/*
===============
idPlayer::Give
===============
*/
bool idPlayer::Give( const char *statname, const char *value, int hand  ) {
	int amount;

	if ( AI_DEAD ) {
		return false;
	}

	if ( !idStr::Icmp( statname, "health" ) ) {
		if ( health >= inventory.maxHealth ) {
			return false;
		}
		amount = atoi( value );
		if ( amount ) {
			health += amount;
			if ( health > inventory.maxHealth ) {
				health = inventory.maxHealth;
			}
			if ( hud ) {
				hud->HandleNamedEvent( "healthPulse" );
			}
		}

	} else if ( !idStr::Icmp( statname, "stamina" ) ) {
		if ( stamina >= 100 ) {
			return false;
		}
		stamina += atof( value );
		if ( stamina > 100 ) {
			stamina = 100;
		}

	} else if ( !idStr::Icmp( statname, "heartRate" ) ) {
		heartRate += atoi( value );
		if ( heartRate > MAX_HEARTRATE ) {
			heartRate = MAX_HEARTRATE;
		}

	} else if ( !idStr::Icmp( statname, "air" ) ) {
		if ( airTics >= pm_airTics.GetInteger() ) {
			return false;
		}
		airTics += atoi( value ) / 100.0 * pm_airTics.GetInteger();
		if ( airTics > pm_airTics.GetInteger() ) {
			airTics = pm_airTics.GetInteger();
		}
	} else if (!idStr::Icmp(statname, "enviroTime")) {
		if (PowerUpActive(ENVIROTIME)) {
			inventory.powerupEndTime[ ENVIROTIME ] += (atof(value) * 1000);
		} else {
			GivePowerUp(ENVIROTIME, atoi(value)*1000);
		}
	} else {
        if( hand < 0 )
            hand = vr_weaponHand.GetInteger();
        bool ret = inventory.Give( this, spawnArgs, statname, value, &hands[hand].idealWeapon, true);

        if (!idStr::Icmp(statname, "ammo_bloodstone")) {
            //int i = inventory.AmmoIndexForAmmoClass( statname );
            //int max = inventory.MaxAmmoForAmmoClass( this, statname );
            //if(hud && inventory.ammo[ i ] >= max) {
            if (hud) {

                //Force an update of the bloodstone ammount
                int ammoRequired;
                ammo_t ammo_i = inventory.AmmoIndexForWeaponClass("weapon_bloodstone_passive", &ammoRequired);
                int bloodstoneAmmo = inventory.HasAmmo(ammo_i, ammoRequired);
                hud->SetStateString("player_bloodstone_ammo", va("%i", bloodstoneAmmo));

                hud->HandleNamedEvent("bloodstoneReady");
                //Make sure we unlock the ability to harvest
                harvest_lock = false;
            }
        }
        return ret;
	}
	return true;
}


/*
===============
idPlayer::GiveHealthPool

adds health to the player health pool
===============
*/
void idPlayer::GiveHealthPool( float amt ) {

	if ( AI_DEAD ) {
		return;
	}

	if ( health > 0 ) {
		healthPool += amt;
		if ( healthPool > inventory.maxHealth - health ) {
			healthPool = inventory.maxHealth - health;
		}
		nextHealthPulse = gameLocal.time;
	}
}

/*
===============
idPlayer::GiveItem

Returns false if the item shouldn't be picked up
===============
*/
bool idPlayer::GiveItem( idItem *item ) {
	int					i;
	const idKeyValue	*arg;
	idDict				attr;
	bool				gave;
	int					numPickup;

	if ( gameLocal.isMultiplayer && spectating ) {
		return false;
	}

    if( idStr::FindText( item->GetName(), "weapon_flashlight_new" ) > -1 )
    {
        return false;
    }

    if( idStr::FindText( item->GetName(), "weapon_flashlight" ) > -1 )
    {
        // don't allow flashlight weapon unless classic mode is enabled
        return false;
    }

	item->GetAttributes( attr );

	gave = false;
	numPickup = inventory.pickupItemNames.Num();
	for( i = 0; i < attr.GetNumKeyVals(); i++ ) {
		arg = attr.GetKeyVal( i );
		if ( Give( arg->GetKey(), arg->GetValue(), -1) ) {
			gave = true;
		}
	}

	arg = item->spawnArgs.MatchPrefix( "inv_weapon", NULL );
	if ( arg && hud ) {
		// We need to update the weapon hud manually, but not
		// the armor/ammo/health because they are updated every
		// frame no matter what
		UpdateHudWeapon( false );
		hud->HandleNamedEvent( "weaponPulse" );

		if (gave)
		{
			common->HapticEvent("pickup_weapon", 0, 0, 100, 0, 0);
		}
	}

	// display the pickup feedback on the hud
	if ( gave && ( numPickup == inventory.pickupItemNames.Num() ) ) {
		inventory.AddPickupName( item->spawnArgs.GetString( "inv_name" ), item->spawnArgs.GetString( "inv_icon" ), this );
	}

	return gave;
}

/*
===============
idPlayer::PowerUpModifier
===============
*/
float idPlayer::PowerUpModifier( int type ) {
	float mod = 1.0f;

	if ( PowerUpActive( BERSERK ) ) {
		switch( type ) {
			case SPEED: {
				mod *= 1.7f;
				break;
			}
			case PROJECTILE_DAMAGE: {
				mod *= 2.0f;
				break;
			}
			case MELEE_DAMAGE: {
				mod *= 30.0f;
				break;
			}
			case MELEE_DISTANCE: {
				mod *= 2.0f;
				break;
			}
		}
	}

	if ( gameLocal.isMultiplayer && !gameLocal.isClient ) {
		if ( PowerUpActive( MEGAHEALTH ) ) {
			if ( healthPool <= 0 ) {
				GiveHealthPool( 100 );
			}
		} else {
			healthPool = 0;
		}
	}

	return mod;
}

/*
===============
idPlayer::PowerUpActive
===============
*/
bool idPlayer::PowerUpActive( int powerup ) const {
	return ( inventory.powerups & ( 1 << powerup ) ) != 0;
}

/*
===============
idPlayer::GivePowerUp
===============
*/
bool idPlayer::GivePowerUp( int powerup, int time ) {
	const char *sound;
	const char *skin;

	if ( powerup >= 0 && powerup < MAX_POWERUPS ) {

		if ( gameLocal.isServer ) {
			idBitMsg	msg;
			byte		msgBuf[MAX_EVENT_PARAM_SIZE];

			msg.Init( msgBuf, sizeof( msgBuf ) );
			msg.WriteShort( powerup );
			msg.WriteBits( 1, 1 );
			ServerSendEvent( EVENT_POWERUP, &msg, false, -1 );
		}

		if ( powerup != MEGAHEALTH ) {
			inventory.GivePowerUp( this, powerup, time );
		}

		const idDeclEntityDef *def = NULL;

		switch( powerup ) {
			case BERSERK: {
				if ( spawnArgs.GetString( "snd_berserk_third", "", &sound ) ) {
					StartSoundShader( declManager->FindSound( sound ), SND_CHANNEL_DEMONIC, 0, false, NULL );
				}
				if ( baseSkinName.Length() ) {
					powerUpSkin = declManager->FindSkin( baseSkinName + "_berserk" );
				}
				break;
			}
			case INVISIBILITY: {
				spawnArgs.GetString( "skin_invisibility", "", &skin );
				powerUpSkin = declManager->FindSkin( skin );
				// remove any decals from the model
				if ( modelDefHandle != -1 ) {
					gameRenderWorld->RemoveDecals( modelDefHandle );
				}
				for( int h = 0; h < 2; h++ )
				{
					if( hands[h].weapon.GetEntity() )
					{
						hands[h].weapon.GetEntity()->UpdateSkin();
					}
				}
				if( flashlight.GetEntity() )
				{
					flashlight.GetEntity()->UpdateSkin();
				}
				if ( spawnArgs.GetString( "snd_invisibility", "", &sound ) ) {
					StartSoundShader( declManager->FindSound( sound ), SND_CHANNEL_ANY, 0, false, NULL );
				}
				break;
			}
			case ADRENALINE: {
				inventory.AddPickupName("#str_00100799", "", this);
				stamina = 100.0f;
				break;
			 }
			case MEGAHEALTH: {
				if ( spawnArgs.GetString( "snd_megahealth", "", &sound ) ) {
					StartSoundShader( declManager->FindSound( sound ), SND_CHANNEL_ANY, 0, false, NULL );
				}
				def = gameLocal.FindEntityDef( "powerup_megahealth", false );
				if ( def ) {
					health = def->dict.GetInt( "inv_health" );
				}
				break;
			 }

			 case HELLTIME: {
				if (spawnArgs.GetString("snd_helltime_start", "", &sound)) {
					 PostEventMS(&EV_StartSoundShader, 0, sound, SND_CHANNEL_ANY);
				}

				if (spawnArgs.GetString("snd_helltime_loop", "", &sound)) {
					PostEventMS(&EV_StartSoundShader, 0, sound, SND_CHANNEL_DEMONIC);
				}

				break;
			}
			case ENVIROSUIT: {
				// Turn on the envirosuit sound
				if (gameSoundWorld) {
					gameSoundWorld->SetEnviroSuit(true);
				}

				// Put the helmet and lights on the player
				idDict	args;

				// Light
				const idDict *lightDef = gameLocal.FindEntityDefDict("envirosuit_light", false);

				if (lightDef) {
					idEntity *temp;
					gameLocal.SpawnEntityDef(*lightDef, &temp, false);

					idLight *eLight = static_cast<idLight *>(temp);
					eLight->GetPhysics()->SetOrigin(firstPersonViewOrigin);
					eLight->UpdateVisuals();
					eLight->Present();

					enviroSuitLight = eLight;
				}

				break;
			}
			case ENVIROTIME: {
				hudPowerup = ENVIROTIME;
				// The HUD display bar is fixed at 60 seconds
				hudPowerupDuration = 60000;
				break;
			}
			case INVULNERABILITY: {
				if (gameLocal.isMultiplayer && !gameLocal.isClient) {
					inventory.AddPickupName("#str_00100630", "", this);
				}

				if (gameLocal.isMultiplayer) {
				/*if ( spawnArgs.GetString( "snd_invulnerable", "", &sound ) ) {
					StartSoundShader( declManager->FindSound( sound ), SND_CHANNEL_DEMONIC, 0, false, NULL );
				}*/
				if (baseSkinName.Length()) {
					powerUpSkin = declManager->FindSkin(baseSkinName + "_invuln");
				}
			}

			break;
			}
		}

		if ( hud ) {
			hud->HandleNamedEvent( "itemPickup" );
		}

		return true;
	} else {
		gameLocal.Warning( "Player given power up %i\n which is out of range", powerup );
	}
	return false;
}

/*
==============
idPlayer::ClearPowerup
==============
*/
void idPlayer::ClearPowerup( int i ) {

	if ( gameLocal.isServer ) {
		idBitMsg	msg;
		byte		msgBuf[MAX_EVENT_PARAM_SIZE];

		msg.Init( msgBuf, sizeof( msgBuf ) );
		msg.WriteShort( i );
		msg.WriteBits( 0, 1 );
		ServerSendEvent( EVENT_POWERUP, &msg, false, -1 );
	}

	powerUpSkin = NULL;
	inventory.powerups &= ~( 1 << i );
	inventory.powerupEndTime[ i ] = 0;
	switch( i ) {
		case BERSERK: {
			StopSound( SND_CHANNEL_DEMONIC, false );
			if (!gameLocal.isMultiplayer) {
				StopHealthRecharge();
			}
			break;
		}
		case INVISIBILITY: {
			for( int h = 0; h < 2; h++ )
			{
				if( hands[ h ].weapon.GetEntity() )
				{
					hands[ h ].weapon.GetEntity()->UpdateSkin();
				}
			}
			if( flashlight.GetEntity() )
			{
				flashlight.GetEntity()->UpdateSkin();
			}
			break;
		}
		case HELLTIME: {
			StopSound(SND_CHANNEL_DEMONIC, false);
			break;
		}
		case ENVIROSUIT: {

			hudPowerup = -1;

			// Turn off the envirosuit sound
			if (gameSoundWorld) {
				gameSoundWorld->SetEnviroSuit(false);
			}

			// Take off the helmet and lights
			if (enviroSuitLight.IsValid()) {
				enviroSuitLight.GetEntity()->PostEventMS(&EV_Remove, 0);
			}

			enviroSuitLight = NULL;
			break;
		}
		case INVULNERABILITY: {
			StopSound(SND_CHANNEL_DEMONIC, false);
		}
	}
}

/*
==============
idPlayer::UpdatePowerUps
==============
*/
void idPlayer::UpdatePowerUps( void ) {
	int i;

	if ( !gameLocal.isClient ) {
		for ( i = 0; i < MAX_POWERUPS; i++ ) {

			if ((inventory.powerups & (1 << i)) && inventory.powerupEndTime[i] > gameLocal.time) {
				switch (i) {
					case ENVIROSUIT: {
						if (enviroSuitLight.IsValid()) {
							idAngles lightAng = firstPersonViewAxis.ToAngles();
							idVec3 lightOrg = firstPersonViewOrigin;
							const idDict *lightDef = gameLocal.FindEntityDefDict("envirosuit_light", false);

							idVec3 enviroOffset = lightDef->GetVector("enviro_offset");
							idVec3 enviroAngleOffset = lightDef->GetVector("enviro_angle_offset");

							lightOrg += (enviroOffset.x * firstPersonViewAxis[0]);
							lightOrg += (enviroOffset.y * firstPersonViewAxis[1]);
							lightOrg += (enviroOffset.z * firstPersonViewAxis[2]);
							lightAng.pitch += enviroAngleOffset.x;
							lightAng.yaw += enviroAngleOffset.y;
							lightAng.roll += enviroAngleOffset.z;

							enviroSuitLight.GetEntity()->GetPhysics()->SetOrigin(lightOrg);
							enviroSuitLight.GetEntity()->GetPhysics()->SetAxis(lightAng.ToMat3());
							enviroSuitLight.GetEntity()->UpdateVisuals();
							enviroSuitLight.GetEntity()->Present();
						}

						break;
					}
					default: {
						break;
					}
				}
			}
			if ( PowerUpActive( i ) && inventory.powerupEndTime[i] <= gameLocal.time ) {
				ClearPowerup( i );
			}
		}
	}

	//Lubos BEGIN
	if ( gameLocal.isMultiplayer && entityNumber == gameLocal.localClientNum ) {
		const char *skin;
		spawnArgs.GetString( "skin_invisibility", "", &skin );
		renderEntity.customSkin =  declManager->FindSkin( skin );
		if ( !pVRClientInfo || !pVRClientInfo->weapon_stabilised ) {
			commonVr->currentFlashlightMode = FLASHLIGHT_HAND;
		}
	}
	//Lubos END
	else if ( health > 0 ) {
		if ( powerUpSkin ) {
			renderEntity.customSkin = powerUpSkin;
		} else {
			renderEntity.customSkin = skin;
		}
	}

	if ( healthPool && gameLocal.time > nextHealthPulse && !AI_DEAD && health > 0 ) {
		assert( !gameLocal.isClient );	// healthPool never be set on client
		int amt = ( healthPool > 5 ) ? 5 : healthPool;
		health += amt;
		if ( health > inventory.maxHealth ) {
			health = inventory.maxHealth;
			healthPool = 0;
		} else {
			healthPool -= amt;
		}
		nextHealthPulse = gameLocal.time + HEALTHPULSE_TIME;
		healthPulse = true;
	}

	if ( !gameLocal.inCinematic && influenceActive == 0 && g_skill.GetInteger() == 3 && gameLocal.time > nextHealthTake && !AI_DEAD && health > g_healthTakeLimit.GetInteger() ) {
		assert( !gameLocal.isClient );	// healthPool never be set on client

		if (!PowerUpActive(INVULNERABILITY)) {
			health -= g_healthTakeAmt.GetInteger();
			if ( health < g_healthTakeLimit.GetInteger()) {
				health = g_healthTakeLimit.GetInteger();
			}
		}
		nextHealthTake = gameLocal.time + g_healthTakeTime.GetInteger() * 1000;
		healthTake = true;
	}
}

/*
===============
idPlayer::ClearPowerUps
===============
*/
void idPlayer::ClearPowerUps( void ) {
	int i;
	for ( i = 0; i < MAX_POWERUPS; i++ ) {
		if ( PowerUpActive( i ) ) {
			ClearPowerup( i );
		}
	}
	inventory.ClearPowerUps();

	if (gameLocal.isMultiplayer) {
		if (enviroSuitLight.IsValid()) {
			enviroSuitLight.GetEntity()->PostEventMS(&EV_Remove, 0);
		}
	}
}

/*
===============
idPlayer::GiveInventoryItem
===============
*/
bool idPlayer::GiveInventoryItem( idDict *item ) {
	if ( gameLocal.isMultiplayer && spectating ) {
		return false;
	}
	inventory.items.Append( new idDict( *item ) );
	idItemInfo info;
	const char* itemName = item->GetString( "inv_name" );
	if ( idStr::Cmpn( itemName, STRTABLE_ID, STRTABLE_ID_LENGTH ) == 0 ) {
		info.name = common->GetLanguageDict()->GetString( itemName );
	} else {
		info.name = itemName;
	}
	info.icon = item->GetString( "inv_icon" );
	inventory.pickupItemNames.Append( info );
	if ( hud ) {
		hud->SetStateString( "itemicon", info.icon );
		hud->HandleNamedEvent( "invPickup" );
	}

	if (item->GetInt("inv_powercell") && focusUI) {
		//Reset the powercell count
		int powerCellCount = 0;

		for (int j = 0; j < inventory.items.Num(); j++) {
			idDict *item = inventory.items[ j ];

			if (item->GetInt("inv_powercell")) {
				powerCellCount++;
			}
		}

		focusUI->SetStateInt("powercell_count", powerCellCount);
	}
	return true;
}

//BSM: Implementing this defined function for scripted give inventory items
/*
==============
idPlayer::GiveInventoryItem
==============
*/
bool idPlayer::GiveInventoryItem(const char *name) {
    idDict args;

    args.Set("classname", name);
    args.Set("owner", this->name.c_str());
    gameLocal.SpawnEntityDef(args);
    return true;
}

/*
==============
idPlayer::UpdateObjectiveInfo
==============
 */
void idPlayer::UpdateObjectiveInfo( void ) {
	if ( objectiveSystem == NULL ) {
		return;
	}
	objectiveSystem->SetStateString( "objective1", "" );
	objectiveSystem->SetStateString( "objective2", "" );
	objectiveSystem->SetStateString( "objective3", "" );
	for ( int i = 0; i < inventory.objectiveNames.Num(); i++ ) {
		objectiveSystem->SetStateString( va( "objective%i", i+1 ), "1" );
		objectiveSystem->SetStateString( va( "objectivetitle%i", i+1 ), inventory.objectiveNames[i].title.c_str() );
		objectiveSystem->SetStateString( va( "objectivetext%i", i+1 ), inventory.objectiveNames[i].text.c_str() );
		objectiveSystem->SetStateString( va( "objectiveshot%i", i+1 ), inventory.objectiveNames[i].screenshot.c_str() );
	}
	objectiveSystem->StateChanged( gameLocal.time );
}

/*
===============
idPlayer::GiveObjective
===============
*/
void idPlayer::GiveObjective( const char *title, const char *text, const char *screenshot ) {
	idObjectiveInfo info;
	info.title = title;
	info.text = text;
	info.screenshot = screenshot;
	inventory.objectiveNames.Append( info );
	ShowObjective( "newObjective" );
	if ( hud ) {
		hud->HandleNamedEvent( "newObjective" );
	}
}

/*
===============
idPlayer::CompleteObjective
===============
*/
void idPlayer::CompleteObjective( const char *title ) {
	int c = inventory.objectiveNames.Num();
	for ( int i = 0;  i < c; i++ ) {
		if ( idStr::Icmp(inventory.objectiveNames[i].title, title) == 0 ) {
			inventory.objectiveNames.RemoveIndex( i );
			break;
		}
	}
	ShowObjective( "newObjectiveComplete" );

	if ( hud ) {
		hud->HandleNamedEvent( "newObjectiveComplete" );
	}
}



/*
===============
idPlayer::GiveVideo
===============
*/
void idPlayer::GiveVideo( const char *videoName, idDict *item ) {

	if ( videoName == NULL || *videoName == 0 ) {
		return;
	}

	inventory.videos.AddUnique( videoName );

	if ( item ) {
		idItemInfo info;
		info.name = item->GetString( "inv_name" );
		info.icon = item->GetString( "inv_icon" );
		inventory.pickupItemNames.Append( info );
	}
	if ( hud ) {
		hud->HandleNamedEvent( "videoPickup" );
	}
}

// Carl: Context sensitive VR grabs, and dual wielding
// 0 = right hand, 1 = left hand; true if pressed, false if released; returns true if handled as grab
// WARNING! Called from the input thread?
bool idPlayer::GrabWorld( int hand, bool pressed )
{
    bool b;
    if( !pressed )
    {
        b = hands[ hand ].grabbingWorld;
        hands[ hand ].grabbingWorld = false;
        // We're releasing something
        if( vr_gripMode.GetInteger() == VR_GRIP_HOLD )
        {
            hands[ hand ].releaseVirtualGrab();
            // releasing outside a holster will drop the weapon
            // releasing inside an empty holster should place the weapon in the holster
            // releasing inside a full holster, when vr_mustEmptyHands is true, should drop the weapon
            // releasing inside a full holster, when vr_mustEmptyHands is false, should put the holster in an intermediate state
            // where it contains two weapons, your hand contains nothing, and you are waiting to pick up the other weapon
            // if you then grab in that state, it should pick up the original weapon that was in the holster and show the gun you just put there
            // if you move your hand away from the holster in that state, we could do one of three things (make it a cvar?)
            //   1. move the original weapon into the inventory (accessed via your back) and show the new weapon in the holster
            //   2. keep the original weapon in the holster and drop the weapon you tried to stash on the floor
            //   3. move the new weapon into the inventory and keep the original weapon in the holster
            // releasing inside a back holster should place the weapon in your inventory
            // but should not change the next/previous weapon until your hand leaves the holster
        }
        else if( vr_gripMode.GetInteger() == VR_GRIP_DEAD_AND_BURIED )
        {
            hands[ hand ].releaseVirtualGrab();
            // releasing outside a holster will return the weapon to the holster it came from
            // (if it didn't come from a holster, move it to the hip holster on the same side as the hand)
            // (anything in that holster always gets moved to inventory)
            // releasing inside an empty holster should place the weapon in the holster
            // releasing inside a full holster, when vr_mustEmptyHands is true, should return the weapon to the holster it came from
            // releasing inside a full holster, when vr_mustEmptyHands is false, should put the holster in an intermediate state
            // (option 2. above would seem weird in this case, but we should probably still allow it)
        }

        return b;
    }
    if ( hand == vr_weaponHand.GetInteger() )
        b = WeaponHandImpulseSlot();
    else
        b = OtherHandImpulseSlot();
    hands[hand].grabbingWorld = b;
    if( vr_gripMode.GetInteger() == VR_GRIP_TOGGLE_WITH_DROP )
    {
        if( !b ) // if we didn't grab a holster
        {
            if( hands[ hand ].holdingSomethingDroppable() )
            {
                b = hands[ hand ].releaseVirtualGrab();
            }
        }
        return b;
    }
    return b;
}


/*
===============
idPlayer::GiveSecurity
===============
*/
void idPlayer::GiveSecurity( const char *security ) {
	GetPDA()->SetSecurity( security );
	if ( hud ) {
		hud->SetStateString( "pda_security", "1" );
		hud->HandleNamedEvent( "securityPickup" );
	}

	common->HapticEvent("pda_alarm", 0, 0, 100, 0, 0);
}

/*
===============
idPlayer::GiveEmail
===============
*/
void idPlayer::GiveEmail( const char *emailName ) {

	if ( emailName == NULL || *emailName == 0 ) {
		return;
	}

	inventory.emails.AddUnique( emailName );
	GetPDA()->AddEmail( emailName );

	if ( hud ) {
		hud->HandleNamedEvent( "emailPickup" );
	}

	common->HapticEvent("pda_alarm", 0, 0, 100, 0, 0);
}


/*
===============
idPlayer::GivePDA
===============
*/
void idPlayer::GivePDA( const char *pdaName, idDict *item, bool toggle )
{
	if ( gameLocal.isMultiplayer && spectating ) {
		return;
	}

	if ( item ) {
		inventory.pdaSecurity.AddUnique( item->GetString( "inv_name" ) );
	}

	if ( pdaName == NULL || *pdaName == 0 ) {
		pdaName = "personal";
	}

	const idDeclPDA *pda = static_cast< const idDeclPDA* >( declManager->FindType( DECL_PDA, pdaName ) );

	inventory.pdas.AddUnique( pdaName );

	// Copy any videos over
	for ( int i = 0; i < pda->GetNumVideos(); i++ ) {
		const idDeclVideo *video = pda->GetVideoByIndex( i );
		if ( video ) {
			inventory.videos.AddUnique( video->GetName() );
		}
	}

	// This is kind of a hack, but it works nicely
	// We don't want to display the 'you got a new pda' message during a map load
	if ( gameLocal.GetFrameNum() > 10 ) {
		if ( pda && hud ) {
			idStr pdaName = pda->GetPdaName();
			pdaName.RemoveColors();
			hud->SetStateString( "pda", "1" );
			hud->SetStateString( "pda_text", pdaName );
			const char *sec = pda->GetSecurity();
			hud->SetStateString( "pda_security", ( sec && *sec ) ? "1" : "0" );
			hud->HandleNamedEvent( "pdaPickup" );
		}

		if ( inventory.pdas.Num() == 1 ) {
			GetPDA()->RemoveAddedEmailsAndVideos();
			if ( !objectiveSystemOpen ) {
				if ( toggle ) // Koz: toggle pda renders a fullscreen PDA in normal play, for VR we need to select the pda 'weapon'.
                {
                    common->Printf( "idPlayer::GivePDA calling Select Weapon for PDA\n" );
                    SetupPDASlot( false ); // show flashlight in PDA holster
                    SetupHolsterSlot( vr_weaponHand.GetInteger(), false ); // stash weapon hand's gun in weapon holster
                    hands[ 1 - vr_weaponHand.GetInteger() ].SelectWeapon( weapon_pda, true, false );
                }
			}
			objectiveSystem->HandleNamedEvent( "showPDATip" );
			//ShowTip( spawnArgs.GetString( "text_infoTitle" ), spawnArgs.GetString( "text_firstPDA" ), true );
		}

		if ( inventory.pdas.Num() > 1 && pda->GetNumVideos() > 0 && hud ) {
			hud->HandleNamedEvent( "videoPickup" );
		}
	}
}

/*
===============
idPlayer::FindInventoryItem
===============
*/
idDict *idPlayer::FindInventoryItem( const char *name ) {
	for ( int i = 0; i < inventory.items.Num(); i++ ) {
		const char *iname = inventory.items[i]->GetString( "inv_name" );
		if ( iname && *iname ) {
			if ( idStr::Icmp( name, iname ) == 0 ) {
				return inventory.items[i];
			}
		}
	}
	return NULL;
}

/*
===============
idPlayer::RemoveInventoryItem
===============
*/
void idPlayer::RemoveInventoryItem( const char *name ) {
	idDict *item = FindInventoryItem(name);
	if ( item ) {
		RemoveInventoryItem( item );
	}
}

/*
===============
idPlayer::RemoveInventoryItem
===============
*/
void idPlayer::RemoveInventoryItem( idDict *item ) {
	inventory.items.Remove( item );
	if (item->GetInt("inv_powercell") && focusUI) {
		//Reset the powercell count
		int powerCellCount = 0;

		for (int j = 0; j < inventory.items.Num(); j++) {
			idDict *item = inventory.items[ j ];

			if (item->GetInt("inv_powercell")) {
				powerCellCount++;
			}
		}

		focusUI->SetStateInt("powercell_count", powerCellCount);
	}
	delete item;
}



/*
===============
idPlayer::GiveItem
===============
*/
void idPlayer::GiveItem( const char *itemname ) {
	idDict args;

	args.Set( "classname", itemname );
	args.Set( "owner", name.c_str() );
	gameLocal.SpawnEntityDef( args );
	if ( hud ) {
		hud->HandleNamedEvent( "itemPickup" );
	}
}

/*
==================
idPlayer::SlotForWeapon
==================
*/
int idPlayer::SlotForWeapon( const char *weaponName ) {
	int i;

	for( i = 0; i < MAX_WEAPONS; i++ ) {
		const char *weap = spawnArgs.GetString( va( "def_weapon%d", i ) );
		if ( !idStr::Cmp( weap, weaponName ) ) {
			return i;
		}
	}

	// not found
	return -1;
}

void idPlayer::SnapBodyToView()
{
    idAngles newBodyAngles;
    newBodyAngles = viewAngles;
    newBodyAngles.pitch = 0;
    newBodyAngles.roll = 0;
    newBodyAngles.yaw += commonVr->lastHMDYaw - commonVr->bodyYawOffset;
    newBodyAngles.Normalize180();
    SetViewAngles( newBodyAngles );
}

/*
===============
idPlayer::Reload
===============
*/
void idPlayer::Reload( void ) {
	if ( gameLocal.isClient ) {
		return;
	}

	if ( spectating || gameLocal.inCinematic || influenceActive ) {
		return;
	}

    // Carl: Dual wielding, just reload both weapons
    for( int h = 0; h < 2; h++ )
    {
        idWeapon* weapon = GetWeaponInHand( h );
        if( weapon && weapon->IsLinked() && weapon->hideOffset == 0.0f ) // Koz don't reload when in gui
        {
            weapon->Reload();
        }
    }
}

/*
===============
idPlayer::NextBestWeapon
===============
*/
void idPlayer::NextBestWeapon( void ) {
    hands[ vr_weaponHand.GetInteger() ].NextBestWeapon();
}

/*
===============
idPlayer::NextWeapon
===============
*/
void idPlayer::NextWeapon( void ) {
    hands[ vr_weaponHand.GetInteger() ].NextWeapon();
}

/*
===============
idPlayer::PrevWeapon
===============
*/
void idPlayer::PrevWeapon( void ) {
    hands[ vr_weaponHand.GetInteger() ].PrevWeapon();
}

/*
===============
idPlayer::SelectWeapon
===============
*/
void idPlayer::SelectWeapon( int num, bool force, bool specific ) {
    if( hands[vr_weaponHand.GetInteger()].handExists() )
        hands[vr_weaponHand.GetInteger()].SelectWeapon( num, force, specific );
    else
        hands[1 - vr_weaponHand.GetInteger()].SelectWeapon( num, force, specific );
}

/*
=================
idPlayer::DropWeapon
=================
*/
void idPlayer::DropWeapons( bool died )
{
    for( int h = 0; h < 2; h++ )
        hands[ h ].DropWeapon( died );
    // Carl: Note that the old drop weapon code here looked like this, so perhaps we should pass a parameter to change the throw speed and drop time?
    // item = weapon.GetEntity()->DropItem( 250.0f * forward + 150.0f * up, 500, WEAPON_DROP_TIME, died );
}

void idPlayerHand::GetControllerShake( int & highMagnitude, int & lowMagnitude ) const
{
    if( gameLocal.inCinematic )
    {
        // no controller shake during cinematics
        highMagnitude = 0;
        lowMagnitude = 0;
        return;
    }

    float lowMag = 0.0f;
    float highMag = 0.0f;

    lowMagnitude = 0;
    highMagnitude = 0;

    // use highest values from active buffers
    for( int i = 0; i < MAX_SHAKE_BUFFER; i++ )
    {
        if( gameLocal.fast.time < controllerShakeLowTime[i] )
        {
            if( controllerShakeLowMag[i] > lowMag )
            {
                lowMag = controllerShakeLowMag[i];
            }
        }
        if( gameLocal.fast.time < controllerShakeHighTime[i] )
        {
            if( controllerShakeHighMag[i] > highMag )
            {
                highMag = controllerShakeHighMag[i];
            }
        }
    }

    lowMagnitude = idMath::Ftoi( lowMag * 65535.0f );
    highMagnitude = idMath::Ftoi( highMag * 65535.0f );
}

/*
=================
idPlayerHand::GetCurrentWeaponString
=================
*/
idStr idPlayerHand::GetCurrentWeaponString()
{
    const char* weapon;

    if( currentWeapon >= 0 && owner )
    {
        weapon = owner->spawnArgs.GetString( va( "def_weapon%d", currentWeapon ) );
        return weapon;
    }
    else
    {
        return "";
    }
}

/*
==============
idPlayerHand::Init()
==============
*/
void idPlayerHand::Init( idPlayer* player, int hand )
{
    if( vr_debugHands.GetBool() )
    {
        common->Printf( "\nBefore Init():\n" );
        debugPrint();
    }
    owner = player;
    whichHand = hand;

    currentWeapon = -1;
    idealWeapon = -1;
    previousWeapon = -1;
    weaponSwitchTime = 0;

    grabbingWorld = false;
    triggerDown = false;
    thumbDown = false;
    oldGrabbingWorld = false;
    oldTriggerDown = false;
    oldFlashlightTriggerDown = false;
    oldThumbDown = false;

    laserSightHandle = -1;
    // laser sight for 3DTV
    memset( &laserSightRenderEntity, 0, sizeof( laserSightRenderEntity ) );
    laserSightRenderEntity.hModel = renderModelManager->FindModel( "_BEAM" );
	laserSightRenderEntity.customShader = declManager->FindMaterial( "_white" );
	laserSightRenderEntity.shaderParms[ SHADERPARM_RED ]	= 1.0f;
	laserSightRenderEntity.shaderParms[ SHADERPARM_GREEN ] = 0.0f;
	laserSightRenderEntity.shaderParms[ SHADERPARM_BLUE ]	= 0.0f;
	laserSightRenderEntity.shaderParms[ SHADERPARM_ALPHA ] = 0.4f;
	laserSightRenderEntity.shaderParms[ SHADERPARM_TIMEOFFSET ] = 0.0f;
	laserSightRenderEntity.shaderParms[5] = 0.0f;
	laserSightRenderEntity.shaderParms[6] = 0.0f;
	laserSightRenderEntity.shaderParms[7] = 0.0f;

    crosshairHandle = -1;
    // model to place crosshair or red dot into 3d space
    memset( &crosshairEntity, 0, sizeof( crosshairEntity ) );
    crosshairEntity.hModel = renderModelManager->FindModel( "/models/mapobjects/weaponsight.lwo" );
    crosshairEntity.weaponDepthHack = true;

    lastCrosshairMode = -1;

    throwDirection = vec3_zero;
    throwVelocity = 0.0f;

    PDAfixed = false;
    lastPdaFixed = PDAfixed;
    playerPdaPos = vec3_zero;
    motionPosition = vec3_zero;
    // motionRotation = idQuat_zero;
    wasPDA = false;

    // Koz begin
    //	laserSightActive = vr_weaponSight.GetInteger() == 0;
    // Koz end

    if( vr_debugHands.GetBool() )
    {
        common->Printf( "\nAfter Init():\n" );
        debugPrint();
    }
}

/*
===============
idPlayerHand::PrevWeapon
===============
*/
void idPlayerHand::PrevWeapon()
{
    NextWeapon( -1 );
}

void idPlayerHand::NextBestWeapon()
{
    if( vr_debugHands.GetBool() )
    {
        common->Printf( "\nBefore NextBestWeapon():\n" );
        debugPrint();
    }
    const char* weap;
    int w = MAX_WEAPONS;

    if( !owner->weaponEnabled || !handExists() )
    {
        return;
    }

    // Carl: Dual wielding, handle switching from flashlight properly
    if( holdingFlashlight() )
    {
        if( vr_flashlightStrict.GetBool() )
            commonVr->currentFlashlightMode = FLASHLIGHT_INVENTORY;
        else
            commonVr->currentFlashlightMode = FLASHLIGHT_HEAD;
    }

    while( w > 0 )
    {
        w--;
        if( w == owner->weapon_flashlight )
        {
            continue;
        }
        weap = owner->spawnArgs.GetString( va( "def_weapon%d", w ) );
        if( !weap[ 0 ] || ( ( owner->inventory.weapons & ( 1 << w ) ) == 0 ) || ( !owner->inventory.HasAmmo( weap, true, owner ) ) )
        {
            continue;
        }
        if( !owner->spawnArgs.GetBool( va( "weapon%d_best", w ) ) )
        {
            continue;
        }

        //Some weapons will report having ammo but the clip is empty and
        //will not have enough to fill the clip (i.e. Double Barrel Shotgun with 1 round left)
        //We need to skip these weapons because they cannot be used
        if( owner->inventory.HasEmptyClipCannotRefill( weap, owner ) ) {
            continue;
        }


        // Carl: dual wielding
        if( w != owner->weapon_fists )
        {
            int availableWeaponsOfThisType = 1;
            if( owner->inventory.duplicateWeapons & ( 1 << w ) )
                availableWeaponsOfThisType++;
            // Carl: skip weapons in the holster unless we have a duplicate, TODO make it optional
            if( w == owner->holsteredWeapon )
            {
                availableWeaponsOfThisType--;
                if( availableWeaponsOfThisType <= 0 )
                    continue;
            }
            // Carl: skip weapons in the other hand unless we have a duplicate (dual wielding)
            if( w == owner->hands[ 1 - whichHand ].idealWeapon)
            {
                availableWeaponsOfThisType--;
                if( availableWeaponsOfThisType <= 0 )
                    continue;
            }
        }

        // Carl: Because this is an automatic weapon switch, we don't want to change our other hand, so just skip weapons we can't dual wield
        if( !owner->CanDualWield( w ) && !owner->CanDualWield( owner->hands[1 - whichHand].currentWeapon ) && !owner->CanDualWield( owner->hands[1 - whichHand].idealWeapon) )
        {
            continue;
        }
        break;
    }
    idealWeapon = w;
    weaponSwitchTime = gameLocal.time + WEAPON_SWITCH_DELAY;
    owner->UpdateHudWeapon( whichHand );
    if( vr_debugHands.GetBool() )
    {
        common->Printf( "After NextBestWeapon():\n" );
        debugPrint();
    }

	common->HapticEvent("weapon_switch", 0, 0, 100, 0, 0);
}

/*
===============
idPlayerHand::NextWeapon
===============
*/
void idPlayerHand::NextWeapon( int dir )
{
    if( vr_debugHands.GetBool() )
    {
        if ( dir > 0 )
            common->Printf( "\nBefore NextWeapon():\n" );
        else
            common->Printf( "\nBefore PrevWeapon():\n" );
        debugPrint();
    }

    // Koz dont change weapon if in gui
    if( !owner || commonVr->handInGui || !owner->weaponEnabled || owner->spectating || owner->hiddenWeapon || gameLocal.inCinematic || gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) || owner->health < 0 )
    {
        return;
    }

    if( !handExists() )
        return;

    // check if we have any weapons
    if( !owner->inventory.weapons )
    {
        return;
    }

    // Carl: Dual wielding, handle switching from flashlight properly
    if( holdingFlashlight() )
    {
        if( vr_flashlightStrict.GetBool() )
            commonVr->currentFlashlightMode = FLASHLIGHT_INVENTORY;
        else
            commonVr->currentFlashlightMode = FLASHLIGHT_HEAD;
    }

    int w = idealWeapon;
    while( 1 )
    {
        w += dir;
        if( w >= MAX_WEAPONS )
        {
            w = 0;
        }
        else if( w < 0 )
        {
            w = MAX_WEAPONS - 1;
        }
        if( w == idealWeapon )
        {
            w = owner->weapon_fists;
            break;
        }
        // if we don't have the weapon in our inventory, skip it
        if( ( ( owner->inventory.weapons & ( 1 << w ) ) == 0 && w != owner->weapon_flashlight ) || ( w == owner->weapon_flashlight && !owner->HasHoldableFlashlight() ) )
        {
            continue;
        }
        // Carl: dual wielding
        if( w != owner->weapon_fists )
        {
            int availableWeaponsOfThisType = 1;
            if( owner->inventory.duplicateWeapons & ( 1 << w ) )
                availableWeaponsOfThisType++;
            // Carl: skip weapons in the other hand unless we have a duplicate (dual wielding)
            if( w == owner->hands[1 - whichHand].idealWeapon )
            {
                availableWeaponsOfThisType--;
                if( availableWeaponsOfThisType <= 0 )
                    continue;
            }
            // Carl: optionally skip weapons in the holster unless we have a duplicate
            if( w == owner->holsteredWeapon )
            {
                availableWeaponsOfThisType--;
                if( availableWeaponsOfThisType <= 0 )
                {
                    vr_weaponcycle_t c = (vr_weaponcycle_t)vr_weaponCycleMode.GetInteger();
                    if( c == VR_WEAPONCYCLE_INCLUDE_HOLSTERED || c == VR_WEAPONCYCLE_HOLSTERED_AND_FLASHLIGHT || c == VR_WEAPONCYCLE_HOLSTERED_AND_FLASHLIGHT_AND_PDA )
                    {
                        // Carl: draw weapon from holster
                        owner->FreeHolsterSlot();
                        owner->holsteredWeapon = owner->weapon_fists;
                        memset( &owner->holsterRenderEntity, 0, sizeof( owner->holsterRenderEntity ) );
                        availableWeaponsOfThisType++;
                    }
                    else
                    {
                        // Skip holstered weapon
                        continue;
                    }
                }
            }
        }
        const char* weap = owner->spawnArgs.GetString( va( "def_weapon%d", w ) );
        if( !weap[0] )
        {
            continue;
        }
        // Carl: skip weapons if we can't automagically empty our other hand, and we can't dual wield this weapon.
        // I'm making it so we can't automagically empty the weapon hand to please the non-weapon hand,
        // so the non-weapon hand will only cycle through the dual-wieldable weapons.
        if( ( vr_mustEmptyHands.GetBool() || whichHand == ( 1 - vr_weaponHand.GetInteger() ) ) && !owner->CanDualWield( w ) && !owner->CanDualWield( owner->hands[1 - whichHand].currentWeapon ) && !owner->CanDualWield( owner->hands[1 - whichHand].idealWeapon ) )
        {
            continue;
        }

        //GB don't let it cycle to the flashlight if  we're the weapon hand
        /*if(w == owner->weapon_flashlight && whichHand == vr_weaponHand.GetInteger())
		{
			continue;
		}*/

        // Cycle to the flashlight if we're using our flashlight hand and the flashlight is in our inventory, OR if we set to include the flashlight in the weapon cycle.
        // todo: don't let it cycle to the flashlight if it's in the other hand and we're the weapon hand
        if( w == owner->weapon_flashlight && ( vr_weaponCycleMode.GetInteger() == VR_WEAPONCYCLE_INCLUDE_FLASHLIGHT || vr_weaponCycleMode.GetInteger() == VR_WEAPONCYCLE_HOLSTERED_AND_FLASHLIGHT
                                               || vr_weaponCycleMode.GetInteger() == VR_WEAPONCYCLE_HOLSTERED_AND_FLASHLIGHT_AND_PDA || ( whichHand == ( 1 - vr_weaponHand.GetInteger() ) && commonVr->currentFlashlightMode == FLASHLIGHT_INVENTORY ) ) )
        {
            break;
        }
        else if( w == owner->weapon_pda && vr_weaponCycleMode.GetInteger() == VR_WEAPONCYCLE_HOLSTERED_AND_FLASHLIGHT_AND_PDA )
        {
            // skip the pda if we haven't been given one yet
            if( owner->weapon_pda >= 0 && owner->inventory.pdas.Num() == 0 )
                continue;
            break;
        }
        else if( !owner->spawnArgs.GetBool( va( "weapon%d_cycle", w ) ) )
        {
            continue;
        }

        if( owner->inventory.HasAmmo( weap, true, owner) || w == owner->weapon_bloodstone )
        {
            break;
        }

    }

    if( ( w != currentWeapon ) && ( w != idealWeapon ) )
    {
        // Carl: If we can't dual-wield it?
        if( !owner->CanDualWield( w ) && !owner->CanDualWield( owner->hands[1 - whichHand].currentWeapon ) && !owner->CanDualWield( owner->hands[1 - whichHand].idealWeapon) )
        {
            // if we can't automagically empty our other hand, do nothing
            if( vr_mustEmptyHands.GetBool() )
                return;
            // otherwise, first automagically empty our other hand
            if( owner->CanDualWield( owner->weapon_fists ) )
                owner->hands[1 - whichHand].SelectWeapon( owner->weapon_fists, true, true );
            else
                owner->hands[1 - whichHand].SelectWeapon( weapon_empty_hand, true, true );
        }

        // Carl: Dual wielding, handle switching to flashlight properly
        if( w == owner->weapon_flashlight )
		{
			w = owner->weapon_fists;
			commonVr->currentFlashlightMode = FLASHLIGHT_HAND;
			commonVr->currentFlashlightPosition = FLASHLIGHT_HAND;
			vr_weaponHand.SetInteger( 1 - whichHand );
		}

    	idealWeapon = w;
        weaponSwitchTime = gameLocal.time + WEAPON_SWITCH_DELAY;
        owner->UpdateHudWeapon( whichHand );
        if( vr_debugHands.GetBool() )
            common->Printf( "Changing weapon\n" );

        if (idealWeapon == WEAPON_CHAINSAW)
        {
        	//Start chainsaw idling haptic immediately
            //common->HapticEvent("chainsaw_idle", vr_weaponHand.GetInteger() ? 1 : 2, 1, 100, 0, 0);
        }
        else
		{
            if (currentWeapon == WEAPON_CHAINSAW)
            {
                //Stop all chainsaw haptics immediately
                //common->HapticStopEvent("chainsaw_idle");
                common->HapticStopEvent("chainsaw_fire");
            }

			common->HapticEvent("weapon_switch", 0, 0, 100, 0, 0);
		}
    }

    if( vr_debugHands.GetBool() )
    {
        if( dir > 0 )
            common->Printf( "After NextWeapon():\n" );
        else
            common->Printf( "After PrevWeapon():\n" );
        debugPrint();
    }
}

void idPlayerHand::ResetControllerShake()
{
    for( int i = 0; i < MAX_SHAKE_BUFFER; i++ )
    {
        controllerShakeHighTime[i] = 0;
    }

    for( int i = 0; i < MAX_SHAKE_BUFFER; i++ )
    {
        controllerShakeHighMag[i] = 0.0f;
    }

    for( int i = 0; i < MAX_SHAKE_BUFFER; i++ )
    {
        controllerShakeLowTime[i] = 0;
    }

    for( int i = 0; i < MAX_SHAKE_BUFFER; i++ )
    {
        controllerShakeLowMag[i] = 0.0f;
    }
}

/*
===============
idPlayerHand::SelectWeapon
===============
*/
void idPlayerHand::SelectWeapon( int num, bool force, bool specific )
{
	common->Printf( "Before SelectWeapon(%d, %d, %d, %d):\n", num, force, specific, whichHand);
    const char* weap;

    //if( !owner->weaponEnabled || owner->spectating || gameLocal.inCinematic || owner->health < 0 /*|| commonVr->handInGui*/ ) // Koz don't let the player change weapons if hand is currently in a gui
	if( !owner->weaponEnabled || owner->spectating || gameLocal.inCinematic || owner->health < 0) // GB For some reason this is active when trying to reinstate holstered weapon
    {
        return;
    }

    if( !handExists() )
        return;

    if( num == weapon_empty_hand )
    {
        idealWeapon = owner->weapon_fists;
        // Carl: TODO
        return;
    }

    if( ( num < 0 ) || ( num >= MAX_WEAPONS ) )
    {
        return;
    }

    if( ( num != owner->weapon_pda ) && gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) )
    {
        num = owner->weapon_fists;
        owner->hiddenWeapon ^= 1;
        if( owner->hiddenWeapon && (!game->isVR || commonVr->handInGui == true) )
        {
            if( weapon )
                weapon->LowerWeapon(); // Koz
        }
        else
        {
            if( weapon )
                weapon->RaiseWeapon();
        }
    }

    // Is there an actual weapon for this weapon slot?
    weap = owner->spawnArgs.GetString( va( "def_weapon%d", num ) );
    if( !weap[ 0 ] )
    {
        gameLocal.Printf( "Invalid weapon\n" );
        return;
    }

    //Is the weapon a toggle weapon
    WeaponToggle_t *weaponToggle;

    if (owner->weaponToggles.Get(va("weapontoggle%d", num), &weaponToggle)) {

        int weaponToggleIndex = 0;

    //Find the current Weapon in the list
    int currentIndex = -1;

    for (int i = 0; i < weaponToggle->toggleList.Num(); i++) {
        if (weaponToggle->toggleList[i] == idealWeapon) {
            currentIndex = i;
            break;
        }
    }

    if (currentIndex == -1) {
        //Didn't find the current weapon so select the first item
        weaponToggleIndex = 0;
    } else {
        //Roll to the next available item in the list
        weaponToggleIndex = currentIndex;
        weaponToggleIndex++;

        if (weaponToggleIndex >= weaponToggle->toggleList.Num()) {
            weaponToggleIndex = 0;
        }
    }

    for (int i = 0; i < weaponToggle->toggleList.Num(); i++) {

            //Is it available
            if (owner->inventory.weapons & (1 << weaponToggle->toggleList[weaponToggleIndex])) {
                break;
            }

            weaponToggleIndex++;

            if (weaponToggleIndex >= weaponToggle->toggleList.Num()) {
                weaponToggleIndex = 0;
            }
        }

        num = weaponToggle->toggleList[weaponToggleIndex];
    }

    // Carl: dual wielding
    int availableWeaponsOfThisType = 0;
    if( num == owner->weapon_fists )
    {
        if( owner->inventory.weapons & ( 1 << num ) )
            availableWeaponsOfThisType++;
    }
    else
    {
        if( owner->inventory.weapons & ( 1 << num ) )
            availableWeaponsOfThisType++;
        if( owner->inventory.duplicateWeapons & ( 1 << num ) )
            availableWeaponsOfThisType++;
        // Carl: skip weapons in the holster unless we have a duplicate, TODO make it optional
        //if( num == owner->holsteredWeapon )
        //    availableWeaponsOfThisType--;
        // Carl: skip weapons in the other hand unless we have a duplicate (dual wielding)
        if( num == owner->hands[ 1 - whichHand ].idealWeapon )
            availableWeaponsOfThisType--;
    }

    if( force || availableWeaponsOfThisType > 0 )
    {
        if( !owner->inventory.HasAmmo( weap, true, owner ) && !owner->spawnArgs.GetBool( va( "weapon%d_allowempty", num ) ) )
        {
            return;
        }

        // If we can't we dual-wield it?
        if( !owner->CanDualWield( num ) && !owner->CanDualWield( owner->hands[1 - whichHand].currentWeapon ) && !owner->CanDualWield( owner->hands[1 - whichHand].idealWeapon ) )
        {
            // if we can't automagically empty our other hand, do nothing
            if( vr_mustEmptyHands.GetBool() )
                return;
            // otherwise, first automagically empty our other hand
            if( owner->CanDualWield( owner->weapon_fists ) )
                owner->hands[1 - whichHand].SelectWeapon( owner->weapon_fists, true, true );
            else
                owner->hands[1 - whichHand].SelectWeapon( weapon_empty_hand, true, true );
        }

        // If we made it all this way, we want weapon "num" and we can switch to it
        idealWeapon = num;
        owner->UpdateHudWeapon( whichHand );

		common->HapticEvent("weapon_switch", 0, 0, 100, 0, 0);
    }

	common->Printf( "After SelectWeapon(%d):\n", idealWeapon);
}

/*
========================
idPlayerHand::SetControllerShake
========================
*/
void idPlayerHand::SetControllerShake( float highMagnitude, int highDuration, float lowMagnitude, int lowDuration )
{
    // the main purpose of having these buffer is so multiple, individual shake events can co-exist with each other,
    // for instance, a constant low rumble from the chainsaw when it's idle and a harsh rumble when it's being used.

    // find active buffer with similar magnitude values
    int activeBufferWithSimilarMags = -1;
    int inactiveBuffer = -1;
    for( int i = 0; i < MAX_SHAKE_BUFFER; i++ )
    {
        if( gameLocal.fast.time <= controllerShakeHighTime[i] || gameLocal.fast.time <= controllerShakeLowTime[i] )
        {
            if( idMath::Fabs( highMagnitude - controllerShakeHighMag[i] ) <= 0.1f && idMath::Fabs( lowMagnitude - controllerShakeLowMag[i] ) <= 0.1f )
            {
                activeBufferWithSimilarMags = i;
                break;
            }
        }
        else
        {
            if( inactiveBuffer == -1 )
            {
                inactiveBuffer = i;		// first, inactive buffer..
            }
        }
    }

    if( activeBufferWithSimilarMags > -1 )
    {
        // average the magnitudes and adjust the time
        controllerShakeHighMag[activeBufferWithSimilarMags] += highMagnitude;
        controllerShakeHighMag[activeBufferWithSimilarMags] *= 0.5f;

        controllerShakeLowMag[activeBufferWithSimilarMags] += lowMagnitude;
        controllerShakeLowMag[activeBufferWithSimilarMags] *= 0.5f;

        controllerShakeHighTime[activeBufferWithSimilarMags] = gameLocal.fast.time + highDuration;
        controllerShakeLowTime[activeBufferWithSimilarMags] = gameLocal.fast.time + lowDuration;
        return;
    }

    if( inactiveBuffer == -1 )
    {
        inactiveBuffer = 0;			// FIXME: probably want to use the oldest buffer..
    }

    controllerShakeHighMag[inactiveBuffer] = highMagnitude;
    controllerShakeLowMag[inactiveBuffer] = lowMagnitude;
    controllerShakeHighTime[inactiveBuffer] = gameLocal.fast.time + highDuration;
    controllerShakeLowTime[inactiveBuffer] = gameLocal.fast.time + lowDuration;
}

/*
==============
Koz idPlayerHand::TrackWeaponDirection
keep track of weapon movement to determine direction of motion
==============
*/
void idPlayerHand::TrackWeaponDirection( idVec3 origin )
{
    frameNum += 1;
    if ( frameNum > 9 ) frameNum = 0;
    frameTime[frameNum] = gameLocal.GetTime();
    position[frameNum] = origin;

    startFrameNum = frameNum - 5;
    if ( startFrameNum < 0 ) startFrameNum = 9 - frameNum;

    timeDelta = frameTime[frameNum] - frameTime[startFrameNum];
    if ( timeDelta == 0 ) timeDelta = 1;

    throwDirection = position[frameNum] - position[startFrameNum];
    throwVelocity = ( throwDirection.Length() / timeDelta ) * 1000;
}

bool idPlayerHand::contextToggleVirtualGrab()
{
    return false;
}
bool idPlayerHand::holdingFlashlight()
{
    // Carl: weapon must be set to fist or empty hand when we're using the flashlight
    if( whichHand == vr_weaponHand.GetInteger() || commonVr->currentFlashlightPosition != FLASHLIGHT_HAND || !owner || !owner->flashlight || !owner->flashlight->IsLinked()
        || owner->spectating || !owner->weaponEnabled || owner->hiddenWeapon || gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) )
        return false;
    return true;
}

bool idPlayerHand::holdingWeapon()
{
    if( !controllingWeapon() )
        return false;
    int w = owner->GetCurrentWeaponSlot();
    return w != owner->weapon_fists && w != owner->weapon_soulcube;
}

bool idPlayerHand::floatingWeapon()
{
    if( !controllingWeapon() )
        return false;
    int w = owner->GetCurrentWeaponSlot();
    return w == owner->weapon_soulcube;
}

bool idPlayerHand::controllingWeapon()
{
    /*if( currentWeapon < 0 || !owner || !weapon || weapon->IdentifyWeapon() == WEAPON_PDA || holdingFlashlight()
        || owner->spectating || !owner->weaponEnabled || owner->hiddenWeapon || gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) )*/
    //GB removed flashlight as not a weapon so it can hit people :-)
    if( currentWeapon < 0 || !owner || !weapon || weapon->IdentifyWeapon() == WEAPON_PDA ||
        owner->spectating || !owner->weaponEnabled || owner->hiddenWeapon || gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) )
        return false;
    return true;
}

bool idPlayerHand::holdingPDA()
{
    return (weapon && weapon->IdentifyWeapon() == WEAPON_PDA); // Carl: Is this the only way? Or do I need to check for forced pda?
}

bool idPlayerHand::holdingPhysics()
{
    // Carl todo
    return false;
}

bool idPlayerHand::holdingItem()
{
    // Carl todo
    return false;
}

bool idPlayerHand::holdingSomethingDroppable()
{
    return holdingWeapon() || holdingFlashlight() || holdingPDA() || holdingPhysics() || holdingItem();
}

bool idPlayerHand::isOverMountedFlashlight()
{
    if( !owner || !owner->flashlight.IsValid() || owner->spectating || !owner->weaponEnabled || owner->hiddenWeapon || gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) )
        return false;
    else if( handSlot == SLOT_FLASHLIGHT_HEAD )
        return commonVr->currentFlashlightPosition == FLASHLIGHT_HEAD;
    else if( handSlot == SLOT_FLASHLIGHT_SHOULDER )
        return commonVr->currentFlashlightPosition == FLASHLIGHT_BODY;
    else
        return false;
}

bool idPlayerHand::tooFullToInteract()
{
    return (vr_mustEmptyHands.GetBool() && ( holdingWeapon() || holdingFlashlight() || holdingPDA() || holdingPhysics() || holdingItem() )) || !handExists();
}

bool idPlayerHand::handExists()
{
    return true;
}

bool idPlayerHand::startVirtualGrab()
{
    virtualGrabDown = true;
    return true;
}

void idPlayerHand::DropWeapon( bool died )
{
	if( vr_debugHands.GetBool() )
	{
		common->Printf( "Before DropWeapon():\n" );
		debugPrint();
	}
	idVec3 forward, up;

	assert( !gameLocal.isClient );

	if( !owner || owner->spectating || weaponGone || weapon.GetEntity() == NULL )
		return;

	if( handExists() )
		return;

	idWeapon* weap = weapon.GetEntity();

	if( !weap || ( !died && !weap->IsReady() ) || weap->IsReloading() )
		return;

	// ammoavailable is how many shots we can fire
	// inclip is which amount is in clip right now
	int ammoavailable = weap->AmmoAvailable();
	int inclip = weap->AmmoInClip();

	// don't drop a grenade if we have none left
	if( !idStr::Icmp( idWeapon::GetAmmoNameForNum( weap->GetAmmoType() ), "ammo_grenades" ) && ( ammoavailable - inclip <= 0 ) )
	{
		return;
	}

	ammoavailable += inclip;

	// expect an ammo setup that makes sense before doing any dropping
	// ammoavailable is -1 for infinite ammo, and weapons like chainsaw
	// a bad ammo config usually indicates a bad weapon state, so we should not drop
	// used to be an assertion check, but it still happens in edge cases

	if( ( ammoavailable != -1 ) && ( ammoavailable < 0 ) )
	{
		common->DPrintf( "idPlayer::DropWeapon: bad ammo setup\n" );
		return;
	}
	idEntity* item = NULL;
	// Carl: todo throwing a weapon when you let go of it (also dual wielding)
	if( died )
	{
		// ain't gonna throw you no weapon if I'm dead
		item = weap->DropItem( vec3_origin, 0, -1, died );
	}
	else
	{
		// Carl If we drop it straight down, we'll automatically pick it up immediately (if we're alive), so throw it forward a bit.
		owner->viewAngles.ToVectors( &forward, NULL, &up );
		item = weap->DropItem( 150.0f * forward + 50.0f * up, 500, -1, died );
	}
	if( !item )
		return;
	// set the appropriate ammo in the dropped object
	if( !died )
		ammoavailable = inclip; // Carl: TODO work out what these keys do, don't put ALL our ammo in the drop
	const idKeyValue* keyval = item->spawnArgs.MatchPrefix( "inv_ammo_" );
	if( keyval )
	{
		item->spawnArgs.SetInt( keyval->GetKey(), 0 );// , ammoavailable );
		idStr inclipKey = keyval->GetKey();
		inclipKey.Insert( "inclip_", 4 );
		inclipKey.Insert( va( "%.2d", currentWeapon ), 11 );
		item->spawnArgs.SetInt( inclipKey, inclip );
	}
	if( !died )
	{
		bool isDuplicate = false;
		if( !isDuplicate )
		{
			// remove from our local inventory completely
			owner->inventory.Drop( owner->spawnArgs, item->spawnArgs.GetString( "inv_weapon" ), -1 );
			weap->ResetAmmoClip();
			SelectWeapon( owner->weapon_fists, true, true );
			weap->WeaponStolen();
			weaponGone = true;
		}
		else
		{
			// just remove from our hand
		}
	}
	if( vr_debugHands.GetBool() )
	{
		common->Printf( "After DropWeapon():\n" );
		debugPrint();
	}
}

bool idPlayerHand::releaseVirtualGrab()
{
    if( holdingWeapon() )
    {
        virtualGrabDown = false;
        if( vr_gripMode.GetInteger() != VR_GRIP_DEAD_AND_BURIED )
            DropWeapon( false );
        else
        {
            // return weapon to holster it was drawn from
            // if that holster is full, move its contents to inventory
            // if its contents can't be stored in the inventory, then store the ammo but drop the weapon
        }
        return true;
    }
    else if( holdingFlashlight() )
    {
        // now dropping the flashlight moves it to your inventory
        commonVr->currentFlashlightMode = FLASHLIGHT_INVENTORY;
        //vr_flashlightMode.SetInteger( FLASHLIGHT_BODY );
        //vr_flashlightMode.SetModified();
        return true;
    }
    else if( holdingPDA() )
    {
        // for now, dropping any PDA will put it away
        return true;
    }
    else if( holdingItem() )
    {
        // either use the item
        // or drop the item
        // depending on the settings and where our hand is
        return true;
    }
    else if( holdingPhysics() )
    {
        // throw the physics object
        return true;
    }
    return false;
}

void idPlayerHand::debugPrint()
{
    if( !vr_debugHands.GetBool() )
        return;
    idStr s = "";
    if( vr_weaponHand.GetInteger() == whichHand )
    {
        s = "(Weapon hand) ";
    }
    else if( vr_weaponHand.GetInteger() < 0 || vr_weaponHand.GetInteger() >= 2 )
    {
        s = "(INVALID vr_weaponHand) ";
        common->Printf( "vr_weaponHand = %d\n", vr_weaponHand.GetInteger() );
    }
    else if( commonVr->currentFlashlightPosition == FLASHLIGHT_HAND )
    {
        s = "(Flashlight in hand) ";
    }
    else if( commonVr->currentFlashlightMode == FLASHLIGHT_HAND )
    {
        s = "(Flashlight hand) ";
    }
    if (whichHand == HAND_RIGHT)
        common->Printf( "*** Right Hand %s***\n", s.c_str() );
    else if( whichHand == HAND_LEFT )
        common->Printf( "*** Left Hand %s***\n", s.c_str() );
    else
        common->Printf( "*** Hand %d %s***\n", whichHand, s.c_str() );
    if( !this->owner )
        common->Printf( "  ERROR: No owner!\n" );
    else
        common->Printf( "  owner = '%s'\n", owner->name.c_str() );
    if( !handExists() )
        common->Printf( "  doesn't exist!\n" );

    // gestures
    common->Printf( "  in holster slot %d\n", handSlot );
    s = "";
    if( grabbingWorld )
        s += "grip ";
    if( virtualGrabDown )
        s += "vGrab ";
    if( triggerDown )
        s += "trigger ";
    if( thumbDown )
        s += "thumb ";
    common->Printf( "  buttons = %s", s.c_str() );
    s = "";
    if( oldGrabbingWorld )
        s += "grip ";
    if( oldVirtualGrabDown )
        s += "vGrab ";
    if( oldTriggerDown )
        s += "trigger ";
    if( oldFlashlightTriggerDown )
        s += "flashlight ";
    if( oldThumbDown )
        s += "thumb ";
    common->Printf( ", old = %s\n", s.c_str() );

    // weapon
    s = "";
    if( isTheDuplicate )
        s += " (duplicate copy)";
    if( weaponGone )
        s += ", gone";
    if( currentWeapon == idealWeapon )
        common->Printf( "  current/ideal weapon = %s = %d%s\n", GetCurrentWeaponString().c_str(), currentWeapon, s.c_str() );
    else
    {
        common->Printf( "  currentWeapon = %s = %d%s\n", GetCurrentWeaponString().c_str(), currentWeapon, s.c_str() );
        common->Printf( "  idealWeapon = %d, weaponSwitchTime (unused?) = %dms\n", idealWeapon, weaponSwitchTime - gameLocal.time );
    }
    common->Printf( "  previousWeapon = %d\n", previousWeapon );
    idWeapon* weap = weapon;
    if( weap )
    {
        if( weap->weaponDef )
        {
            common->Printf( "  weapon (%s) = %s = %d\n", weap->GetName(), weap->weaponDef->GetName(), weap->IdentifyWeapon() );
        }
        else
        {
            common->Printf( "  weapon (%s): weaponDef = NULL, IdentifyWeapon = %d\n", weap->GetName(), weap->IdentifyWeapon() );
        }
    }
    else
    {
        common->Printf( "  weapon = NULL\n" );
    }
    // sight
    common->Printf( "  laser sight: active=%d, handle=%d\n", laserSightActive, laserSightHandle );
    common->Printf( "  crosshair: lastMode=%d, handle=%d\n", lastCrosshairMode, crosshairHandle );
    // PDA
    common->Printf( "  PDA (was %d): fixed=%d (last=%d), ", PDAfixed, lastPdaFixed, wasPDA );
    // Functions
    common->Printf( "  holdingWeapon=%d, floatingWeapon=%d, controllingWeapon=%d, tooFullToInteract=%d\n", holdingWeapon(), floatingWeapon(), controllingWeapon(), tooFullToInteract() );
    common->Printf( "  holdingFlashlight=%d, isOverMountedFlashlight=%d, holdingPDA=%d, holdingPhysics=%d, holdingItem=%d, holdingSomethingDroppable=%d\n", holdingFlashlight(), isOverMountedFlashlight(), holdingPDA(), holdingPhysics(), holdingItem(), holdingSomethingDroppable() );
}

/*
=================
idPlayer::StealWeapon
steal the target player's current weapon
=================
*/
void idPlayer::StealWeapon( idPlayer *player ) {
	assert( !gameLocal.isClient );

	// make sure there's something to steal
    int h = player->GetBestWeaponHandToSteal( this );
	//idWeapon *player_weapon = static_cast< idWeapon * >( player->weapon );
    idWeapon* player_weapon = player->GetWeaponInHand( h );
	if ( !player_weapon || !player_weapon->CanDrop() || weaponGone ) {
		return;
	}
	// steal - we need to effectively force the other player to abandon his weapon
    int newweap = player->hands[ h ].currentWeapon;
	if ( newweap == -1 ) {
		return;
	}
	// might be just dropped - check inventory
	if ( ! ( player->inventory.weapons & ( 1 << newweap ) ) ) {
		return;
	}
	const char *weapon_classname = spawnArgs.GetString( va( "def_weapon%d", newweap ) );
	assert( weapon_classname );
	int ammoavailable = player_weapon->AmmoAvailable();
	int inclip = player_weapon->AmmoInClip();
	ammoavailable += inclip;
	if ( ( ammoavailable != -1 ) && ( ammoavailable - inclip < 0 ) ) {
		// see DropWeapon
		common->DPrintf( "idPlayer::StealWeapon: bad ammo setup\n" );
		// we still steal the weapon, so let's use the default ammo levels
		inclip = -1;
		const idDeclEntityDef *decl = gameLocal.FindEntityDef( weapon_classname );
		assert( decl );
		const idKeyValue *keypair = decl->dict.MatchPrefix( "inv_ammo_" );
		assert( keypair );
		ammoavailable = atoi( keypair->GetValue() );
	}

	player_weapon->WeaponStolen();
	player->inventory.Drop( player->spawnArgs, NULL, newweap );
	player->SelectWeapon( weapon_fists, false );
	// in case the robbed player is firing rounds with a continuous fire weapon like the chaingun/plasma etc.
	// this will ensure the firing actually stops
    player->hands[ h ].weaponGone = true;

	// give weapon, setup the ammo count
    int myhand = vr_weaponHand.GetInteger();
	Give( "weapon", weapon_classname , -1);
	ammo_t ammo_i = player->inventory.AmmoIndexForWeaponClass( weapon_classname, NULL );
    hands[ myhand ].idealWeapon = newweap;
	inventory.ammo[ ammo_i ] += ammoavailable;
	inventory.clip[ newweap ] = inclip;
}

/*
===============
idPlayer::ActiveGui
===============
*/
idUserInterface *idPlayer::ActiveGui( void ) {
	if (fileSystem->RunningPhobos()) {
		if (itemSystemOpen) {
			return itemSystem;
		}
	} else if ( objectiveSystemOpen ) {
		return objectiveSystem;
	}

	return focusUI;
}

/*
===============
idPlayer::Weapon_Combat
===============
*/
void idPlayer::Weapon_Combat( void ) {
	if ( influenceActive || !weaponEnabled || gameLocal.inCinematic || privateCameraView ) {
        commonVr->ForceChaperone(0, false);
	    return;
	}

    // Carl: check if we're reloading either weapon
    bool reloading = false;
    for( int h = 0; h < 2; h++ )
    {
        hands[h].weapon->RaiseWeapon();
        if( hands[h].weapon->IsReloading() )
        {
            if( !AI_RELOAD )
            {
                reloading = true;
                AI_RELOAD = true;
                SetState( "ReloadWeapon" );
                UpdateScript();
            }
        }
    }
    if( !reloading )
        AI_RELOAD = false;

    for( int h = 0; h < 2; h++ )
    {
        // Carl: If we're trying to change to the soul cube, but the soul cube is already flying towards the enemy, stop changing weapon
        if( hands[h].idealWeapon == weapon_soulcube && soulCubeProjectile != NULL )
        {
            hands[h].idealWeapon = hands[h].currentWeapon;
        }
        // Carl: Otherwise, change to the chosen weapon before we fire
        // Carl: TODO dual wielding, currently this only does the vr_weaponHand hand
        if( hands[ h ].idealWeapon != hands[ h ].currentWeapon &&  hands[ h ].idealWeapon < MAX_WEAPONS )
        {
            // multiplayer stuff
            if( weaponCatchup )
            {
                assert( gameLocal.isClient );

                hands[h].currentWeapon = hands[h].idealWeapon;
                weaponGone = false;
                hands[h].animPrefix = spawnArgs.GetString( va( "def_weapon%d", hands[h].currentWeapon ) );
                // Carl: Dual Wielding, we need to choose whether this is the original weapon or the duplicate
                // Carl: if we're already holding the same weapon in the other hand, use the opposite copy
                // Carl: otherwise use whichever copy has more ammo in it
                if( hands[h].currentWeapon == hands[1 - h].currentWeapon )
                    hands[h].isTheDuplicate = !hands[1 - h].isTheDuplicate;
                else
                    hands[h].isTheDuplicate = inventory.GetClipAmmoForWeapon( hands[h].currentWeapon, true ) > inventory.GetClipAmmoForWeapon( hands[h].currentWeapon, false );
                //hands[h].weapon->GetWeaponDef( hands[h].animPrefix, inventory.GetClipAmmoForWeapon( hands[h].currentWeapon, hands[h].isTheDuplicate ) );
                hands[h].animPrefix.Strip( "weapon_" );

                hands[h].weapon->NetCatchup();
                const function_t* newstate = GetScriptFunction( "NetCatchup" );
                if( newstate )
                {
                    SetState( newstate );
                    UpdateScript();
                }
                weaponCatchup = false;
            }
            else
            {
                if( hands[ h ].weapon->IsReady() )
                {
                    hands[ h ].weapon->PutAway();
                }

                if( hands[ h ].weapon->IsHolstered() )
                {
                    assert( hands[h].idealWeapon >= 0 );
                    assert( hands[h].idealWeapon < MAX_WEAPONS );

                    if( hands[h].currentWeapon != weapon_pda && !spawnArgs.GetBool( va( "weapon%d_toggle", hands[h].currentWeapon ) ) )
                    {
                        hands[h].previousWeapon = hands[h].currentWeapon;
                    }
                    hands[h].currentWeapon = hands[h].idealWeapon;
                    weaponGone = false;
                    hands[h].weaponGone = false;
                    hands[h].animPrefix = spawnArgs.GetString( va( "def_weapon%d", hands[h].currentWeapon ) );
                    // Carl: Dual Wielding, we need to choose whether this is the original weapon or the duplicate
                    // Carl: if we're already holding the same weapon in the other hand, use the opposite copy
                    // Carl: otherwise use whichever copy has more ammo in it
                    if( hands[h].currentWeapon == hands[1 - h].currentWeapon )
                        hands[h].isTheDuplicate = !hands[1 - h].isTheDuplicate;
                    else
                        hands[h].isTheDuplicate = inventory.GetClipAmmoForWeapon( hands[h].currentWeapon, true ) > inventory.GetClipAmmoForWeapon( hands[h].currentWeapon, false );
                    hands[h].weapon->GetWeaponDef( hands[h].animPrefix, inventory.GetClipAmmoForWeapon( hands[h].currentWeapon, hands[h].isTheDuplicate ) );
                    hands[h].animPrefix.Strip( "weapon_" );

                    hands[h].weapon->Raise();
                    if( hands[h].holdingFlashlight() )
                        PlayAnim( h ? ANIMCHANNEL_LEFTHAND : ANIMCHANNEL_RIGHTHAND, "flashlight_idle" );
                    else
                        PlayAnim( h ? ANIMCHANNEL_LEFTHAND : ANIMCHANNEL_RIGHTHAND, "idle" );
                }
            }
        }
        else // Carl: If we're already using the chosen weapon
        {
            weaponGone = false;	// if you drop and re-get weap, you may miss the = false above
            hands[ h ].weaponGone = false;
            if( hands[ h ].weapon->IsHolstered() )
            {
                if( !hands[ h ].weapon->AmmoAvailable() )
                {
                    // weapons can switch automatically if they have no more ammo
                    hands[ h ].NextBestWeapon();
                }
                else
                {
                    risingWeaponHand = h;
                    hands[ risingWeaponHand ].weapon->Raise();
                    state = GetScriptFunction( "RaiseWeapon" );
                    if( state )
                    {
                        SetState( state );
                    }
                    if( !hands[h].holdingFlashlight() )
                        PlayAnim( h ? ANIMCHANNEL_LEFTHAND : ANIMCHANNEL_RIGHTHAND, "idle" );
                }
            }
        }
    }

	// check for attack
	AI_WEAPON_FIRED = false;
	if ( !influenceActive ) {
        // Carl Dual wielding - check both hands for weapons being fired
        for( int h = 0; h < 2; h++ )
        {
            bool pullingTrigger = hands[ h ].triggerDown;
            // Check if we're actually turning our helmet/armour-mounted light on or off instead of firing
            if( pullingTrigger && hands[ h ].isOverMountedFlashlight() && !hands[ h ].tooFullToInteract() && !hands[h].oldFlashlightTriggerDown )
            {
                pullingTrigger = false;
                if( flashlight->lightOn )
                    FlashlightOff();
                else
                    FlashlightOn();
                hands[ h ].oldFlashlightTriggerDown = true;
            }

            if( (pullingTrigger && !hands[ h ].oldFlashlightTriggerDown ) || ( usercmd.buttons & BUTTON_ATTACK && h == vr_weaponHand.GetInteger()) || (pVRClientInfo != nullptr && pVRClientInfo->velocitytriggeredoffhandstate && h == 1 - vr_weaponHand.GetInteger()))
            {
                //common->Printf( "trigger down\n" );
                if( hands[ h ].controllingWeapon() && !hands[h].weaponGone && !weaponGone )
                {
                    FireWeapon( h, GetWeaponInHand( h ) );
                    if( !hands[h].oldTriggerDown )
                        PlayAnim( h ? ANIMCHANNEL_LEFTHAND : ANIMCHANNEL_RIGHTHAND, "fire1" );
                    velocityPunched = true;
                }
            }
            else if( (hands[ h ].oldTriggerDown && !hands[ h ].oldFlashlightTriggerDown) || (oldButtons & BUTTON_ATTACK && h == vr_weaponHand.GetInteger()) || (velocityPunched && h == 1 - vr_weaponHand.GetInteger()))
            {
                //common->Printf( "old trigger down\n" );
                AI_ATTACK_HELD = false;
                GetWeaponInHand( h )->EndAttack();
                PlayAnim( h ? ANIMCHANNEL_LEFTHAND : ANIMCHANNEL_RIGHTHAND, "idle" );
                velocityPunched = false;
            }
            // remember the old state
            if( hands[ h ].oldFlashlightTriggerDown && !hands[ h ].triggerDown )
            {
                hands[ h ].oldFlashlightTriggerDown = false;
            }
            hands[ h ].oldTriggerDown = hands[ h ].triggerDown;
        }
	}

	// update our ammo clip in our inventory
    // update our ammo clip in our inventory
    for( int h = 0; h < 2; h++ ) {
        if ((hands[h].currentWeapon >= 0) && (hands[h].currentWeapon < MAX_WEAPONS)) {
            inventory.clip[ hands[h].currentWeapon ] = hands[h].weapon->AmmoInClip();
            if ( hud && ( hands[h].currentWeapon == hands[h].idealWeapon ) ) {
                UpdateHudAmmo(hud, h);
            }
        }
    }
}

/*
===============
idInventory::GetClipAmmoForWeapon
===============
*/
int idInventory::GetClipAmmoForWeapon( const int weapon, const bool duplicate ) const
{
	if( duplicate )
		return clipDuplicate[weapon];
	else
		return clip[weapon];
}

/*
===============
idPlayer::Weapon_NPC
===============
*/
void idPlayer::Weapon_NPC( void ) {
    if( hands[0].idealWeapon != hands[0].currentWeapon || hands[ 1 ].idealWeapon != hands[ 1 ].currentWeapon )
    {
        Weapon_Combat();
    }
	StopFiring();
    for ( int h=0; h<2; h++ )
        hands[h].weapon->LowerWeapon();

    int talkButtons = 0;
    talkButtons |= BUTTON_ATTACK | BUTTON_USE;
    bool wasDown = ( oldButtons & talkButtons ) != 0;
    bool isDown = ( usercmd.buttons & talkButtons ) != 0;
    if ( isDown && !wasDown )
    {
        buttonMask |= BUTTON_ATTACK;
        focusCharacter->ListenTo( this );
    }
    else if ( wasDown && !isDown )
    {
        focusCharacter->TalkTo( this );
    }

    /*if ( ( usercmd.buttons & BUTTON_ATTACK ) && !( oldButtons & BUTTON_ATTACK ) ) {
		buttonMask |= BUTTON_ATTACK;
		focusCharacter->TalkTo( this );
	}*/
}

/*
===============
idPlayer::ClickableCallScript
===============
*/
bool idPlayer::ClickableCallScript(idStr funcname, int delay = 0) {
  function_t *scriptFunction;
  idThread *thread;

  bool called = false;

  if (funcname.Length()) {
    scriptFunction = gameLocal.program.FindFunction(funcname);
    if (scriptFunction == NULL) {
      gameLocal.Warning("clickable calls unknown function '%s'",
                        funcname.c_str());
    }
  } else {
    scriptFunction = NULL;
  }

  if (scriptFunction) {
    called = true;
    thread = new idThread(scriptFunction);
    thread->DelayedStart(delay);
  }

  return called;
}

/*
==============
idPlayer::ToggleItemSystem
==============
*/
void idPlayer::ToggleItemSystem(void) {
  if (itemSystem == NULL) {
    return;
  }

  if (focusClickable && focusClickable->spawnArgs.GetInt("inspect_delay")) {
    ClickableCallScript(
        focusClickable->spawnArgs.GetString("inspect_delay_call"));
    return;
  }

  if (!itemSystemOpen) {
    if (focusClickable == NULL) {
      return;
    }

    int numItems = focusClickable->spawnArgs.GetInt("click_inspect_numitems");

    itemSystem->SetStateInt("numitems", numItems);
    itemSystem->SetStateFloat("aspectCorrection",
                              gameLocal.CalculateUIAspectCorrection());

    itemSystemCallExit = focusClickable->spawnArgs.GetString("call_exit");

    for (int i = 1; i <= numItems; i++) {
      const char *itemTexPath =
          focusClickable->spawnArgs.GetString(va("click_inspect_item%i", i));
      const char *itemSeenScript = focusClickable->spawnArgs.GetString(
          va("click_inspect_item%i_seen", i));
      itemSystem->SetStateString(va("item%i", i), itemTexPath);
      itemSystem->SetStateString(va("item%i_seen", i), itemSeenScript);
    }

    itemSystem->Activate(true, gameLocal.time);
  } else {
    ClickableCallScript(itemSystemCallExit);
    itemSystemCallExit.Clear();
    itemSystem->Activate(false, gameLocal.time);
  }
  itemSystemOpen ^= 1;
}

/*
===============
idPlayer::Weapon_Clickable
===============
*/
void idPlayer::Weapon_Clickable(void) {
  if (focusClickable == NULL) {
    return;
  }

  if (hands[ vr_weaponHand.GetInteger() ].idealWeapon != hands[ vr_weaponHand.GetInteger() ].currentWeapon) {
    Weapon_Combat();
  }
  StopFiring();
  hands[ vr_weaponHand.GetInteger() ].weapon.GetEntity()->LowerWeapon();

  if ((usercmd.buttons & BUTTON_ATTACK) && !(oldButtons & BUTTON_ATTACK)) {
    buttonMask |= BUTTON_ATTACK;

    if (nextTriggerTime > gameLocal.time) {
      // can't retrigger until the wait is over
      return;
    }

    bool clicked = false;
    bool callSuccess =
        ClickableCallScript(focusClickable->spawnArgs.GetString("call"));
    int clickOnce = focusClickable->spawnArgs.GetInt("clickonce");
    int clickInspect = focusClickable->spawnArgs.GetInt("click_inspect");

    if (clickInspect) {
      ToggleItemSystem();
    }

    if (callSuccess) {
      clicked = true;
    }

    idStr targetname = focusClickable->spawnArgs.GetString("target");
    if (targetname) {
      clicked = true;
      focusClickable->ActivateTargets(this);
    }

    if (!clicked) {
      return;
    }

    // don't allow it to trigger twice in a single frame
    nextTriggerTime = gameLocal.time + 1;
    // add one second delay until we can click again
    nextTriggerTime += SEC2MS(1);

    if (clickOnce) {
      if (clickOnce > 1) {
        focusClickable->spawnArgs.SetInt("clickonce", clickOnce--);
      } else {
        focusClickable->spawnArgs.SetInt("click_item", 0);
        focusClickable = NULL;
        if (hud) {
          hud->HandleNamedEvent("hideNPC");
        }
      }
    }
  }
}

/*
==================
idPlayer::Event_WeaponAvailable
==================
*/
void idPlayer::Event_WeaponAvailable( const char* name )
{
	idThread::ReturnInt( WeaponAvailable( name ) ? 1 : 0 );
}

bool idPlayer::WeaponAvailable( const char* name )
{
	for( int i = 0; i < MAX_WEAPONS; i++ )
	{
		if( inventory.weapons & ( 1 << i ) )
		{
			const char* weap = spawnArgs.GetString( va( "def_weapon%d", i ) );
			if( !idStr::Cmp( weap, name ) )
			{
				return true;
			}
		}
	}
	return false;
}

/*
===============
idPlayer::WeaponLoweringCallback
===============
*/
void idPlayer::WeaponLoweringCallback( void ) {
	SetState( "LowerWeapon" );
	UpdateScript();
}

/*
===============
idPlayer::WeaponRisingCallback
===============
*/
void idPlayer::WeaponRisingCallback( void ) {
	SetState( "RaiseWeapon" );
	UpdateScript();
}

/*
===============
idPlayer::Weapon_GUI
===============
*/
void idPlayer::Weapon_GUI( void ) {

	if ( !objectiveSystemOpen || (fileSystem->RunningPhobos() && !itemSystemOpen) ) {
        if( hands[ 0 ].idealWeapon != hands[ 0 ].currentWeapon || hands[ 1 ].idealWeapon != hands[ 1 ].currentWeapon )
        {
            Weapon_Combat();
        }
		StopFiring();
        for( int h = 0; h < 2; h++ )
        {
            hands[ h ].weapon->LowerWeapon();
            hands[ h ].weapon->GetRenderEntity()->allowSurfaceInViewID = -1;
            hands[ h ].weapon->GetRenderEntity()->suppressShadowInViewID = entityNumber + 1;
        }
	}

	// disable click prediction for the GUIs. handy to check the state sync does the right thing
	if ( gameLocal.isClient && !net_clientPredictGUI.GetBool() ) {
		return;
	}

	if ( ( oldButtons ^ usercmd.buttons ) & BUTTON_ATTACK ) {
		sysEvent_t ev;
		const char *command = NULL;
		bool updateVisuals = false;

		idUserInterface *ui = ActiveGui();
		if ( ui ) {
			ev = sys->GenerateMouseButtonEvent( 1, ( usercmd.buttons & BUTTON_ATTACK ) != 0 );
			command = ui->HandleEvent( &ev, gameLocal.time, &updateVisuals );
			if ( updateVisuals && focusGUIent && ui == focusUI ) {
				focusGUIent->UpdateVisuals();
			}
		}
		if ( gameLocal.isClient ) {
			// we predict enough, but don't want to execute commands
			return;
		}
		if ( focusGUIent ) {
			HandleGuiCommands( focusGUIent, command );
		} else {
			HandleGuiCommands( this, command );
		}
	}
}

/*
===============
idPlayer::UpdateWeapon
===============
*/
void idPlayer::UpdateWeapon( void ) {
	if ( health <= 0 ) {
		return;
	}

	assert( !spectating );

	if ( gameLocal.isClient ) {
		// clients need to wait till the weapon and it's world model entity
		// are present and synchronized ( weapon.worldModel idEntityPtr to idAnimatedEntity )
		if( !hands[ 0 ].weapon.GetEntity() || !hands[ 0 ].weapon.GetEntity()->IsWorldModelReady() ) {
			return;
		}
		if( !hands[ 1 ].weapon.GetEntity() || !hands[ 1 ].weapon.GetEntity()->IsWorldModelReady() ) {
			return;
		}
	}

	// always make sure the weapons are correctly setup before accessing them
    for( int h = 0; h < 2; h++ )
    {
        if( !hands[h].weapon->IsLinked() )
        {
            if( hands[ h ].idealWeapon == -1 )
                hands[ h ].idealWeapon = 0;
            if( hands[ h ].idealWeapon != -1 )
            {
                animPrefix = spawnArgs.GetString( va( "def_weapon%d", hands[h].idealWeapon) );
                // Carl: Dual Wielding, we need to choose whether this is the original weapon or the duplicate
                // if we're already holding the same weapon in the other hand, use the opposite copy
                // otherwise use whichever copy has more ammo in it
                if( hands[1-h].weapon->IsLinked() && hands[h].idealWeapon == hands[1 - h].idealWeapon )
                    hands[h].isTheDuplicate = !hands[1 - h].isTheDuplicate;
                else
                    hands[h].isTheDuplicate = inventory.GetClipAmmoForWeapon( hands[h].idealWeapon, true ) > inventory.GetClipAmmoForWeapon( hands[h].idealWeapon, false );
                int ammoInClip = inventory.GetClipAmmoForWeapon( hands[ h ].idealWeapon, hands[h].isTheDuplicate );

                hands[ h ].weapon->GetWeaponDef( animPrefix, inventory.clip[ hands[ h ].idealWeapon ] );
                assert( hands[ h ].weapon->IsLinked() );
            }
            else
            {
                return;
            }
        }
    }

	if ( hiddenWeapon && tipUp && usercmd.buttons & BUTTON_ATTACK ) {
		HideTip();
	}

	if ( g_dragEntity.GetBool() ) {
		StopFiring();
		for( int h = 0; h < 2; h++ )
			hands[ h ].weapon.GetEntity()->LowerWeapon();
		dragEntity.Update( this );
	} else if ( ActiveGui() ) {
		// gui handling overrides weapon use
		Weapon_GUI();
	} else	if ( focusCharacter && ( focusCharacter->health > 0 ) ) {
		Weapon_NPC();
	} else if ( focusClickable ) {
		Weapon_Clickable();
	} else {
		Weapon_Combat();
	}

	if ( hiddenWeapon ) {
        if( !game->isVR || !pVRClientInfo || commonVr->handInGui == false )
        {
            for( int h = 0; h < 2; h++ )
                hands[ h ].weapon->LowerWeapon();  // KOZ FIXME HIDE WEAPon
        }
	} else {
        for (int h = 0; h < 2; h++)
            hands[h].weapon->GetRenderEntity()->suppressShadowInViewID = 0;
    }

    if( game->isVR && pVRClientInfo && commonVr->handInGui )
    {
        for( int h = 0; h < 2; h++ )
            hands[ h ].weapon->GetRenderEntity()->suppressShadowInViewID = entityNumber + 1;
    }

    if (fileSystem->RunningPhobos() && influenceActive > INFLUENCE_LEVEL3) {
        hands[ vr_weaponHand.GetInteger() ].weapon.GetEntity()->EnterCinematic();
    }

    // update weapon state, particles, dlights, etc
    for( int h = 0; h < 2; h++ )
        hands[ h ].weapon->PresentWeapon( CanShowWeaponViewmodel(), h );
}

/*
===============
idPlayer::SpectateFreeFly
===============
*/
void idPlayer::SpectateFreeFly( bool force ) {
	idPlayer	*player;
	idVec3		newOrig;
	idVec3		spawn_origin;
	idAngles	spawn_angles;

	player = gameLocal.GetClientByNum( spectator );
	if ( force || gameLocal.time > lastSpectateChange ) {
		spectator = entityNumber;
		if ( player && player != this && !player->spectating && !player->IsInTeleport() ) {
			newOrig = player->GetPhysics()->GetOrigin();
			if ( player->physicsObj.IsCrouching() ) {
				newOrig[ 2 ] += pm_crouchviewheight.GetFloat();
			} else {
				newOrig[ 2 ] += pm_normalviewheight.GetFloat();
			}
			newOrig[ 2 ] += SPECTATE_RAISE;
			idBounds b = idBounds( vec3_origin ).Expand( pm_spectatebbox.GetFloat() * 0.5f );
			idVec3 start = player->GetPhysics()->GetOrigin();
			start[2] += pm_spectatebbox.GetFloat() * 0.5f;
			trace_t t;
			// assuming spectate bbox is inside stand or crouch box
			gameLocal.clip.TraceBounds( t, start, newOrig, b, MASK_PLAYERSOLID, player );
			newOrig.Lerp( start, newOrig, t.fraction );
			SetOrigin( newOrig );
			idAngles angle = player->viewAngles;
			angle[ 2 ] = 0;
			SetViewAngles( angle );
		} else {
			SelectInitialSpawnPoint( spawn_origin, spawn_angles );
			spawn_origin[ 2 ] += pm_normalviewheight.GetFloat();
			spawn_origin[ 2 ] += SPECTATE_RAISE;
			SetOrigin( spawn_origin );
			SetViewAngles( spawn_angles );
		}
		lastSpectateChange = gameLocal.time + 500;
	}
}

/*
===============
idPlayer::SpectateCycle
===============
*/
void idPlayer::SpectateCycle( void ) {
	idPlayer *player;

	if ( gameLocal.time > lastSpectateChange ) {
		int latchedSpectator = spectator;
		spectator = gameLocal.GetNextClientNum( spectator );
		player = gameLocal.GetClientByNum( spectator );
		assert( player ); // never call here when the current spectator is wrong
		// ignore other spectators
		while ( latchedSpectator != spectator && player->spectating ) {
			spectator = gameLocal.GetNextClientNum( spectator );
			player = gameLocal.GetClientByNum( spectator );
		}
		lastSpectateChange = gameLocal.time + 500;
	}
}

/*
===============
idPlayer::UpdateSpectating
===============
*/
void idPlayer::UpdateSpectating( void ) {
	assert( spectating );
	assert( !gameLocal.isClient );
	assert( IsHidden() );
	idPlayer *player;
	if ( !gameLocal.isMultiplayer ) {
		return;
	}
	player = gameLocal.GetClientByNum( spectator );
	if ( !player || ( player->spectating && player != this ) ) {
		SpectateFreeFly( true );
	} else if ( usercmd.upmove > 0 ) {
		SpectateFreeFly( false );
	} else if ( usercmd.buttons & BUTTON_ATTACK ) {
		SpectateCycle();
	}
}

/*
Koz
idPlayer::UpdateTeleportAim

equation for parabola :	y = y0 + vy0 * t - .5 * g * t^2
						x = x0 + vx0 * t
*/

void idPlayer::UpdateTeleportAim()// idVec3 beamOrigin, idMat3 beamAxis )// idVec3 p0, idVec3 v0, idVec3 a, float dist, int points, idVec3 hitLocation, idVec3 hitNormal, float timeToHit )
{
    // teleport target is a .md5 model
    // model has 3 components:
    // the aiming beam comprised of a ribbon with 23 segments/24  ( joints teleportBeamJoint[ 0 - 23 ] )
    // the telepad itself (big circle with the fx and cylinder - joint teleportPadJoint )
    // the center aiming dot ( terminates the beam, shown whenever beam is on - joint teleportCenterPadJoint )
    // the origin of the model should be set to the starting point of the aiming beam
    // teleportBeamJoint[0] should also be set to the origin
    // teleportBeamJoint[1 - 22] trace the arc
    // teleportBeamJoint[23] and teleportCenterPadJoint should be set to the end position of the beam
    // teleportPadJoint should be set to the position of the teleport target ( can be different from beam if aim assist is active )

    const int slTime = 200; // 200ms, remove vr_teleportSlerpTime.GetFloat();

    const float grav = 9.81f * 39.3701f;

    static bool pleaseDuck = false; // Low Headroom Please Duck

    float numPoints = vr_teleportMaxPoints.GetFloat();// 24;
    float vel = vr_teleportVel.GetFloat();
    float dist = vr_teleportDist.GetFloat();

    trace_t traceResults;
    idVec3 last = vec3_zero;
    idVec3 next = vec3_zero;
    idVec3 endPos = vec3_zero;

    idVec3 up = idVec3( 0, 0, 1 );

    idMat3 padAxis = mat3_identity;
    float t = 0;

    idMat3 forward = mat3_identity;
    float beamAngle = 0.0f;
    float vx, vz = 0.0f;
    float z0 = 0.0f;
    float tDisX = 0.0f;
    float zDelt, xDelt = 0.0f;

    idVec2 st, en, df = vec2_zero;
    idVec3 jpos = vec3_zero;

    static bool isShowing = false;
    static bool wasShowing = false;

    static idVec3 beamOrigin = vec3_zero;
    static idMat3 beamAxis = mat3_identity;

    bool showTeleport = ( vr_teleport.GetInteger() > 1 || ( !commonVr->VR_USE_MOTION_CONTROLS && vr_teleport.GetInteger() > 0 ) ) && commonVr->teleportButtonCount != 0;
    static bool lastShowTeleport = false;

    if ( !lastShowTeleport )
    {
        isShowing = false;
    }

    lastShowTeleport = showTeleport;

    pleaseDuck = false;

    if ( !showTeleport || !GetTeleportBeamOrigin( beamOrigin, beamAxis ) )
    {
        if (vr_teleport.GetInteger() != 1)
            aimValidForTeleport = false;

        if (teleportTarget) teleportTarget->Hide();
        return;
    }

    aimValidForTeleport = false;

    forward = idAngles(0.0f, beamAxis.ToAngles().yaw, 0.0f).ToMat3();
    teleportTarget->SetAxis( forward );

    beamAngle = idMath::ClampFloat( -65.0f, 65.0f, beamAxis.ToAngles().pitch );

    dist *= idMath::Cos( DEG2RAD( beamAngle ) ); // we want to be able to aim farther horizontally than vertically, so modify velocity and dist based on pitch.
    vel *= idMath::Cos( DEG2RAD( beamAngle ) );

    vx = vel * idMath::Cos( DEG2RAD( beamAngle ));
    vz = vel * idMath::Sin( DEG2RAD( beamAngle ));

    tDisX = dist / vel;

    last = beamOrigin;
    z0 = beamOrigin.z;

    for ( int i = 0; i < numPoints; i++ )
    {
        t += tDisX;

        zDelt = z0 - vz * t - 0.5 * grav * ( t * t );
        xDelt = vx * tDisX;

        next = last + forward[0] * xDelt;

        if ( z0 - zDelt >= vr_teleportMaxDrop.GetFloat() )
        {
            zDelt = z0 - vr_teleportMaxDrop.GetFloat();
            t = (idMath::Sqrt( (2 * grav * z0) - (2 * grav * zDelt) + (vz * vz) ) - vz) / grav;
            xDelt = vx * t;
            next = beamOrigin + forward[0] * xDelt;
            i = numPoints;

        }

        next.z = zDelt;
        teleportPoint = teleportAimPoint = next;

        padAxis = forward;

        if ( gameLocal.clip.TracePoint( traceResults, last, next, MASK_SHOT_RENDERMODEL, this ) )
        {

            const char * hitMat;

            hitMat = traceResults.c.material->GetName();
            float hitPitch = traceResults.c.normal.ToAngles().pitch;

            // handrails really make aiming suck, so skip any non floor hits that consist of these materials
            // really need to verify this doesn't break anything.
            // Carl: It does break teleporting onto handrails, which I intended to be able to do.


            if ( vr_teleportSkipHandrails.GetInteger() == 1 && hitPitch != -90 )
            {
                if ( idStr::FindText( hitMat, "base_trim" ) > -1 ||
                     idStr::FindText( hitMat, "swatch" ) > -1 ||
                     idStr::FindText( hitMat, "mchangar2" ) > -1 ||
                     idStr::FindText( hitMat, "mchangar3" ) > -1 )
                {

                    common->Printf( "Beam hit rejected: material %s hitpitch %f\n", hitMat,hitPitch );
                    last = next;
                    endPos = last;
                    continue;
                }

            }

            //common->Printf( "Beam hit material = %s\n", traceResults.c.material->GetName() );
            next = traceResults.c.point;
            endPos = next;


            //set the axis for the telepad to match the surface
            static idAngles surfaceAngle = ang_zero;

            static idQuat lastQ = idAngles( 0.0f, 0.0f, 90.0f ).ToQuat();
            static idQuat nextQ = lastQ;
            static idQuat lastSet = lastQ;

            static idMat3 lastAxis = mat3_zero;
            static idMat3 curAxis = mat3_zero;

            static int slerpEnd = commonVr->Sys_Milliseconds() - 500;
            static idVec3 lastHitNormal = vec3_zero;

            idVec3 hitNormal = traceResults.c.normal;

            static idAngles muzzleAngle = ang_zero;
            static idAngles diffAngle = ang_zero;
            static float rollDiff = 0.0f;

            surfaceAngle = traceResults.c.normal.ToAngles().Normalize180();
            muzzleAngle = beamAxis.ToAngles().Normalize180();
            muzzleAngle.roll = 0;

            surfaceAngle.pitch *= -1;
            surfaceAngle.yaw += 180;
            surfaceAngle.Normalize180();

            diffAngle = idAngles( 0, 0, muzzleAngle.yaw - surfaceAngle.yaw ).Normalize180();

            rollDiff = diffAngle.roll * 1 / (90 / surfaceAngle.pitch);

            surfaceAngle.roll = muzzleAngle.roll - rollDiff;
            surfaceAngle.Normalize180();
            curAxis = surfaceAngle.ToMat3();

            if ( hitNormal != lastHitNormal )
            {

                lastHitNormal = hitNormal;

                if ( slerpEnd - commonVr->Sys_Milliseconds() <= 0 )
                {
                    lastQ = lastAxis.ToQuat();
                }
                else
                {
                    lastQ = lastSet;
                }

                slerpEnd = commonVr->Sys_Milliseconds() + slTime;
                nextQ = curAxis.ToQuat();

            }

            if ( slerpEnd - commonVr->Sys_Milliseconds() <= 0 )
            {
                padAxis = curAxis;
            }
            else
            {
                float qt = (float)((float)(slTime + 1.0f) - (float)(slerpEnd - commonVr->Sys_Milliseconds())) / (float)slTime;
                lastSet.Slerp( lastQ, nextQ, qt );
                padAxis = lastSet.ToMat3();
            }

            lastAxis = curAxis;

            bool aimLadder = false, aimActor = false, aimElevator = false;
            aimLadder = traceResults.c.material && (traceResults.c.material->GetSurfaceFlags() & SURF_LADDER);
            idEntity* aimEntity = gameLocal.GetTraceEntity(traceResults);
            if (aimEntity)
            {
                if (aimEntity->IsType(idActor::Type))
                    aimActor = aimEntity->health > 0;
                else if (aimEntity->IsType(idElevator::Type))
                    aimElevator = true;
                else if (aimEntity->IsType(idStaticEntity::Type) || aimEntity->IsType(idLight::Type))
                {
                    renderEntity_t *rend = aimEntity->GetRenderEntity();
                    if (rend)
                    {
                        idRenderModel *model = rend->hModel;
                        aimElevator = (model && idStr::Cmp(model->Name(), "models/mapobjects/elevators/elevator.lwo") == 0);
                    }
                }
            }

            teleportPoint = teleportAimPoint = traceResults.c.point;
            float beamLengthSquared = 0;
            if (aimElevator)
            {
                teleportPoint = teleportAimPoint + idVec3(0, 0, 10);
                beamLengthSquared = (teleportPoint - beamOrigin).LengthSqr();
            }
            if ((hitPitch >= -90.0f && hitPitch <= -45.0f && !aimActor) || aimLadder)
            {
                bool aimValid = (aimElevator && beamLengthSquared <= 300 * 300) || CanReachPosition(teleportAimPoint, teleportPoint);
                // check if we are teleporting OUT of an elevator
                if (!aimValid && !aimActor && fabs(teleportPoint.z - physicsObj.GetOrigin().z) <= 10 && (teleportPoint - beamOrigin).LengthSqr() <= 300 * 300)
                {
                    // do a trace to see if we're in an elevator, and if so, set aimValid to true
                    trace_t result;
                    physicsObj.ClipTranslation(result, GetPhysics()->GetGravityNormal() * 10, NULL);
                    if (result.fraction < 1.0f)
                    {
                        aimEntity = gameLocal.GetTraceEntity(result);
                        if (aimEntity)
                        {
                            if (aimEntity->IsType(idElevator::Type))
                                aimValid = true;
                            else if (aimEntity->IsType(idStaticEntity::Type) || aimEntity->IsType(idLight::Type))
                            {
                                renderEntity_t *rend = aimEntity->GetRenderEntity();
                                if (rend)
                                {
                                    idRenderModel *model = rend->hModel;
                                    aimValid = (model && idStr::Cmp(model->Name(), "models/mapobjects/elevators/elevator.lwo") == 0);
                                }
                            }
                        }
                    }
                }
                if ( aimValid )
                {

                    // pitch indicates a flat surface or <45 deg slope,
                    // check to see if a clip test passes at the location AFTER
                    // checking reachability.
                    // the clip test will prevent us from teleporting too close to walls.

                    static trace_t trace;
                    static idClipModel* clip;
                    static idMat3 clipAxis;
                    static idVec3 tracePt;
                    tracePt = teleportPoint;

                    clip = physicsObj.GetClipModel();
                    clipAxis = physicsObj.GetClipModel()->GetAxis();

                    gameLocal.clip.Translation(trace, tracePt, tracePt, clip, clipAxis, CONTENTS_SOLID, NULL);
                    // Carl: check if we're on stairs
                    if (trace.fraction < 1.0f)
                    {
                        tracePt.z += pm_stepsize.GetFloat();
                        gameLocal.clip.Translation(trace, tracePt, tracePt, clip, clipAxis, CONTENTS_SOLID, NULL);
                    }

                    if (trace.fraction < 1.0f)
                    {
                        aimValidForTeleport = false;
                        isShowing = false;
                        // Koz
                        //please duck sometimes shows incorrectly if a moveable object is in the way.
                        //add a clip check at the crouch height, and if it passes, show the please duck graphic.
                        //otherwise something is in the way.

                        if ( clip->GetBounds()[1][2] == pm_crouchheight.GetFloat() ) // player is already crouching and we failed the clip test
                        {
                            pleaseDuck = false;
                        }
                        else
                        {

                            // change bounds to crouch height and check again.
                            idBounds bounds = clip->GetBounds();
                            bounds[1][2] = pm_crouchheight.GetFloat();
                            if ( pm_usecylinder.GetBool() )
                            {
                                clip->LoadModel( idTraceModel( bounds, 8 ) );
                            }
                            else
                            {
                                clip->LoadModel( idTraceModel( bounds ) );
                            }

                            gameLocal.clip.Translation( trace, tracePt, tracePt, clip, clipAxis, CONTENTS_SOLID, NULL );

                            pleaseDuck = trace.fraction >= 1.0f ? true : false;

                            //reset bounds
                            bounds[1][2] = pm_normalheight.GetFloat();
                            if ( pm_usecylinder.GetBool() )
                            {
                                clip->LoadModel( idTraceModel( bounds, 8 ) );
                            }
                            else
                            {
                                clip->LoadModel( idTraceModel( bounds ) );
                            }
                        }
                    }
                    else
                    {
                        aimValidForTeleport = true;
                        if ( !isShowing )
                        {
                            slerpEnd = commonVr->Sys_Milliseconds() - 100;
                            padAxis = curAxis;
                            lastAxis = curAxis;
                        }

                        teleportTarget->GetRenderEntity()->weaponDepthHack = false;
                        isShowing = true;
                    }
                }
            }

            const idMat3 correct = idAngles( -90.0f, 0.0f, 90.0f ).ToMat3();
            padAxis *= forward.Inverse();
            padAxis = correct * padAxis;

            break;
        }

        last = next;
        endPos = last;
    }

    teleportTarget->SetOrigin( beamOrigin );

    // an approximation of a parabola has been scanned for surface hits and the endpoint has been calculated.
    // update the beam model by setting the joint positions along the beam to trace the arc.
    // joint 0 should be set to the origin, and the origin of the model is set to the origin of the beam in worldspace.

    teleportTargetAnimator->SetJointPos( teleportBeamJoint[0], JOINTMOD_WORLD_OVERRIDE, vec3_zero ); // joint 0 always the origin.

    st = beamOrigin.ToVec2();
    en = endPos.ToVec2();

    tDisX = ( (en - st).Length() / vx ) / 23.0f; // time for each segment of arc at velocity vx.
    t = 0.0f;

    next = beamOrigin;

    idMat3 forwardInv = forward.Inverse();

    for ( int i = 1; i < 23; i++ )
    {
        t += tDisX;

        zDelt = z0 - vz * t - 0.5f * grav * (t * t);
        xDelt = vx * tDisX;

        next = next + forward[0] * xDelt;
        next.z = zDelt;

        jpos = ( next - beamOrigin ) * forwardInv;
        teleportTargetAnimator->SetJointPos( teleportBeamJoint[i], JOINTMOD_WORLD_OVERRIDE, jpos );
    }

    jpos = ( endPos - beamOrigin ) * forwardInv;
    teleportTargetAnimator->SetJointPos( teleportBeamJoint[23], JOINTMOD_WORLD_OVERRIDE, jpos );
    //teleportCenterPadJoint

    //set the center aiming point
    jpos = (teleportAimPoint - beamOrigin) * forwardInv;
    teleportTargetAnimator->SetJointPos( teleportCenterPadJoint, JOINTMOD_WORLD_OVERRIDE, jpos );
    teleportTargetAnimator->SetJointAxis( teleportCenterPadJoint, JOINTMOD_WORLD_OVERRIDE, padAxis );

    if ( vr_teleportShowAimAssist.GetInteger() )
    {
        // if we want to have the telepad reflect the aim assist, update the joint for the telepad
        //otherwise it will use the same origin as the aiming point
        jpos = (teleportPoint - beamOrigin) * forwardInv;
    }

    teleportTargetAnimator->SetJointPos( teleportPadJoint, JOINTMOD_WORLD_OVERRIDE, jpos );
    teleportTargetAnimator->SetJointAxis( teleportPadJoint, JOINTMOD_WORLD_OVERRIDE, padAxis );

    if ( !aimValidForTeleport )
    {
        // this will show the beam, but hide the teleport target

        if ( pleaseDuck )
        {
            teleportTarget->GetRenderEntity()->shaderParms[0] = 1;
            teleportTarget->GetRenderEntity()->shaderParms[1] = 1;
            teleportTarget->Show();
            teleportTarget->GetRenderEntity()->customSkin = skinTelepadCrouch;
            isShowing = true;

            if ( !vr_teleportHint.GetBool() )
            {
                ShowTip( "Duck! Low Headroom!", "If the teleport target turns red, there is limited headroom at the teleport destination. You must crouch before you can teleport to this location.", false );
                vr_teleportHint.SetBool( true );
            }
            //gameRenderWorld->DrawText( "Low Headroom\nPlease Duck", teleportPoint + idVec3( 0, 0, 18 ), 0.2f, colorOrange, viewAngles.ToMat3() );
        }
        else
        {
            teleportTarget->GetRenderEntity()->customSkin = NULL;
            teleportTarget->GetRenderEntity()->shaderParms[0] = 0;
            teleportTarget->GetRenderEntity()->shaderParms[1] = 0;
            teleportTarget->Show();
            isShowing = false;
        }
    }
    else
    {
        teleportTarget->GetRenderEntity()->customSkin = NULL;
        teleportDir = ( physicsObj.GetOrigin() - teleportPoint );
        teleportDir.Normalize();
        teleportTarget->GetRenderEntity()->shaderParms[0] = 255;
        teleportTarget->GetRenderEntity()->shaderParms[1] = 1;
        teleportTarget->Show();
        isShowing = true;
    }

    teleportTarget->Present();

    return;
}

/*
===============
idPlayer::HandleSingleGuiCommand
===============
*/
bool idPlayer::HandleSingleGuiCommand( idEntity *entityGui, idLexer *src ) {
	idToken token;

	if ( !src->ReadToken( &token ) ) {
		return false;
	}

	if ( token == ";" ) {
		return false;
	}

	if ( token.Icmp( "addhealth" ) == 0 ) {
		if ( entityGui && health < 100 ) {
			int _health = entityGui->spawnArgs.GetInt( "gui_parm1" );
			int amt = ( _health >= HEALTH_PER_DOSE ) ? HEALTH_PER_DOSE : _health;
			_health -= amt;
			entityGui->spawnArgs.SetInt( "gui_parm1", _health );
			if ( entityGui->GetRenderEntity() && entityGui->GetRenderEntity()->gui[ 0 ] ) {
				entityGui->GetRenderEntity()->gui[ 0 ]->SetStateInt( "gui_parm1", _health );
			}
			if (health < 100)
			{
				//Ass health increases, play the effect higher up the body
				float yHeight = -0.5f + ((float)(health+amt) / 100.0f);
				common->HapticEvent("healstation", 0, 0, 100, 0, yHeight);
			}
			health += amt;
			if ( health > 100 ) {
				health = 100;
			}
		}
		return true;
	}

	if ( token.Icmp( "ready" ) == 0 ) {
		PerformImpulse( IMPULSE_17 );
		return true;
	}

	if ( token.Icmp( "updatepda" ) == 0 ) {
		UpdatePDAInfo( true );
		return true;
	}

	if ( token.Icmp( "updatepda2" ) == 0 ) {
		UpdatePDAInfo( false );
		return true;
	}

	if ( token.Icmp( "stoppdavideo" ) == 0 ) {
		if ( objectiveSystem && objectiveSystemOpen && pdaVideoWave.Length() > 0 ) {
			StopSound( SND_CHANNEL_PDA, false );
		}
		return true;
	}

	if ( token.Icmp( "close" ) == 0 ) {
		if ( objectiveSystem && objectiveSystemOpen ) {
			TogglePDA(1 - vr_weaponHand.GetInteger());
		} else if (itemSystem && itemSystemOpen) {
			ToggleItemSystem();
		}
	}

	if ( token.Icmp( "playpdavideo" ) == 0 ) {
		if ( objectiveSystem && objectiveSystemOpen && pdaVideo.Length() > 0 ) {
			const idMaterial *mat = declManager->FindMaterial( pdaVideo );
			if ( mat ) {
				int c = mat->GetNumStages();
				for ( int i = 0; i < c; i++ ) {
					const shaderStage_t *stage = mat->GetStage(i);
					if ( stage && stage->texture.cinematic ) {
						stage->texture.cinematic->ResetTime( gameLocal.time );
					}
				}
				if ( pdaVideoWave.Length() ) {
					const idSoundShader *shader = declManager->FindSound( pdaVideoWave );
					StartSoundShader( shader, SND_CHANNEL_PDA, 0, false, NULL );
				}
			}
		}
	}

	if ( token.Icmp( "playpdaaudio" ) == 0 ) {
		if ( objectiveSystem && objectiveSystemOpen && pdaAudio.Length() > 0 ) {
			const idSoundShader *shader = declManager->FindSound( pdaAudio );
			int ms;
			StartSoundShader( shader, SND_CHANNEL_PDA, 0, false, &ms );
			StartAudioLog();
			CancelEvents( &EV_Player_StopAudioLog );
			PostEventMS( &EV_Player_StopAudioLog, ms + 150 );
		}
		return true;
	}

	if ( token.Icmp( "stoppdaaudio" ) == 0 ) {
		if ( objectiveSystem && objectiveSystemOpen && pdaAudio.Length() > 0 ) {
			// idSoundShader *shader = declManager->FindSound( pdaAudio );
			StopAudioLog();
			StopSound( SND_CHANNEL_PDA, false );
		}
		return true;
	}

	src->UnreadToken( &token );
	return false;
}

/*
==============
idPlayer::Collide
==============
*/
bool idPlayer::Collide( const trace_t &collision, const idVec3 &velocity ) {
	idEntity *other;

	if ( gameLocal.isClient ) {
		return false;
	}

	other = gameLocal.entities[ collision.c.entityNum ];
	if ( other ) {
		other->Signal( SIG_TOUCH );
		if ( !spectating ) {
			if ( other->RespondsTo( EV_Touch ) ) {
				other->ProcessEvent( &EV_Touch, this, &collision );
			}
		} else {
			if ( other->RespondsTo( EV_SpectatorTouch ) ) {
				other->ProcessEvent( &EV_SpectatorTouch, this, &collision );
			}
		}
	}
	return false;
}


/*
================
idPlayer::UpdateLocation

Searches nearby locations
================
*/
void idPlayer::UpdateLocation( void ) {
	if ( hud ) {
		idLocationEntity *locationEntity = gameLocal.LocationForPoint( GetEyePosition() );
		if ( locationEntity ) {
			hud->SetStateString( "location", locationEntity->GetLocation() );
		} else {
			hud->SetStateString( "location", common->GetLanguageDict()->GetString( "#str_02911" ) );
		}
	}
}

/*
==============
Koz idPlayer::UpdateNeckPose
In Vr, if viewing the player body, update the neck joint with the orientation of the HMD.
==============
*/
void idPlayer::UpdateNeckPose()
{
    static idAngles headAngles, lastView = ang_zero;

    if ( !game->isVR || !pVRClientInfo ) return;

    // if showing the player body, move the head/neck based on HMD
    lastView = commonVr->lastHMDViewAxis.ToAngles();
    headAngles.roll = lastView.pitch;
    headAngles.pitch = commonVr->lastHMDYaw - commonVr->bodyYawOffset;
    headAngles.yaw = lastView.roll;
    headAngles.Normalize360();
    animator.SetJointAxis( neckJoint, JOINTMOD_LOCAL, headAngles.ToMat3() );
}

/*
==============
idPlayer::UpdatePDASlot
==============
*/
void idPlayer::UpdatePDASlot()
{
    if( vr_slotDisable.GetBool() )
    {
        return;
    }
    if( pdaRenderEntity.hModel ) // && inventory.pdas.Num()
    {
        //pdaRenderEntity.timeGroup = timeGroup;

        pdaRenderEntity.entityNum = ENTITYNUM_NONE;

        pdaRenderEntity.axis = pdaHolsterAxis * waistAxis;
        idVec3 slotOrigin = slots[SLOT_PDA_HIP].origin;
        if (vr_weaponHand.GetInteger())
            slotOrigin.y *= -1.0f;
        pdaRenderEntity.origin = waistOrigin + slotOrigin * waistAxis;

        pdaRenderEntity.allowSurfaceInViewID = entityNumber + 1;
        pdaRenderEntity.weaponDepthHack = g_useWeaponDepthHack.GetBool();

        if( pdaModelDefHandle == -1 )
        {
            pdaModelDefHandle = gameRenderWorld->AddEntityDef( &pdaRenderEntity );
        }
        else
        {
            gameRenderWorld->UpdateEntityDef( pdaModelDefHandle, &pdaRenderEntity );
        }
    }
}

/*
================
idPlayer::ClearFocus

Clears the focus cursor
================
*/
void idPlayer::ClearFocus( void ) {
	focusClickable	= NULL;
	focusCharacter	= NULL;
	focusGUIent		= NULL;
	focusUI			= NULL;
	focusVehicle	= NULL;
	talkCursor		= 0;

    // Koz
    commonVr->handInGui = false;
    //weapon->GetRenderEntity()->allowSurfaceInViewID = 0;
}

/*
================
idPlayer::UpdateFocus

Searches nearby entities for interactive guis, possibly making one of them
the focus and sending it a mouse move event
================
*/
void idPlayer::UpdateFocus( void ) {
	idClipModel *clipModelList[ MAX_GENTITIES ];
	idClipModel *clip;
	int			listedClipModels;
	idEntity	*oldFocus;
	idEntity	*ent;
	idUserInterface *oldUI;
	idAI		*oldChar;
	idEntity	*oldClickable;
	int			oldTalkCursor;
	int			i, j;
	idVec3		start, end;
	bool		allowFocus;
	const char *command;
	trace_t		trace;
	guiPoint_t	pt;
	const idKeyValue *kv;
	sysEvent_t	ev;
	idUserInterface *ui;

	static idMat3 lastScanAxis = commonVr->lastViewAxis;
	static idVec3 lastScanStart = commonVr->lastViewOrigin;
	static idVec3 lastBodyPosition = physicsObj.GetOrigin();
	static float lastBodyYaw = 0.0f;
	static bool lowered = false;
	static bool raised = false;

	static idVec3 surfaceNormal = vec3_zero;
	static idVec3 fingerPosLocal = vec3_zero;
	static idMat3 fingerAxisLocal = mat3_identity;
	static idVec3 fingerPosGlobal = vec3_zero;
	static idVec3 scanStart = vec3_zero;
	static idVec3 scanEnd = vec3_zero;
	static idVec3 talkScanEnd = vec3_zero;
	static jointHandle_t fingerJoint;
	static bool	touching = false;

	static idMat3 weaponAxis;
	static bool scanFromWeap;
	static float scanRange;
	static float scanRangeCorrected;
	static float hmdAbsPitch;

	idMat3 viewPitchAdj = idAngles( vr_guiFocusPitchAdj.GetFloat(), 0.0f, 0.0f ).ToMat3();

	scanRange = 50.0f;

	if ( !fileSystem->RunningPhobos() || !(gameLocal.GetCamera() && gameLocal.GetCamera()->spawnArgs.GetInt("AllowClickers")) ) {
		if ( gameLocal.inCinematic  || commonVr->thirdPersonMovement ) {
			return;
    	}
    }

	//check for PDA interaction.
	//if the PDA is being interacted with, there is no need to check for other guis
	//or scan for character names so bail.
	if ( UpdateFocusPDA() )
	{
		return;
	}

	// only update the focus character when attack button isn't pressed so players
	// can still chainsaw NPC's
	if ( gameLocal.isMultiplayer || ( !focusCharacter && ( usercmd.buttons & BUTTON_ATTACK ) ) ) {
		allowFocus = false;
	} else {
		allowFocus = true;
	}

	oldFocus		= focusGUIent;
	oldUI			= focusUI;
	oldChar			= focusCharacter;
	oldClickable	= focusClickable;
	oldTalkCursor	= talkCursor;

	if ( focusTime <= gameLocal.time  || commonVr->teleportButtonCount != 0) {
		ClearFocus();
		raised = false;
		lowered = false;
		if ( commonVr->teleportButtonCount != 0 ) return;
	}

	// don't let spectators interact with GUIs
	if ( spectating ) {
		return;
	}

	start = GetEyePosition();
	end = start + viewAngles.ToForward() * 80.0f;

	if (fileSystem->RunningPhobos()) {
		if (gameLocal.GetCamera() && gameLocal.GetCamera()->spawnArgs.GetInt("AllowClickers")) {
			start = gameLocal.GetCamera()->GetRenderView()->vieworg;
			end = start + gameLocal.GetCamera()->GetRenderView()->viewaxis[0] * 80.0f;
		}
	}

	// Koz begin
	if ( game->isVR && !gameLocal.isMultiplayer ) // Koz fixme only when vr actually active.
	{
		// Koz  in VR, if weapon equipped, use muzzle orientation to scan for accessible guis,
		// otherwise use player center eye.

		scanFromWeap = hands[vr_weaponHand.GetInteger()].weapon->GetMuzzlePositionWithHacks( start, weaponAxis );
		if ( !scanFromWeap || vr_guiMode.GetInteger() == 1 || (  vr_guiMode.GetInteger() == 2 && commonVr->VR_USE_MOTION_CONTROLS ) ) // guiMode 2 = use guis as touch screen
		{
			//weapon has no muzzle ( fists, grenades, chainsaw) or we are using the guis as touchscreens so scan from center of view.
			start = commonVr->lastViewOrigin;
			weaponAxis = commonVr->lastViewAxis;

			scanRange += 36.0f;

			if ( vr_guiMode.GetInteger() == 2 )
			{

				weaponAxis = viewPitchAdj * weaponAxis; // add a little down pitch to help my neck.

				scanRange = 36.0f; // get pretty close for touchscreens
				// if the hand is already in the gui, or has completed the lower cycle after hitting a gui,
				// scan from the same point in space until the player exceeds movement threshold
				// this prevents getting kicked from a gui when just looking around
				if ( commonVr->handInGui || raised )
				{
					static idMat3 yawAdj;
					static idVec3 distMoved;
					scanRange += 4;
					yawAdj = idAngles( 0.0f, viewAngles.yaw - lastBodyYaw, 0.0f ).Normalize180().ToMat3();
					weaponAxis = yawAdj * lastScanAxis;
					distMoved = physicsObj.GetOrigin() - lastBodyPosition;
					if ( distMoved.Length() < 12.0f ) distMoved = vec3_zero;
					start = lastScanStart + distMoved;

				}
				else
				{
					lastScanStart = start;
					lastScanAxis = weaponAxis;
					lastBodyYaw = viewAngles.yaw;
					lastBodyPosition = physicsObj.GetOrigin();
				}
			}
			hmdAbsPitch = abs( commonVr->lastHMDViewAxis.ToAngles().Normalize180().pitch + vr_guiFocusPitchAdj.GetFloat() );
			if ( hmdAbsPitch > 60.0f ) hmdAbsPitch = 60.0f;
			scanRangeCorrected = scanRange / cos( DEG2RAD( hmdAbsPitch ) );
			scanRange = scanRangeCorrected;
		}
		else
		{
			// Koz - if weapon has been lowered (in gui), raise pointer to compensate.
			start.z -= hands[ vr_weaponHand.GetInteger() ].weapon->hideOffset;
		}

		end = start + weaponAxis[0] * scanRange;//  Koz originial value was 80.0f - allowed access to gui from too great a distance (IMO), reduced to 50.0f Koz fixme - make cvar?
	}
	else
	{
		end = start + firstPersonViewAxis[0] * 80.0f;
	}
	// Koz end
	if ( game->isVR && commonVr->VR_USE_MOTION_CONTROLS && vr_guiMode.GetInteger() == 2 )
	{
		talkScanEnd = start + weaponAxis[0] * (scanRange + 40);
	}
	else
	{
		talkScanEnd = end;
	}


	/*
	//Use unadjusted weapon angles to control GUIs
	idMat3 weaponViewAxis;
	idAngles weaponViewAngles;
    CalculateViewWeaponPos(true, start, weaponViewAxis, weaponViewAngles);
	end = start + (weaponViewAngles.ToForward() * 80.0f);
	*/

	// player identification -> names to the hud
	if ( gameLocal.isMultiplayer && entityNumber == gameLocal.localClientNum ) {
		idVec3 end = start + viewAngles.ToForward() * 768.0f;
		gameLocal.clip.TracePoint( trace, start, end, MASK_SHOT_BOUNDINGBOX, this );
		int iclient = -1;
		if ( ( trace.fraction < 1.0f ) && ( trace.c.entityNum < MAX_CLIENTS ) ) {
			iclient = trace.c.entityNum;
		}
		if ( MPAim != iclient ) {
			lastMPAim = MPAim;
			MPAim = iclient;
			lastMPAimTime = gameLocal.realClientTime;
		}
	}

	idBounds bounds( start );
	bounds.AddPoint( end );

	listedClipModels = gameLocal.clip.ClipModelsTouchingBounds( bounds, -1, clipModelList, MAX_GENTITIES );

	if ( vr_debugGui.GetBool() )
	{
		gameRenderWorld->DebugLine( colorRed, start, end, 20, true );
		//common->Printf( "Handin gui %d raised %d lowered %d hideoffset %f\n", commonVr->handInGui, raised, lowered, hands[ vr_weaponHand.GetInteger() ].weapon->hideOffset );
	}

	// no pretense at sorting here, just assume that there will only be one active
	// gui within range along the trace
	for ( i = 0; i < listedClipModels; i++ ) {
		clip = clipModelList[ i ];
		ent = clip->GetEntity();

		if ( ent->IsHidden() ) {
			continue;
		}

		if ( allowFocus ) {
			if ( ent->IsType( idAFAttachment::Type ) ) {
				idEntity *body = static_cast<idAFAttachment *>( ent )->GetBody();
				if ( body && body->IsType( idAI::Type ) && ( static_cast<idAI *>( body )->GetTalkState() >= TALK_OK ) ) {
					gameLocal.clip.TracePoint( trace, start, end, MASK_SHOT_RENDERMODEL, this );
					if ( ( trace.fraction < 1.0f ) && ( trace.c.entityNum == ent->entityNumber ) ) {
						ClearFocus();
						focusCharacter = static_cast<idAI *>( body );
						talkCursor = 1;
						focusTime = gameLocal.time + FOCUS_TIME;
						break;
					}
				}
				continue;
			}

			if (fileSystem->RunningPhobos()) {
				if (ent->IsType(idEntity::Type) && ent->spawnArgs.GetInt("clickable") &&
					ent->spawnArgs.GetInt("click_item") &&
					!ent->spawnArgs.GetInt("item_disabled")) {
						gameLocal.clip.TracePoint(trace, start, end, MASK_SHOT_RENDERMODEL, this);
						if ((trace.fraction < 1.0f) && (trace.c.entityNum == ent->entityNumber)) {
							ClearFocus();
							focusClickable = static_cast<idEntity *>(ent);
							talkCursor = 1;
							focusTime = gameLocal.time + FOCUS_TIME;

							idMat3 viewAxis;
							idVec3 center = focusClickable->GetPhysics()->GetAbsBounds().GetCenter();
							if (gameLocal.GetCamera() && gameLocal.GetCamera()->spawnArgs.GetInt("AllowClickers")) {
								viewAxis = gameLocal.GetCamera()->GetRenderView()->viewaxis;
							} else {
								viewAxis = viewAngles.ToMat3();
							}
							gameRenderWorld->DrawText(focusClickable->spawnArgs.GetString("item_action", "Touch"),
							idVec3(center.x, center.y, center.z - 1.5f), 0.05f, colorCyan, viewAxis, 1);
							break;
					}
				}
			}

			if ( ent->IsType( idAI::Type ) ) {
				if ( static_cast<idAI *>( ent )->GetTalkState() >= TALK_OK ) {
					gameLocal.clip.TracePoint( trace, start, end, MASK_SHOT_RENDERMODEL, this );
					if ( ( trace.fraction < 1.0f ) && ( trace.c.entityNum == ent->entityNumber ) ) {
						ClearFocus();
						focusCharacter = static_cast<idAI *>( ent );
						talkCursor = 1;
						focusTime = gameLocal.time + FOCUS_TIME;
						break;
					}
				}
				continue;
			}

			if ( ent->IsType( idAFEntity_Vehicle::Type ) ) {
				gameLocal.clip.TracePoint( trace, start, end, MASK_SHOT_RENDERMODEL, this );
				if ( ( trace.fraction < 1.0f ) && ( trace.c.entityNum == ent->entityNumber ) ) {
					ClearFocus();
					focusVehicle = static_cast<idAFEntity_Vehicle *>( ent );
					focusTime = gameLocal.time + FOCUS_TIME;
					break;
				}
				continue;
			}
		}

		//if ( !ent->GetRenderEntity() || !ent->GetRenderEntity()->gui[ 0 ] || !ent->GetRenderEntity()->gui[ 0 ]->IsInteractive() ) {
		if ( (!ent->GetRenderEntity() || !ent->GetRenderEntity()->gui[0] || !ent->GetRenderEntity()->gui[0]->IsInteractive()) && (!game->IsPDAOpen() /* && !commonVr->PDAclipModelSet */) ) { // Koz don't bail if the PDA is open and clipmodel is set.
			continue;
		}

		if ( ent->spawnArgs.GetBool( "inv_item" ) ) {
			// don't allow guis on pickup items focus
			continue;
		}

		int fingerHand = vr_weaponHand.GetInteger();

		// Koz : if the weapon is reloading, don't let the hand enter a gui, or the weapon anims
		// will still be driving the hand and it looks stupid.

		if ( hands[ fingerHand ].weapon->IsReloading() ) continue;

		// Koz : the shotgun reload script cycles through WP_RELOAD and WP_READY for each shell,
		// so we cant just check the reload state or else the hand will enter
		// the gui in between two shells loading.  Make sure when using the shotgun
		// that the idle animation is playing before entering a gui.
		// fixme : this sucks - find a better way to check this.
		if( hands[ fingerHand ].currentWeapon == weapon_shotgun )
		{
			if( idStr::Cmp( hands[ fingerHand ].weapon->GetAnimator()->CurrentAnim( ANIMCHANNEL_ALL )->AnimName(), "idle" ) != 0 ) continue;
		}


		ent->GetAnimator();
		pt = gameRenderWorld->GuiTrace( ent->GetModelDefHandle(), ent->GetAnimator(), start, end );
		if ( pt.x != -1 ) {
			// we have a hit
			renderEntity_t *focusGUIrenderEntity = ent->GetRenderEntity();
			if ( !focusGUIrenderEntity ) {
				continue;
			}

			if ( pt.guiId == 1 ) {
				ui = focusGUIrenderEntity->gui[ 0 ];
			} else if ( pt.guiId == 2 ) {
				ui = focusGUIrenderEntity->gui[ 1 ];
			} else {
				ui = focusGUIrenderEntity->gui[ 2 ];
			}

			if ( ui == NULL ) {
				continue;
			}

			ClearFocus();
			focusGUIent = ent;
			focusUI = ui;

			if ( oldFocus != ent ) {
				// new activation
				// going to see if we have anything in inventory a gui might be interested in
				// need to enumerate inventory items
				focusUI->SetStateInt( "inv_count", inventory.items.Num() );
				for ( j = 0; j < inventory.items.Num(); j++ ) {
					idDict *item = inventory.items[ j ];
					const char *iname = item->GetString( "inv_name" );
					const char *iicon = item->GetString( "inv_icon" );
					const char *itext = item->GetString( "inv_text" );

					focusUI->SetStateString( va( "inv_name_%i", j), iname );
					focusUI->SetStateString( va( "inv_icon_%i", j), iicon );
					focusUI->SetStateString( va( "inv_text_%i", j), itext );
					kv = item->MatchPrefix("inv_id", NULL);
					if ( kv ) {
						focusUI->SetStateString( va( "inv_id_%i", j ), kv->GetValue() );
					}
					focusUI->SetStateInt( iname, 1 );
				}


				for( j = 0; j < inventory.pdaSecurity.Num(); j++ ) {
					const char *p = inventory.pdaSecurity[ j ];
					if ( p && *p ) {
						focusUI->SetStateInt( p, 1 );
					}
				}

				//BSM: Added for powercells
				int powerCellCount = 0;

				for (j = 0; j < inventory.items.Num(); j++) {
					idDict *item = inventory.items[ j ];

					if (item->GetInt("inv_powercell")) {
						powerCellCount++;
					}
				}

				focusUI->SetStateInt("powercell_count", powerCellCount);

				int staminapercentage = ( int )( 100.0f * stamina / pm_stamina.GetFloat() );
				focusUI->SetStateString( "player_health", va("%i", health ) );
				focusUI->SetStateString( "player_stamina", va( "%i%%", staminapercentage ) );
				focusUI->SetStateString( "player_armor", va( "%i%%", inventory.armor ) );

				kv = focusGUIent->spawnArgs.MatchPrefix( "gui_parm", NULL );
				while ( kv ) {
					focusUI->SetStateString( kv->GetKey(), kv->GetValue() );
					kv = focusGUIent->spawnArgs.MatchPrefix( "gui_parm", kv );
				}
			}

			if ( !game->isVR || !( game->isVR && ( vr_guiMode.GetInteger() == 2 && commonVr->VR_USE_MOTION_CONTROLS) ) )
			{
				// handle event normally
				// clamp the mouse to the corner
				ev = sys->GenerateMouseMoveEvent( -2000, -2000 );

				command = focusUI->HandleEvent( &ev, gameLocal.time );
				HandleGuiCommands( focusGUIent, command );

				// move to an absolute position
				ev = sys->GenerateMouseMoveEvent( pt.x * SCREEN_WIDTH, pt.y * SCREEN_HEIGHT );
				command = focusUI->HandleEvent( &ev, gameLocal.time );
				HandleGuiCommands( focusGUIent, command );

				focusTime = gameLocal.time + FOCUS_GUI_TIME;
				break;
			}
			else
			{
				// the game is vr, player is using motion controls, gui mode is set to use touchscreens
				// and the view has found a gui to interact with.
				// wait for lower weapon to drop the hand to hidedistance and hide the weapon model
				// then raise the empty hand with pointy finger back to original position
				int fingerHand = vr_weaponHand.GetInteger();
				idWeapon* fingerWeapon = hands[ fingerHand ].weapon;
				focusTime = gameLocal.time + FOCUS_GUI_TIME * 3 ;
				if ( !lowered )
				{
					focusTime = gameLocal.time + FOCUS_GUI_TIME * 3;
					if ( fingerWeapon->hideOffset != fingerWeapon->hideDistance ) break;
					lowered = true;
				}

				commonVr->handInGui = true;

				if ( !raised )
				{
					fingerWeapon->hideStart = fingerWeapon->hideDistance;
					fingerWeapon->hideEnd = 0.0f;
					if ( gameLocal.time - fingerWeapon->hideStartTime < fingerWeapon->hideTime )
					{
						fingerWeapon->hideStartTime = gameLocal.time - ( fingerWeapon->hideTime - (gameLocal.time - fingerWeapon->hideStartTime));
					}
					else
					{
						fingerWeapon->hideStartTime = gameLocal.time;
					}

					raised = true;
					focusTime = gameLocal.time + FOCUS_GUI_TIME * 3;
					break;
				}

				if ( raised == true && fingerWeapon->hideOffset != 0.0f )
				{
					focusTime = gameLocal.time + FOCUS_GUI_TIME;
					break;
				}


				// so now get current position of the pointy finger tip joint and
				// see if we have touched the gui

				gameLocal.clip.TracePoint( trace, start, end, MASK_SHOT_RENDERMODEL, this );

				if ( fingerHand == HAND_RIGHT )
				{
					fingerJoint = animator.GetJointHandle( "RindexTip" );
				}
				else
				{
					fingerJoint = animator.GetJointHandle( "LindexTip" );
				}

				surfaceNormal = -trace.c.normal;

				animator.GetJointTransform( fingerJoint, gameLocal.time, fingerPosLocal, fingerAxisLocal );
				fingerPosGlobal = fingerPosLocal * GetRenderEntity()->axis + GetRenderEntity()->origin;

				static int fingerForwDist = 1.0f;
				static int fingerBackwDist = 12.0f;

				scanStart = fingerPosGlobal - fingerBackwDist * surfaceNormal;
				scanEnd = fingerPosGlobal + fingerForwDist * surfaceNormal;

				//gameRenderWorld->DebugLine( colorRed, scanStart, scanEnd, 20 );

				focusTime = gameLocal.time + FOCUS_GUI_TIME;

				pt = gameRenderWorld->GuiTrace( focusGUIent->GetModelDefHandle(), focusGUIent->GetAnimator(), scanStart, scanEnd );

				if ( pt.fraction >= 1.0f ) // no hit if > = 1.0f
				{
					//send mouse button up
					ev = sys->GenerateMouseButtonEvent( 1, false );
					command = focusUI->HandleEvent( &ev, gameLocal.time );
					HandleGuiCommands( focusGUIent, command );
					touching = false;
					break;
				}
				else
				{
					//we have a hit
					if ( touching )
					{
						if ( pt.fraction >= 0.94f )//  || pt.fraction < 0.875f ) // not touching any more was 0.965 and 0.0875
						{
							//common->Printf( "Sending mouse off 1 fraction = %f\n",trace.fraction );
							ev = sys->GenerateMouseButtonEvent( 1, false );
							command = focusUI->HandleEvent( &ev, gameLocal.time );
							HandleGuiCommands( focusGUIent, command );
							touching = false;
							break;
						}
						else
						{
							if ( pt.x != -1 )
							{
								// clamp the mouse to the corner
								ev = sys->GenerateMouseMoveEvent( -2000, -2000 );
								command = focusUI->HandleEvent( &ev, gameLocal.time );
								HandleGuiCommands( focusGUIent, command );

								// move to an absolute position
								ev = sys->GenerateMouseMoveEvent( pt.x * SCREEN_WIDTH, pt.y * SCREEN_HEIGHT );
								command = focusUI->HandleEvent( &ev, gameLocal.time );
								HandleGuiCommands( focusGUIent, command );
								break;
							}
						}
					}
					else // if ( !touching )
					{
						//common->Printf( "Fraction = %f\n", pt.fraction );
						if ( pt.fraction < 0.94f )
						{
							//common->Printf( "Setting touching true\n" );
							touching = true;
							if ( pt.x != -1 )
							{
								// clamp the mouse to the corner
								ev = sys->GenerateMouseMoveEvent( -2000, -2000 );
								command = focusUI->HandleEvent( &ev, gameLocal.time );
								HandleGuiCommands( focusGUIent, command );

								// move to an absolute position
								ev = sys->GenerateMouseMoveEvent( pt.x * SCREEN_WIDTH, pt.y * SCREEN_HEIGHT );
								command = focusUI->HandleEvent( &ev, gameLocal.time );
								HandleGuiCommands( focusGUIent, command );

								//send mouse button down
								ev = sys->GenerateMouseButtonEvent( 1, true );
								command = focusUI->HandleEvent( &ev, gameLocal.time );
								HandleGuiCommands( focusGUIent, command );

								//Rumble the controller to let player know they scored a touch.
								hands[fingerHand].SetControllerShake( 0.1f, 12, 0.8f, 12 );

								common->HapticEvent("pda_touch", 0, 0, 100, 0, 0);

								focusTime = gameLocal.time + FOCUS_GUI_TIME;
								break;

							}
						}
						/*else
						{

							//send mouse button up
							ev = sys->GenerateMouseButtonEvent( 1, false );
							command = focusUI->HandleEvent( &ev, gameLocal.time );
							HandleGuiCommands( focusGUIent, command );
							touching = false;
							break;
						}*/
					}
				}
			}
		}
	}

	if ( focusGUIent && focusUI ) {
		if ( !oldFocus || oldFocus != focusGUIent ) {
			if (fileSystem->RunningPhobos()) {
				// DG: tell the old UI it isn't focused anymore
				if (oldFocus != NULL && oldUI != NULL) {
					command = oldUI->Activate(false, gameLocal.time);
					// TODO: HandleGuiCommands( oldFocus, command ); ?
				} // DG end
			}
			command = focusUI->Activate( true, gameLocal.time );
			HandleGuiCommands( focusGUIent, command );
			StartSound( "snd_guienter", SND_CHANNEL_ANY, 0, false, NULL );
			// HideTip();
			// HideObjective();
		}
	} else if ( oldFocus && oldUI ) {
		lowered = false;
		raised = false;
		command = oldUI->Activate( false, gameLocal.time );
		HandleGuiCommands( oldFocus, command );
		StartSound( "snd_guiexit", SND_CHANNEL_ANY, 0, false, NULL );
	}

	if ( cursor && ( oldTalkCursor != talkCursor ) ) {
		cursor->SetStateInt( "talkcursor", talkCursor );
	}

	if ( oldChar != focusCharacter && hud ) {
		if ( focusCharacter ) {
			hud->SetStateString( "npc", focusCharacter->spawnArgs.GetString( "npc_name", "Joe" ) );
			//Use to code to update the npc action string to fix bug 1159
			hud->SetStateString("npc_action", common->GetLanguageDict()->GetString("#str_02036"));
			hud->HandleNamedEvent( "showNPC" );
			// HideTip();
			// HideObjective();
		} else {
			hud->SetStateString( "npc", "" );
			hud->SetStateString("npc_action", "");
			hud->HandleNamedEvent( "hideNPC" );
		}
	}

	if (fileSystem->RunningPhobos()) {
		if (oldClickable != focusClickable && hud) {
			if (focusClickable) {
				hud->HandleNamedEvent("showNPC");
			} else {
				hud->HandleNamedEvent("hideNPC");
			}
		}
	}
}

/*
================
idPlayer::UpdateFocusPDA

Searches nearby entities for interactive guis, possibly making one of them
the focus and sending it a mouse move event
================
*/
bool idPlayer::UpdateFocusPDA()
{
	static bool touching = false;
	jointHandle_t fingerJoint[2] = { animator.GetJointHandle( "RindexTip" ), animator.GetJointHandle( "LindexTip" ) };
	idVec3 fingerPosLocal = vec3_zero;
	idMat3 fingerAxisLocal = mat3_identity;
	idVec3 fingerPosGlobal = vec3_zero;
	idVec3 scanStart = vec3_zero;
	idVec3 scanEnd = vec3_zero;
	int pdaScrX = renderSystem->GetScreenWidth();// vr_pdaScreenX.GetInteger();
	int pdaScrY = pdaScrX * 0.75; // vr_pdaScreenY.GetInteger();
	const int fingerForwDist = 1.0f;
	const int fingerBackwDist = 12.0f;
	guiPoint_t	pt;
	sysEvent_t	ev;

	if ( !game->isVR || !pVRClientInfo || !( game->IsPDAOpen() || commonVr->VR_GAME_PAUSED || hands[0].currentWeapon == weapon_pda || hands[1].currentWeapon == weapon_pda ) )
	{
		touching = false;
		return false;
	}

	int pdahand;
	if( hands[ 0 ].holdingPDA() )
		pdahand = 0;
	else if( hands[ 1 ].holdingPDA() )
		pdahand = 1;
	else
		pdahand = 1 - vr_weaponHand.GetInteger();
	int fingerHand = 1 - pdahand;

	commonVr->scanningPDA = true; // let the swf event handler know its ok to take mouse input, even if the mouse cursor is not in the window.


	// game is VR and PDA active
	if ( vr_guiMode.GetInteger() == 2 && commonVr->VR_USE_MOTION_CONTROLS )
	{
		// the game is vr, player is using motion controls, gui mode is set to use touchscreens
		// and the pda is open
		// wait for lower weapon to drop the hand to hidedistance and hide the weapon model
		// then raise the empty hand with pointy finger back to original position

		if ( !touching )
		{
			// send a cursor event to get it off the screen since it is hidden.
			ev = sys->GenerateMouseMoveEvent( -2000, -2000 );
			SendPDAEvent( &ev );
		}

		commonVr->handInGui = true;
		focusTime = gameLocal.time + FOCUS_TIME;

		// get current position of the pointy finger tip joint and
		// see if we have touched the gui
		idWeapon* pdaWeapon = GetPDAWeapon();

		animator.GetJointTransform( fingerJoint[ fingerHand ], gameLocal.time, fingerPosLocal, fingerAxisLocal );
		fingerPosGlobal = fingerPosLocal * GetRenderEntity()->axis + GetRenderEntity()->origin;


		scanStart = fingerPosGlobal - PDAaxis[0] * fingerBackwDist;//  PDAaxis.Inverse();// surfaceNormal;
		scanEnd = fingerPosGlobal + PDAaxis[0] * fingerForwDist;// *surfaceNormal;


		pt = gameRenderWorld->GuiTrace( pdaWeapon->GetModelDefHandle(), pdaWeapon->GetAnimator(), scanStart, scanEnd );
		pt.y = 1.0f - pt.y;

		/*
		//debug finger
		if ( pt.fraction >= 1.0f )
		{
			gameRenderWorld->DebugLine( colorRed, scanStart, scanEnd, 20 );
		}
		else
		{
			gameRenderWorld->DebugLine( colorGreen, scanStart, scanEnd, 20 );
		}
		*/

		/*if ( common->Dialog().IsDialogActive() || game->Shell_IsActive() ) // commonVr->VR_GAME_PAUSED )
		{
			pt.y -= .12f;
		}
		else
		{
			pt.y += 0.12f; // add offset for screensafe borders
		}*/

		if ( pt.fraction >= 1.0f ) // no hit if > = 1.0f
		{
			//send mouse button up
			ev = sys->GenerateMouseButtonEvent( 1, false );
			SendPDAEvent( &ev );

			touching = false;
		}
		else
		{
			//we have a hit
			if ( touching )
			{
				if ( pt.fraction >= 0.94f ) // no longer touching
				{
					//common->Printf( "Sending mouse off 1 fraction = %f\n",trace.fraction );
					ev = sys->GenerateMouseButtonEvent( 1, false );
					SendPDAEvent( &ev );

					touching = false;
				}
				else
				{
					if ( pt.x != -1 )
					{
						// clamp the mouse to the corner
						ev = sys->GenerateMouseMoveEvent( -2000, -2000 );
						SendPDAEvent( &ev );

						// move to an absolute position
						ev = sys->GenerateMouseMoveEvent( pt.x * pdaScrX, pt.y * pdaScrY );//( pt.x * SCREEN_WIDTH, pt.y * SCREEN_HEIGHT );
						SendPDAEvent( &ev );

					}
				}
			}
			else // if ( !touching )
			{
				//common->Printf( "Fraction = %f\n", pt.fraction );
				if ( pt.fraction < 0.94f )
				{
					//common->Printf( "Setting touching true\n" );
					touching = true;
					if ( pt.x != -1 )
					{
						// clamp the mouse to the corner
						ev = sys->GenerateMouseMoveEvent( -2000, -2000 );
						SendPDAEvent( &ev );

						// move to an absolute position
						ev = sys->GenerateMouseMoveEvent( pt.x * pdaScrX, pt.y * pdaScrY );
						SendPDAEvent( &ev );

						//send mouse button down and back up again
						ev = sys->GenerateMouseButtonEvent( 1, true );
						SendPDAEvent( &ev );

						//Rumble the controller to let player know they scored a touch.
						// Carl: Should the PDA vibrate/haptic feedback too?
						hands[fingerHand].SetControllerShake( 0.1f, 12, 0.8f, 12 );
						hands[pdahand].SetControllerShake( 0.1f, 12, 0.8f, 12 );

                        common->HapticEvent("pda_touch", 0, 0, 100, 0, 0);
					}
					commonVr->scanningPDA = false;
					return true;
				}
				else
				{
					//send mouse button up
					ev = sys->GenerateMouseButtonEvent( 1, false );
					SendPDAEvent( &ev );


					touching = false;
				}
			}
		}
		commonVr->scanningPDA = false;
		return false;
	}

	//-------------------------------------------------------
	//not using motion controls, scan from view

	scanStart = commonVr->lastViewOrigin;
	scanEnd = scanStart + commonVr->lastViewAxis[0] * 60.0f; // not sure why the PDA would be farther than 60 inches away. Thats one LOOONG arm.

	//gameRenderWorld->DebugLine( colorYellow, scanStart, scanEnd, 10 );

	idWeapon* pdaWeapon = GetPDAWeapon();
	pt = gameRenderWorld->GuiTrace( pdaWeapon->GetModelDefHandle(), pdaWeapon->GetAnimator(), scanStart, scanEnd );

	if ( pt.x != -1 )
	{

		pt.y = 1 - pt.y; // texture was copied from framebuffer and is upside down so invert y
		if ( !commonVr->VR_GAME_PAUSED ) pt.y += 0.12f; // add offset for screensafe borders if using PDA

		focusTime = gameLocal.time + FOCUS_TIME;

		// move to an absolute position

		ev = sys->GenerateMouseMoveEvent( -2000, -2000 );
		SendPDAEvent( &ev );

		ev = sys->GenerateMouseMoveEvent( pdaScrX * pt.x, pdaScrY * pt.y );
		SendPDAEvent( &ev );

		commonVr->scanningPDA = false;

		return true;
	}

	commonVr->scanningPDA = false;

	return false; // view didn't hit pda
}

void idPlayer::SendPDAEvent( const sysEvent_t* sev )
{
	const char *command = NULL;
	command = objectiveSystem->HandleEvent(sev, gameLocal.time );
	HandleGuiCommands( this, command );
}

/*
==============
idPlayer::HandleGuiEvents
==============
*/
bool idPlayer::HandleGuiEvents( const sysEvent_t* ev )
{

    bool handled = false;
    if(hud)
    {
        hud->HandleEvent(ev, gameLocal.time );
    }
    /*if( hudManager != NULL && hudManager->IsActive() )
    {
        handled = hudManager->HandleGuiEvent( ev );
    }
	/*
    if ( pdaMenu != NULL && pdaMenu->IsActive() )
    {
        handled = pdaMenu->HandleGuiEvent( ev );
    }*/

    return handled;
}

/*
==============
idPlayer::UpdateHolsterSlot
==============
*/
void idPlayer::UpdateHolsterSlot()
{
    if( vr_slotDisable.GetBool() )
    {
        FreeHolsterSlot();
        holsteredWeapon = weapon_fists;
        return;
    }
    if( holsterRenderEntity.hModel )
    {
        //holsterRenderEntity.timeGroup = timeGroup;

        holsterRenderEntity.entityNum = ENTITYNUM_NONE;

        holsterRenderEntity.axis = holsterAxis * waistAxis;
        idVec3 slotOrigin = slots[SLOT_WEAPON_HIP].origin + idVec3(-5, 0, 0);
        if (vr_weaponHand.GetInteger())
            slotOrigin.y *= -1.0f;
        holsterRenderEntity.origin = waistOrigin + slotOrigin * waistAxis;

        holsterRenderEntity.allowSurfaceInViewID = entityNumber + 1;
        holsterRenderEntity.weaponDepthHack = g_useWeaponDepthHack.GetBool();

        if( holsterModelDefHandle == -1 )
        {
            holsterModelDefHandle = gameRenderWorld->AddEntityDef( &holsterRenderEntity );
        }
        else
        {
            gameRenderWorld->UpdateEntityDef( holsterModelDefHandle, &holsterRenderEntity );
        }
    }
}

/*
=================
idPlayer::CrashLand

Check for hard landings that generate sound events
=================
*/
void idPlayer::CrashLand( const idVec3 &oldOrigin, const idVec3 &oldVelocity ) {
	idVec3		origin, velocity;
	idVec3		gravityVector, gravityNormal;
	float		delta;
	float		hardDelta, fatalDelta;
	float		dist;
	float		vel, acc;
	float		t;
	float		a, b, c, den;
	waterLevel_t waterLevel;
	bool		noDamage;

	AI_SOFTLANDING = false;
	AI_HARDLANDING = false;

	// if the player is not on the ground
	if ( !physicsObj.HasGroundContacts() ) {
		return;
	}

	gravityNormal = physicsObj.GetGravityNormal();

	// if the player wasn't going down
	if ( ( oldVelocity * -gravityNormal ) >= 0.0f ) {
		return;
	}

	waterLevel = physicsObj.GetWaterLevel();

	// never take falling damage if completely underwater
	if ( waterLevel == WATERLEVEL_HEAD ) {
		return;
	}

	// no falling damage if touching a nodamage surface
	noDamage = false;
	for ( int i = 0; i < physicsObj.GetNumContacts(); i++ ) {
		const contactInfo_t &contact = physicsObj.GetContact( i );
		if ( contact.material->GetSurfaceFlags() & SURF_NODAMAGE ) {
			noDamage = true;
			StartSound( "snd_land_hard", SND_CHANNEL_ANY, 0, false, NULL );
			break;
		}
	}

	origin = GetPhysics()->GetOrigin();
	gravityVector = physicsObj.GetGravity();

	// calculate the exact velocity on landing
	dist = ( origin - oldOrigin ) * -gravityNormal;
	vel = oldVelocity * -gravityNormal;
	acc = -gravityVector.Length();

	a = acc / 2.0f;
	b = vel;
	c = -dist;

	den = b * b - 4.0f * a * c;
	if ( den < 0 ) {
		return;
	}
	t = ( -b - idMath::Sqrt( den ) ) / ( 2.0f * a );

	delta = vel + t * acc;
	delta = delta * delta * 0.0001;

	// reduce falling damage if there is standing water
	if ( waterLevel == WATERLEVEL_WAIST ) {
		delta *= 0.25f;
	}
	if ( waterLevel == WATERLEVEL_FEET ) {
		delta *= 0.5f;
	}

	if ( delta < 1.0f ) {
		return;
	}

	// allow falling a bit further for multiplayer
	if ( gameLocal.isMultiplayer ) {
		fatalDelta	= 75.0f;
		hardDelta	= 50.0f;
	} else {
		fatalDelta	= 65.0f;
		hardDelta	= 45.0f;
	}

	if ( delta > fatalDelta ) {
		AI_HARDLANDING = true;
		landChange = -32;
		landTime = gameLocal.time;
		if ( !noDamage ) {
			pain_debounce_time = gameLocal.time + pain_delay + 1;  // ignore pain since we'll play our landing anim
			Damage( NULL, NULL, idVec3( 0, 0, -1 ), "damage_fatalfall", 1.0f, 0 );
		}
	} else if ( delta > hardDelta ) {
		AI_HARDLANDING = true;
		landChange	= -24;
		landTime	= gameLocal.time;
		if ( !noDamage ) {
			pain_debounce_time = gameLocal.time + pain_delay + 1;  // ignore pain since we'll play our landing anim
			Damage( NULL, NULL, idVec3( 0, 0, -1 ), "damage_hardfall", 1.0f, 0 );
		}
	} else if ( delta > 30 ) {
		AI_HARDLANDING = true;
		landChange	= -16;
		landTime	= gameLocal.time;
		if ( !noDamage ) {
			pain_debounce_time = gameLocal.time + pain_delay + 1;  // ignore pain since we'll play our landing anim
			Damage( NULL, NULL, idVec3( 0, 0, -1 ), "damage_softfall", 1.0f, 0 );
		}
	} else if ( delta > 7 ) {
		AI_SOFTLANDING = true;
		landChange	= -8;
		landTime	= gameLocal.time;
	} else if ( delta > 3 ) {
		// just walk on
	}
}

/*
===============
idPlayer::BobCycle
===============
*/
void idPlayer::BobCycle( const idVec3 &pushVelocity ) {
	float		bobmove;
	int			old, deltaTime;
	idVec3		vel, gravityDir, velocity;
	idMat3		viewaxis;
	float		bob;
	float		delta;
	float		speed;
	float		f;

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	velocity = physicsObj.GetLinearVelocity() - pushVelocity;

	gravityDir = physicsObj.GetGravityNormal();
	vel = velocity - ( velocity * gravityDir ) * gravityDir;
	xyspeed = vel.LengthFast();

	// do not evaluate the bob for other clients
	// when doing a spectate follow, don't do any weapon bobbing
	if ( gameLocal.isClient && entityNumber != gameLocal.localClientNum ) {
		viewBobAngles.Zero();
		viewBob.Zero();
		return;
	}

/*	if ( !physicsObj.HasGroundContacts() || influenceActive == INFLUENCE_LEVEL2 || ( gameLocal.isMultiplayer && spectating ) ) {
		// airborne
		bobCycle = 0;
		bobFoot = 0;
		bobfracsin = 0;
	} else if ( ( !usercmd.forwardmove && !usercmd.rightmove ) || ( xyspeed <= MIN_BOB_SPEED ) ) {
		// start at beginning of cycle again
		bobCycle = 0;
		bobFoot = 0;
		bobfracsin = 0;
	} else {
		if ( physicsObj.IsCrouching() ) {
			bobmove = pm_crouchbob.GetFloat();
			// ducked characters never play footsteps
		} else {
			// vary the bobbing based on the speed of the player
			bobmove = pm_walkbob.GetFloat() * ( 1.0f - bobFrac ) + pm_runbob.GetFloat() * bobFrac;
		}

		// check for footstep / splash sounds
		old = bobCycle;
		bobCycle = (int)( old + bobmove * USERCMD_MSEC ) & 255;
		bobFoot = ( bobCycle & 128 ) >> 7;
		bobfracsin = idMath::Fabs( sin( ( bobCycle & 127 ) / 127.0 * idMath::PI ) );
	}*/

	//Disable bobbing in VR
	bobCycle = 0;
	bobFoot = 0;
	bobfracsin = 0;

	// calculate angles for view bobbing
	viewBobAngles.Zero();

	viewaxis = viewAngles.ToMat3() * physicsObj.GetGravityAxis();

	// add angles based on velocity
	delta = velocity * viewaxis[0];
	viewBobAngles.pitch += delta * pm_runpitch.GetFloat();

	delta = velocity * viewaxis[1];
	viewBobAngles.roll -= delta * pm_runroll.GetFloat();

	// add angles based on bob
	// make sure the bob is visible even at low speeds
	speed = xyspeed > 200 ? xyspeed : 200;

	delta = bobfracsin * pm_bobpitch.GetFloat() * speed;
	if ( physicsObj.IsCrouching() ) {
		delta *= 3;		// crouching
	}
	viewBobAngles.pitch += delta;
	delta = bobfracsin * pm_bobroll.GetFloat() * speed;
	if ( physicsObj.IsCrouching() ) {
		delta *= 3;		// crouching accentuates roll
	}
	if ( bobFoot & 1 ) {
		delta = -delta;
	}
	viewBobAngles.roll += delta;

	// calculate position for view bobbing
	viewBob.Zero();

	if ( physicsObj.HasSteppedUp() ) {

		// check for stepping up before a previous step is completed
		deltaTime = gameLocal.time - stepUpTime;
		if ( deltaTime < STEPUP_TIME ) {
			stepUpDelta = stepUpDelta * ( STEPUP_TIME - deltaTime ) / STEPUP_TIME + physicsObj.GetStepUp();
		} else {
			stepUpDelta = physicsObj.GetStepUp();
		}
		if ( stepUpDelta > 2.0f * pm_stepsize.GetFloat() ) {
			stepUpDelta = 2.0f * pm_stepsize.GetFloat();
		}
		stepUpTime = gameLocal.time;
	}


	idVec3 gravity = physicsObj.GetGravityNormal();

	// if the player stepped up recently
	deltaTime = gameLocal.time - stepUpTime;
	if ( deltaTime < STEPUP_TIME ) {
		viewBob += gravity * ( stepUpDelta * ( STEPUP_TIME - deltaTime ) / STEPUP_TIME );
	}

	// add bob height after any movement smoothing
	bob = bobfracsin * xyspeed * pm_bobup.GetFloat();
	if ( bob > 6 ) {
		bob = 6;
	}
	viewBob[2] += bob;

	// add fall height
	delta = gameLocal.time - landTime;
	if ( delta < LAND_DEFLECT_TIME ) {
		f = delta / LAND_DEFLECT_TIME;
		viewBob -= gravity * ( landChange * f );
	} else if ( delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME ) {
		delta -= LAND_DEFLECT_TIME;
		f = 1.0 - ( delta / LAND_RETURN_TIME );
		viewBob -= gravity * ( landChange * f );
	}
}

/*
================
idPlayer::UpdateDeltaViewAngles
================
*/
void idPlayer::UpdateDeltaViewAngles( const idAngles &angles ) {
	// set the delta angle
	idAngles delta;
	for( int i = 0; i < 3; i++ ) {
		delta[ i ] = angles[ i ] - SHORT2ANGLE( usercmd.angles[ i ] );
	}
	SetDeltaViewAngles( delta );
}

/*
===============
idPlayer::UpdateFlashLight
===============
*/
void idPlayer::UpdateFlashlight()
{
	for( int h = 0; h < 2; h++ )
	{
		if( hands[h].idealWeapon == weapon_flashlight )
		{
			// force classic flashlight to go away
			hands[h].NextWeapon();
		}
	}

	if( !flashlight.IsValid() )
	{
		return;
	}

	if( !flashlight->GetOwner() )
	{
		return;
	}

	// Don't update the flashlight if dead in MP.
	// Otherwise you can see a floating flashlight worldmodel near player's skeletons.
	if( gameLocal.isMultiplayer )
	{
		if( health < 0 )
		{
			return;
		}
	}

	// Flashlight has an infinite battery in multiplayer and in RoE XBox mode when mounted on your pistol
	if( !gameLocal.isMultiplayer && commonVr->currentFlashlightMode != FLASHLIGHT_PISTOL )
	{
		if( flashlight->lightOn )
		{
			if( flashlight_batteryDrainTimeMS.GetInteger() > 0 )
			{
				flashlightBattery -= ( gameLocal.time - gameLocal.previousTime );
				if( flashlightBattery < 0 )
				{
					FlashlightOff();
					flashlightBattery = 0;
				}
			}
		}
		else
		{
			if( flashlightBattery < flashlight_batteryDrainTimeMS.GetInteger() )
			{
				flashlightBattery += ( gameLocal.time - gameLocal.previousTime ) * Max( 1, ( flashlight_batteryDrainTimeMS.GetInteger() / flashlight_batteryChargeTimeMS.GetInteger() ) );
				if( flashlightBattery > flashlight_batteryDrainTimeMS.GetInteger() )
				{
					flashlightBattery = flashlight_batteryDrainTimeMS.GetInteger();
				}
			}
		}
	}

	if( hud )
	{
		//hud->UpdateFlashlight( this );
	}

	if( gameLocal.isClient )
	{
		// clients need to wait till the weapon and it's world model entity
		// are present and synchronized ( weapon.worldModel idEntityPtr to idAnimatedEntity )
		if( !flashlight->IsWorldModelReady() )
		{
			return;
		}
	}

	// always make sure the weapon is correctly setup before accessing it
	if( !flashlight->IsLinked() )
	{
		flashlight->GetWeaponDef( "weapon_flashlight_new", 0 );
		flashlight->SetIsPlayerFlashlight( true );

		// adjust position / orientation of flashlight
		idAnimatedEntity* worldModel = flashlight->GetWorldModel();
		worldModel->BindToJoint( this, "Chest", true );
		// Don't interpolate the flashlight world model in mp, let it bind like normal.
		//worldModel->SetUseClientInterpolation( false );

		assert( flashlight->IsLinked() );
	}

	// this positions the third person flashlight model! (as seen in the mirror)
	idAnimatedEntity* worldModel = flashlight->GetWorldModel();
	static const idVec3 fl_pos = idVec3( 3.0f, 9.0f, 2.0f );
	worldModel->GetPhysics()->SetOrigin( fl_pos );
	static float fl_pitch = 0.0f;
	static float fl_yaw = 0.0f;
	static float fl_roll = 0.0f;
	static idAngles ang = ang_zero;
	ang.Set( fl_pitch, fl_yaw, fl_roll );
	worldModel->GetPhysics()->SetAxis( ang.ToMat3() );

	if( flashlight->lightOn )
	{
		if( ( flashlightBattery < flashlight_batteryChargeTimeMS.GetInteger() / 2 ) && ( gameLocal.random.RandomFloat() < flashlight_batteryFlickerPercent.GetFloat() ) )
		{
			flashlight->RemoveMuzzleFlashlight();
		}
		else
		{
			flashlight->MuzzleFlashLight();
		}
	}

	flashlight->PresentWeapon( true, 1 - vr_weaponHand.GetInteger() );

	if( gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) || gameLocal.inCinematic || spectating || fl.hidden || commonVr->currentFlashlightPosition == FLASHLIGHT_INVENTORY )
	{
		worldModel->Hide();
	}
	else
	{
		worldModel->Show();
	}
}

/*
================
idPlayer::SetViewAngles
================
*/
void idPlayer::SetViewAngles( const idAngles &angles ) {
	UpdateDeltaViewAngles( angles );
	viewAngles = angles;
}

/*
================
idPlayer::UpdateViewAngles
================
*/
void idPlayer::UpdateViewAngles( void ) {
	int i;
	idAngles delta;

	if ( !noclip && ( gameLocal.inCinematic || privateCameraView || gameLocal.GetCamera() || influenceActive == INFLUENCE_LEVEL2) ) {
		// no view changes at all, but we still want to update the deltas or else when
		// we get out of this mode, our view will snap to a kind of random angle
		UpdateDeltaViewAngles( viewAngles );

        // Koz fixme - this was in tmeks fork, verify what we are doing here is still appropriate.
        for ( i = 0; i < 3; i++ )
        {
            cmdAngles[i] = SHORT2ANGLE( usercmd.angles[i] );
            if ( influenceActive == INFLUENCE_LEVEL3 )
            {
                viewAngles[i] += idMath::ClampFloat( -1.0f, 1.0f, idMath::AngleDelta( idMath::AngleNormalize180( SHORT2ANGLE( usercmd.angles[i] ) + deltaViewAngles[i] ), viewAngles[i] ) );
            }
            else
            {
                viewAngles[i] = idMath::AngleNormalize180( SHORT2ANGLE( usercmd.angles[i] ) + deltaViewAngles[i] );
            }
        }
        // Koz end

		return;
	}

	// if dead
	if ( health <= 0 ) {
		if ( pm_thirdPersonDeath.GetBool() ) {
			viewAngles.roll = 0.0f;
			viewAngles.pitch = 30.0f;
		} else {
			viewAngles.roll = 40.0f;
			viewAngles.pitch = -15.0f;
		}
		return;
	}

	// circularly clamp the angles with deltas
	for ( i = 1; i < 3; i++ ) {
		cmdAngles[i] = SHORT2ANGLE( usercmd.angles[i] );
		if ( influenceActive == INFLUENCE_LEVEL3 ) {
			viewAngles[i] += idMath::ClampFloat( -1.0f, 1.0f, idMath::AngleDelta( idMath::AngleNormalize180( SHORT2ANGLE( usercmd.angles[i]) + deltaViewAngles[i] ) , viewAngles[i] ) );
		} else {
			viewAngles[i] = idMath::AngleNormalize180( SHORT2ANGLE( usercmd.angles[i]) + deltaViewAngles[i] );
		}
	}

	if ( !centerView.IsDone( gameLocal.time ) ) {
		viewAngles.pitch = centerView.GetCurrentValue(gameLocal.time);
	}

	// clamp the pitch
	if ( noclip ) {
		if ( viewAngles.pitch > 89.0f ) {
			// don't let the player look down more than 89 degrees while noclipping
			viewAngles.pitch = 89.0f;
		} else if ( viewAngles.pitch < -89.0f ) {
			// don't let the player look up more than 89 degrees while noclipping
			viewAngles.pitch = -89.0f;
		}
	} else if (mountedObject) {
		int yaw_min, yaw_max, varc;

		mountedObject->GetAngleRestrictions(yaw_min, yaw_max, varc);

		if (yaw_min < yaw_max) {
			viewAngles.yaw = idMath::ClampFloat(yaw_min, yaw_max, viewAngles.yaw);
		} else {
			if (viewAngles.yaw < 0) {
				viewAngles.yaw = idMath::ClampFloat(-180.f, yaw_max, viewAngles.yaw);
			} else {
				viewAngles.yaw = idMath::ClampFloat(yaw_min, 180.f, viewAngles.yaw);
			}
		}

		viewAngles.pitch = idMath::ClampFloat(-varc, varc, viewAngles.pitch);
	} else {
		if ( viewAngles.pitch > pm_maxviewpitch.GetFloat() ) {
			// don't let the player look down enough to see the shadow of his (non-existant) feet
			viewAngles.pitch = pm_maxviewpitch.GetFloat();
		} else if ( viewAngles.pitch < pm_minviewpitch.GetFloat() ) {
			// don't let the player look up more than 89 degrees
			viewAngles.pitch = pm_minviewpitch.GetFloat();
		}
	}

	UpdateDeltaViewAngles( viewAngles );

	// orient the model towards the direction we're looking
	SetAngles( idAngles( 0, viewAngles.yaw, 0 ) );

	// save in the log for analyzing weapon angle offsets
	loggedViewAngles[ gameLocal.framenum & (NUM_LOGGED_VIEW_ANGLES-1) ] = viewAngles;
}

/*
==============
idPlayer::AdjustHeartRate

Player heartrate works as follows

DEF_HEARTRATE is resting heartrate

Taking damage when health is above 75 adjusts heart rate by 1 beat per second
Taking damage when health is below 75 adjusts heart rate by 5 beats per second
Maximum heartrate from damage is MAX_HEARTRATE

Firing a weapon adds 1 beat per second up to a maximum of COMBAT_HEARTRATE

Being at less than 25% stamina adds 5 beats per second up to ZEROSTAMINA_HEARTRATE

All heartrates are target rates.. the heart rate will start falling as soon as there have been no adjustments for 5 seconds
Once it starts falling it always tries to get to DEF_HEARTRATE

The exception to the above rule is upon death at which point the rate is set to DYING_HEARTRATE and starts falling
immediately to zero

Heart rate volumes go from zero ( -40 db for DEF_HEARTRATE to 5 db for MAX_HEARTRATE ) the volume is
scaled linearly based on the actual rate

Exception to the above rule is once the player is dead, the dying heart rate starts at either the current volume if
it is audible or -10db and scales to 8db on the last few beats
==============
*/
void idPlayer::AdjustHeartRate( int target, float timeInSecs, float delay, bool force ) {

	if ( heartInfo.GetEndValue() == target ) {
		return;
	}

	if ( AI_DEAD && !force ) {
		return;
	}

	lastHeartAdjust = gameLocal.time;

	heartInfo.Init( gameLocal.time + delay * 1000, timeInSecs * 1000, heartRate, target );
}

/*
==============
idPlayer::GetBaseHeartRate
==============
*/
int idPlayer::GetBaseHeartRate( void ) {
	int base = idMath::FtoiFast( ( BASE_HEARTRATE + LOWHEALTH_HEARTRATE_ADJ ) - ( (float)health / 100.0f ) * LOWHEALTH_HEARTRATE_ADJ );
	int rate = idMath::FtoiFast( base + ( ZEROSTAMINA_HEARTRATE - base ) * ( 1.0f - stamina / pm_stamina.GetFloat() ) );
	int diff = ( lastDmgTime ) ? gameLocal.time - lastDmgTime : 99999;
	rate += ( diff < 5000 ) ? ( diff < 2500 ) ? ( diff < 1000 ) ? 15 : 10 : 5 : 0;
	return rate;
}

int idPlayer::GetBestWeaponHand()
{
    if( !weaponEnabled || hands[1 - vr_weaponHand.GetInteger()].idealWeapon == weapon_fists )
    {
        return vr_weaponHand.GetInteger();
    }
    if( hands[ vr_weaponHand.GetInteger() ].idealWeapon == weapon_fists )
    {
        return 1 - vr_weaponHand.GetInteger();
    }

    int w = MAX_WEAPONS;
    while( w > 0 )
    {
        w--;
        if( w == weapon_flashlight )
        {
            continue;
        }
        const char* weap = spawnArgs.GetString( va( "def_weapon%d", w ) );
        common->Printf( "Checking Best Hand = %s\n", spawnArgs.GetString( va( "def_weapon%d", w ) ));
        //if( !weap[ 0 ] || ( ( inventory.weapons & ( 1 << w ) ) == 0 ) || ( !inventory.HasAmmo( weap, true, this ) ) )
        if( !weap[ 0 ] || ( ( inventory.weapons & ( 1 << w ) ) == 0 ) || ( !inventory.HasAmmo(weap) ) )
        {
            continue;
        }
        if( !spawnArgs.GetBool( va( "weapon%d_best", w ) ) )
        {
            continue;
        }

        //Some weapons will report having ammo but the clip is empty and
        //will not have enough to fill the clip (i.e. Double Barrel Shotgun with 1 round left)
        //We need to skip these weapons because they cannot be used
        //GB Not sure any of these guns are in D3
        /*
        if( inventory.HasEmptyClipCannotRefill( weap, this, false ) && inventory.HasEmptyClipCannotRefill( weap, this, true ) )
        {
            continue;
        }*/

        if( hands[ vr_weaponHand.GetInteger() ].idealWeapon == w )
            return vr_weaponHand.GetInteger();
        if( hands[ 1- vr_weaponHand.GetInteger() ].idealWeapon == w )
            return 1 - vr_weaponHand.GetInteger();
    }
    w = MAX_WEAPONS;
    while( w > 0 )
    {
        w--;
        if( w == weapon_flashlight )
        {
            continue;
        }
        const char* weap = spawnArgs.GetString( va( "def_weapon%d", w ) );
        //if( !weap[ 0 ] || ( ( inventory.weapons & ( 1 << w ) ) == 0 ) || ( !inventory.HasAmmo( weap, true, this ) ) )
        if( !weap[ 0 ] || ( ( inventory.weapons & ( 1 << w ) ) == 0 ) || ( !inventory.HasAmmo( weap ) ) )
        {
            continue;
        }

        //Some weapons will report having ammo but the clip is empty and
        //will not have enough to fill the clip (i.e. Double Barrel Shotgun with 1 round left)
        //We need to skip these weapons because they cannot be used
        /*if( inventory.HasEmptyClipCannotRefill( weap, this, false ) && inventory.HasEmptyClipCannotRefill( weap, this, true ) )
        {
            continue;
        }*/

        if( hands[ vr_weaponHand.GetInteger() ].idealWeapon == w )
            return vr_weaponHand.GetInteger();
        if( hands[ 1 - vr_weaponHand.GetInteger() ].idealWeapon == w )
            return 1 - vr_weaponHand.GetInteger();
    }
    w = MAX_WEAPONS;
    while( w > 0 )
    {
        w--;
        if( w == weapon_flashlight )
        {
            continue;
        }
        const char* weap = spawnArgs.GetString( va( "def_weapon%d", w ) );
        if( !weap[ 0 ] || ( ( inventory.weapons & ( 1 << w ) ) == 0 ) )
        {
            continue;
        }

        if( hands[ vr_weaponHand.GetInteger() ].idealWeapon == w )
            return vr_weaponHand.GetInteger();
        if( hands[ 1 - vr_weaponHand.GetInteger() ].idealWeapon == w )
            return 1 - vr_weaponHand.GetInteger();
    }

    return vr_weaponHand.GetInteger();
}

/*
=================
idPlayer::GetCurrentHarvestWeapon
Carl: Dual wielding, get the specific weapon used to harvest souls (Soul Cube or Artifact)
Returns the required one if you're holding it, or the other one, or the main weapon
=================
*/
idStr idPlayer::GetCurrentHarvestWeapon( idStr requiredWeapons )
{
    // Carl: TODO dual wielding
    if( hands[ 0 ].weapon && ( hands[ 0 ].weapon->IdentifyWeapon() == WEAPON_SOULCUBE || hands[ 0 ].weapon->IdentifyWeapon() == WEAPON_ARTIFACT ) )
        return hands[ 0 ].GetCurrentWeaponString();
    if( hands[ 1 ].weapon && ( hands[ 1 ].weapon->IdentifyWeapon() == WEAPON_SOULCUBE || hands[ 1 ].weapon->IdentifyWeapon() == WEAPON_ARTIFACT ) )
        return hands[ 1 ].GetCurrentWeaponString();
    return GetCurrentWeapon();
}

/*
===============
idPlayer::GetBestWeaponHandToSteal
Carl: Dual wielding, when the code needs just one weapon, guess which one is the "main" one
===============
*/
int idPlayer::GetBestWeaponHandToSteal( idPlayer* thief )
{
    // Carl: TODO dual wielding
    return GetBestWeaponHand();
}

/*
==============
idPlayer::SetCurrentHeartRate
==============
*/
void idPlayer::SetCurrentHeartRate( void ) {

	int base = idMath::FtoiFast( ( BASE_HEARTRATE + LOWHEALTH_HEARTRATE_ADJ ) - ( (float) health / 100.0f ) * LOWHEALTH_HEARTRATE_ADJ );

	if ( PowerUpActive( ADRENALINE )) {
		heartRate = 135;
	} else {
		heartRate = idMath::FtoiFast( heartInfo.GetCurrentValue( gameLocal.time ) );
		int currentRate = GetBaseHeartRate();
		if ( health >= 0 && gameLocal.time > lastHeartAdjust + 2500 ) {
			AdjustHeartRate( currentRate, 2.5f, 0.0f, false );
		}
	}

	int bps = idMath::FtoiFast( 60.0f / heartRate * 1000.0f );
	if ( gameLocal.time - lastHeartBeat > bps ) {
		int dmgVol = DMG_VOLUME;
		int deathVol = DEATH_VOLUME;
		int zeroVol = ZERO_VOLUME;
		float pct = 0.0;
		if ( heartRate > BASE_HEARTRATE && health > 0 ) {
			pct = (float)(heartRate - base) / (MAX_HEARTRATE - base);
			pct *= ((float)dmgVol - (float)zeroVol);
		} else if ( health <= 0 ) {
			pct = (float)(heartRate - DYING_HEARTRATE) / (BASE_HEARTRATE - DYING_HEARTRATE);
			if ( pct > 1.0f ) {
				pct = 1.0f;
			} else if (pct < 0.0f) {
				pct = 0.0f;
			}
			pct *= ((float)deathVol - (float)zeroVol);
		}

		pct += (float)zeroVol;

		if ( pct != zeroVol ) {
			StartSound( "snd_heartbeat", SND_CHANNEL_HEART, SSF_PRIVATE_SOUND, false, NULL );
			// modify just this channel to a custom volume
			soundShaderParms_t	parms;
			memset( &parms, 0, sizeof( parms ) );
			parms.volume = pct;
			refSound.referenceSound->ModifySound( SND_CHANNEL_HEART, &parms );
		}

		lastHeartBeat = gameLocal.time;
	}
}

/*
==============
idPlayer::SetFlashHandPose() // Call set flashlight hand pose script function
Updates the pose of the player model flashlight hand
======
*/
void idPlayer::SetFlashHandPose() // Call set flashlight hand pose script function
{

    const function_t* func;

    func = scriptObject.GetFunction( "SetFlashHandPose" ); // Set flashlight hand pose
    if ( func )
    {
        // use the frameCommandThread since it's safe to use outside of framecommands
        // Koz debug common->Printf( "Calling SetFlashHandPose\n" ); // Set flashlight hand pose
        gameLocal.frameCommandThread->CallFunction( this, func, true );
        gameLocal.frameCommandThread->Execute();

    }
    else
    {
        common->Warning( "Can't find function 'SetFlashHandPose' in object '%s'", scriptObject.GetTypeName() ); // Set flashlight hand pose
        return;
    }
}

/*
==============
Koz idPlayer::SetHandIKPos
Set the position for the hand based on weapon origin
==============
Carl: TODO Dual wielding
*/
void idPlayer::SetHandIKPos( int hand, idVec3 handOrigin, idMat3 handAxis, idQuat rotation, bool isFlashlight )
{
    // this is for arm IK when viewing player body
    // the position for the player hand joint is modified
    // to reflect the position of the viewmodel.
    // armIK / reach_ik then performs crude IK on arm using new positon.

    jointHandle_t weaponHandAttachJoint = INVALID_JOINT; // this is the joint on the WEAPON the hand should meet
    jointHandle_t handWeaponAttachJoint = INVALID_JOINT; // the is the joint on the HAND the weapon should meet

    static idVec3 weaponHandAttachJointPositionLocal = vec3_zero;
    static idMat3 weaponHandAttachJointAxisLocal = mat3_identity;

    idVec3 weaponHandAttachJointDefaultPositionLocal = vec3_zero;

    idVec3 weaponAttachDelta = vec3_zero;

    idVec3 handAttacherPositionLocal = vec3_zero;
    idVec3 handAttacherPositionGlobal = vec3_zero;
    idMat3 handmat = mat3_identity;

    //idMat3 rot180 = idAngles( 0.0f, 180.0f, 0.0f ).ToMat3();

    commonVr->currentHandWorldPosition[hand] = handOrigin;

    hands[hand].handOrigin = handOrigin;
    hands[hand].handAxis = handAxis;

    weapon_t currentWeaponEnum = hands[hand].weapon->IdentifyWeapon();
    idEntityPtr<idWeapon> curEntity;
    int activeWeapon;
    if( isFlashlight && commonVr->currentFlashlightPosition == FLASHLIGHT_HAND && flashlight.IsValid() )
    {
        curEntity = flashlight;
        activeWeapon = weapon_flashlight;
    }
    else
    {
        curEntity = hands[hand].weapon;
        activeWeapon = hands[hand].currentWeapon;
    }

    handWeaponAttachJoint = ik_handAttacher[hand]; // joint on the hand the weapon attaches to
    weaponHandAttachJoint = curEntity->weaponHandAttacher[hand]; // joint on the weapon the hand should attach to
    weaponHandAttachJointDefaultPositionLocal = curEntity->weaponHandDefaultPos[hand];// the default position of the attacher on the weapon. used to calc movement deltas from anims


    //get the local and global orientations for the hand and weapon attacher joints

    //weapon hand attacher - the joint on the weapon the hand should align to
    curEntity->GetAnimator()->GetJointTransform( weaponHandAttachJoint, gameLocal.time, weaponHandAttachJointPositionLocal, weaponHandAttachJointAxisLocal );

    /*
    // for debugging
    weaponHandAttachJointPositionGlobal = weaponHandAttachJointPositionLocal * curEntity->GetRenderEntity()->axis + curEntity->GetRenderEntity()->origin;
    weaponHandAttachJointAxisGlobal = weaponHandAttachJointAxisLocal * curEntity->GetRenderEntity()->axis;
    DebugCross( weaponHandAttachJointPositionGlobal, weaponHandAttachJointAxisGlobal, colorBlue );
    */

    // calculate the delta between the weaponAttach joint default position and the animated position for this frame
    // this is in model space
    weaponAttachDelta = weaponHandAttachJointPositionLocal - weaponHandAttachJointDefaultPositionLocal;

    if ( activeWeapon == weapon_pda && hand == vr_weaponHand.GetInteger() )
    {
        //the PDA is actually being held in the off hand,
        //so don't let the weapon animation adjust the hand position.
        weaponHandAttachJointAxisLocal = mat3_identity;
        weaponAttachDelta = vec3_zero;
    }

    //handOrigin and handAxis are the points in world space where the attacher joint on the hand model should be moved to.
    //this location is updated by weaponAttachDelta so the weapon animation can drive the hand.

    handOrigin += handWeaponAttacherToDefaultOffset[hand][activeWeapon] * handAxis;

    handAttacherPositionGlobal = handOrigin + ( weaponAttachDelta * handAxis );

    // for debugging
    //DebugCross( handAttacherPositionGlobal, handAxis, colorRed );

    handAttacherPositionLocal = handAttacherPositionGlobal - renderEntity.origin;
    handAttacherPositionLocal *= renderEntity.axis.Inverse();

    handmat = weaponHandAttachJointAxisLocal * rotation.ToMat3();

    handAttacherPositionLocal -= handWeaponAttachertoWristJointOffset[hand][activeWeapon] * handmat;

    /*
    idVec3 debug1 = handAttacherPositionLocal * renderEntity.axis;
    debug1 += renderEntity.origin;
    DebugCross( debug1, handmat * renderEntity.axis, colorGreen );
    */

    handmat = ik_handCorrectAxis[hand][activeWeapon] * handmat;

    GetAnimator()->SetJointPos( armIK.handJoints[hand], JOINTMOD_WORLD_OVERRIDE, handAttacherPositionLocal );
    GetAnimator()->SetJointAxis( armIK.handJoints[hand], JOINTMOD_WORLD_OVERRIDE, handmat );

    commonVr->handRoll[hand] = rotation.ToAngles().roll;

}

/*
==============
idPlayer::UpdateAir
==============
*/
void idPlayer::UpdateAir( void ) {
	if ( health <= 0 ) {
		return;
	}

	// see if the player is connected to the info_vacuum
	bool	newAirless = false;

	if ( gameLocal.vacuumAreaNum != -1 ) {
		int	num = GetNumPVSAreas();
		if ( num > 0 ) {
			int		areaNum;

			// if the player box spans multiple areas, get the area from the origin point instead,
			// otherwise a rotating player box may poke into an outside area
			if ( num == 1 ) {
				const int	*pvsAreas = GetPVSAreas();
				areaNum = pvsAreas[0];
			} else {
				areaNum = gameRenderWorld->PointInArea( this->GetPhysics()->GetOrigin() );
			}
			newAirless = gameRenderWorld->AreasAreConnected( gameLocal.vacuumAreaNum, areaNum, PS_BLOCK_AIR );
		}
	}

	if (PowerUpActive(ENVIROTIME)) {
		airTics = pm_airTics.GetInteger(); //Lubos
		newAirless = false;
	}

	if ( newAirless ) {
		if ( !airless ) {
			StartSound( "snd_decompress", SND_CHANNEL_ANY, SSF_GLOBAL, false, NULL );
			StartSound( "snd_noAir", SND_CHANNEL_BODY2, 0, false, NULL );
			if ( hud ) {
				hud->HandleNamedEvent( "noAir" );
			}
		}
		airTics--;
		if ( airTics < 0 ) {
			airTics = 0;
			// check for damage
			const idDict *damageDef = gameLocal.FindEntityDefDict( "damage_noair", false );
			int dmgTiming = 1000 * ((damageDef) ? damageDef->GetFloat( "delay", "3.0" ) : 3.0f );
			if ( gameLocal.time > lastAirDamage + dmgTiming ) {
				Damage( NULL, NULL, vec3_origin, "damage_noair", 1.0f, 0 );
				lastAirDamage = gameLocal.time;
			}
		}

	} else {
		if ( airless ) {
			StartSound( "snd_recompress", SND_CHANNEL_ANY, SSF_GLOBAL, false, NULL );
			StopSound( SND_CHANNEL_BODY2, false );
			if ( hud ) {
				hud->HandleNamedEvent( "Air" );
			}
		}
		airTics+=2;	// regain twice as fast as lose
		if ( airTics > pm_airTics.GetInteger() ) {
			airTics = pm_airTics.GetInteger();
		}
	}

	airless = newAirless;

	if ( hud ) {
		hud->SetStateInt( "player_air", 100 * airTics / pm_airTics.GetInteger() );
	}
}

void idPlayer::UpdatePowerupHud() {

    if (health <= 0) {
       return;
    }

    if (lastHudPowerup != hudPowerup) {

        if (hudPowerup == -1) {
            //The powerup hud should be turned off
            if (hud) {
                hud->HandleNamedEvent("noPowerup");
            }
        } else {
            //Turn the pwoerup hud on
            if (hud) {
                hud->HandleNamedEvent("Powerup");
            }
        }

        lastHudPowerup = hudPowerup;
    }

    if (hudPowerup != -1) {
        if (PowerUpActive(hudPowerup)) {
            int remaining = inventory.powerupEndTime[ hudPowerup ] - gameLocal.time;
            int filledbar = idMath::ClampInt(0, hudPowerupDuration, remaining);

            if (hud) {
                hud->SetStateInt("player_powerup", 100 * filledbar / hudPowerupDuration);
                hud->SetStateInt("player_poweruptime", remaining / 1000);
            }
        }
    }
}

/*
==============
idPlayer::AddGuiPDAData
==============
 */
int idPlayer::AddGuiPDAData( const declType_t dataType, const char *listName, const idDeclPDA *src, idUserInterface *gui ) {
	int c, i;
	idStr work;
	if ( dataType == DECL_EMAIL ) {
		c = src->GetNumEmails();
		for ( i = 0; i < c; i++ ) {
			const idDeclEmail *email = src->GetEmailByIndex( i );
			if ( email == NULL ) {
				work = va( "-\tEmail %d not found\t-", i );
			} else {
				work = email->GetFrom();
				work += "\t";
				work += email->GetSubject();
				work += "\t";
				work += email->GetDate();
			}
			gui->SetStateString( va( "%s_item_%i", listName, i ), work );
		}
		return c;
	} else if ( dataType == DECL_AUDIO ) {
		c = src->GetNumAudios();
		for ( i = 0; i < c; i++ ) {
			const idDeclAudio *audio = src->GetAudioByIndex( i );
			if ( audio == NULL ) {
				work = va( "Audio Log %d not found", i );
			} else {
				work = audio->GetAudioName();
			}
			gui->SetStateString( va( "%s_item_%i", listName, i ), work );
		}
		return c;
	} else if ( dataType == DECL_VIDEO ) {
		c = inventory.videos.Num();
		for ( i = 0; i < c; i++ ) {
			const idDeclVideo *video = GetVideo( i );
			if ( video == NULL ) {
				work = va( "Video CD %s not found", inventory.videos[i].c_str() );
			} else {
				work = video->GetVideoName();
			}
			gui->SetStateString( va( "%s_item_%i", listName, i ), work );
		}
		return c;
	}
	return 0;
}

/*
==============
idPlayer::GetPDA
==============
 */
const idDeclPDA *idPlayer::GetPDA( void ) const {
	if ( inventory.pdas.Num() ) {
		return static_cast< const idDeclPDA* >( declManager->FindType( DECL_PDA, inventory.pdas[ 0 ] ) );
	} else {
		return NULL;
	}
}

/*
===============
idPlayer::GetGrabberWeapon
Carl: Dual wielding
===============
*/
idWeapon* idPlayer::GetGrabberWeapon() const
{
	if( hands[ 1 - vr_weaponHand.GetInteger() ].weapon.GetEntity() && hands[ 1 - vr_weaponHand.GetInteger() ].weapon->IdentifyWeapon() == WEAPON_GRABBER )
		return hands[ 1 - vr_weaponHand.GetInteger() ].weapon.GetEntity();
	else
		return hands[ vr_weaponHand.GetInteger() ].weapon.GetEntity();
}

idWeapon * idPlayer::GetPDAWeapon() const
{
    if( hands[ 1 - vr_weaponHand.GetInteger() ].weapon && hands[ 1 - vr_weaponHand.GetInteger() ].weapon->IdentifyWeapon() == WEAPON_PDA )
        return hands[ 1 - vr_weaponHand.GetInteger() ].weapon;
    else
        return hands[ vr_weaponHand.GetInteger() ].weapon;
}

/*
==============
idPlayer::GetVideo
==============
*/
const idDeclVideo *idPlayer::GetVideo( int index ) {
	if ( index >= 0 && index < inventory.videos.Num() ) {
		return static_cast< const idDeclVideo* >( declManager->FindType( DECL_VIDEO, inventory.videos[index], false ) );
	}
	return NULL;
}


/*
==============
idPlayer::UpdatePDAInfo
==============
*/
void idPlayer::UpdatePDAInfo( bool updatePDASel ) {
	int j, sel;

	if ( objectiveSystem == NULL ) {
		return;
	}

	assert( hud );

	int currentPDA = objectiveSystem->State().GetInt( "listPDA_sel_0", "0" );
	if ( currentPDA == -1 ) {
		currentPDA = 0;
	}

	if ( updatePDASel ) {
		objectiveSystem->SetStateInt( "listPDAVideo_sel_0", 0 );
		objectiveSystem->SetStateInt( "listPDAEmail_sel_0", 0 );
		objectiveSystem->SetStateInt( "listPDAAudio_sel_0", 0 );
	}

	if ( currentPDA > 0 ) {
		currentPDA = inventory.pdas.Num() - currentPDA;
	}

	// Mark in the bit array that this pda has been read
	if ( currentPDA < 128 ) {
		inventory.pdasViewed[currentPDA >> 5] |= 1 << (currentPDA & 31);
	}

	pdaAudio = "";
	pdaVideo = "";
	pdaVideoWave = "";
	idStr name, data, preview, info, wave;
	for ( j = 0; j < MAX_PDAS; j++ ) {
		objectiveSystem->SetStateString( va( "listPDA_item_%i", j ), "" );
	}
	for ( j = 0; j < MAX_PDA_ITEMS; j++ ) {
		objectiveSystem->SetStateString( va( "listPDAVideo_item_%i", j ), "" );
		objectiveSystem->SetStateString( va( "listPDAAudio_item_%i", j ), "" );
		objectiveSystem->SetStateString( va( "listPDAEmail_item_%i", j ), "" );
		objectiveSystem->SetStateString( va( "listPDASecurity_item_%i", j ), "" );
	}
	for ( j = 0; j < inventory.pdas.Num(); j++ ) {

		const idDeclPDA *pda = static_cast< const idDeclPDA* >( declManager->FindType( DECL_PDA, inventory.pdas[j], false ) );

		if ( pda == NULL ) {
			continue;
		}

		int index = inventory.pdas.Num() - j;
		if ( j == 0 ) {
			// Special case for the first PDA
			index = 0;
		}

		if ( j != currentPDA && j < 128 && inventory.pdasViewed[j >> 5] & (1 << (j & 31)) ) {
			// This pda has been read already, mark in gray
			objectiveSystem->SetStateString( va( "listPDA_item_%i", index), va(S_COLOR_GRAY "%s", pda->GetPdaName()) );
		} else {
			// This pda has not been read yet
		objectiveSystem->SetStateString( va( "listPDA_item_%i", index), pda->GetPdaName() );
		}

		const char *security = pda->GetSecurity();
		if ( j == currentPDA || (currentPDA == 0 && security && *security ) ) {
			if ( *security == 0 ) {
				security = common->GetLanguageDict()->GetString( "#str_00066" );
			}
			objectiveSystem->SetStateString( "PDASecurityClearance", security );
		}

		if ( j == currentPDA ) {

			objectiveSystem->SetStateString( "pda_icon", pda->GetIcon() );
			objectiveSystem->SetStateString( "pda_id", pda->GetID() );
			objectiveSystem->SetStateString( "pda_title", pda->GetTitle() );

			if ( j == 0 ) {
				// Selected, personal pda
				// Add videos
				if ( updatePDASel || !inventory.pdaOpened ) {
				objectiveSystem->HandleNamedEvent( "playerPDAActive" );
				objectiveSystem->SetStateString( "pda_personal", "1" );
					inventory.pdaOpened = true;
				}
				objectiveSystem->SetStateString( "pda_location", hud->State().GetString("location") );
				objectiveSystem->SetStateString( "pda_name", cvarSystem->GetCVarString( "ui_name") );
				AddGuiPDAData( DECL_VIDEO, "listPDAVideo", pda, objectiveSystem );
				sel = objectiveSystem->State().GetInt( "listPDAVideo_sel_0", "0" );
				const idDeclVideo *vid = NULL;
				if ( sel >= 0 && sel < inventory.videos.Num() ) {
					vid = static_cast< const idDeclVideo * >( declManager->FindType( DECL_VIDEO, inventory.videos[ sel ], false ) );
				}
				if ( vid ) {
					pdaVideo = vid->GetRoq();
					pdaVideoWave = vid->GetWave();
					objectiveSystem->SetStateString( "PDAVideoTitle", vid->GetVideoName() );
					objectiveSystem->SetStateString( "PDAVideoVid", vid->GetRoq() );
					objectiveSystem->SetStateString( "PDAVideoIcon", vid->GetPreview() );
					objectiveSystem->SetStateString( "PDAVideoInfo", vid->GetInfo() );
				} else {
					//FIXME: need to precache these in the player def
					objectiveSystem->SetStateString( "PDAVideoVid", "sound/vo/video/welcome.tga" );
					objectiveSystem->SetStateString( "PDAVideoIcon", "sound/vo/video/welcome.tga" );
					objectiveSystem->SetStateString( "PDAVideoTitle", "" );
					objectiveSystem->SetStateString( "PDAVideoInfo", "" );
				}
			} else {
				// Selected, non-personal pda
				// Add audio logs
				if ( updatePDASel ) {
				objectiveSystem->HandleNamedEvent( "playerPDANotActive" );
				objectiveSystem->SetStateString( "pda_personal", "0" );
					inventory.pdaOpened = true;
				}
				objectiveSystem->SetStateString( "pda_location", pda->GetPost() );
				objectiveSystem->SetStateString( "pda_name", pda->GetFullName() );
				int audioCount = AddGuiPDAData( DECL_AUDIO, "listPDAAudio", pda, objectiveSystem );
				objectiveSystem->SetStateInt( "audioLogCount", audioCount );
				sel = objectiveSystem->State().GetInt( "listPDAAudio_sel_0", "0" );
				const idDeclAudio *aud = NULL;
				if ( sel >= 0 ) {
					aud = pda->GetAudioByIndex( sel );
				}
				if ( aud ) {
					pdaAudio = aud->GetWave();
					objectiveSystem->SetStateString( "PDAAudioTitle", aud->GetAudioName() );
					objectiveSystem->SetStateString( "PDAAudioIcon", aud->GetPreview() );
					objectiveSystem->SetStateString( "PDAAudioInfo", aud->GetInfo() );
				} else {
					objectiveSystem->SetStateString( "PDAAudioIcon", "sound/vo/video/welcome.tga" );
					objectiveSystem->SetStateString( "PDAAutioTitle", "" );
					objectiveSystem->SetStateString( "PDAAudioInfo", "" );
				}
			}
			// add emails
			name = "";
			data = "";
			int numEmails = pda->GetNumEmails();
			if ( numEmails > 0 ) {
				AddGuiPDAData( DECL_EMAIL, "listPDAEmail", pda, objectiveSystem );
				sel = objectiveSystem->State().GetInt( "listPDAEmail_sel_0", "-1" );
				if ( sel >= 0 && sel < numEmails ) {
					const idDeclEmail *email = pda->GetEmailByIndex( sel );
					name = email->GetSubject();
					data = email->GetBody();
				}
			}
			objectiveSystem->SetStateString( "PDAEmailTitle", name );
			objectiveSystem->SetStateString( "PDAEmailText", data );
		}
	}
	if ( objectiveSystem->State().GetInt( "listPDA_sel_0", "-1" ) == -1 ) {
		objectiveSystem->SetStateInt( "listPDA_sel_0", 0 );
	}
	objectiveSystem->StateChanged( gameLocal.time );
}

int idPlayer::MapWeaponHudId(int fpNumber)
{
	if(fpNumber == 1) //Chainsaw
		return 10;
	if(fpNumber == 2 || fpNumber == 3) // Pistol / Shotgun
		return fpNumber - 1;
	if(fpNumber >= 5 && fpNumber <= 10)
		return fpNumber - 2;
	if(fpNumber == 12) //Soul Cube
		return 9;
	if(fpNumber == 16) //Flashlight
		return 11;
	if(fpNumber == 17) //Flashlight New
		return 11;
	if(fpNumber == 18) //PDA
		return 12;
	return fpNumber;
    /*else if(fpNumber == 5) //Machine Gun
		return 3;
	else if(fpNumber == 6) //Chain Gun
		return 4;
	else if(fpNumber == 7) //Hand grenade
		return 5;
	else if(fpNumber == 8) //Plasma Gun
		return 6;
	else if(fpNumber == 9) //Rocket Launch
		return 7;
	else if(fpNumber == 10) //BFG
		return 8;*/

}

/*
==============
idPlayer::TogglePDA
==============
*/
void idPlayer::TogglePDA( int hand  ) {

	// Koz begin : reset PDA controls
	commonVr->forceLeftStick = true;

	if ( objectiveSystem == NULL ) {
		return;
	}

	if ( inventory.pdas.Num() == 0 ) {
		ShowTip( spawnArgs.GetString( "text_infoTitle" ), spawnArgs.GetString( "text_noPDA" ), true );
		return;
	}

	assert( hud );

	SetupPDASlot( objectiveSystemOpen );
	SetupHolsterSlot( 1 - hand, objectiveSystemOpen );

	if ( !objectiveSystemOpen ) {
		int j, c = inventory.items.Num();
		objectiveSystem->SetStateInt( "inv_count", c );
		for ( j = 0; j < MAX_INVENTORY_ITEMS; j++ ) {
			objectiveSystem->SetStateString( va( "inv_name_%i", j ), "" );
			objectiveSystem->SetStateString( va( "inv_icon_%i", j ), "" );
			objectiveSystem->SetStateString( va( "inv_text_%i", j ), "" );
		}
		for ( j = 0; j < c; j++ ) {
			idDict *item = inventory.items[j];
			if ( !item->GetBool( "inv_pda" ) ) {
				const char *iname = item->GetString( "inv_name" );
				const char *iicon = item->GetString( "inv_icon" );
				const char *itext = item->GetString( "inv_text" );
				objectiveSystem->SetStateString( va( "inv_name_%i", j ), iname );
				objectiveSystem->SetStateString( va( "inv_icon_%i", j ), iicon );
				objectiveSystem->SetStateString( va( "inv_text_%i", j ), itext );
				const idKeyValue *kv = item->MatchPrefix( "inv_id", NULL );
				if ( kv ) {
					objectiveSystem->SetStateString( va( "inv_id_%i", j ), kv->GetValue() );
				}
			}
		}

		for ( j = 1; j < MAX_WEAPONS; j++ ) {
			const char *weapnum = va( "def_weapon%d", j );
			const char *hudWeap = va( "weapon%d", MapWeaponHudId(j));
			int weapstate = 0;
			if ( inventory.weapons & ( 1 << j ) ) {
				const char *weap = spawnArgs.GetString( weapnum );
				if ( weap && *weap ) {
					weapstate++;
				}
			}
			objectiveSystem->SetStateInt( hudWeap, weapstate );
		}

		objectiveSystem->SetStateInt( "listPDA_sel_0", inventory.selPDA );
		objectiveSystem->SetStateInt( "listPDAVideo_sel_0", inventory.selVideo );
		objectiveSystem->SetStateInt( "listPDAAudio_sel_0", inventory.selAudio );
		objectiveSystem->SetStateInt( "listPDAEmail_sel_0", inventory.selEMail );
		UpdatePDAInfo( false );
		UpdateObjectiveInfo();
		objectiveSystem->Activate( true, gameLocal.time );
		hud->HandleNamedEvent( "pdaPickupHide" );
		hud->HandleNamedEvent( "videoPickupHide" );

		common->HapticEvent("pda_open", 0, 0, 100, 0, 0);
	} else {
		inventory.selPDA = objectiveSystem->State().GetInt( "listPDA_sel_0" );
		inventory.selVideo = objectiveSystem->State().GetInt( "listPDAVideo_sel_0" );
		inventory.selAudio = objectiveSystem->State().GetInt( "listPDAAudio_sel_0" );
		inventory.selEMail = objectiveSystem->State().GetInt( "listPDAEmail_sel_0" );
		objectiveSystem->Activate( false, gameLocal.time );

		common->HapticEvent("pda_close", 0, 0, 100, 0, 0);
	}
	//objectiveSystemOpen ^= 1;
	objectiveSystemOpen = !objectiveSystemOpen;
}

// Carl: Context sensitive VR trigger buttons, and dual wielding
// 0 = right hand, 1 = left hand; true if pressed, false if released; returns true if handled as trigger pull
// WARNING! Called from the input thread?
bool idPlayer::TriggerClickWorld( int hand, bool pressed )
{
	bool b;
	if( !pressed )
	{
		b = hands[ hand ].triggerDown;
		hands[ hand ].triggerDown = false;
		//common->Printf( " Releasing Trigger\n" );
		return b;
	}
	// if we're holding or floating a weapon, then the trigger button activates the trigger of that weapon
	b = hands[ hand ].controllingWeapon();
	// if we're over a flashlight then the trigger button activates the button of that flashlight
	b = b || ( hands[ hand ].isOverMountedFlashlight() && !hands[ hand ].tooFullToInteract() );
	hands[ hand ].triggerDown = b;
	//common->Printf( " Pressing Trigger = %d\n", b );
	return b;
}

/*
==============
idPlayer::ToggleScoreboard
==============
*/
void idPlayer::ToggleScoreboard( void ) {
	scoreBoardOpen ^= 1;
}

/*
==============
idPlayer::Spectate
==============
*/
void idPlayer::Spectate( bool spectate ) {
	idBitMsg	msg;
	byte		msgBuf[MAX_EVENT_PARAM_SIZE];

	// track invisible player bug
	// all hiding and showing should be performed through Spectate calls
	// except for the private camera view, which is used for teleports
	assert( ( teleportEntity != NULL ) || ( IsHidden() == spectating ) );

	if ( spectating == spectate ) {
		return;
	}

	spectating = spectate;

	if ( gameLocal.isServer ) {
		msg.Init( msgBuf, sizeof( msgBuf ) );
		msg.WriteBits( spectating, 1 );
		ServerSendEvent( EVENT_SPECTATE, &msg, false, -1 );
	}

	if ( spectating ) {
		// join the spectators
		ClearPowerUps();
		spectator = this->entityNumber;
		Init();
		StopRagdoll();
		SetPhysics( &physicsObj );
		physicsObj.DisableClip();
		Hide();
		Event_DisableWeapon();
		if ( hud ) {
			hud->HandleNamedEvent( "aim_clear" );
			MPAimFadeTime = 0;
		}
	} else {
		// put everything back together again
        // put everything back together again
        hands[ 0 ].currentWeapon = -1;	// to make sure the def will be loaded if necessary
        hands[ 1 ].currentWeapon = -1;
		Show();
		Event_EnableWeapon();
        SetEyeHeight( pm_normalviewheight.GetFloat() );
	}
	SetClipModel();
}

/*
==============
idPlayer::SetClipModel
==============
*/
void idPlayer::SetClipModel( void ) {
	idBounds bounds;

	if ( spectating ) {
		bounds = idBounds( vec3_origin ).Expand( pm_spectatebbox.GetFloat() * 0.5f );
	} else {
		bounds[0].Set( -pm_bboxwidth.GetFloat() * 0.5f, -pm_bboxwidth.GetFloat() * 0.5f, 0 );
		bounds[1].Set( pm_bboxwidth.GetFloat() * 0.5f, pm_bboxwidth.GetFloat() * 0.5f, pm_normalheight.GetFloat() );
	}
	// the origin of the clip model needs to be set before calling SetClipModel
	// otherwise our physics object's current origin value gets reset to 0
	idClipModel *newClip;
	if ( pm_usecylinder.GetBool() ) {
		newClip = new idClipModel( idTraceModel( bounds, 8 ) );
		newClip->Translate( physicsObj.PlayerGetOrigin() );
		physicsObj.SetClipModel( newClip, 1.0f );
	} else {
		newClip = new idClipModel( idTraceModel( bounds ) );
		newClip->Translate( physicsObj.PlayerGetOrigin() );
		physicsObj.SetClipModel( newClip, 1.0f );
	}

    if ( game->isVR && pVRClientInfo )
    {
        commonVr->bodyClip = newClip;

        static idClipModel* newClip2;

        bounds[0].Set( -vr_headbbox.GetFloat() * 0.5f, -vr_headbbox.GetFloat() * 0.5f, 0 );
        bounds[1].Set( vr_headbbox.GetFloat() * 0.5f, vr_headbbox.GetFloat() * 0.5f, pm_normalheight.GetFloat() );

        if ( pm_usecylinder.GetBool() )
        {
            newClip2 = new idClipModel( idTraceModel( bounds, 8 ) );
            newClip2->Translate( physicsObj.PlayerGetOrigin() );
            //physicsObj.SetClipModel( newClip2, 1.0f );
        }
        else
        {
            newClip2 = new idClipModel( idTraceModel( bounds ) );
            newClip2->Translate( physicsObj.PlayerGetOrigin() );
            //physicsObj.SetClipModel( newClip2, 1.0f );
        }

        commonVr->headClip = newClip2;

    }
}

void idPlayer::SetControllerShake( float magnitude, int duration, const idVec3 &direction )
{
    idVec3 dir = direction;
    dir.Normalize();
    idVec3 left = hands[HAND_LEFT].handOrigin - hands[HAND_RIGHT].handOrigin;
    float side = left * dir * 0.5 + 0.5;

    // push magnitude up so the middle doesn't feel as weak
    float invSide = 1.0 - side;
    float rightSide = 1.0 - side*side;
    float leftSide = 1.0 - invSide*invSide;

    float leftMag = magnitude * leftSide;
    float rightMag = magnitude * rightSide;

    hands[HAND_LEFT].SetControllerShake( leftMag, duration, leftMag, duration );
    hands[HAND_RIGHT].SetControllerShake( rightMag, duration, rightMag, duration );
}

/*
==============
idPlayer::UseVehicle
==============
*/
void idPlayer::UseVehicle( void ) {
	trace_t	trace;
	idVec3 start, end;
	idEntity *ent;

	if ( GetBindMaster() && GetBindMaster()->IsType( idAFEntity_Vehicle::Type ) ) {
		Show();
		static_cast<idAFEntity_Vehicle*>(GetBindMaster())->Use( this );
	} else {
		start = GetEyePosition();
		end = start + viewAngles.ToForward() * 80.0f;
		gameLocal.clip.TracePoint( trace, start, end, MASK_SHOT_RENDERMODEL, this );
		if ( trace.fraction < 1.0f ) {
			ent = gameLocal.entities[ trace.c.entityNum ];
			if ( ent && ent->IsType( idAFEntity_Vehicle::Type ) ) {
				Hide();
				static_cast<idAFEntity_Vehicle*>(ent)->Use( this );
			}
		}
	}
}

/*
==============
idPlayer::PerformImpulse
==============
*/
void idPlayer::PerformImpulse( int impulse ) {

    //bool isIntroMap = ( idStr::FindText( gameLocal.GetMapFileName(), "mars_city1" ) >= 0 );
    bool isIntroMap = false;

	if ( gameLocal.isClient ) {
		idBitMsg	msg;
		byte		msgBuf[MAX_EVENT_PARAM_SIZE];

		assert( entityNumber == gameLocal.localClientNum );
		msg.Init( msgBuf, sizeof( msgBuf ) );
		msg.BeginWriting();
		msg.WriteBits( impulse, 6 );
		ClientSendEvent( EVENT_IMPULSE, &msg );
	}

	if ( impulse >= IMPULSE_0 && impulse <= IMPULSE_12 ) {
		SelectWeapon( impulse, false );
		return;
	}


	switch( impulse ) {
		case IMPULSE_13: {
			Reload();
			break;
		}
		case IMPULSE_14: {
			NextWeapon();
			break;
		}
		case IMPULSE_15: {
			PrevWeapon();
			break;
		}
        case IMPULSE_16:
        {
            if( flashlight.IsValid() )
            {
                if( flashlight->lightOn )
                {
                    FlashlightOff();
                }
                else if( !spectating && weaponEnabled && !hiddenWeapon && !gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) )
                {
                    FlashlightOn();
                }
            }
            break;
        }
		case IMPULSE_17: {
			if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum ) {
				gameLocal.mpGame.ToggleReady();
			}
			break;
		}
		case IMPULSE_18: {
			centerView.Init(gameLocal.time, 200, viewAngles.pitch, 0);
			break;
		}
		case IMPULSE_19: {
			// when we're not in single player, IMPULSE_19 is used for showScores
			// otherwise it opens the pda
			if ( !gameLocal.isMultiplayer ) {
				if ( objectiveSystemOpen ) {
                    TogglePDA( 1 - vr_weaponHand.GetInteger() );
				} else if ( weapon_pda >= 0 && inventory.pdas.Num()) {
					commonVr->pdaToggleTime = commonVr->Sys_Milliseconds();
					SetupPDASlot( false );
                    SetupHolsterSlot( vr_weaponHand.GetInteger(), false );
                    hands[ 1 - vr_weaponHand.GetInteger() ].SelectWeapon(weapon_pda, true, false);
				}
			}
			break;
		}
		case IMPULSE_20: {
			if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum ) {
				gameLocal.mpGame.ToggleTeam();
			}
			break;
		}
		case IMPULSE_22: {
			if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum ) {
				gameLocal.mpGame.ToggleSpectate();
			}
			break;
		}
        //Lubos BEGIN
        case IMPULSE_23: {
            if (!pVRClientInfo) {
                return;
            }
            gameLocal.Printf("Show weapon wheel");
            if (objectiveSystemOpen) {
                TogglePDA(1 - vr_weaponHand.GetInteger());
            }
            vr_weaponWheelCurrent.SetInteger(hands[vr_weaponHand.GetInteger()].currentWeapon);
            vr_weaponWheelDir[PITCH] = pVRClientInfo->weaponangles_temp[PITCH] - pVRClientInfo->hmdorientation_temp[PITCH];
            vr_weaponWheelDir[YAW] = pVRClientInfo->weaponangles_temp[YAW] - pVRClientInfo->hmdorientation_temp[YAW];
            vr_weaponWheelDir[ROLL] = pVRClientInfo->weaponangles_temp[ROLL] - pVRClientInfo->hmdorientation_temp[ROLL];
            vr_weaponWheel.SetBool(true);
            if (hands[vr_weaponHand.GetInteger()].currentWeapon != weapon_fists) {
                SelectWeapon(weapon_fists, false);
            }
            break;
        }
        case IMPULSE_24: {
            gameLocal.Printf("Hide weapon wheel");
            int newWeapon = vr_weaponWheelCurrent.GetInteger();
            if (IsWeaponReady(newWeapon)) {
                SelectWeapon(newWeapon, false);
            }
            vr_weaponWheel.SetBool(false);
            break;
        }
        case IMPULSE_25:
        {
            if (weapon_bloodstone > 0 && (inventory.weapons & (1 << weapon_bloodstone))) {
                if ( objectiveSystemOpen ) {
                    TogglePDA( 1 - vr_weaponHand.GetInteger() );
                }
                if (hands[vr_weaponHand.GetInteger()].currentWeapon != weapon_bloodstone) {
                    SelectWeapon(weapon_bloodstone, false);
                } else {
                    SelectWeapon(hands[vr_weaponHand.GetInteger()].previousWeapon, false);
                }
            } else if (weapon_soulcube > 0 && (inventory.weapons & (1 << weapon_soulcube))) {
                if ( objectiveSystemOpen ) {
                    TogglePDA( 1 - vr_weaponHand.GetInteger() );
                }
                if (hands[vr_weaponHand.GetInteger()].currentWeapon != weapon_soulcube) {
                    SelectWeapon(weapon_soulcube, false);
                } else {
                    SelectWeapon(hands[vr_weaponHand.GetInteger()].previousWeapon, false);
                }
            }
            break;
        }
        //Lubos END
        // Carl specific fists weapon
        case IMPULSE_26:
        {
            if ( !isIntroMap )
                SelectWeapon( weapon_fists, false, true );
            break;
        }
		case IMPULSE_28: {
			if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum ) {
				gameLocal.mpGame.CastVote( gameLocal.localClientNum, true );
			}
			break;
		}
		case IMPULSE_29: {
			if ( gameLocal.isClient || entityNumber == gameLocal.localClientNum ) {
				gameLocal.mpGame.CastVote( gameLocal.localClientNum, false );
			}
			break;
		}
        // Koz Begin
            // Koz Begin
        case IMPULSE_32: // HMD/Body orienataion reset. Make fisrtperson axis match view direction.
        {
            OrientHMDBody();
            break;
        }

        case IMPULSE_33: // toggle lasersight on/off.
        {
            ToggleLaserSight();
            break;
        }

        case IMPULSE_34: // toggle body on/off.
        {
            if(vr_playerBodyMode.GetInteger() == 0) {
                vr_playerBodyMode.SetInteger(1);
            }
            else if(vr_playerBodyMode.GetInteger() == 1) {
                vr_playerBodyMode.SetInteger(0);
            }
            break;
        }

        case IMPULSE_35: // toggle flashlight modes
        {
            if(vr_flashlightMode.GetInteger() < 3) {
                vr_flashlightMode.SetInteger(vr_flashlightMode.GetInteger() + 1);
            }
            else if(vr_flashlightMode.GetInteger() == 3) {
                vr_flashlightMode.SetInteger(0);
            }
            break;
        }

        case IMPULSE_36: //  Toggle Hud
        {
            ToggleHud();
            break;
        }

        case IMPULSE_39:// next flashlight mode
        {
            commonVr->NextFlashlightMode();
            break;
        }
        case IMPULSE_40: {
            UseVehicle();
            break;
        }
        //Hack so the chainsaw will work in MP
        case IMPULSE_27: {
            SelectWeapon(18, false);
            break;
        }
        // Koz end
        // Carl:
        case IMPULSE_PAUSE:
        {
            if ( gameLocal.inCinematic)
            {

            }
            else if ( commonVr->PDAforced && !commonVr->PDAforcetoggle )
            {
                // If we're in the menu, just exit
                PerformImpulse( 40 );
            }
            else
            {
                g_stopTime.SetBool( !g_stopTime.GetBool() );
            }
            break;
        }
        case IMPULSE_RESUME:
        {
            if ( gameLocal.inCinematic)
            {

            }
            else
            {
                g_stopTime.SetBool( false );
                if ( objectiveSystemOpen || ( commonVr->PDAforced && !commonVr->PDAforcetoggle ))
                {
                    // If we're in the menu, just exit
                    PerformImpulse( 40 );
                }
            }
            break;
        }

	}
}

/* Carl: Teleport
=====================
idPlayer::PointReachableAreaNum
=====================
*/
int idPlayer::PointReachableAreaNum(const idVec3& pos, const float boundsScale) const
{
    int areaNum;
    idVec3 size;
    idBounds bounds;

    if (!aas)
    {
        return 0;
    }

    size = aas->GetSettings()->boundingBoxes[0][1] * boundsScale;
    bounds[0] = -size;
    size.z = 32.0f;
    bounds[1] = size;

    areaNum = aas->PointReachableAreaNum(pos, bounds, AREA_REACHABLE_WALK);

    return areaNum;
}

bool idPlayer::HandleESC( void ) {
	if ( gameLocal.inCinematic ) {
		return SkipCinematic();
	}

	if ( objectiveSystemOpen ) {
        TogglePDA( 1 - vr_weaponHand.GetInteger() );
		return true;
	}

	return false;
}

bool idPlayer::SkipCinematic( void ) {
	StartSound( "snd_skipcinematic", SND_CHANNEL_ANY, 0, false, NULL );
	return gameLocal.SkipCinematic();
}

/*
==============
idPlayer::EvaluateControls
==============
*/
void idPlayer::EvaluateControls( void ) {
	// check for respawning
	if ( health <= 0 ) {
		if ( ( gameLocal.time > minRespawnTime ) && ( usercmd.buttons & BUTTON_ATTACK ) ) {
			forceRespawn = true;
		} else if ( gameLocal.time > maxRespawnTime ) {
			forceRespawn = true;
		}
	}

	// in MP, idMultiplayerGame decides spawns
	if ( forceRespawn && !gameLocal.isMultiplayer && !g_testDeath.GetBool() ) {
		// in single player, we let the session handle restarting the level or loading a game
		gameLocal.sessionCommand = "died";
	}

	if ( ( usercmd.flags & UCF_IMPULSE_SEQUENCE ) != ( oldFlags & UCF_IMPULSE_SEQUENCE ) ) {
		PerformImpulse( usercmd.impulse );
	}

	scoreBoardOpen = ( ( usercmd.buttons & BUTTON_SCORES ) != 0 || forceScoreBoard );

	oldFlags = usercmd.flags;

	AdjustSpeed();

	// update the viewangles
	UpdateViewAngles();
}

/*
==============
idPlayer::AdjustSpeed
==============
*/
void idPlayer::AdjustSpeed( void ) {
	float speed;
	float rate;

	if ( spectating ) {
		speed = pm_spectatespeed.GetFloat();
		bobFrac = 0.0f;
	} else if ( noclip ) {
		speed = pm_noclipspeed.GetFloat();
		bobFrac = 0.0f;
	} else if ( !physicsObj.OnLadder() && ( usercmd.buttons & BUTTON_RUN ) && ( usercmd.forwardmove || usercmd.rightmove ) && ( usercmd.upmove >= 0 ) ) {
		if ( !gameLocal.isMultiplayer && !physicsObj.IsCrouching() && !PowerUpActive( ADRENALINE ) ) {
			stamina -= MS2SEC( USERCMD_MSEC );
		}
		if ( stamina < 0 ) {
			stamina = 0;
		}
		if ( ( !pm_stamina.GetFloat() ) || ( stamina > pm_staminathreshold.GetFloat() ) ) {
			bobFrac = 1.0f;
		} else if ( pm_staminathreshold.GetFloat() <= 0.0001f ) {
			bobFrac = 0.0f;
		} else {
			bobFrac = stamina / pm_staminathreshold.GetFloat();
		}
        if ( game->isVR )
        {
            speed = ( pm_walkspeed.GetFloat() + vr_walkSpeedAdjust.GetFloat() ) * (1.0f - bobFrac) + pm_runspeed.GetFloat() * bobFrac;
        }
        else
        {
            speed = pm_walkspeed.GetFloat() * (1.0f - bobFrac) + pm_runspeed.GetFloat() * bobFrac;
        }
	} else {
		rate = pm_staminarate.GetFloat();

		// increase 25% faster when not moving
		if ( ( usercmd.forwardmove == 0 ) && ( usercmd.rightmove == 0 ) && ( !physicsObj.OnLadder() || ( usercmd.upmove == 0 ) ) ) {
			 rate *= 1.25f;
		}

		stamina += rate * MS2SEC( USERCMD_MSEC );
		if ( stamina > pm_stamina.GetFloat() ) {
			stamina = pm_stamina.GetFloat();
		}
        if ( game->isVR )
        {
            speed = pm_walkspeed.GetFloat() + vr_walkSpeedAdjust.GetFloat();
        }
        else
        {
            speed = pm_walkspeed.GetFloat();
        }
		bobFrac = 0.0f;
	}

	speed *= PowerUpModifier(SPEED);

	if ( influenceActive == INFLUENCE_LEVEL3 ) {
		speed *= 0.33f;
	}

	physicsObj.SetSpeed( speed, pm_crouchspeed.GetFloat() );
}

/*
==============
idPlayer::AdjustBodyAngles
==============
*/
void idPlayer::AdjustBodyAngles( void ) {
	idMat3	lookAxis;
	idMat3	legsAxis;
	bool	blend;
	float	diff;
	float	frac;
	float	upBlend;
	float	forwardBlend;
	float	downBlend;

	if ( health < 0 ) {
		return;
	}

	blend = true;

	if ( !physicsObj.HasGroundContacts() ) {
		idealLegsYaw = 0.0f;
		legsForward = true;
	} else if ( usercmd.forwardmove < 0 ) {
		idealLegsYaw = idMath::AngleNormalize180( idVec3( -usercmd.forwardmove, usercmd.rightmove, 0.0f ).ToYaw() );
		legsForward = false;
	} else if ( usercmd.forwardmove > 0 ) {
		idealLegsYaw = idMath::AngleNormalize180( idVec3( usercmd.forwardmove, -usercmd.rightmove, 0.0f ).ToYaw() );
		legsForward = true;
	} else if ( ( usercmd.rightmove != 0 ) && physicsObj.IsCrouching() ) {
		if ( !legsForward ) {
			idealLegsYaw = idMath::AngleNormalize180( idVec3( idMath::Abs( usercmd.rightmove ), usercmd.rightmove, 0.0f ).ToYaw() );
		} else {
			idealLegsYaw = idMath::AngleNormalize180( idVec3( idMath::Abs( usercmd.rightmove ), -usercmd.rightmove, 0.0f ).ToYaw() );
		}
	} else if ( usercmd.rightmove != 0 ) {
		idealLegsYaw = 0.0f;
		legsForward = true;
	} else {
		legsForward = true;
		diff = idMath::Fabs( idealLegsYaw - legsYaw );
		idealLegsYaw = idealLegsYaw - idMath::AngleNormalize180( viewAngles.yaw - oldViewYaw );
		if ( diff < 0.1f ) {
			legsYaw = idealLegsYaw;
			blend = false;
		}
	}

	if ( !physicsObj.IsCrouching() ) {
		legsForward = true;
	}

	oldViewYaw = viewAngles.yaw;

	AI_TURN_LEFT = false;
	AI_TURN_RIGHT = false;
	if ( idealLegsYaw < -45.0f ) {
		idealLegsYaw = 0;
		AI_TURN_RIGHT = true;
		blend = true;
	} else if ( idealLegsYaw > 45.0f ) {
		idealLegsYaw = 0;
		AI_TURN_LEFT = true;
		blend = true;
	}

	if ( blend ) {
		legsYaw = legsYaw * 0.9f + idealLegsYaw * 0.1f;
	}
	legsAxis = idAngles( 0.0f, legsYaw, 0.0f ).ToMat3();
	animator.SetJointAxis( hipJoint, JOINTMOD_WORLD, legsAxis );

	// calculate the blending between down, straight, and up
	frac = viewAngles.pitch / 90.0f;
    //mmdanggg2: stop the model from bending down and getting in the way!!
    if ( frac > 0.0f ) {
        downBlend = 0.0f;	// frac;
        forwardBlend = 1.0f;// -frac;
        upBlend = 0.0f;
    }
    else {
        downBlend = 0.0f;
        forwardBlend = 1.0f;// +frac;
        upBlend = 0.0f;		// -frac;
    }

	animator.CurrentAnim( ANIMCHANNEL_TORSO )->SetSyncedAnimWeight( 0, downBlend );
	animator.CurrentAnim( ANIMCHANNEL_TORSO )->SetSyncedAnimWeight( 1, forwardBlend );
	animator.CurrentAnim( ANIMCHANNEL_TORSO )->SetSyncedAnimWeight( 2, upBlend );

	animator.CurrentAnim( ANIMCHANNEL_LEGS )->SetSyncedAnimWeight( 0, downBlend );
	animator.CurrentAnim( ANIMCHANNEL_LEGS )->SetSyncedAnimWeight( 1, forwardBlend );
	animator.CurrentAnim( ANIMCHANNEL_LEGS )->SetSyncedAnimWeight( 2, upBlend );
}

/*
==============
idPlayer::InitAASLocation
==============
*/
void idPlayer::InitAASLocation( void ) {
	int		i;
	int		num;
	idVec3	size;
	idBounds bounds;
	idAAS	*aas;
	idVec3	origin;

	GetFloorPos( 64.0f, origin );

	num = gameLocal.NumAAS();
	aasLocation.SetGranularity( 1 );
	aasLocation.SetNum( num );
	for( i = 0; i < aasLocation.Num(); i++ ) {
		aasLocation[ i ].areaNum = 0;
		aasLocation[ i ].pos = origin;
		aas = gameLocal.GetAAS( i );
		if ( aas && aas->GetSettings() ) {
			size = aas->GetSettings()->boundingBoxes[0][1];
			bounds[0] = -size;
			size.z = 32.0f;
			bounds[1] = size;

			aasLocation[ i ].areaNum = aas->PointReachableAreaNum( origin, bounds, AREA_REACHABLE_WALK );
		}
	}
}

/*
==============
idPlayer::InitPlayerBones
Koz - moved bone inits here, called during player restore as well.
==============
*/
void idPlayer::InitPlayerBones()
{
	const char*			value;

	value = spawnArgs.GetString( "bone_hips", "" );
	hipJoint = animator.GetJointHandle( value );
	if ( hipJoint == INVALID_JOINT )
	{
		gameLocal.Error( "Joint '%s' not found for 'bone_hips' on '%s'", value, name.c_str() );
	}

	value = spawnArgs.GetString( "bone_chest", "" );
	chestJoint = animator.GetJointHandle( value );
	if ( chestJoint == INVALID_JOINT )
	{
		gameLocal.Error( "Joint '%s' not found for 'bone_chest' on '%s'", value, name.c_str() );
	}

	value = spawnArgs.GetString( "bone_head", "" );
	headJoint = animator.GetJointHandle( value );
	if ( headJoint == INVALID_JOINT )
	{
		gameLocal.Error( "Joint '%s' not found for 'bone_head' on '%s'", value, name.c_str() );
	}

	// Koz begin
	value = spawnArgs.GetString( "bone_neck", "" );
	neckJoint = animator.GetJointHandle( value );
	if ( neckJoint == INVALID_JOINT )
	{
		gameLocal.Error( "Joint '%s' not found for 'bone_neck' on '%s'", value, name.c_str() );
	}

	value = spawnArgs.GetString( "bone_chest_pivot", "" );
	chestPivotJoint = animator.GetJointHandle( value );
	if ( chestPivotJoint == INVALID_JOINT )
	{
		gameLocal.Error( "Joint '%s' not found for 'bone_chest_pivot' on '%s'", value, name.c_str() );
	}

	// we need to load the starting joint orientations for the hands so we can compute correct offsets later
	value = spawnArgs.GetString( "ik_hand1", "" ); // right hand
	ik_hand[0] = animator.GetJointHandle( value );
	if ( ik_hand[0] == INVALID_JOINT )
	{
		gameLocal.Error( "Joint '%s' not found for 'ik_hand1' on '%s'", value, name.c_str() );
	}

	value = spawnArgs.GetString( "ik_hand2", "" );// left hand
	ik_hand[1] = animator.GetJointHandle( value );
	if ( ik_hand[1] == INVALID_JOINT )
	{
		gameLocal.Error( "Joint '%s' not found for 'ik_hand2' on '%s'", value, name.c_str() );
	}

	ik_handAttacher[0] = animator.GetJointHandle( "RhandWeap" );
	if ( ik_handAttacher[0] == INVALID_JOINT )
	{
		gameLocal.Warning( "Joint RhandWeap not found for player anim default\n" );
		ik_handAttacher[0] = ik_hand[0];
	}

	ik_handAttacher[1] = animator.GetJointHandle( "LhandWeap" );

	if ( ik_handAttacher[1] == INVALID_JOINT )
	{
		gameLocal.Warning( "Joint LhandWeap not found for player anim default\n" );
		ik_handAttacher[1] = ik_hand[1];
	}

	idStr animPre = "default";// this is the anim that has the default/normal hand and weapon attacher orientations (relationsh

	int animNo = animator.GetAnim( animPre.c_str() );
	if ( animNo == 0 )
	{
		gameLocal.Error( "Player default animation not found\n" );
	}

	int numJoints = animator.NumJoints();

	idJointMat* joints = (idJointMat*)_alloca16( numJoints * sizeof( joints[0] ) );

	// create the idle default pose ( in this case set to default which should tranlsate to pistol_idle )
	gameEdit->ANIM_CreateAnimFrame( animator.ModelHandle(), animator.GetAnim( animNo )->MD5Anim( 0 ), numJoints, joints, 1, animator.ModelDef()->GetVisualOffset() + modelOffset, animator.RemoveOrigin() );



	static idVec3 defaultWeapAttachOff[2]; // the default distance between the weapon attacher and the hand joint;
	defaultWeapAttachOff[0] = joints[ik_handAttacher[0]].ToVec3() - joints[ik_hand[0]].ToVec3(); // default
	defaultWeapAttachOff[1] = joints[ik_handAttacher[1]].ToVec3() - joints[ik_hand[1]].ToVec3();

	jointHandle_t j1;
	value = spawnArgs.GetString( "ik_elbow1", "" );// right
	j1 = animator.GetJointHandle( value );
	if ( j1 == INVALID_JOINT )
	{
		gameLocal.Error( "Joint ik_elbow1 not found for player anim default\n" );
	}
	ik_elbowCorrectAxis[0] = joints[j1].ToMat3();

	value = spawnArgs.GetString( "ik_elbow2", "" );// left
	j1 = animator.GetJointHandle( value );
	if ( j1 == INVALID_JOINT )
	{
		gameLocal.Error( "Joint ik_elbow2 not found for player anim default\n" );
	}
	ik_elbowCorrectAxis[1] = joints[j1].ToMat3();

	chestPivotCorrectAxis = joints[chestPivotJoint].ToMat3();
	chestPivotDefaultPos = joints[chestPivotJoint].ToVec3();
	commonVr->chestDefaultDefined = true;



	common->Printf( "Animpre hand 0 default offset = %s\n", defaultWeapAttachOff[0].ToString() );
	common->Printf( "Animpre hand 1 default offset = %s\n", defaultWeapAttachOff[1].ToString() );

	// now calc the weapon attacher offsets
	for ( int hand = 0; hand < 2; hand++ )
	{
		for ( int weap = 0; weap < 32; weap++ ) // should be max weapons
		{

			idStr animPre = spawnArgs.GetString( va( "def_weapon%d", weap ) );
			animPre.Strip( "weapon_" );
			animPre += "_idle";

			int animNo = animator.GetAnim( animPre.c_str() );
			int numJoints = animator.NumJoints();

			if ( animNo == 0 ) continue;

			//	common->Printf( "Animpre = %s animNo = %d\n", animPre.c_str(), animNo );

			// create the idle pose for this weapon
			gameEdit->ANIM_CreateAnimFrame( animator.ModelHandle(), animator.GetAnim( animNo )->MD5Anim( 0 ), numJoints, joints, 1, animator.ModelDef()->GetVisualOffset() + modelOffset, animator.RemoveOrigin() );

			ik_handCorrectAxis[hand][weap] = joints[ik_hand[hand]].ToMat3();
			//	common->Printf( "Hand %d weap %d anim %s attacher pos %s   default pos %s\n", hand, weap, animPre.c_str(), joints[ik_handAttacher[hand]].ToVec3().ToString(), defaultWeapAttachOff[hand].ToString() );

			//this is the translation between the hand joint ( the wrist ) and the attacher joint.  The attacher joint is
			//the location in space where the motion control is locating the weapon / hand, but IK is using the 'wrist' to
			//drive animation, so use this offset to derive the wrist position from the attacher joint orientation
			handWeaponAttachertoWristJointOffset[hand][weap] = joints[ik_handAttacher[hand]].ToVec3() - joints[ik_hand[hand]].ToVec3();

			// the is the delta if the attacher joint was moved from the position in the default animation to aid with alignment in
			// different weapon animations.  To keep the hand in a consistant location when weapon is changed,
			// the weapon and hand positions will need to be adjusted by this amount when presented
			handWeaponAttacherToDefaultOffset[hand][weap] = handWeaponAttachertoWristJointOffset[hand][weap] - defaultWeapAttachOff[hand];

			//	common->Printf( "Hand %d weap %d anim %s attacher offset = %s\n", hand, weap, animPre.c_str(), handWeaponAttacherToDefaultOffset[hand][weap].ToString() );
		}
	}

}

/*
===============
idPlayer::IsSoundChannelPlaying
===============
*/
bool idPlayer::IsSoundChannelPlaying( const s_channelType channel )
{
	if( GetSoundEmitter() != NULL )
	{
		//return GetSoundEmitter()->CurrentlyPlaying( channel );
        return GetSoundEmitter()->CurrentlyPlaying();
	}

	return false;
}

/*
==============
idPlayer::InitTeleportTarget
==============
*/
void idPlayer::InitTeleportTarget()
{
	idVec3 origin;
	int targetAnim;
	idStr jointName;

	skinTelepadCrouch = declManager->FindSkin( "skins/vr/padcrouch" );

	commonVr->teleportButtonCount = 0;

	common->Printf( "Initializing teleport target\n" );
	origin = GetPhysics()->GetOrigin() + (origin + modelOffset) * GetPhysics()->GetAxis();

    teleportTarget = (idAnimatedEntity*)gameLocal.FindEntity( "vrTeleportTarget" );
	if ( !teleportTarget )
	{
		teleportTarget = (idAnimatedEntity*)gameLocal.SpawnEntityType( idAnimatedEntity::Type, NULL );
		teleportTarget->SetName( "vrTeleportTarget" );
	}
	teleportTarget->SetModel( "telepad1" );
	teleportTarget->SetOrigin( origin );
	teleportTarget->SetAxis( GetPhysics()->GetAxis() );

	idAnimatedEntity *duplicate;
	if (duplicate = (idAnimatedEntity*)gameLocal.FindEntity("vrTeleportTarget2"))
	{
		common->Warning("Loading game which had a duplicate vrTeleportTarget.");
		duplicate->PostEventMS(&EV_Remove, 0);
	}

	teleportTargetAnimator = teleportTarget->GetAnimator();
	targetAnim = teleportTargetAnimator->GetAnim( "idle" );
	//common->Printf( "Teleport target idle anim # = %d\n", targetAnim );
	teleportTargetAnimator->PlayAnim( ANIMCHANNEL_ALL, targetAnim, gameLocal.time, 0 );

	teleportPadJoint = teleportTargetAnimator->GetJointHandle( "pad" );

	if ( teleportPadJoint == INVALID_JOINT )
	{
		common->Printf( "Unable to find joint teleportPadJoint \n" );
	}

	teleportCenterPadJoint = teleportTargetAnimator->GetJointHandle( "centerpad" );

	if ( teleportCenterPadJoint == INVALID_JOINT )
	{
		common->Printf( "Unable to find joint teleportCenterPadJoint \n" );
	}

	for ( int i = 0; i < 24; i++ )
	{
		jointName = va( "padbeam%d", i + 1 );
		teleportBeamJoint[i] = teleportTargetAnimator->GetJointHandle( jointName.c_str() );
		if ( teleportBeamJoint[i] == INVALID_JOINT )
		{
			common->Printf( "Unable to find teleportBeamJoint %s\n", jointName.c_str() );
		}
	}

}

/* Carl
=====================
idPlayer::SetAAS
=====================
*/
void idPlayer::SetAAS( bool forceAAS48 )
{
    idStr use_aas;

    spawnArgs.GetString("use_aas", NULL, use_aas);
    gameLocal.Printf("Player AAS: use_aas = %s\n", use_aas.c_str());
    aas = gameLocal.GetAAS(use_aas);
    // Carl: use our own custom generated AAS specifically for player movement (teleporting).
    if (!aas && !forceAAS48 )
        aas = gameLocal.GetAAS("aas_player");
    // Every map has aas48, used for zombies and imps. It's close enough to player.
    if (!aas || forceAAS48 )
        aas = gameLocal.GetAAS("aas48");
    if (aas)
    {
        const idAASSettings* settings = aas->GetSettings();
        if (settings)
        {
            /*
            if (!ValidForBounds(settings, physicsObj.GetBounds()))
            {
                gameLocal.Error("%s cannot use use_aas %s\n", name.c_str(), use_aas.c_str());
            }
            */
            float height = settings->maxStepHeight;
            gameLocal.Printf("Player AAS = %s: AAS step height = %f, player step height = %f\n", settings->fileExtension.c_str(), height, pm_stepsize.GetFloat());
            return;
        }
        else
        {
            aas = NULL;
        }
    }
    gameLocal.Printf("WARNING: Player %s has no AAS file\n", name.c_str());
}

/*
==============
idPlayer::SetAASLocation
==============
*/
void idPlayer::SetAASLocation( void ) {
	int		i;
	int		areaNum;
	idVec3	size;
	idBounds bounds;
	idAAS	*aas;
	idVec3	origin;

	if ( !GetFloorPos( 64.0f, origin ) ) {
		return;
	}

	for( i = 0; i < aasLocation.Num(); i++ ) {
		aas = gameLocal.GetAAS( i );
		if ( !aas ) {
			continue;
		}

		size = aas->GetSettings()->boundingBoxes[0][1];
		bounds[0] = -size;
		size.z = 32.0f;
		bounds[1] = size;

		areaNum = aas->PointReachableAreaNum( origin, bounds, AREA_REACHABLE_WALK );
		if ( areaNum ) {
			aasLocation[ i ].pos = origin;
			aasLocation[ i ].areaNum = areaNum;
		}
	}
}

/*
==============
idPlayer::GetAASLocation
==============
*/
void idPlayer::GetAASLocation( idAAS *aas, idVec3 &pos, int &areaNum ) const {
	int i;

	if ( aas != NULL ) {
		for( i = 0; i < aasLocation.Num(); i++ ) {
			if ( aas == gameLocal.GetAAS( i ) ) {
				areaNum = aasLocation[ i ].areaNum;
				pos = aasLocation[ i ].pos;
				return;
			}
		}
	}

	areaNum = 0;
	pos = physicsObj.GetOrigin();
}

/*
==============
idPlayer::Move
==============
*/
void idPlayer::Move( void ) {
	float newEyeOffset;
	idVec3 oldOrigin;
	idVec3 oldVelocity;
	idVec3 pushVelocity;

    static bool testLean = false;
    static idVec3 leanOrigin = vec3_zero;

	// save old origin and velocity for crashlanding
	oldOrigin = physicsObj.GetOrigin();
	oldVelocity = physicsObj.GetLinearVelocity();
	pushVelocity = physicsObj.GetPushedLinearVelocity();

	// set physics variables
	physicsObj.SetMaxStepHeight( pm_stepsize.GetFloat() );
	physicsObj.SetMaxJumpHeight( pm_jumpheight.GetFloat() );

	if ( noclip ) {
		physicsObj.SetContents( 0 );
		physicsObj.SetMovementType( PM_NOCLIP );
	} else if ( spectating ) {
		physicsObj.SetContents( 0 );
		physicsObj.SetMovementType( PM_SPECTATOR );
	} else if ( health <= 0 ) {
		physicsObj.SetContents( CONTENTS_CORPSE | CONTENTS_MONSTERCLIP );
		physicsObj.SetMovementType( PM_DEAD );
	} else if ( gameLocal.inCinematic || gameLocal.GetCamera() || privateCameraView || ( influenceActive == INFLUENCE_LEVEL2 ) ) {
		physicsObj.SetContents( CONTENTS_BODY );
		physicsObj.SetMovementType( PM_FREEZE );
	} else if (mountedObject) {
		physicsObj.SetContents(0);
		physicsObj.SetMovementType(PM_FREEZE);
	} else {
		physicsObj.SetContents( CONTENTS_BODY );
		physicsObj.SetMovementType( PM_NORMAL );
	}

	if ( spectating ) {
		physicsObj.SetClipMask( MASK_DEADSOLID );
	} else if ( health <= 0 ) {
		physicsObj.SetClipMask( MASK_DEADSOLID );
	} else {
		physicsObj.SetClipMask( MASK_PLAYERSOLID );
	}

	physicsObj.SetDebugLevel( g_debugMove.GetBool() );

    {
        idVec3	org;
        idMat3	axis;
        if ( !game->isVR || !pVRClientInfo )
        {
            GetViewPos( org, axis ); // Koz default movement
            physicsObj.SetPlayerInput( usercmd, axis[0] );
        }
        else
        {
            if ( !physicsObj.OnLadder() ) // Koz fixme, dont move if on a ladder or player will fall/stick
            {

                idVec3 bodyOrigin = vec3_zero;
                idVec3 movedBodyOrigin = vec3_zero;
                idVec3 movedRemainder = vec3_zero;
                idMat3 bodyAxis;
                idMat3 origPhysAxis;

                GetViewPos( bodyOrigin, bodyAxis );
                bodyOrigin = physicsObj.GetOrigin();
                origPhysAxis = physicsObj.GetAxis();

                idVec3 newBodyOrigin;

                idAngles bodyAng = bodyAxis.ToAngles();
                idMat3 bodyAx = idAngles( bodyAng.pitch, bodyAng.yaw - commonVr->bodyYawOffset, bodyAng.roll ).Normalize180().ToMat3();

                newBodyOrigin = bodyOrigin + bodyAx[0] * commonVr->remainingMoveHmdBodyPositionDelta.x + bodyAx[1] * commonVr->remainingMoveHmdBodyPositionDelta.y;
                commonVr->remainingMoveHmdBodyPositionDelta.x = commonVr->remainingMoveHmdBodyPositionDelta.y = 0;

                commonVr->motionMoveDelta = newBodyOrigin - bodyOrigin;
                commonVr->motionMoveVelocity = commonVr->motionMoveDelta / ((1000 / commonVr->hmdHz) * 0.001f);

                if ( !commonVr->isLeaning )
                {
                    movedBodyOrigin = physicsObj.MotionMove( commonVr->motionMoveVelocity );
                    physicsObj.SetAxis( origPhysAxis ); // make sure motion move doesnt change the axis

                    movedRemainder = (newBodyOrigin - movedBodyOrigin);

                    if ( movedRemainder.Length() > commonVr->motionMoveDelta.Length() * 0.25f )
                    {
                        commonVr->isLeaning = true;
                        testLean = false;
                        leanOrigin = movedBodyOrigin;
                        commonVr->leanOffset = movedRemainder;
                    }
                    else
                    {
                        // if the pda is fixed in space, we need to keep track of how much we have moved the player body
                        // so we can keep the PDA in the same position relative to the player while accounting for external movement ( on a lift / eleveator etc )
                        if ( !hands[0].PDAfixed && !hands[1].PDAfixed )
                        {
                            commonVr->fixedPDAMoveDelta = vec3_zero;
                        }
                        else
                        {
                            commonVr->fixedPDAMoveDelta += (movedBodyOrigin - bodyOrigin);
                        }
                    }
                }
                else

                {
                    // player body blocked by object. let the head move some by accruing all ( body ) movement  here.
                    // check to see if player body can move to the new location without clipping anything
                    // if it can, move it and clear leanoffsets, otherwise limit the distance
                    idVec3 testOrigin = vec3_zero;

                    commonVr->leanOffset += commonVr->motionMoveDelta;

                    if ( commonVr->leanOffset.LengthSqr() > 36.0f * 36.0f ) // dont move head more than 36 inches // Koz fixme should me measure distance from waist?
                    {
                        commonVr->leanOffset.Normalize();
                        commonVr->leanOffset *= 36.0f;
                    }

                    if ( commonVr->leanBlank )
                    {
                        if ( commonVr->leanOffset.LengthSqr() > commonVr->leanBlankOffsetLengthSqr )
                        {
                            commonVr->leanOffset = commonVr->leanBlankOffset;
                        }
                    }

                    testOrigin = bodyOrigin + commonVr->leanOffset;

                    if ( commonVr->leanOffset.LengthSqr() > 4.0f || bodyOrigin != leanOrigin ) testLean = true; // dont check to cancel lean until player body has moved, or head has moved at least two inches.

                    if ( testLean )
                    {
                        // clip against the player clipmodel
                        trace_t trace;
                        idMat3 clipAxis;

                        idClipModel* clip;
                        clip = physicsObj.GetClipModel();
                        clipAxis = physicsObj.GetClipModel()->GetAxis();


                        gameLocal.clip.Translation( trace, testOrigin, testOrigin, clip, clipAxis, MASK_SHOT_RENDERMODEL /* CONTENTS_SOLID */, this );
                        if ( trace.fraction < 1.0f )
                        {

                            // do ik stuff here
                            // trying to do this now in player walkIk

                        }
                        else
                        {
                            // not leaning, clear the offsets and move the player origin
                            physicsObj.SetOrigin( testOrigin );
                            commonVr->isLeaning = false;
                            //common->Printf("Setting Leaning FALSE %d\n", Sys_Milliseconds());
                            commonVr->leanOffset = vec3_zero;
                            //animator.ClearJoint( chestPivotJoint );
                        }
                    }
                }
            }
            GetViewPos( org, axis ); // Koz default movement
            physicsObj.SetPlayerInput( usercmd, axis[0] );
        }

    }


	// FIXME: physics gets disabled somehow
	BecomeActive( TH_PHYSICS );
    // Carl: check if we're experiencing artificial locomotion
    idVec3 before = physicsObj.GetOrigin();
    RunPhysics();
    idVec3 after = physicsObj.GetOrigin();

	// update our last valid AAS location for the AI
	SetAASLocation();

	if ( spectating ) {
		newEyeOffset = 0.0f;
	} else if ( health <= 0 ) {
		newEyeOffset = pm_deadviewheight.GetFloat();
	} else if ( physicsObj.IsCrouching() ) {
        // Koz begin
        if ( game->isVR && pVRClientInfo )
        {
            if ( vr_crouchMode.GetInteger() != 0 || (usercmd.buttons & BUTTON_CROUCH) )
            {
                newEyeOffset = 34;  //Carl: When showing our body, our body doesn't crouch enough, so move eyes as high as possible (any higher and the top of our head wouldn't fit)
                if ( vr_crouchMode.GetInteger() != 0 && commonVr->poseHmdHeadPositionDelta.z < -vr_crouchTriggerDist.GetFloat() )
                {
                    // crouch was initiated by the trigger, adjust eyeOffset by trigger val so view isnt too low.
                    newEyeOffset += vr_crouchTriggerDist.GetFloat();
                }
            }
            else
                newEyeOffset = pm_normalviewheight.GetFloat();
        }
        else
        {
            newEyeOffset = pm_crouchviewheight.GetFloat();
        }
        // Koz end
	} else if ( GetBindMaster() && GetBindMaster()->IsType( idAFEntity_Vehicle::Type ) ) {
		newEyeOffset = 0.0f;
	} else {
		newEyeOffset = pm_normalviewheight.GetFloat();
	}
    float oldEyeOffset = EyeHeight();
	if ( EyeHeight() != newEyeOffset ) {
		if ( spectating ) {
			SetEyeHeight( newEyeOffset );
		} else {
			// smooth out duck height changes
			SetEyeHeight( EyeHeight() * pm_crouchrate.GetFloat() + newEyeOffset * ( 1.0f - pm_crouchrate.GetFloat() ) );
		}
	}

	if ( noclip || gameLocal.inCinematic || ( influenceActive == INFLUENCE_LEVEL2 ) ) {
		AI_CROUCH	= false;
		AI_ONGROUND	= ( influenceActive == INFLUENCE_LEVEL2 );
		AI_ONLADDER	= false;
		AI_JUMP		= false;
	} else {
		AI_CROUCH	= physicsObj.IsCrouching();
		AI_ONGROUND	= physicsObj.HasGroundContacts();
		AI_ONLADDER	= physicsObj.OnLadder();
		AI_JUMP		= physicsObj.HasJumped();

		// check if we're standing on top of a monster and give a push if we are
		idEntity *groundEnt = physicsObj.GetGroundEntity();
		if ( groundEnt && groundEnt->IsType( idAI::Type ) ) {
			idVec3 vel = physicsObj.GetLinearVelocity();
			if ( vel.ToVec2().LengthSqr() < 0.1f ) {
				vel.ToVec2() = physicsObj.GetOrigin().ToVec2() - groundEnt->GetPhysics()->GetAbsBounds().GetCenter().ToVec2();
				vel.ToVec2().NormalizeFast();
				vel.ToVec2() *= pm_walkspeed.GetFloat();
			} else {
				// give em a push in the direction they're going
				vel *= 1.1f;
			}
			physicsObj.SetLinearVelocity( vel );
		}
	}

	if ( AI_JUMP ) {
		// bounce the view weapon
		loggedAccel_t	*acc = &loggedAccel[currentLoggedAccel&(NUM_LOGGED_ACCELS-1)];
		currentLoggedAccel++;
		acc->time = gameLocal.time;
		acc->dir[2] = 200;
		acc->dir[0] = acc->dir[1] = 0;
	}

	if ( AI_ONLADDER ) {
		int old_rung = oldOrigin.z / LADDER_RUNG_DISTANCE;
		int new_rung = physicsObj.GetOrigin().z / LADDER_RUNG_DISTANCE;

		if ( old_rung != new_rung ) {
			StartSound( "snd_stepladder", SND_CHANNEL_ANY, 0, false, NULL );
		}
	}

	BobCycle( pushVelocity );

    // Carl: Motion sickness detection
    float distance = ((after - before) - commonVr->motionMoveDelta).LengthSqr();
    static float oldHeadHeightDiff = 0;
    float crouchDistance = EyeHeight() + commonVr->headHeightDiff - oldEyeOffset - oldHeadHeightDiff;
    oldHeadHeightDiff = commonVr->headHeightDiff;
    distance += crouchDistance * crouchDistance + viewBob.LengthSqr();
    blink = (distance > 0.005f);


	CrashLand( oldOrigin, oldVelocity );

    // Handling vr_comfortMode
    if (vr_motionSickness.IsModified() && !warpMove && !warpAim)
    {
        timescale.SetFloat(1);
        vr_motionSickness.ClearModified();
    }
    const int comfortMode = vr_motionSickness.GetInteger();
    //"	0 off | 2 = tunnel | 5 = tunnel + chaperone | 6 slow mo | 7 slow mo + chaperone | 8 tunnel + slow mo | 9 = tunnel + slow mo + chaperone
    if ( comfortMode < 2 || game->InCinematic() )
    {
        this->playerView.EnableVrComfortVision( false );
        commonVr->thirdPersonMovement = false;
        return;
    }

    float speed = physicsObj.GetLinearVelocity().LengthFast();
    if ( comfortMode == 10 && speed == 0 && usercmd.forwardmove == 0 && usercmd.rightmove == 0 )
        commonVr->thirdPersonMovement = false;

    if (( comfortMode == 2 ) || ( comfortMode == 5 ) || ( comfortMode == 8 ) || ( comfortMode == 9 ))
    {
        if (speed == 0 && !blink)
        {
            this->playerView.EnableVrComfortVision(false);
        }
        else
        {
            this->playerView.EnableVrComfortVision(true);
        }
    }
    else
        this->playerView.EnableVrComfortVision(false);

    if ((( comfortMode == 6 ) || ( comfortMode == 7 ) || ( comfortMode == 8 ) || ( comfortMode == 9 )) && !warpAim && !warpMove )
    {
        float speedFactor = (( pm_runspeed.GetFloat() - speed ) / pm_runspeed.GetFloat());
        if ( speedFactor < 0 )
        {
            speedFactor = 0;
        }
        timescale.SetFloat( 0.5f + 0.5f * speedFactor );
    }
}

/*
==============
idPlayer::Move_Interpolated
==============
*/
void idPlayer::Move_Interpolated( float fraction )
{

    float newEyeOffset = 0.0f;
    idVec3 oldOrigin;
    idVec3 oldVelocity;
    idVec3 pushVelocity;

    // save old origin and velocity for crashlanding
    oldOrigin = physicsObj.GetOrigin();
    oldVelocity = physicsObj.GetLinearVelocity();
    pushVelocity = physicsObj.GetPushedLinearVelocity();

    // set physics variables
    physicsObj.SetMaxStepHeight( pm_stepsize.GetFloat() );
    physicsObj.SetMaxJumpHeight( pm_jumpheight.GetFloat() );

    if( noclip )
    {
        physicsObj.SetContents( 0 );
        physicsObj.SetMovementType( PM_NOCLIP );
    }
    else if( spectating )
    {
        physicsObj.SetContents( 0 );
        physicsObj.SetMovementType( PM_SPECTATOR );
    }
    else if( health <= 0 )
    {
        physicsObj.SetContents( CONTENTS_CORPSE | CONTENTS_MONSTERCLIP );
        physicsObj.SetMovementType( PM_DEAD );
    }
    else if( gameLocal.inCinematic || gameLocal.GetCamera() || privateCameraView || ( influenceActive == INFLUENCE_LEVEL2 ) )
    {
        physicsObj.SetContents( CONTENTS_BODY );
        physicsObj.SetMovementType( PM_FREEZE );
    }
    /*else if( mountedObject )
    {
        physicsObj.SetContents( 0 );
        physicsObj.SetMovementType( PM_FREEZE );
    }*/
    else
    {
        physicsObj.SetContents( CONTENTS_BODY );
        physicsObj.SetMovementType( PM_NORMAL );
    }

    if( spectating )
    {
        physicsObj.SetClipMask( MASK_DEADSOLID );
    }
    else if( health <= 0 )
    {
        physicsObj.SetClipMask( MASK_DEADSOLID );
    }
    else
    {
        physicsObj.SetClipMask( MASK_PLAYERSOLID );
    }

    physicsObj.SetDebugLevel( g_debugMove.GetBool() );

    {
        idVec3	org;
        idMat3	axis;
        GetViewPos( org, axis );

        physicsObj.SetPlayerInput( usercmd, axis[0] );
    }

    // FIXME: physics gets disabled somehow
    BecomeActive( TH_PHYSICS );
    //GB Dont think this function is ever used (MoveInterpolated)
    //InterpolatePhysics( fraction );

    // update our last valid AAS location for the AI
    SetAASLocation();

    if( spectating )
    {
        newEyeOffset = 0.0f;
    }
    else if( health <= 0 )
    {
        newEyeOffset = pm_deadviewheight.GetFloat();
    }
    else if ( physicsObj.IsCrouching() )
    {
        // Koz begin
        // dont change the eyeoffset if using full motion crouch.
        if ( game->isVR && pVRClientInfo )
        {
            if ( vr_crouchMode.GetInteger() != 0 || (usercmd.buttons & BUTTON_CROUCH) )
            {
                newEyeOffset = 34; //Carl: When showing our body, our body doesn't crouch enough, so move eyes as high as possible (any higher and the top of our head wouldn't fit)
            }
        }
        else
        {
            newEyeOffset = pm_crouchviewheight.GetFloat();
        }
        // Koz end
    }
    else if( GetBindMaster() && GetBindMaster()->IsType( idAFEntity_Vehicle::Type ) )
    {
        newEyeOffset = 0.0f;
    }
        // Koz begin
    else if ( game->isVR && pVRClientInfo )
    {
        newEyeOffset = pm_normalviewheight.GetFloat();
        //Carl: Our body is too tall, so move our eyes higher so they don't clip the body
    }
        // Koz end
    else
    {
        newEyeOffset = pm_normalviewheight.GetFloat();
    }

    if( EyeHeight() != newEyeOffset ) // Koz fixme - do we want a slow or instant crouch in VR?
    {
        if( spectating )
        {
            SetEyeHeight( newEyeOffset );
        }
        else
        {
            // smooth out duck height changes
            SetEyeHeight( EyeHeight() * pm_crouchrate.GetFloat() + newEyeOffset * ( 1.0f - pm_crouchrate.GetFloat() ) );
        }
    }

    if( AI_JUMP )
    {
        // bounce the view weapon
        loggedAccel_t*	acc = &loggedAccel[currentLoggedAccel & ( NUM_LOGGED_ACCELS - 1 )];
        currentLoggedAccel++;
        acc->time = gameLocal.time;
        acc->dir[2] = 200;
        acc->dir[0] = acc->dir[1] = 0;
    }

    if( AI_ONLADDER )
    {
        int old_rung = oldOrigin.z / LADDER_RUNG_DISTANCE;
        int new_rung = physicsObj.GetOrigin().z / LADDER_RUNG_DISTANCE;

        if( old_rung != new_rung )
        {
            StartSound( "snd_stepladder", SND_CHANNEL_ANY, 0, false, NULL );
        }
    }

    BobCycle( pushVelocity );
    CrashLand( oldOrigin, oldVelocity );

}

/*
==============
idPlayer::UpdateHud
==============
*/
void idPlayer::UpdateHud( void ) {
	idPlayer *aimed;

	if ( !hud ) {
		return;
	}

	if ( entityNumber != gameLocal.localClientNum ) {
		return;
	}

    if ( game->isVR )
    {
        UpdateVrHud();
    }

	int c = inventory.pickupItemNames.Num();
	if ( c > 0 ) {
		if ( gameLocal.time > inventory.nextItemPickup ) {
			if ( inventory.nextItemPickup && gameLocal.time - inventory.nextItemPickup > 2000 ) {
				inventory.nextItemNum = 1;
			}
			int i;
			int count = 5;
			if (gameLocal.isMultiplayer) {
				count = 3;
			}
			for (i = 0; i < count, i < c; i++) {
				hud->SetStateString( va( "itemtext%i", inventory.nextItemNum ), inventory.pickupItemNames[0].name );
				hud->SetStateString( va( "itemicon%i", inventory.nextItemNum ), inventory.pickupItemNames[0].icon );
				hud->HandleNamedEvent( va( "itemPickup%i", inventory.nextItemNum++ ) );
				inventory.pickupItemNames.RemoveIndex( 0 );
				if (inventory.nextItemNum == 1 ) {
					inventory.onePickupTime = gameLocal.time;
				} else 	if (inventory.nextItemNum > count) {   //_D3XP
					inventory.nextItemNum = 1;
					inventory.nextItemPickup = inventory.onePickupTime + 2000;
				} else {
					inventory.nextItemPickup = gameLocal.time + 400;
				}
			}
		}
	}

	if ( gameLocal.realClientTime == lastMPAimTime ) {
		if ( MPAim != -1 && gameLocal.gameType == GAME_TDM
			&& gameLocal.entities[ MPAim ] && gameLocal.entities[ MPAim ]->IsType( idPlayer::Type )
			&& static_cast< idPlayer * >( gameLocal.entities[ MPAim ] )->team == team ) {
				aimed = static_cast< idPlayer * >( gameLocal.entities[ MPAim ] );
				hud->SetStateString( "aim_text", gameLocal.userInfo[ MPAim ].GetString( "ui_name" ) );
				hud->SetStateFloat( "aim_color", aimed->colorBarIndex );
				hud->HandleNamedEvent( "aim_flash" );
				MPAimHighlight = true;
				MPAimFadeTime = 0;	// no fade till loosing focus
		} else if ( MPAimHighlight ) {
			hud->HandleNamedEvent( "aim_fade" );
			MPAimFadeTime = gameLocal.realClientTime;
			MPAimHighlight = false;
		}
	}
	if ( MPAimFadeTime ) {
		assert( !MPAimHighlight );
		if ( gameLocal.realClientTime - MPAimFadeTime > 2000 ) {
			MPAimFadeTime = 0;
		}
	}

	hud->SetStateInt( "g_showProjectilePct", g_showProjectilePct.GetInteger() );
	if ( numProjectilesFired ) {
		hud->SetStateString( "projectilepct", va( "Hit %% %.1f", ( (float) numProjectileHits / numProjectilesFired ) * 100 ) );
	} else {
		hud->SetStateString( "projectilepct", "Hit % 0.0" );
	}

	if ( isLagged && gameLocal.isMultiplayer && gameLocal.localClientNum == entityNumber ) {
		hud->SetStateString( "hudLag", "1" );
	} else {
		hud->SetStateString( "hudLag", "0" );
	}
}

/*
==============
idPlayer::UpdateDeathSkin
==============
*/
void idPlayer::UpdateDeathSkin( bool state_hitch ) {
	if ( !( gameLocal.isMultiplayer || g_testDeath.GetBool() ) ) {
		return;
	}
	if ( health <= 0 ) {
		if ( !doingDeathSkin ) {
			deathClearContentsTime = spawnArgs.GetInt( "deathSkinTime" );
			doingDeathSkin = true;
			renderEntity.noShadow = true;
			if ( state_hitch ) {
				renderEntity.shaderParms[ SHADERPARM_TIME_OF_DEATH ] = gameLocal.time * 0.001f - 2.0f;
			} else {
				renderEntity.shaderParms[ SHADERPARM_TIME_OF_DEATH ] = gameLocal.time * 0.001f;
			}
			UpdateVisuals();
		}

		// wait a bit before switching off the content
		if ( deathClearContentsTime && gameLocal.time > deathClearContentsTime ) {
			SetCombatContents( false );
			deathClearContentsTime = 0;
		}
	} else {
		renderEntity.noShadow = false;
		renderEntity.shaderParms[ SHADERPARM_TIME_OF_DEATH ] = 0.0f;
		UpdateVisuals();
		doingDeathSkin = false;
	}
}

/*
==============
idPlayer::StartFxOnBone
==============
*/
void idPlayer::StartFxOnBone( const char *fx, const char *bone ) {
	idVec3 offset;
	idMat3 axis;
	jointHandle_t jointHandle = GetAnimator()->GetJointHandle( bone );

	if ( jointHandle == INVALID_JOINT ) {
		gameLocal.Printf( "Cannot find bone %s\n", bone );
		return;
	}

	if ( GetAnimator()->GetJointTransform( jointHandle, gameLocal.time, offset, axis ) ) {
		offset = GetPhysics()->GetOrigin() + offset * GetPhysics()->GetAxis();
		axis = axis * GetPhysics()->GetAxis();
	}

	idEntityFx::StartFx( fx, &offset, &axis, this, true );
}

/*
==============
idPlayer::SetupFlashlightSlot
==============
*/
void idPlayer::SetupFlashlightHolster()
{
    memset( &flashlightRenderEntity, 0, sizeof( flashlightRenderEntity ) );
    flashlightRenderEntity.hModel = renderModelManager->FindModel( "models/items/flashlight/flashlight2_world.lwo" );
    if( flashlightRenderEntity.hModel )
    {
        flashlightRenderEntity.hModel->Reset();
        flashlightRenderEntity.bounds = flashlightRenderEntity.hModel->Bounds( &flashlightRenderEntity );
    }
    flashlightRenderEntity.shaderParms[ SHADERPARM_RED ]	= 1.0f;
    flashlightRenderEntity.shaderParms[ SHADERPARM_GREEN ] = 1.0f;
    flashlightRenderEntity.shaderParms[ SHADERPARM_BLUE ]	= 1.0f;
    flashlightRenderEntity.shaderParms[ SHADERPARM_ALPHA ] = 0.75f;
    flashlightRenderEntity.shaderParms[ SHADERPARM_TIMEOFFSET ] = 0.0f;
    flashlightRenderEntity.shaderParms[5] = 0.0f;
    flashlightRenderEntity.shaderParms[6] = 0.0f;
    flashlightRenderEntity.shaderParms[7] = 0.0f;
}

/*
==============
idPlayer::UpdateFlashlightHolster
==============
*/
void idPlayer::UpdateFlashlightHolster()
{
    /*
     * bool hasFlashlight = !( ( weapon_flashlight < 0 ) || ( inventory.weapons & ( 1 << weapon_flashlight ) ) == 0 );
	if( hasFlashlight &&
		GetCurrentWeaponId() != weapon_flashlight &&
    	flashlightRenderEntity.hModel &&
    	pVRClientInfo != nullptr)
    {
        idVec3	holsterOffset( -pVRClientInfo->flashlightHolsterOffset[2],
                                 -pVRClientInfo->flashlightHolsterOffset[0],
                                 pVRClientInfo->flashlightHolsterOffset[1]);

		idAngles a(0, viewAngles.yaw - pVRClientInfo->hmdorientation[YAW], 0);
		holsterOffset *= a.ToMat3();
        holsterOffset *= ((100.0f / 2.54f) * vr_scale.GetFloat());
        flashlightRenderEntity.origin = firstPersonViewOrigin + holsterOffset;

        flashlightRenderEntity.entityNum = ENTITYNUM_NONE;

        flashlightRenderEntity.axis = idAngles(-60, 90, 0).ToMat3() * firstPersonViewAxis * 0.8f;

        flashlightRenderEntity.allowSurfaceInViewID = entityNumber + 1;
        flashlightRenderEntity.weaponDepthHack = true;

        if( flashlightModelDefHandle == -1 )
        {
            flashlightModelDefHandle = gameRenderWorld->AddEntityDef( &flashlightRenderEntity );
        }
        else
        {
            gameRenderWorld->UpdateEntityDef( flashlightModelDefHandle, &flashlightRenderEntity );
        }
    }
    else if (!hasFlashlight || // User hasn't got flashlight yet
    	GetCurrentWeaponId() == weapon_flashlight)
	{
		if( flashlightModelDefHandle != -1 )
		{
			gameRenderWorld->FreeEntityDef( flashlightModelDefHandle );
			flashlightModelDefHandle = -1;
		}
	}*/
}

/*
==============
idPlayer::Think

Called every tic for each player
==============
*/
void idPlayer::Think( void ) {
	renderEntity_t *headRenderEnt;

	UpdatePlayerIcons();

    UpdateSkinSetup();

    //Lubos BEGIN
    bool laserRequested = cvarSystem->GetCVarInteger( "vr_weaponSight" ) > 0;
    bool laserActive = hands[ 0 ].laserSightActive || hands[ 1 ].laserSightActive;
    if ( laserRequested != laserActive ) {
        ToggleLaserSight();
    }
    //Lubos END

	// latch button actions
	oldButtons = usercmd.buttons;

	// grab out usercmd
	usercmd_t oldCmd = usercmd;
	usercmd = gameLocal.usercmds[ entityNumber ];
	buttonMask &= usercmd.buttons;
	usercmd.buttons &= ~buttonMask;

	if ( gameLocal.inCinematic && gameLocal.skipCinematic ) {
		return;
	}

    if (pVRClientInfo != nullptr)
    {
        if (inventory.weapons > 1) {
            pVRClientInfo->weaponid = GetCurrentWeaponId();
        }
        else {
            pVRClientInfo->weaponid = -1;
        }

        int _currentWeapon = GetCurrentWeaponId();
        if(_currentWeapon == WEAPON_FLASHLIGHT || _currentWeapon == WEAPON_FISTS)
        	pVRClientInfo->velocitytriggered = true;
		else
			pVRClientInfo->velocitytriggered = false;
		pVRClientInfo->velocitytriggeredoffhand = true;

        pVRClientInfo->oneHandOnly = (_currentWeapon == WEAPON_FISTS) ||
			(_currentWeapon == weapon_handgrenade);
    }

    // clear the ik before we do anything else so the skeleton doesn't get updated twice
	walkIK.ClearJointMods();

    // Koz
    armIK.ClearJointMods();

	// if this is the very first frame of the map, set the delta view angles
	// based on the usercmd angles
	if ( !spawnAnglesSet && ( gameLocal.GameState() != GAMESTATE_STARTUP ) ) {
		spawnAnglesSet = true;
		SetViewAngles( spawnAngles );
		oldFlags = usercmd.flags;
	}

	if (mountedObject) {
		usercmd.forwardmove = 0;
		usercmd.rightmove = 0;
		usercmd.upmove = 0;
	}

	if ( objectiveSystemOpen || gameLocal.inCinematic || influenceActive ) {
		if ( objectiveSystemOpen && AI_PAIN ) {
			TogglePDA( 1 - vr_weaponHand.GetInteger() );
		}
		usercmd.forwardmove = 0;
		usercmd.rightmove = 0;
		usercmd.upmove = 0;
	}

	// log movement changes for weapon bobbing effects
	if ( usercmd.forwardmove != oldCmd.forwardmove ) {
		loggedAccel_t	*acc = &loggedAccel[currentLoggedAccel&(NUM_LOGGED_ACCELS-1)];
		currentLoggedAccel++;
		acc->time = gameLocal.time;
		acc->dir[0] = usercmd.forwardmove - oldCmd.forwardmove;
		acc->dir[1] = acc->dir[2] = 0;
	}

	if ( usercmd.rightmove != oldCmd.rightmove ) {
		loggedAccel_t	*acc = &loggedAccel[currentLoggedAccel&(NUM_LOGGED_ACCELS-1)];
		currentLoggedAccel++;
		acc->time = gameLocal.time;
		acc->dir[1] = usercmd.rightmove - oldCmd.rightmove;
		acc->dir[0] = acc->dir[2] = 0;
	}

	// freelook centering
	if ( ( usercmd.buttons ^ oldCmd.buttons ) & BUTTON_MLOOK ) {
		centerView.Init( gameLocal.time, 200, viewAngles.pitch, 0 );
	}

	/* No Zoom in VR
	// zooming
	if ( ( usercmd.buttons ^ oldCmd.buttons ) & BUTTON_ZOOM ) {
		if ( ( usercmd.buttons & BUTTON_ZOOM ) && weapon ) {
			zoomFov.Init( gameLocal.time, 200.0f, CalcFov( false ), weapon->GetZoomFov() );
		} else {
			zoomFov.Init( gameLocal.time, 200.0f, zoomFov.GetCurrentValue( gameLocal.time ), DefaultFov() );
		}
	}*/

	// if we have an active gui, we will unrotate the view angles as
	// we turn the mouse movements into gui events
	idUserInterface *gui = ActiveGui();
	if ( gui && gui != focusUI ) {
		RouteGuiMouse( gui );
	}

	// set the push velocity on the weapons before running the physics
	for( int h = 0; h < 2; h++ )
	{
		if( hands[h].weapon )
			hands[h].weapon->SetPushVelocity( physicsObj.GetPushedLinearVelocity() );
	}

	EvaluateControls();

	if ( !af.IsActive() ) {
		AdjustBodyAngles();
		CopyJointsFromBodyToHead();
	}

	Move();
	SetWeaponHandPose();
	SetFlashHandPose(); // Call set flashlight hand pose script function

	if ( !g_stopTime.GetBool() ) {

		if ( !noclip && !spectating && ( health > 0 ) && !IsHidden() ) {
			TouchTriggers();
		}

		// not done on clients for various reasons. don't do it on server and save the sound channel for other things
		if ( !gameLocal.isMultiplayer ) {
			SetCurrentHeartRate();
			float scale = new_g_damageScale;
			if ( g_useDynamicProtection.GetBool() && scale < 1.0f && gameLocal.time - lastDmgTime > 500 ) {
				if ( scale < 1.0f ) {
					scale += 0.05f;
				}
				if ( scale > 1.0f ) {
					scale = 1.0f;
				}
				new_g_damageScale = scale;
			}
		}

		// update GUIs, Items, and character interactions
		// UpdateFocus(); // Koz moved to just before update flashlight.

		UpdateLocation();

		// update player script
		UpdateScript();

		// service animations
		if ( !spectating && !af.IsActive() && !gameLocal.inCinematic ) {
			UpdateConditions();
			UpdateAnimState();
			CheckBlink();
		}

		// clear out our pain flag so we can tell if we recieve any damage between now and the next time we think
		AI_PAIN = false;
	}

	// calculate the exact bobbed view position, which is used to
	// position the view weapon, among other things
	CalculateFirstPersonView();

	// this may use firstPersonView, or a thirdPeroson / camera view
	CalculateRenderView();

	inventory.UpdateArmor();

	if ( spectating ) {
		UpdateSpectating();
	} else if ( health > 0 ) {
		UpdateWeapon();
	}

	// Koz
	UpdateNeckPose();

	UpdateFocus(); // Koz move here update GUIs, Items, and character interactions.

	UpdateFlashlight();

	UpdateAir();

	UpdatePowerupHud();

	UpdateHud();

	UpdatePowerUps();

	if (health > 0 && health < 40)
	{
		//heartbeat is a special case that uses intensity for a different purpose
		common->HapticEvent("heartbeat", 0, 0, health, 0, 0);
	}

	//UpdateFlashlightHolster();

	//Dr Beef version - maybe minimise
	//UpdateLaserSight();
    //his is already done above in UpdateHUD()
	//UpdateVrHud();

	UpdateDeathSkin( false );

	if ( gameLocal.isMultiplayer ) {
		DrawPlayerIcons();
		if (enviroSuitLight.IsValid()) {
			idAngles lightAng = firstPersonViewAxis.ToAngles();
			idVec3 lightOrg = firstPersonViewOrigin;
			const idDict *lightDef = gameLocal.FindEntityDefDict("envirosuit_light", false);

			idVec3 enviroOffset = lightDef->GetVector("enviro_offset");
			idVec3 enviroAngleOffset = lightDef->GetVector("enviro_angle_offset");

			lightOrg += (enviroOffset.x * firstPersonViewAxis[0]);
			lightOrg += (enviroOffset.y * firstPersonViewAxis[1]);
			lightOrg += (enviroOffset.z * firstPersonViewAxis[2]);
			lightAng.pitch += enviroAngleOffset.x;
			lightAng.yaw += enviroAngleOffset.y;
			lightAng.roll += enviroAngleOffset.z;

			enviroSuitLight.GetEntity()->GetPhysics()->SetOrigin(lightOrg);
			enviroSuitLight.GetEntity()->GetPhysics()->SetAxis(lightAng.ToMat3());
			enviroSuitLight.GetEntity()->UpdateVisuals();
			enviroSuitLight.GetEntity()->Present();
		}
	}

	static int lastFlashlightMode = commonVr->GetCurrentFlashlightMode();
	static bool lastFists = false;
	static bool lastHandInGui = false;

	// Koz turn body on or off in vr, update hand poses/skins if body or weapon hand changes.
	if ( vr_weaponHand.IsModified()  )
	{
		UpdatePlayerSkinsPoses();
		vr_weaponHand.ClearModified();

	}
	if ( vr_flashlightMode.IsModified() || lastFlashlightMode != commonVr->GetCurrentFlashlightMode() )
	{
		if ( vr_flashlightMode.IsModified() )
		{
			commonVr->currentFlashlightMode = vr_flashlightMode.GetInteger();
			vr_flashlightMode.ClearModified();
		}

		lastFlashlightMode = commonVr->GetCurrentFlashlightMode();
		UpdatePlayerSkinsPoses();
	}

	if ( head ) {
		headRenderEnt = head->GetRenderEntity();
	} else {
		headRenderEnt = NULL;
	}

	if ( headRenderEnt ) {
		if ( influenceSkin ) {
			headRenderEnt->customSkin = influenceSkin;
		} else {
			headRenderEnt->customSkin = NULL;
		}

		// Koz show the head if the player is using third person movement mode && the model has moved more than 8 inches.
		if ( commonVr->thirdPersonMovement && commonVr->thirdPersonDelta > 45.0f )
		{
			headRenderEnt->suppressSurfaceInViewID = 0; // show head
			headRenderEnt->allowSurfaceInViewID = 0;
		}
		else
		{
			headRenderEnt->allowSurfaceInViewID = 0; // GB show the head in mirrors.
		}
	}

	if ( gameLocal.isMultiplayer || g_showPlayerShadow.GetBool() ) {
		renderEntity.suppressShadowInViewID	= 0;
		if ( headRenderEnt ) {
			headRenderEnt->suppressShadowInViewID = 0;
		}
	} else {
		renderEntity.suppressShadowInViewID	= entityNumber+1;
		if( headRenderEnt )
		{
			headRenderEnt->suppressShadowInViewID = 0;
			// Koz end
		}
	}
	// never cast shadows from our first-person muzzle flashes
	renderEntity.suppressShadowInLightID = LIGHTID_VIEW_MUZZLE_FLASH + entityNumber;
	if ( headRenderEnt ) {
		headRenderEnt->suppressShadowInLightID = LIGHTID_VIEW_MUZZLE_FLASH + entityNumber;
	}

	if ( !g_stopTime.GetBool() ) {
		if ( armIK.IsInitialized() ) armIK.Evaluate();

		UpdateAnimation();

		Present();

		UpdateDamageEffects();

		LinkCombat();

		playerView.CalculateShake();
	}

	if ( !( thinkFlags & TH_THINK ) ) {
		gameLocal.Printf( "player %d not thinking?\n", entityNumber );
	}

	if ( g_showEnemies.GetBool() ) {
		idActor *ent;
		int num = 0;
		for( ent = enemyList.Next(); ent != NULL; ent = ent->enemyNode.Next() ) {
			gameLocal.Printf( "enemy (%d)'%s'\n", ent->entityNumber, ent->name.c_str() );
			gameRenderWorld->DebugBounds( colorRed, ent->GetPhysics()->GetBounds().Expand( 2 ), ent->GetPhysics()->GetOrigin() );
			num++;
		}
		gameLocal.Printf( "%d: enemies\n", num );
	}

	inventory.RechargeAmmo(this);

	if (healthRecharge) {
		int elapsed = gameLocal.time - lastHealthRechargeTime;

		if (elapsed >= rechargeSpeed) {
			int intervals = (gameLocal.time - lastHealthRechargeTime)/rechargeSpeed;
			Give("health", va("%d", intervals), vr_weaponHand.GetInteger());
			lastHealthRechargeTime += intervals*rechargeSpeed;
		}
	}

	// determine if portal sky is in pvs
	gameLocal.portalSkyActive = gameLocal.pvs.CheckAreasForPortalSky(gameLocal.GetPlayerPVS(), GetPhysics()->GetOrigin());

	// stereo rendering laser sight that replaces the crosshair
	for( int h = 0; h < 2; h++ )
	{
		UpdateLaserSight( h );
	}
	UpdateTeleportAim();

	if ( vr_teleportMode.GetInteger() != 0 )
	{
		if  (warpMove ) {
			if ( gameLocal.time > warpTime )
			{
				extern idCVar timescale;
				warpTime = 0;
				noclip = false;
				warpMove = false;
				warpVel = vec3_origin;
				timescale.SetFloat( 1.0f );
				//playerView.EnableBFGVision(false);
				Teleport( warpDest, viewAngles, NULL ); //Carl: get the destination exact
			}
			physicsObj.SetLinearVelocity( warpVel );
		}

		if ( jetMove ) {

			if ( gameLocal.time > jetMoveTime )
			{
				jetMoveTime = 0;
				jetMove = false;
				jetMoveVel = vec3_origin;
				jetMoveCoolDownTime = gameLocal.time + 30;
			}

			physicsObj.SetLinearVelocity( jetMoveVel );

		}
	}

	UpdatePDASlot();
	UpdateHolsterSlot();

	if( vr_slotDebug.GetBool() )
	{
		for( int i = 0; i < SLOT_COUNT; i++ )
		{
			idVec3 slotOrigin = slots[i].origin;
			if ( vr_weaponHand.GetInteger() && i != SLOT_FLASHLIGHT_SHOULDER )
				slotOrigin.y *= -1;
			idVec3 origin = waistOrigin + slotOrigin * waistAxis;
			idSphere tempSphere( origin, sqrtf(slots[i].radiusSq) );
			gameRenderWorld->DebugSphere( colorWhite, tempSphere, 18, true );
		}
	}
}

/*
=================
idPlayer::StartHealthRecharge
=================
*/
void idPlayer::StartHealthRecharge(int speed) {
    lastHealthRechargeTime = gameLocal.time;
    healthRecharge = true;
    rechargeSpeed = speed;
}

/*
=================
idPlayer::StopHealthRecharge
=================
*/
void idPlayer::StopHealthRecharge() {
    healthRecharge = false;
}

/*
=================
idPlayer::CanGive
=================
*/
bool idPlayer::CanGive(const char *statname, const char *value) {
    if (AI_DEAD) {
        return false;
    }

    if (!idStr::Icmp(statname, "health")) {
        if (health >= inventory.maxHealth) {
            return false;
        }

        return true;
    } else if (!idStr::Icmp(statname, "stamina")) {
        if (stamina >= 100) {
            return false;
        }

        return true;

    } else if (!idStr::Icmp(statname, "heartRate")) {
        return true;

    } else if (!idStr::Icmp(statname, "air")) {
        if (airTics >= pm_airTics.GetInteger()) {
            return false;
        }

        return true;
    } else {
        int idealWeapon = GetIdealWeapon();
        return inventory.CanGive(this, spawnArgs, statname, value, &idealWeapon);
    }

    return false;
}


/*
=================
idPlayer::StopHelltime

provides a quick non-ramping way of stopping helltime
=================
*/
void idPlayer::StopHelltime(bool quick) {
    if (!PowerUpActive(HELLTIME)) {
        return;
    }

    // take away the powerups
    if (PowerUpActive(INVULNERABILITY)) {
        ClearPowerup(INVULNERABILITY);
    }

    if (PowerUpActive(BERSERK)) {
        ClearPowerup(BERSERK);
    }

    if (PowerUpActive(HELLTIME)) {
        ClearPowerup(HELLTIME);
    }

    // stop the looping sound
    StopSound(SND_CHANNEL_DEMONIC, false);

    // reset the game vars
    if (quick) {
        gameLocal.QuickSlowmoReset();
    }
}

/*
=================
idPlayer::Event_ToggleBloom
=================
*/
void idPlayer::Event_ToggleBloom(int on) {
    if (on) {
        bloomEnabled = true;
    } else {
        bloomEnabled = false;
    }
}

/*
=================
idPlayer::Event_SetBloomParms
=================
*/
void idPlayer::Event_SetBloomParms(float speed, float intensity) {
    bloomSpeed = speed;
    bloomIntensity = intensity;
}

/*
=================
idPlayer::PlayHelltimeStopSound
=================
*/
void idPlayer::PlayHelltimeStopSound() {
    const char *sound;

    if (spawnArgs.GetString("snd_helltime_stop", "", &sound)) {
        PostEventMS(&EV_StartSoundShader, 0, sound, SND_CHANNEL_ANY);
    }
}
/*
=================
idPlayer::Event_ShowConsequences
=================
*/
void idPlayer::Event_ShowConsequences() {
    if (hud) {
        hud->HandleNamedEvent("Consequences");
    }
}

/*
=================
idPlayer::Event_SetViewAngles
=================
*/
void idPlayer::Event_SetViewAngles(const idVec3 &angles) {
    viewAngles[0] = angles[0];
    viewAngles[1] = angles[1];
    viewAngles[2] = angles[2];
}

/*
=================
idPlayer::Event_GetEyeHeight
=================
*/
void idPlayer::Event_GetEyeHeight() { idThread::ReturnFloat(eyeOffset.z); }

/*
==============
idPlayer::ToggleHud  Koz toggle hud
==============
*/
void idPlayer::ToggleHud()
{
	hudActive = !hudActive;
}

/*
==============
idPlayer::ToggleLaserSight  Koz toggle the lasersight
==============
*/
void idPlayer::ToggleLaserSight()
{
	if( !hands[ 0 ].laserSightActive || !hands[ 1 ].laserSightActive )
	{
		hands[ 0 ].laserSightActive = true;
		hands[ 1 ].laserSightActive = true;
	}
	else
	{
		hands[ 0 ].laserSightActive = false;
		hands[ 1 ].laserSightActive = false;
	}
}

// Carl: Context sensitive VR thumb clicks, and dual wielding
// 0 = right hand, 1 = left hand; true if pressed, false if released; returns true if handled as thumb click
// WARNING! Called from the input thread?
bool idPlayer::ThumbClickWorld( int hand, bool pressed )
{
	bool b;
	if( !pressed )
	{
		b = hands[ hand ].thumbDown;
		hands[ hand ].thumbDown = false;
		return b;
	}
	// if we're holding or over a flashlight, then the thumb button activates the button of that flashlight
	b = hands[ hand ].holdingFlashlight();
	b = b || ( hands[ hand ].isOverMountedFlashlight() && !hands[ hand ].tooFullToInteract() );
	hands[ hand ].thumbDown = b;
	return b;
}

/*
=================
idPlayer::RouteGuiMouse
=================
*/
void idPlayer::RouteGuiMouse( idUserInterface *gui ) {
	sysEvent_t ev;

	if ( usercmd.mx != oldMouseX || usercmd.my != oldMouseY ) {
		ev = sys->GenerateMouseMoveEvent( usercmd.mx - oldMouseX, usercmd.my - oldMouseY );
		gui->HandleEvent( &ev, gameLocal.time );
		oldMouseX = usercmd.mx;
		oldMouseY = usercmd.my;
	}
}

/*
==================
idPlayer::LookAtKiller
==================
*/
void idPlayer::LookAtKiller( idEntity *inflictor, idEntity *attacker ) {
	idVec3 dir;

	if ( attacker && attacker != this ) {
		dir = attacker->GetPhysics()->GetOrigin() - GetPhysics()->GetOrigin();
	} else if ( inflictor && inflictor != this ) {
		dir = inflictor->GetPhysics()->GetOrigin() - GetPhysics()->GetOrigin();
	} else {
		dir = viewAxis[ 0 ];
	}

	idAngles ang( 0, dir.ToYaw(), 0 );
	SetViewAngles( ang );
}

/*
==============
idPlayer::Kill
==============
*/
void idPlayer::Kill( bool delayRespawn, bool nodamage ) {
	if ( spectating ) {
		SpectateFreeFly( false );
	} else if ( health > 0 ) {
		godmode = false;
		if ( nodamage ) {
			ServerSpectate( true );
			forceRespawn = true;
		} else {
			Damage( this, this, vec3_origin, "damage_suicide", 1.0f, INVALID_JOINT );
			if ( delayRespawn ) {
				forceRespawn = false;
				int delay = spawnArgs.GetFloat( "respawn_delay" );
				if ( gameLocal.isMultiplayer ) {
					minRespawnTime = gameLocal.time + MAX_RESPAWN_TIME_MP / 4;
					maxRespawnTime = minRespawnTime + MAX_RESPAWN_TIME_MP;
				} else {
					minRespawnTime = gameLocal.time + SEC2MS( delay );
					maxRespawnTime = minRespawnTime + MAX_RESPAWN_TIME;
				}
			}
		}
	}
}

/*
==================
idPlayer::Killed
==================
*/
void idPlayer::Killed( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location ) {
	float delay;

	assert( !gameLocal.isClient );

	// stop taking knockback once dead
	fl.noknockback = true;
	if ( health < -999 ) {
		health = -999;
	}

	if ( AI_DEAD ) {
		AI_PAIN = true;
		return;
	}

	heartInfo.Init( 0, 0, 0, BASE_HEARTRATE );
	AdjustHeartRate( DEAD_HEARTRATE, 10.0f, 0.0f, true );

	if ( !g_testDeath.GetBool() ) {
		playerView.Fade( colorBlack, 12000 );
	}

	AI_DEAD = true;
	SetAnimState( ANIMCHANNEL_LEGS, "Legs_Death", 4 );
	SetAnimState( ANIMCHANNEL_TORSO, "Torso_Death", 4 );
	SetWaitState( "" );

	animator.ClearAllJoints();

	if ( gameLocal.isMultiplayer ) {
		minRespawnTime = gameLocal.time + MAX_RESPAWN_TIME_MP / 4;
		maxRespawnTime = minRespawnTime + MAX_RESPAWN_TIME_MP;
	} else if ( StartRagdoll() ) {
		pm_modelView.SetInteger( 0 );
		minRespawnTime = gameLocal.time + RAGDOLL_DEATH_TIME;
		maxRespawnTime = minRespawnTime + MAX_RESPAWN_TIME;
	} else {
		// don't allow respawn until the death anim is done
		// g_forcerespawn may force spawning at some later time
		delay = spawnArgs.GetFloat( "respawn_delay" );
		minRespawnTime = gameLocal.time + SEC2MS( delay );
		maxRespawnTime = minRespawnTime + MAX_RESPAWN_TIME;
	}

	physicsObj.SetMovementType( PM_DEAD );
	StartSound( "snd_death", SND_CHANNEL_VOICE, 0, false, NULL );
	StopSound( SND_CHANNEL_BODY2, false );

	fl.takedamage = true;		// can still be gibbed

    // get rid of weapons
    for( int h = 0; h < 2; h++ )
        hands[ h ].weapon->OwnerDied();

    // drop the weapons as items
    DropWeapons( true );

	if ( !g_testDeath.GetBool() ) {
		LookAtKiller( inflictor, attacker );
	}

	if ( gameLocal.isMultiplayer || g_testDeath.GetBool() ) {
		idPlayer *killer = NULL;
		// no gibbing in MP. Event_Gib will early out in MP
		if ( attacker->IsType( idPlayer::Type ) ) {
			killer = static_cast<idPlayer*>(attacker);
			if ( health < -20 || killer->PowerUpActive( BERSERK ) ) {
				gibDeath = true;
				gibsDir = dir;
				gibsLaunched = false;
			}
		}
		gameLocal.mpGame.PlayerDeath( this, killer, isTelefragged );
	} else {
		physicsObj.SetContents( CONTENTS_CORPSE | CONTENTS_MONSTERCLIP );
	}

	ClearPowerUps();

	UpdateVisuals();

	isChatting = false;
}

/*
=====================
idPlayer::GetAIAimTargets

Returns positions for the AI to aim at.
=====================
*/
void idPlayer::GetAIAimTargets( const idVec3 &lastSightPos, idVec3 &headPos, idVec3 &chestPos ) {
	idVec3 offset;
	idMat3 axis;
	idVec3 origin;

	origin = lastSightPos - physicsObj.GetOrigin();

	GetJointWorldTransform( chestJoint, gameLocal.time, offset, axis );
	headPos = offset + origin;

	GetJointWorldTransform( headJoint, gameLocal.time, offset, axis );
	chestPos = offset + origin;
}

/*
=====================
idPlayer::GetAnim
Carl: Virtual method idActor::GetAnim() needs to work differently on a dual wielding player
=====================
*/
int idPlayer::GetAnim( int channel, const char* animname )
{
    int			anim;
    const char* temp;
    idAnimator* animatorPtr;

    if( channel == ANIMCHANNEL_HEAD )
    {
        if( !head )
        {
            return 0;
        }
        animatorPtr = head->GetAnimator();
    }
    else
    {
        animatorPtr = &animator;
    }

    if( channel == ANIMCHANNEL_LEFTHAND && hands[HAND_LEFT].animPrefix.Length() )
    {
        temp = va( "%s_%s", hands[HAND_LEFT].animPrefix.c_str(), animname );
        anim = animatorPtr->GetAnim( temp );
        if( anim )
            return anim;
    }
    else if( channel == ANIMCHANNEL_RIGHTHAND && hands[HAND_RIGHT].animPrefix.Length() )
    {
        temp = va( "%s_%s", hands[HAND_RIGHT].animPrefix.c_str(), animname );
        anim = animatorPtr->GetAnim( temp );
        if( anim )
            return anim;
    }
    if( animPrefix.Length() )
    {
        temp = va( "%s_%s", animPrefix.c_str(), animname );
        anim = animatorPtr->GetAnim( temp );
        if( anim )
            return anim;
    }

    anim = animatorPtr->GetAnim( animname );

    return anim;
}

/*
================
idPlayer::DamageFeedback

callback function for when another entity received damage from this entity.  damage can be adjusted and returned to the caller.
================
*/
void idPlayer::DamageFeedback( idEntity *victim, idEntity *inflictor, int &damage ) {
	assert( !gameLocal.isClient );
	damage *= PowerUpModifier( BERSERK );
    if( damage && ( victim != this ) && ( victim->IsType( idActor::Type ) || victim->IsType( idDamagable::Type ) ) )
    {
        SetLastHitTime( gameLocal.time );
    }
	/*
    if (victim == this)
	{
		common->Vibrate(250, 0, damage / 50);
		common->Vibrate(250, 1, damage / 50);
	}
	*/
}

/*
=================
idPlayer::CalcDamagePoints

Calculates how many health and armor points will be inflicted, but
doesn't actually do anything with them.  This is used to tell when an attack
would have killed the player, possibly allowing a "saving throw"
=================
*/
void idPlayer::CalcDamagePoints( idEntity *inflictor, idEntity *attacker, const idDict *damageDef,
							   const float damageScale, const int location, int *health, int *armor ) {
	int		damage;
	int		armorSave;

	damageDef->GetInt( "damage", "20", damage );
	damage = GetDamageForLocation( damage, location );

	idPlayer *player = attacker->IsType( idPlayer::Type ) ? static_cast<idPlayer*>(attacker) : NULL;
	if ( !gameLocal.isMultiplayer ) {
		if ( inflictor != gameLocal.world ) {
			switch ( g_skill.GetInteger() ) {
				case 0:
					damage *= 0.80f;
					if ( damage < 1 ) {
						damage = 1;
					}
					break;
				case 2:
					damage *= 1.70f;
					break;
				case 3:
					damage *= 3.5f;
					break;
				default:
					break;
			}
		}
	}

	damage *= damageScale;

	// always give half damage if hurting self
	if ( attacker == this ) {
		if ( gameLocal.isMultiplayer ) {
			// only do this in mp so single player plasma and rocket splash is very dangerous in close quarters
			damage *= damageDef->GetFloat( "selfDamageScale", "0.5" );
		} else {
			damage *= damageDef->GetFloat( "selfDamageScale", "1" );
		}
	}

	// check for completely getting out of the damage
	if ( !damageDef->GetBool( "noGod" ) ) {
		// check for godmode
		if ( godmode ) {
			damage = 0;
		}
		//Invulnerability is just like god mode
		if (PowerUpActive(INVULNERABILITY)) {
			damage = 0;
		}
	}

	// inform the attacker that they hit someone
	attacker->DamageFeedback( this, inflictor, damage );

	// save some from armor
	if ( !damageDef->GetBool( "noArmor" ) ) {
		float armor_protection;

		armor_protection = ( gameLocal.isMultiplayer ) ? g_armorProtectionMP.GetFloat() : g_armorProtection.GetFloat();

		armorSave = ceil( damage * armor_protection );
		if ( armorSave >= inventory.armor ) {
			armorSave = inventory.armor;
		}

		if ( !damage ) {
			armorSave = 0;
		} else if ( armorSave >= damage ) {
			armorSave = damage - 1;
			damage = 1;
		} else {
			damage -= armorSave;
		}
	} else {
		armorSave = 0;
	}

	// check for team damage
	if ( gameLocal.gameType == GAME_TDM
		&& !gameLocal.serverInfo.GetBool( "si_teamDamage" )
		&& !damageDef->GetBool( "noTeam" )
		&& player
		&& player != this		// you get self damage no matter what
		&& player->team == team ) {
			damage = 0;
	}

	*health = damage;
	*armor = armorSave;
}

/*
============
idPlayer::ControllerShakeFromDamage
============
*/
void idPlayer::ControllerShakeFromDamage( int damage )
{
	// If the player is local. SHAkkkkkkeeee!
	if(  IsLocallyControlled() )
	{

		int maxMagScale = pm_controllerShake_damageMaxMag.GetFloat();
		int maxDurScale = pm_controllerShake_damageMaxDur.GetFloat();

		// determine rumble
		// >= 100 damage - will be 300 Mag
		float highMag = ( Max( damage, 100 ) / 100.0f ) * maxMagScale;
		int highDuration = idMath::Ftoi( ( Max( damage, 100 ) / 100.0f ) * maxDurScale );
		float lowMag = highMag * 0.75f;
		int lowDuration = idMath::Ftoi( highDuration );

		// Multiplayer damage from an unknown source to an unknown location. So shake both hands.
		for( int h = 0; h < 2; h++ )
			hands[h].SetControllerShake( highMag, highDuration, lowMag, lowDuration );
	}

}

/*
============
idPlayer::ControllerShakeFromDamage
============
*/
void idPlayer::ControllerShakeFromDamage( int damage, const idVec3 &dir )
{
	// If the player is local. SHAkkkkkkeeee!
	if( IsLocallyControlled() )
	{
		int maxMagScale = pm_controllerShake_damageMaxMag.GetFloat();
		int maxDurScale = pm_controllerShake_damageMaxDur.GetFloat();

		// determine rumble
		// >= 100 damage - will be 300 Mag
		float highMag = ( Max( damage, 100 ) / 100.0f ) * maxMagScale;
		int highDuration = idMath::Ftoi( ( Max( damage, 100 ) / 100.0f ) * maxDurScale );

		if( commonVr->hasHMD )
		{
			SetControllerShake( highMag, highDuration, dir );
		}
		else
		{
			float lowMag = highMag * 0.75f;
			int lowDuration = idMath::Ftoi( highDuration );
			hands[HAND_LEFT].SetControllerShake( highMag, highDuration, lowMag, lowDuration );
			hands[HAND_RIGHT].SetControllerShake( highMag, highDuration, lowMag, lowDuration );
		}
	}

}

/*
============
Damage

this		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
	example: this=monster, inflictor=rocket, attacker=player

dir			direction of the attack for knockback in global space

damageDef	an idDict with all the options for damage effects

inflictor, attacker, dir, and point can be NULL for environmental effects
============
*/
void idPlayer::Damage( idEntity *inflictor, idEntity *attacker, const idVec3 &dir,
					   const char *damageDefName, const float damageScale, const int location ) {
	idVec3		kick;
	int			damage;
	int			armorSave;
	int			knockback;
	idVec3		damage_from;
	idVec3		localDamageVector;
	float		attackerPushScale;
	SetTimeState ts(timeGroup);

	// damage is only processed on server
	if ( gameLocal.isClient ) {
		return;
	}

	if ( !fl.takedamage || noclip || spectating || gameLocal.inCinematic ) {
		return;
	}

	if ( !inflictor ) {
		inflictor = gameLocal.world;
	}
	if ( !attacker ) {
		attacker = gameLocal.world;
	}

	if ( attacker->IsType( idAI::Type ) ) {
		if ( PowerUpActive( BERSERK ) ) {
			return;
		}
		// don't take damage from monsters during influences
		if ( influenceActive != 0 ) {
			return;
		}
	}

	const idDeclEntityDef *damageDef = gameLocal.FindEntityDef( damageDefName, false );
	if ( !damageDef ) {
		gameLocal.Warning( "Unknown damageDef '%s'", damageDefName );
		return;
	}

	if ( damageDef->dict.GetBool( "ignore_player" ) ) {
		return;
	}

	CalcDamagePoints( inflictor, attacker, &damageDef->dict, damageScale, location, &damage, &armorSave );

    // determine knockback
    if ( vr_knockback.GetBool() )
    {
        // determine knockback
        damageDef->dict.GetInt("knockback", "20", knockback);

        if (knockback != 0 && !fl.noknockback) {
            if (attacker == this) {
                damageDef->dict.GetFloat("attackerPushScale", "0", attackerPushScale);
            } else {
                attackerPushScale = 1.0f;
            }

            kick = dir;
            kick.Normalize();
            kick *= g_knockback.GetFloat() * knockback * attackerPushScale / 200.0f;
            physicsObj.SetLinearVelocity(physicsObj.GetLinearVelocity() + kick);

            // set the timer so that the player can't cancel out the movement immediately
            physicsObj.SetKnockBack(idMath::ClampInt(50, 200, knockback * 2));
        }
    }

	// give feedback on the player view and audibly when armor is helping
	if ( armorSave ) {
		inventory.armor -= armorSave;

		if (inventory.armor == 0)
		{
			if ( IsType( idPlayer::Type ) ) {
				common->HapticEvent("shield_break", 0, 0, 100, 0, 0);
			}
		}

		if ( gameLocal.time > lastArmorPulse + 200 ) {
			StartSound( "snd_hitArmor", SND_CHANNEL_ITEM, 0, false, NULL );
		}
		lastArmorPulse = gameLocal.time;
	}

	if ( damageDef->dict.GetBool( "burn" ) ) {
		StartSound( "snd_burn", SND_CHANNEL_BODY3, 0, false, NULL );

		if ( IsType( idPlayer::Type ) ) {
			common->HapticEvent("fire", 0, 0, 50, 0, 0);
		}

	} else if ( damageDef->dict.GetBool( "no_air" ) ) {
		if ( !armorSave && health > 0 ) {
			StartSound( "snd_airGasp", SND_CHANNEL_ITEM, 0, false, NULL );
		}
	}

	if ( g_debugDamage.GetInteger() ) {
		gameLocal.Printf( "client:%i health:%i damage:%i armor:%i\n",
			entityNumber, health, damage, armorSave );
	}

	// move the world direction vector to local coordinates
	damage_from = dir;
	damage_from.Normalize();

	viewAxis.ProjectVector( damage_from, localDamageVector );

	// add to the damage inflicted on a player this frame
	// the total will be turned into screen blends and view angle kicks
	// at the end of the frame
	if ( health > 0 ) {
		playerView.DamageImpulse( localDamageVector, &damageDef->dict );
	}

	// do the damage
	if ( damage > 0 ) {

		if ( !gameLocal.isMultiplayer ) {
			float scale = new_g_damageScale;
			if ( g_useDynamicProtection.GetBool() && g_skill.GetInteger() < 2 ) {
				if ( gameLocal.time > lastDmgTime + 500 && scale > 0.25f ) {
					scale -= 0.05f;
					new_g_damageScale = scale;
				}
			}

			if ( scale > 0.0f ) {
				damage *= scale;
			}

			if( IsLocallyControlled() )
			{
				ControllerShakeFromDamage( damage, dir );
			}
		}

		if ( damage < 1 ) {
			damage = 1;
		}

		health -= damage;

		if ( health <= 0 ) {

			if ( health < -999 ) {
				health = -999;
			}

			isTelefragged = damageDef->dict.GetBool( "telefrag" );

			lastDmgTime = gameLocal.time;
			Killed( inflictor, attacker, damage, dir, location );

		} else {
			// force a blink
			blink_time = 0;

			// let the anim script know we took damage
			AI_PAIN = Pain( inflictor, attacker, damage, dir, location );
			if ( !g_testDeath.GetBool() ) {
				lastDmgTime = gameLocal.time;
			}
		}
	} else {
		// don't accumulate impulses
		if ( af.IsLoaded() ) {
			// clear impacts
			af.Rest();

			// physics is turned off by calling af.Rest()
			BecomeActive( TH_PHYSICS );
		}
	}

	if ( IsType( idPlayer::Type )) {
		idVec3 bodyOrigin = vec3_zero;
		idMat3 bodyAxis;
		GetViewPos( bodyOrigin, bodyAxis );
		idAngles bodyAng = bodyAxis.ToAngles();

		float pitch = damage_from.ToPitch();
		if (pitch > 180)
			pitch -= 360;
		float yHeight = idMath::ClampFloat(-0.4f, 0.4f, -pitch / 90.0f);
        idAngles damageYaw(0, 180 + (damage_from.ToYaw() - bodyAng.yaw), 0);
        damageYaw.Normalize360();

		//Ensure a decent level of haptic feedback for any damage
		float hapticLevel = 80 + Min<float>(damage * 4, 120.0);

		//Indicate head damage if appropriate
		if ( location >= 0 && location < damageGroups.Size() &&
				strstr( damageGroups[location].c_str(), "head" ) ) {
			common->HapticEvent(damageDefName, 4, 0, hapticLevel, damageYaw.yaw, yHeight);
		} else {
            common->HapticEvent(damageDefName, 0, 0, hapticLevel, damageYaw.yaw, yHeight);
        }
	}

	lastDamageDef = damageDef->Index();
	lastDamageDir = damage_from;
	lastDamageLocation = location;
}

/*
===========
idPlayer::Teleport
============
*/
void idPlayer::Teleport( const idVec3 &origin, const idAngles &angles, idEntity *destination ) {
	idVec3 org;

    for( int h = 0; h < 2; h++ )
    {
        if( hands[ h ].weapon )
            hands[ h ].weapon->LowerWeapon();
    }

	SetOrigin( origin + idVec3( 0, 0, CM_CLIP_EPSILON ) );
	if ( !gameLocal.isMultiplayer && GetFloorPos( 16.0f, org ) ) {
		SetOrigin( org );
	}

	// clear the ik heights so model doesn't appear in the wrong place
	walkIK.EnableAll();

	GetPhysics()->SetLinearVelocity( vec3_origin );

	SetViewAngles( angles );

	legsYaw = 0.0f;
	idealLegsYaw = 0.0f;
	oldViewYaw = viewAngles.yaw;

	if ( gameLocal.isMultiplayer ) {
		playerView.Flash( colorWhite, 140 );
	}

	UpdateVisuals();

	teleportEntity = destination;

	if ( !gameLocal.isClient && !noclip ) {
		if ( gameLocal.isMultiplayer ) {
			// kill anything at the new position or mark for kill depending on immediate or delayed teleport
			gameLocal.KillBox( this, destination != NULL );
		} else {
			// kill anything at the new position
			gameLocal.KillBox( this, true );
		}
	}

	if (PowerUpActive(HELLTIME)) {
		StopHelltime();
	}
}

/*
====================
idPlayer::SetPrivateCameraView
====================
*/
void idPlayer::SetPrivateCameraView( idCamera *camView ) {
	privateCameraView = camView;
	if ( camView ) {
		StopFiring();
		Hide();
	} else {
		if ( !spectating ) {
			Show();
		}
	}
}

/*
===============
idPlayer::SetQuickSlot
===============
*/
void idPlayer::SetQuickSlot( int index, int val )
{
    if( index >= NUM_QUICK_SLOTS || index < 0 )
    {
        return;
    }

    quickSlot[ index ] = val;
}

/*
====================
idPlayer::DefaultFov

Returns the base FOV
====================
*/
float idPlayer::DefaultFov( void ) const {
	float fov;

	fov = renderSystem->GetFOV();
	if ( gameLocal.isMultiplayer ) {
		if ( fov < 90.0f ) {
			return 90.0f;
		} else if (fov > 110.0f) {
			return 110.0f;
		}
	}

	return fov;
}

/*
====================
idPlayer::CalcFov

Fixed fov at intermissions, otherwise account for fov variable and zooms.
====================
*/
float idPlayer::CalcFov( bool honorZoom ) {
	float fov;

	if ( fxFov ) {
		return DefaultFov() + 10.0f + cos( ( gameLocal.time + 2000 ) * 0.01 ) * 10.0f;
	}

	if ( influenceFov ) {
		return influenceFov;
	}

    // Koz, no zoom in VR.
	/*if ( zoomFov.IsDone( gameLocal.time ) ) {
		fov = ( honorZoom && usercmd.buttons & BUTTON_ZOOM ) && weapon ? weapon->GetZoomFov() : DefaultFov();
	} else {
		fov = zoomFov.GetCurrentValue( gameLocal.time );
	}*/

	fov = DefaultFov();

	// bound normal viewsize
	if ( fov < 1 ) {
		fov = 1;
	} else if ( fov > 179 ) {
		fov = 179;
	}

	return fov;
}

/*
==============
idPlayer::GunTurningOffset

generate a rotational offset for the gun based on the view angle
history in loggedViewAngles
==============
*/
idAngles idPlayer::GunTurningOffset( void ) {
	idAngles	a;

	a.Zero();

	if ( gameLocal.framenum < NUM_LOGGED_VIEW_ANGLES ) {
		return a;
	}

	idAngles current = loggedViewAngles[ gameLocal.framenum & (NUM_LOGGED_VIEW_ANGLES-1) ];

	idAngles	av, base;
	int weaponAngleOffsetAverages;
	float weaponAngleOffsetScale, weaponAngleOffsetMax;

	//weapon->GetWeaponAngleOffsets( &weaponAngleOffsetAverages, &weaponAngleOffsetScale, &weaponAngleOffsetMax );
    // Carl: todo dual wielding
    hands[vr_weaponHand.GetInteger()].weapon->GetWeaponAngleOffsets( &weaponAngleOffsetAverages, &weaponAngleOffsetScale, &weaponAngleOffsetMax );

	av = current;

	// calcualte this so the wrap arounds work properly
	for ( int j = 1 ; j < weaponAngleOffsetAverages ; j++ ) {
		idAngles a2 = loggedViewAngles[ ( gameLocal.framenum - j ) & (NUM_LOGGED_VIEW_ANGLES-1) ];

		idAngles delta = a2 - current;

		if ( delta[1] > 180 ) {
			delta[1] -= 360;
		} else if ( delta[1] < -180 ) {
			delta[1] += 360;
		}

		av += delta * ( 1.0f / weaponAngleOffsetAverages );
	}

	a = ( av - current ) * weaponAngleOffsetScale;

	for ( int i = 0 ; i < 3 ; i++ ) {
		if ( a[i] < -weaponAngleOffsetMax ) {
			a[i] = -weaponAngleOffsetMax;
		} else if ( a[i] > weaponAngleOffsetMax ) {
			a[i] = weaponAngleOffsetMax;
		}
	}

	return a;
}

/*
==============
idPlayer::GunAcceleratingOffset

generate a positional offset for the gun based on the movement
history in loggedAccelerations
==============
*/
idVec3	idPlayer::GunAcceleratingOffset( void ) {
	idVec3	ofs;

	float weaponOffsetTime, weaponOffsetScale;

	ofs.Zero();

	//weapon->GetWeaponTimeOffsets( &weaponOffsetTime, &weaponOffsetScale );
    // Carl: todo dual wielding
    hands[vr_weaponHand.GetInteger()].weapon->GetWeaponTimeOffsets( &weaponOffsetTime, &weaponOffsetScale );

	int stop = currentLoggedAccel - NUM_LOGGED_ACCELS;
	if ( stop < 0 ) {
		stop = 0;
	}
	for ( int i = currentLoggedAccel-1 ; i > stop ; i-- ) {
		loggedAccel_t	*acc = &loggedAccel[i&(NUM_LOGGED_ACCELS-1)];

		float	f;
		float	t = gameLocal.time - acc->time;
		if ( t >= weaponOffsetTime ) {
			break;	// remainder are too old to care about
		}

		f = t / weaponOffsetTime;
		f = ( cos( f * 2.0f * idMath::PI ) - 1.0f ) * 0.5f;
		ofs += f * weaponOffsetScale * acc->dir;
	}

	return ofs;
}


/*
==============
idPlayer::UpdateLaserSight
==============
*/
idCVar	g_laserSightWidth( "g_laserSightWidth", "0.3", CVAR_FLOAT | CVAR_ARCHIVE, "laser sight beam width" ); // Koz default was 2, IMO too big in VR.
idCVar	g_laserSightLength( "g_laserSightLength", "1000", CVAR_FLOAT | CVAR_ARCHIVE, "laser sight beam length" ); // Koz default was 250, but was to short in VR.  Length will be clipped if object is hit, this is max length for the hit trace.
void idPlayer::UpdateLaserSight( int hand )
{
    idVec3	muzzleOrigin;
    idMat3	muzzleAxis;

    idVec3 end, start;
    trace_t traceResults;

    float beamLength = g_laserSightLength.GetFloat(); // max length to run trace.

    int sightMode = vr_weaponSight.GetInteger();

    bool hideSight = false;

    bool traceHit = false;
    idWeapon* weapon = hands[ hand ].weapon;

    // In Multiplayer, weapon might not have been spawned yet.
    if( weapon ==  NULL )
    {
        return;
    }

    // Carl: teleport
    static bool oldTeleport = false;
    bool showTeleport = vr_teleport.GetInteger() == 1 && commonVr->VR_USE_MOTION_CONTROLS; // only show the teleport gun cursor if we're teleporting using the gun aim mode
    showTeleport = showTeleport && !AI_DEAD && !gameLocal.inCinematic && !game->IsPDAOpen();

    // check if lasersight should be hidden
    if ( !hands[hand].laserSightActive ||							// Koz allow user to toggle lasersight.
         sightMode == -1 ||
         !weapon->ShowCrosshair() ||
         AI_DEAD ||
         weapon->IsHidden() ||
         weapon->hideOffset != 0 ||						// Koz - turn off lasersight If gun is lowered ( in gui ).
         commonVr->handInGui ||							// turn off lasersight if hand is in gui.
         gameLocal.inCinematic ||
         game->IsPDAOpen() ||							// Koz - turn off laser sight if using pda.
         weapon->GetGrabberState() >= 2 ||	// Koz turn off laser sight if grabber is dragging an entity
         showTeleport || !weapon->GetMuzzlePositionWithHacks(muzzleOrigin, muzzleAxis)) // no lasersight for fists,grenades,soulcube etc

    {
        hideSight = !showTeleport;
    }

    if ( hideSight == true || ( sightMode != 0 && sightMode < 4 ) )
    {
        hands[hand].laserSightRenderEntity.allowSurfaceInViewID = -1;
        if( hands[hand].laserSightHandle == -1 )
        {
            hands[hand].laserSightHandle = gameRenderWorld->AddEntityDef( &hands[hand].laserSightRenderEntity );
        }
        else
        {
            gameRenderWorld->UpdateEntityDef( hands[hand].laserSightHandle, &hands[hand].laserSightRenderEntity );
        }
    }

    if ( !showTeleport && ( hideSight == true || sightMode == 0 ) )
    {
        hands[hand].crosshairEntity.allowSurfaceInViewID = -1;
        if ( hands[hand].crosshairHandle == -1 )
        {
            hands[hand].crosshairHandle = gameRenderWorld->AddEntityDef( &hands[hand].crosshairEntity );
        }
        else
        {
            gameRenderWorld->UpdateEntityDef( hands[hand].crosshairHandle, &hands[hand].crosshairEntity );
        }
    }


    if ( hideSight && !showTeleport ) return;

    if ( showTeleport )
    {
        GetHandOrHeadPositionWithHacks(vr_teleport.GetInteger(), muzzleOrigin, muzzleAxis);
    }

    // calculate the beam origin and length.
    start = muzzleOrigin - muzzleAxis[0] * 2.0f;

    if ( vr_laserSightUseOffset.GetBool() ) start += weapon->laserSightOffset * muzzleAxis;

    end = start + muzzleAxis[0] * beamLength;

    // Koz begin : Keep the lasersight from clipping through everything.

    traceHit = gameLocal.clip.TracePoint( traceResults, start, end, MASK_SHOT_RENDERMODEL, this );
    if ( traceHit )
    {
        beamLength *= traceResults.fraction;
    }


    if ( (vr_weaponSight.GetInteger() == 0 || vr_weaponSight.GetInteger() > 3 )  && !showTeleport && !hideSight ) // using the lasersight

    {
        // only show in the player's view
        // Koz - changed show lasersight shows up in all views/reflections in VR
        hands[hand].laserSightRenderEntity.allowSurfaceInViewID = 0;// entityNumber + 1;
        hands[hand].laserSightRenderEntity.axis.Identity();
        hands[hand].laserSightRenderEntity.origin = start;


        // program the beam model
        idVec3&	target = *reinterpret_cast<idVec3*>( &hands[hand].laserSightRenderEntity.shaderParms[SHADERPARM_BEAM_END_X] );
        target = start + muzzleAxis[0] * beamLength;

        hands[hand].laserSightRenderEntity.shaderParms[SHADERPARM_BEAM_WIDTH] = g_laserSightWidth.GetFloat();

        if ( hands[hand].laserSightHandle == -1 )
        {
            hands[hand].laserSightHandle = gameRenderWorld->AddEntityDef( &hands[hand].laserSightRenderEntity );
        }
        else
        {
            gameRenderWorld->UpdateEntityDef( hands[hand].laserSightHandle, &hands[hand].laserSightRenderEntity );
        }
    }

    if ( vr_teleport.GetInteger() == 1 && commonVr->VR_USE_MOTION_CONTROLS && vr_weaponSight.GetInteger() == 0 )
        vr_weaponSight.SetInteger( 1 );

    sightMode = vr_weaponSight.GetInteger();
    vr_weaponSight.ClearModified();

    if ( !showTeleport && ( sightMode < 1 || hideSight )) return;

    // update the crosshair model
    // set the crosshair skin

    switch ( sightMode )
    {
        case 4:
        case 1:
            hands[hand].crosshairEntity.customSkin = skinCrosshairDot;
            break;

        case 5:
        case 2:
            hands[hand].crosshairEntity.customSkin = skinCrosshairCircleDot;
            break;

        case 6:
        case 3:
            hands[hand].crosshairEntity.customSkin = skinCrosshairCross;
            break;

        default:
            hands[hand].crosshairEntity.customSkin = skinCrosshairDot;

    }

    if ( showTeleport || sightMode > 0 ) hands[hand].crosshairEntity.allowSurfaceInViewID = entityNumber + 1;
    hands[hand].crosshairEntity.axis.Identity();

    static float muzscale = 0.0f ;

    muzscale = 1 + beamLength / 100;
    hands[hand].crosshairEntity.axis = muzzleAxis * muzscale;

    bool aimLadder = false, aimActor = false, aimElevator = false;

    static idAngles surfaceAngle = ang_zero;

    if ( traceHit )
    {
        muzscale = 1 + beamLength / 100;

        if ( showTeleport || vr_weaponSightToSurface.GetBool() )
        {
            aimLadder = traceResults.c.material && ( traceResults.c.material->GetSurfaceFlags() & SURF_LADDER );
            idEntity* aimEntity = gameLocal.GetTraceEntity(traceResults);
            if (aimEntity)
            {
                if (aimEntity->IsType(idActor::Type))
                    aimActor = aimEntity->health > 0;
                else if (aimEntity->IsType(idElevator::Type))
                    aimElevator = true;
                else if (aimEntity->IsType(idStaticEntity::Type) || aimEntity->IsType(idLight::Type))
                {
                    renderEntity_t *rend = aimEntity->GetRenderEntity();
                    if (rend)
                    {
                        idRenderModel *model = rend->hModel;
                        aimElevator = (model && idStr::Cmp(model->Name(), "models/mapobjects/elevators/elevator.lwo") == 0);
                    }
                }
            }

            // fake it till you make it. there must be a better way. Too bad my brain is broken.

            static idAngles muzzleAngle = ang_zero;
            static idAngles diffAngle = ang_zero;
            static float rollDiff = 0.0f;

            surfaceAngle = traceResults.c.normal.ToAngles().Normalize180();
            muzzleAngle = muzzleAxis.ToAngles().Normalize180();

            surfaceAngle.pitch *= -1;
            surfaceAngle.yaw += 180;
            surfaceAngle.Normalize180();

            diffAngle = idAngles( 0, 0, muzzleAngle.yaw - surfaceAngle.yaw ).Normalize180();

            rollDiff = diffAngle.roll * 1 / ( 90 / surfaceAngle.pitch );

            surfaceAngle.roll = muzzleAngle.roll - rollDiff;
            surfaceAngle.Normalize180();

            hands[hand].crosshairEntity.axis = surfaceAngle.ToMat3() * muzscale;
        }
        else
        {
            hands[hand].crosshairEntity.axis = muzzleAxis * muzscale;
        }
    }

    hands[hand].crosshairEntity.origin = start + muzzleAxis[0] * beamLength;


    // Carl: teleport
    if  ( showTeleport )
    {

        // teleportAimPoint is where you are actually aiming. teleportPoint is where AAS has nudged the teleport cursor to (so you can't teleport too close to a wall).
        // teleportAimPointPitch is the pitch of the surface you are aiming at, where 90 is the floor and 0 is the wall
        teleportAimPoint = hands[hand].crosshairEntity.origin;
        teleportAimPointPitch = surfaceAngle.pitch;		// if the elevator is moving up, we don't want to fall through the floor
        if ( aimElevator )
            teleportPoint = teleportAimPoint + idVec3(0, 0, 10);
        // 45 degrees is maximum slope you can walk up
        bool pitchValid = ( teleportAimPointPitch >= 45 && !aimActor ) || aimLadder; // -90 = ceiling, 0 = wall, 90 = floor
        // can always teleport into nearby elevator, otherwise we need to check
        aimValidForTeleport = pitchValid && (( aimElevator && beamLength <= 300 ) || CanReachPosition( teleportAimPoint, teleportPoint ));

        if ( aimValidForTeleport )
        {
            hands[hand].crosshairEntity.origin = teleportPoint;
            hands[hand].crosshairEntity.customSkin = skinCrosshairCircleDot;
        }
        else if ( pitchValid )
        {
            hands[hand].crosshairEntity.origin = teleportPoint;
            hands[hand].crosshairEntity.customSkin = skinCrosshairCross;
        }
        else if ( vr_teleport.GetInteger() == 1 && commonVr->VR_USE_MOTION_CONTROLS )
        {
            hands[hand].crosshairEntity.customSkin = skinCrosshairDot;
        }
        else
        {
            hands[hand].crosshairEntity.customSkin = skinCrosshairCross;
        }
    }
    else
    {
        aimValidForTeleport = false;
    }
    oldTeleport = showTeleport;

    if ( hands[hand].crosshairHandle == -1 )
    {
        hands[hand].crosshairHandle = gameRenderWorld->AddEntityDef( &hands[hand].crosshairEntity );
    }
    else
    {
        gameRenderWorld->UpdateEntityDef( hands[hand].crosshairHandle, &hands[hand].crosshairEntity );
    }

}

/*
=================
idPlayer::GetCurrentWeapon
=================
*/
idStr idPlayer::GetCurrentWeapon()
{
    return hands[ vr_weaponHand.GetInteger() ].GetCurrentWeaponString();
}

/*
=================
idPlayer::GetCurrentWeapon
=================
*/
int idPlayer::GetCurrentWeaponId()
{
    return hands[ vr_weaponHand.GetInteger() ].currentWeapon;
}

/*
========================
idPlayer::GetExpansionType
========================
*/
gameExpansionType_t idPlayer::GetExpansionType() const
{
    const char* expansion = spawnArgs.GetString( "player_expansion", "d3" );
    if( idStr::Icmp( expansion, "d3" ) == 0 )
    {
        return GAME_BASE;
    }
    if( idStr::Icmp( expansion, "d3xp" ) == 0 )
    {
        return GAME_D3XP;
    }
    if( idStr::Icmp( expansion, "d3le" ) == 0 )
    {
        return GAME_D3LE;
    }
    return GAME_UNKNOWN;
}

/*
==============
idPlayer::CalculateViewWeaponPos

Calculate the bobbing position of the view weapon
==============
*/

void idPlayer::CalculateViewWeaponPos( int hand, idVec3& origin, idMat3& axis )
{
	if ( game->isVR && pVRClientInfo )
	{
		CalculateViewWeaponPosVR( hand, origin, axis );
		return;
	}
	/*

	float		scale;
	float		fracsin;
	int			delta;

	// CalculateRenderView must have been called first
	const idVec3 &viewOrigin = firstPersonViewOrigin;
	const idMat3 &viewAxis = firstPersonViewAxis;

	if (pVRClientInfo &&
			GetCurrentWeaponId() != weapon_pda)
    {
		float *pAngles = pointer ? pVRClientInfo->weaponangles_unadjusted : pVRClientInfo->weaponangles;
		angles.pitch = pAngles[PITCH];
		angles.yaw = viewAngles.yaw +
					 (pAngles[YAW] - pVRClientInfo->hmdorientation[YAW]);
		angles.roll = pAngles[ROLL];

		axis = angles.ToMat3();

        idVec3	gunpos( -pVRClientInfo->current_weaponoffset[2],
                          -pVRClientInfo->current_weaponoffset[0],
                          pVRClientInfo->current_weaponoffset[1]);

        idAngles a(0, viewAngles.yaw - pVRClientInfo->hmdorientation[YAW], 0);
        gunpos *= a.ToMat3();
        gunpos *= ((100.0f / 2.54f) * vr_scale.GetFloat());

        if (GetCurrentWeaponId( == WEAPON_FLASHLIGHT) // Flashlight adjustment
        {
            idVec3	gunOfs( -14, 9, 24 );
            origin = viewOrigin + gunpos + (gunOfs * axis);
        }
        else
        {
            idVec3	gunOfs( g_gun_x.GetFloat(), g_gun_y.GetFloat(), g_gun_z.GetFloat() );
            origin = viewOrigin + gunpos + (gunOfs * axis);
        }

        return;
    }

	// these cvars are just for hand tweaking before moving a value to the weapon def
	idVec3	gunpos( g_gun_x.GetFloat(), g_gun_y.GetFloat(), g_gun_z.GetFloat() );

	// as the player changes direction, the gun will take a small lag
	idVec3	gunOfs = GunAcceleratingOffset();
	origin = viewOrigin + ( gunpos + gunOfs ) * viewAxis;

	///HACK
	if (GetCurrentWeaponId( == weapon_pda)
    {
        //idVec3	pdaOffs( 30, -7, -14 );

        //Put it behind us
        idVec3	pdaOffs( -30, -7, -14 );
	    origin += pdaOffs * viewAxis;
    }

	// on odd legs, invert some angles
	if ( bobCycle & 128 ) {
		scale = -xyspeed;
	} else {
		scale = xyspeed;
	}

	// gun angles from bobbing
	angles.roll		= scale * bobfracsin * 0.005f;
	angles.yaw		= scale * bobfracsin * 0.01f;
	angles.pitch	= xyspeed * bobfracsin * 0.005f;

	// gun angles from turning
	if ( gameLocal.isMultiplayer ) {
		idAngles offset = GunTurningOffset();
		offset *= g_mpWeaponAngleScale.GetFloat();
		angles += offset;
	} else {
		angles += GunTurningOffset();
	}

	idVec3 gravity = physicsObj.GetGravityNormal();

	// drop the weapon when landing after a jump / fall
	delta = gameLocal.time - landTime;
	if ( delta < LAND_DEFLECT_TIME ) {
		origin -= gravity * ( landChange*0.25f * delta / LAND_DEFLECT_TIME );
	} else if ( delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME ) {
		origin -= gravity * ( landChange*0.25f * (LAND_DEFLECT_TIME + LAND_RETURN_TIME - delta) / LAND_RETURN_TIME );
	}

	// speed sensitive idle drift
	scale = xyspeed + 40.0f;
	fracsin = scale * sin( MS2SEC( gameLocal.time ) ) * 0.01f;
	angles.roll		+= fracsin;
	angles.yaw		+= fracsin;
	angles.pitch	+= fracsin;

	axis = angles.ToMat3() * viewAxis;*/
}

// Carl: TODO Dual wielding
void idPlayer::CalculateViewWeaponPosVR( int hand, idVec3 &origin, idMat3 &axis )
{
    weapon_t currentWeaponEnum = WEAPON_NONE;
    idVec3	gunOrigin;
    idVec3	originOffset = vec3_zero;
    idAngles hmdAngles;
    idVec3	headPositionDelta;
    idVec3	bodyPositionDelta;
    idVec3	absolutePosition;
    idQuat	weaponPitch;

    currentWeaponEnum = hands[ hand ].weapon->IdentifyWeapon();
    idWeapon* weapon = hands[ hand ].weapon;
    int currentWeaponIndex = hands[ hand ].currentWeapon;

    if ( hands[hand].holdingFlashlight() || weapon->isPlayerFlashlight ) return;

    gunOrigin = GetEyePosition();

    gunOrigin += commonVr->leanOffset;

    // direction the player body is facing.
    idMat3		bodyAxis = idAngles( 0.0, viewAngles.yaw, 0.0f ).ToMat3();
    idVec3		gravity = physicsObj.GetGravityNormal();


    if ( currentWeaponEnum != WEAPON_PDA )
    {
        hands[hand].PDAfixed = false; // release the PDA if weapon has been switched.
    }

    if ( !commonVr->VR_USE_MOTION_CONTROLS || ( vr_PDAfixLocation.GetBool() && currentWeaponEnum == WEAPON_PDA ) ) // non-motion control & fixed pda positioning.
    {

        idMat3 pdaPitch = idAngles( vr_pdaPitch.GetFloat(), 0.0f, 0.0f ).ToMat3();

        axis = bodyAxis;
        origin = gunOrigin;

        if ( currentWeaponEnum == WEAPON_PDA ) //&& weapon->GetStatus() == WP_READY )
        {

            if ( hands[ hand ].PDAfixed )
            { // pda has already been locked in space, use stored values

                origin = PDAorigin;
                axis = PDAaxis;


                //if the player has moved ( or been moved, if on an elevator or lift )
                //move the PDA to maintain a constant relative position
                idVec3 curPlayerPos = physicsObj.GetOrigin();
                origin -= ( hands[hand].playerPdaPos - curPlayerPos ) + commonVr->fixedPDAMoveDelta;
                //common->Printf( "playerPDA x %f y %f  currentPlay x %f y %f  fixMoveDel x %f y %f\n", playerPdaPos.x, playerPdaPos.y, curPlayerPos.x, curPlayerPos.y, commonVr->fixedPDAMoveDelta.x, commonVr->fixedPDAMoveDelta.y );

                SetHandIKPos( hand, origin, axis, pdaPitch.ToQuat() , false );
                originOffset = weapon->weaponHandDefaultPos[hand];
                origin -= originOffset * axis;
                origin += handWeaponAttacherToDefaultOffset[hand][currentWeaponIndex] * axis; // add the attacher offsets
            }
            else
            { // fix the PDA in space, set flag and store position

                hands[ hand ].playerPdaPos = physicsObj.GetOrigin();

                origin = gunOrigin;
                origin += vr_pdaPosX.GetFloat() * bodyAxis[0] + vr_pdaPosY.GetFloat() *  bodyAxis[1] + vr_pdaPosZ.GetFloat() * bodyAxis[2];
                PDAorigin = origin;
                PDAaxis = pdaPitch * bodyAxis;
                axis = PDAaxis;
                hands[ hand ].PDAfixed = true;

                SetHandIKPos( hand, origin, axis, pdaPitch.ToQuat(), false );
                originOffset = weapon->weaponHandDefaultPos[hand];
                origin -= originOffset * axis;
                origin += handWeaponAttacherToDefaultOffset[hand][currentWeaponIndex] * axis; // add the attacher offsets


                // the non weapon hand was set to the PDA fixed location, now fall thru and normal motion controls will place the pointer hand location

            }
        }
    }


	// motion control weapon positioning.
	//-----------------------------------

	idVec3 weapOrigin = vec3_zero;
	idMat3 weapAxis = mat3_identity;

	// idVec3 fixPosVec = idVec3( -17.0f, 6.0f, 0.0f );
	// idVec3 fixPos = fixPosVec;
	// idQuat fixRot = idAngles( 40.0f, -40.0f, 20.0f ).ToQuat();
	idVec3 attacherToDefault = vec3_zero;
	// idMat3 rot180 = idAngles( 0.0f, 180.0f, 0.0f ).ToMat3();

	if ( !hands[ hand ].PDAfixed && currentWeaponEnum == WEAPON_PDA )
	{
		// do the non-PDA hand first (the hand with the pointy finger)
		int fingerHand = 1 - hand;

		attacherToDefault = handWeaponAttacherToDefaultOffset[fingerHand][currentWeaponIndex];
		originOffset = weapon->weaponHandDefaultPos[fingerHand];
		commonVr->MotionControlGetHand( fingerHand, hands[fingerHand].motionPosition, hands[fingerHand].motionRotation );

		weaponPitch = idAngles( vr_motionWeaponPitchAdj.GetFloat(), 0.f, 0.0f ).ToQuat();
		hands[fingerHand].motionRotation = weaponPitch * hands[fingerHand].motionRotation;

		GetViewPos( weapOrigin, weapAxis );

		weapOrigin += commonVr->leanOffset;
		//commonVr->HMDGetOrientation( hmdAngles, headPositionDelta, bodyPositionDelta, absolutePosition, false );// gameLocal.inCinematic );

		hmdAngles = commonVr->poseHmdAngles;
		headPositionDelta = commonVr->poseHmdHeadPositionDelta;
		bodyPositionDelta = commonVr->poseHmdBodyPositionDelta;
		absolutePosition = commonVr->poseHmdAbsolutePosition;



		weapAxis = idAngles( 0.0, weapAxis.ToAngles().yaw - commonVr->bodyYawOffset, 0.0f ).ToMat3();

		weapOrigin += weapAxis[0] * headPositionDelta.x + weapAxis[1] * headPositionDelta.y + weapAxis[2] * headPositionDelta.z;

		weapOrigin += hands[fingerHand].motionPosition * weapAxis;
		weapAxis = hands[fingerHand].motionRotation.ToMat3() * weapAxis;

		//weapon->CalculateHideRise( weapOrigin, weapAxis );

		idAngles motRot = hands[fingerHand].motionRotation.ToAngles();
		motRot.yaw -= commonVr->bodyYawOffset;
		motRot.Normalize180();
		hands[fingerHand].motionRotation = motRot.ToQuat();

		SetHandIKPos( fingerHand, weapOrigin, weapAxis, hands[fingerHand].motionRotation, false );

		// now switch hands and fall through again.,
	}
	// NOT the PDA

	attacherToDefault = handWeaponAttacherToDefaultOffset[hand][currentWeaponIndex];
	originOffset = weapon->weaponHandDefaultPos[hand];

	commonVr->MotionControlGetHand( hand, hands[ hand ].motionPosition, hands[ hand ].motionRotation );

	if (!commonVr->GetWeaponStabilised())
	{
		weaponPitch = idAngles( vr_motionWeaponPitchAdj.GetFloat(), 0.0f, 0.0f ).ToQuat();
		hands[hand].motionRotation = weaponPitch * hands[hand].motionRotation;
	}

	GetViewPos( weapOrigin, weapAxis );

	weapOrigin += commonVr->leanOffset;

	//commonVr->HMDGetOrientation( hmdAngles, headPositionDelta, bodyPositionDelta, absolutePosition, false );// gameLocal.inCinematic );

	hmdAngles = commonVr->poseHmdAngles;
	headPositionDelta = commonVr->poseHmdHeadPositionDelta;
	bodyPositionDelta = commonVr->poseHmdBodyPositionDelta;
	absolutePosition = commonVr->poseHmdAbsolutePosition;


	weapAxis = idAngles( 0.0f, weapAxis.ToAngles().yaw - commonVr->bodyYawOffset, 0.0f ).ToMat3();

	weapOrigin += weapAxis[0] * headPositionDelta.x + weapAxis[1] * headPositionDelta.y + weapAxis[2] * headPositionDelta.z;

	weapOrigin += hands[ hand ].motionPosition * weapAxis;

	if ( currentWeaponEnum != WEAPON_ARTIFACT && currentWeaponEnum != WEAPON_SOULCUBE )
	{
		weapAxis = hands[ hand ].motionRotation.ToMat3() * weapAxis;
	}
	else
	{
		weapAxis = idAngles( 0.0f ,viewAngles.yaw , 0.0f).ToMat3();
	}

	//DebugCross( weapOrigin, weapAxis, colorYellow );

	if ( currentWeaponEnum != WEAPON_PDA )
	{
		hands[hand].TrackWeaponDirection( weapOrigin );
		//GB why do both hands here
		//hands[1 - hand].TrackWeaponDirection( weapOrigin );
		weapon->CalculateHideRise( weapOrigin, weapAxis );
		//check for melee hit?
	}
	else if( !hands[ hand ].PDAfixed )
	{
		// Koz FIXME hack hack hack this is getting so ungodly ugly.
		// Lovely.  I forgot to correct the origin for the PDA model when I switched
		// to always showing the body, so now when holding the PDA it doesn't align with your controller.
		// will fix the assets later but for now hack this correction in.

		/* GB Debugger Change Values
		 * float _vr_pdaPosX = 0.0;
		float _vr_pdaPosY = 0.0;
		float _vr_pdaPosZ = 0.0;
		if(_vr_pdaPosX != 0.0f && _vr_pdaPosX != vr_pdaPosX.GetFloat())
			cvarSystem->SetCVarFloat("vr_pdaPosX", _vr_pdaPosX);
		if(_vr_pdaPosY != 0.0f && _vr_pdaPosY != vr_pdaPosY.GetFloat())
			cvarSystem->SetCVarFloat("vr_pdaPosY", _vr_pdaPosY);
		if(_vr_pdaPosZ != 0.0f && _vr_pdaPosZ != vr_pdaPosZ.GetFloat())
			cvarSystem->SetCVarFloat("vr_pdaPosZ", _vr_pdaPosZ);
		*/
		const idVec3 pdaHackOrigin[2] { idVec3( vr_pdaPosX.GetFloat(), vr_pdaPosY.GetFloat(), vr_pdaPosZ.GetFloat() ), idVec3( vr_pdaPosX.GetFloat(), -vr_pdaPosY.GetFloat(), vr_pdaPosZ.GetFloat() ) };
		weapOrigin += pdaHackOrigin[hand] * weapAxis;
	}

	idAngles motRot = hands[ hand ].motionRotation.ToAngles();
	motRot.yaw -= commonVr->bodyYawOffset;
	motRot.Normalize180();
	hands[ hand ].motionRotation = motRot.ToQuat();

	SetHandIKPos( hand, weapOrigin, weapAxis, hands[ hand ].motionRotation, false );

	if ( hands[ hand ].PDAfixed ) return;

	if ( currentWeaponEnum == WEAPON_PDA )
	{

		PDAaxis = weapAxis;
		PDAorigin = weapOrigin;
		if ( hands[ hand ].wasPDA == false )
		{
			SetFlashHandPose(); // Call set flashlight hand pose script function
			SetWeaponHandPose();
			hands[ hand ].wasPDA = true;
		}
	}
	else
	{
		hands[hand].wasPDA = false;
	}

	axis = weapAxis;
	origin = weapOrigin;

	origin -= originOffset * weapAxis;
	origin += attacherToDefault  * weapAxis; // handWeaponAttacherToDefaultOffset[hand][currentWeaponIndex] * weapAxis; // add the attacher offsets

}

bool idPlayer::CanDualWield( int num ) const
{
    if( num == weapon_flashlight || num == weapon_flashlight_new || num == weapon_fists || num == -1)
        return true;
    return false;

    /*vr_dualwield_t d = (vr_dualwield_t)vr_dualWield.GetInteger();
    if( d == VR_DUALWIELD_YES || num == -1 )
        return true;
    if( d == VR_DUALWIELD_NOT_EVEN_FISTS )
        return false;
    if( num == weapon_fists )
        return d != VR_DUALWIELD_NOT_EVEN_FISTS;
    if( num == weapon_flashlight || num == weapon_flashlight_new )
        return d == VR_DUALWIELD_ONLY_FLASHLIGHT || d == VR_DUALWIELD_ONLY_GRENADES_FLASHLIGHT || d == VR_DUALWIELD_ONLY_PISTOLS_FLASHLIGHT || d == VR_DUALWIELD_ONLY_PISTOLS_GRENADES_FLASHLIGHT;
    if( num == weapon_pistol )
        return d == VR_DUALWIELD_ONLY_PISTOLS || d == VR_DUALWIELD_ONLY_PISTOLS_FLASHLIGHT || d == VR_DUALWIELD_ONLY_PISTOLS_GRENADES_FLASHLIGHT;
    if( num == weapon_handgrenade )
        return d == VR_DUALWIELD_ONLY_GRENADES || d == VR_DUALWIELD_ONLY_GRENADES_FLASHLIGHT || d == VR_DUALWIELD_ONLY_PISTOLS_GRENADES_FLASHLIGHT;
    return false;*/
}

/*
===============
idPlayer::OffsetThirdPersonView
===============
*/
void idPlayer::OffsetThirdPersonView( float angle, float range, float height, bool clip ) {
	idVec3			view;
	idVec3			focusAngles;
	trace_t			trace;
	idVec3			focusPoint;
	float			focusDist;
	float			forwardScale, sideScale;
	idVec3			origin;
	idAngles		angles;
	idMat3			axis;
	idBounds		bounds;

	angles = viewAngles;
	GetViewPos( origin, axis );

	if ( angle ) {
		angles.pitch = 0.0f;
	}

	if ( angles.pitch > 45.0f ) {
		angles.pitch = 45.0f;		// don't go too far overhead
	}

	focusPoint = origin + angles.ToForward() * THIRD_PERSON_FOCUS_DISTANCE;
	focusPoint.z += height;
	view = origin;
	view.z += 8 + height;

	angles.pitch *= 0.5f;
	renderView->viewaxis = angles.ToMat3() * physicsObj.GetGravityAxis();

	idMath::SinCos( DEG2RAD( angle ), sideScale, forwardScale );
	view -= range * forwardScale * renderView->viewaxis[ 0 ];
	view += range * sideScale * renderView->viewaxis[ 1 ];

	if ( clip ) {
		// trace a ray from the origin to the viewpoint to make sure the view isn't
		// in a solid block.  Use an 8 by 8 block to prevent the view from near clipping anything
		bounds = idBounds( idVec3( -4, -4, -4 ), idVec3( 4, 4, 4 ) );
		gameLocal.clip.TraceBounds( trace, origin, view, bounds, MASK_SOLID, this );
		if ( trace.fraction != 1.0f ) {
			view = trace.endpos;
			view.z += ( 1.0f - trace.fraction ) * 32.0f;

			// try another trace to this position, because a tunnel may have the ceiling
			// close enough that this is poking out
			gameLocal.clip.TraceBounds( trace, origin, view, bounds, MASK_SOLID, this );
			view = trace.endpos;
		}
	}

	// select pitch to look at focus point from vieword
	focusPoint -= view;
	focusDist = idMath::Sqrt( focusPoint[0] * focusPoint[0] + focusPoint[1] * focusPoint[1] );
	if ( focusDist < 1.0f ) {
		focusDist = 1.0f;	// should never happen
	}

	angles.pitch = - RAD2DEG( atan2( focusPoint.z, focusDist ) );
	angles.yaw -= angle;

	renderView->vieworg = view;
	renderView->viewaxis = angles.ToMat3() * physicsObj.GetGravityAxis();
	renderView->viewID = 0;
}

// Carl:
#define TP_HAND_DISABLED 0
#define TP_HAND_GUNSIGHT 1
#define TP_HAND_RIGHT 2
#define TP_HAND_LEFT 3
#define TP_HAND_HEAD 4
bool idPlayer::GetHandOrHeadPositionWithHacks( int hand, idVec3& origin, idMat3& axis )
{
    int weaponHand;
    if( hand == TP_HAND_RIGHT )
        weaponHand = HAND_RIGHT;
    else if( hand == TP_HAND_LEFT )
        weaponHand = HAND_LEFT;
    else
        weaponHand = vr_weaponHand.GetInteger();

    idWeapon* weapon = hands[ weaponHand ].weapon;
    // In Multiplayer, weapon might not have been spawned yet.
    if (weapon == NULL || hand == TP_HAND_HEAD)
    {
        origin = commonVr->lastViewOrigin; // Koz fixme set the origin and axis to the players view
        axis = commonVr->lastViewAxis;
        return false;
    }
    weapon_t currentWeap = weapon->IdentifyWeapon();
    // Carl: weapon hand
    if ( hand == 1 || hand == TP_HAND_RIGHT + vr_weaponHand.GetInteger() )
    {
        switch ( currentWeap )
        {
            case WEAPON_NONE:
            case WEAPON_FISTS:
            case WEAPON_SOULCUBE:
            case WEAPON_PDA:
            case WEAPON_HANDGRENADE:
                origin = weapon->viewWeaponOrigin; // Koz fixme set the origin and axis to the weapon default
                axis = weapon->viewWeaponAxis;
                return false;
                break;

            default:
                return weapon->GetMuzzlePositionWithHacks( origin, axis );
                break;
        }
    }
        // Carl: flashlight hand
    else if ( commonVr->GetCurrentFlashlightMode() == FLASHLIGHT_HAND && weaponEnabled && !spectating && !gameLocal.world->spawnArgs.GetBool("no_Weapons") && !game->IsPDAOpen() && !commonVr->PDAforcetoggle && hands[0].currentWeapon != weapon_pda && hands[1].currentWeapon != weapon_pda )
    {
        weapon_t currentWeapon = flashlight->IdentifyWeapon();
        CalculateViewFlashlightPos( origin, axis, flashlightOffsets[hands[vr_weaponHand.GetInteger()].currentWeapon] );

        return false;
    }
        // Carl: todo empty non-weapon hand (currently using head instead)
    else
    {
        origin = commonVr->lastViewOrigin; // Koz fixme set the origin and axis to the players view
        axis = commonVr->lastViewAxis;
        return false;
    }
}

/*
==============
Koz idPlayer::CalculateViewFlashlightPos
Calculate the flashlight orientation
==============
*/
void idPlayer::CalculateViewFlashlightPos( idVec3 &origin, idMat3 &axis, idVec3 flashlightOffset )
{
    static idVec3 viewOrigin = vec3_zero;
    static idMat3 viewAxis = mat3_identity;
    static bool setLeftHand = false;
    static weapon_t curWeap = WEAPON_NONE;
    static idMat3 swapMat = idAngles( 180.0f, 0.0f, 180.0f ).ToMat3();

    origin = GetEyePosition();

    origin += commonVr->leanOffset;

    axis = idAngles( 0.0, viewAngles.yaw, 0.0f ).ToMat3();
    axis = idAngles( 0.0, viewAngles.yaw - commonVr->bodyYawOffset, 0.0f ).ToMat3();

    int flashlightMode = commonVr->GetCurrentFlashlightMode();
    if (commonVr->GetWeaponStabilised() && flashlightMode == FLASHLIGHT_HAND )
	{
		//GB Changed as previous wasn't persistent across functions
		commonVr->restoreFlashlightMode = true;
		vr_flashlightMode.SetInteger( FLASHLIGHT_GUN );
		vr_flashlightMode.SetModified();
	}
    else if (!commonVr->GetWeaponStabilised() && commonVr->restoreFlashlightMode)
	{
		commonVr->restoreFlashlightMode = false;
    	vr_flashlightMode.SetInteger( FLASHLIGHT_HAND );
		vr_flashlightMode.SetModified();
	}

    setLeftHand = false;
    //move the flashlight to alternate location for items with no mount

    if ( spectating || !weaponEnabled || gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) ) flashlightMode = FLASHLIGHT_BODY;

    if ( flashlightMode == FLASHLIGHT_HAND )
    {
        if ( game->IsPDAOpen() || commonVr->PDAforcetoggle || hands[ 0 ].currentWeapon == weapon_pda || hands[ 1 ].currentWeapon == weapon_pda || !commonVr->VR_USE_MOTION_CONTROLS  || (commonVr->handInGui && flashlightMode == FLASHLIGHT_GUN) )
        {
            flashlightMode = FLASHLIGHT_HEAD;
        }
    }

    idWeapon* weaponWithFlashlightMounted = NULL;

    if( flashlightMode == FLASHLIGHT_GUN )
    {
        weaponWithFlashlightMounted = GetWeaponWithMountedFlashlight();
		//GB ALTERNATIVE (think I have fixed elsewhere)
        flashlightOffset = flashlightOffsets[int( weaponWithFlashlightMounted->IdentifyWeapon() )];

        if( !weaponWithFlashlightMounted || !weaponWithFlashlightMounted->GetMuzzlePositionWithHacks( origin, axis ) || commonVr->handInGui )
        {
            idAngles flashlightAx = axis.ToAngles();
            flashlightMode = FLASHLIGHT_HEAD;
            if( game->isVR && pVRClientInfo ) axis = idAngles( flashlightAx.pitch, flashlightAx.yaw - commonVr->bodyYawOffset, flashlightAx.roll ).ToMat3();

        }
    }
    else if( flashlightMode == FLASHLIGHT_PISTOL )
    {
        if( hands[0].currentWeapon == weapon_pistol && !hands[0].isTheDuplicate )
            weaponWithFlashlightMounted = hands[0].weapon.GetEntity();
        else if( hands[1].currentWeapon == weapon_pistol && !hands[1].isTheDuplicate )
            weaponWithFlashlightMounted = hands[1].weapon.GetEntity();
        else
            weaponWithFlashlightMounted = NULL;

        if( !weaponWithFlashlightMounted || !weaponWithFlashlightMounted->GetMuzzlePositionWithHacks( origin, axis ) || commonVr->handInGui )
        {
            idAngles flashlightAx = axis.ToAngles();
            flashlightMode = FLASHLIGHT_INVENTORY;
            if( game->isVR && pVRClientInfo ) axis = idAngles( flashlightAx.pitch, flashlightAx.yaw - commonVr->bodyYawOffset, flashlightAx.roll ).ToMat3();
        }
        else
            flashlightMode = FLASHLIGHT_GUN;
    }

    int oldFlashlightPosition = commonVr->currentFlashlightPosition;
    commonVr->currentFlashlightPosition = flashlightMode;

    switch ( flashlightMode )
    {

        case FLASHLIGHT_GUN:
            // move the flashlight to the weapon

            /* was for adjusting
            flashlightOffset.x += ftx.GetFloat();
            flashlightOffset.y += fty.GetFloat();
            flashlightOffset.z += ftz.GetFloat();
            */

            origin += flashlightOffset.x * axis[1] + flashlightOffset.y * axis[0] + flashlightOffset.z * axis[2];

            curWeap = weaponWithFlashlightMounted->IdentifyWeapon();
			if ( curWeap == WEAPON_SHOTGUN_DOUBLE || curWeap == WEAPON_ROCKETLAUNCHER )
            {
                //hack was already present in the code to fix borked alignments for these weapons,
                //we need to put them back
                //std::swap( axis[0], axis[2] );
                axis = -1 * axis;
                //axis = idAngles( 180.0f, 0.0f, 180.0f ).ToMat3() * axis;
                axis = swapMat * axis;
            }

            flashlight->GetRenderEntity()->allowSurfaceInViewID = 0;
            flashlight->GetRenderEntity()->suppressShadowInViewID = 0;
            // if we just got the flashlight/weapon out, start with the flashlight turned on
            if( oldFlashlightPosition == FLASHLIGHT_INVENTORY || oldFlashlightPosition == FLASHLIGHT_NONE )
                FlashlightOn();
            setLeftHand = true;
            break;

        case FLASHLIGHT_HEAD:
            // Flashlight on helmet
            origin = commonVr->lastViewOrigin;
            axis = commonVr->lastViewAxis;

            origin += vr_flashlightHelmetPosY.GetFloat() * axis[1] + vr_flashlightHelmetPosZ.GetFloat() * axis[0] + vr_flashlightHelmetPosX.GetFloat() * axis[2];

            flashlight->GetRenderEntity()->allowSurfaceInViewID = -1;
            flashlight->GetRenderEntity()->suppressShadowInViewID = entityNumber + 1;

            if ( curWeap == WEAPON_PDA && commonVr->VR_USE_MOTION_CONTROLS ) return;

            setLeftHand = true;
            break;

        case FLASHLIGHT_INVENTORY:
        case FLASHLIGHT_NONE:
            // don't draw the flashlight or its shadow
            flashlight->GetRenderEntity()->allowSurfaceInViewID = -1;
            flashlight->GetRenderEntity()->suppressShadowInViewID = entityNumber + 1;
            // if we just put the flashlight/weapon away, turn it off
            if( oldFlashlightPosition != FLASHLIGHT_INVENTORY && oldFlashlightPosition != FLASHLIGHT_NONE )
                FlashlightOff();
            setLeftHand = true;
            break;

        case FLASHLIGHT_BODY:
        default: // this is the original body mount code.
        {
            idWeapon* weapon = GetMainWeapon();
            origin = weapon->playerViewOrigin;
            axis = weapon->playerViewAxis;
            float fraccos = cos( (gameLocal.framenum & 255) / 127.0f * idMath::PI );
            static unsigned int divisor = 32;
            unsigned int val = (gameLocal.framenum + gameLocal.framenum / divisor) & 255;
            float fraccos2 = cos( val / 127.0f * idMath::PI );
            static idVec3 baseAdjustPos = idVec3( -8.0f, -20.0f, -10.0f ); // rt, fwd, up
            //static idVec3 baseAdjustPos = idVec3( 0, 0, 0 ); // rt, fwd, up

            if ( game->isVR && pVRClientInfo )
            {
                baseAdjustPos.x = vr_flashlightBodyPosX.GetFloat();
                baseAdjustPos.y = vr_flashlightBodyPosY.GetFloat();
                baseAdjustPos.z = vr_flashlightBodyPosZ.GetFloat();
            }

            static float pscale = 0.5f;
            static float yscale = 0.125f;
            idVec3 adjustPos = baseAdjustPos;// + ( idVec3( fraccos, 0.0f, fraccos2 ) * scale );
            origin += adjustPos.x * axis[1] + adjustPos.y * axis[0] + adjustPos.z * axis[2];
            //viewWeaponOrigin += owner->viewBob;
            //	static idAngles baseAdjustAng = idAngles( 88.0f, 10.0f, 0.0f ); //
            static idAngles baseAdjustAng = idAngles( 0.0f,10.0f, 0.0f ); //
            idAngles adjustAng = baseAdjustAng + idAngles( fraccos * pscale, fraccos2 * yscale, 0.0f );
            // adjustAng += owner->GetViewBobAngles();
            axis = adjustAng.ToMat3() * axis;
            flashlight->GetRenderEntity()->allowSurfaceInViewID = -1;
            // Carl: This fixes the flashlight shadow in Mars City 1.
            // I could check if ( spectating || !weaponEnabled || gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) )
            // but I don't think an armor-mounted light should ever cast a flashlight-shaped shadow
            flashlight->GetRenderEntity()->suppressShadowInViewID = entityNumber + 1;
            setLeftHand = true;
        }
    }


    // Koz fixme this is where we set the left hand position. Yes it's a stupid place to do it move later
    //GBFIX - For some reason it always thinks the left hand has PDA open
    //if ( game->IsPDAOpen() || commonVr->PDAforcetoggle || hands[0].currentWeapon == weapon_pda || hands[1].currentWeapon == weapon_pda ) return; //dont dont anything with the left hand if motion controlling the PDA, only if fixed.

    if ( commonVr->VR_USE_MOTION_CONTROLS ) // && ( !game->IsPDAOpen() || commonVr->PDAforcetoggle || currentWeapon == weapon_pda ) )
    {
        static idVec3 motionPosition = vec3_zero;
        static idQuat motionRotation;
        static idVec3 originOffset = vec3_zero;
        static int currentHand = 0;
        static idAngles hmdAngles;
        static idVec3 headPositionDelta;
        static idVec3 bodyPositionDelta;
        static idVec3 absolutePosition;
        static idQuat flashlightPitch;
        static bool isFlashlight = false;

        currentHand = 1 - vr_weaponHand.GetInteger();
        originOffset = flashlight->weaponHandDefaultPos[currentHand];
        flashlightPitch = idAngles( vr_motionFlashPitchAdj.GetFloat(), 0.f, 0.0f ).ToQuat();
        isFlashlight = true;

        commonVr->MotionControlGetHand( currentHand, motionPosition, motionRotation );

        motionRotation = flashlightPitch * motionRotation;

        GetViewPos( viewOrigin, viewAxis );  //GetEyePosition();

        viewOrigin += commonVr->leanOffset;

        //commonVr->HMDGetOrientation( hmdAngles, headPositionDelta, bodyPositionDelta, absolutePosition, false );

        hmdAngles = commonVr->poseHmdAngles;
        headPositionDelta = commonVr->poseHmdHeadPositionDelta;
        bodyPositionDelta = commonVr->poseHmdBodyPositionDelta;
        absolutePosition = commonVr->poseHmdAbsolutePosition;

        viewAxis = idAngles( 0.0, viewAxis.ToAngles().yaw - commonVr->bodyYawOffset, 0.0f ).ToMat3();
        viewOrigin += viewAxis[0] * headPositionDelta.x + viewAxis[1] * headPositionDelta.y + viewAxis[2] * headPositionDelta.z;

        viewOrigin += motionPosition * viewAxis;
        viewAxis = motionRotation.ToMat3() * viewAxis;

        idAngles motRot = motionRotation.ToAngles();
        motRot.yaw -= commonVr->bodyYawOffset;
        motRot.Normalize180();
        motionRotation = motRot.ToQuat();


        // Koz fixme:
        // Koz hack , the alignment isn't quite right, so do a quick hack here so the hand and flashlight align
        // better with the controllers.
        // need to really fix this right, the whole body/viewweapon pose attacher code is a complete trainwreck now.

        const idVec3 flashlightPosHack[2] = { idVec3( 0.0f, -1.0f, 0.5f ), idVec3( 0.0f, 0.85f, 0.5f ) };
        viewOrigin += flashlightPosHack[currentHand] * viewAxis;

        //DebugCross( viewOrigin, viewAxis, colorYellow );
        SetHandIKPos( currentHand , viewOrigin, viewAxis, motionRotation, isFlashlight );

        if ( flashlightMode == FLASHLIGHT_HAND   )
        {
            origin = viewOrigin;
            origin -= originOffset * viewAxis;

            int wepn = (hands[0].weapon->IdentifyWeapon() == WEAPON_PDA || hands[ 1 ].weapon->IdentifyWeapon() == WEAPON_PDA ) ? weapon_pda : weapon_flashlight;
            origin += handWeaponAttacherToDefaultOffset[currentHand][wepn/*weapon_flashlight*/] * viewAxis;
            axis = viewAxis;

            //DebugCross( origin, axis, colorOrange );

            flashlight->GetRenderEntity()->allowSurfaceInViewID = 0;
            flashlight->GetRenderEntity()->suppressShadowInViewID = 0;
        }
    }

    if ( gameLocal.inCinematic )
    {
        flashlight->GetRenderEntity()->allowSurfaceInViewID = -1;
        flashlight->GetRenderEntity()->suppressShadowInViewID = entityNumber + 1;
    }
}

/*
===============
idPlayer::GetHarvestWeapon
Carl: Dual wielding, get the specific weapon used to harvest souls (Soul Cube or Artifact)
Returns the required one if you're holding it, or the other one, or the main weapon
===============
*/
idWeapon* idPlayer::GetHarvestWeapon( idStr requiredWeapons )
{
    // Carl: TODO dual wielding
    if( hands[ 0 ].weapon && ( hands[ 0 ].weapon->IdentifyWeapon() == WEAPON_SOULCUBE || hands[ 0 ].weapon->IdentifyWeapon() == WEAPON_ARTIFACT ) )
        return hands[ 0 ].weapon;
    if( hands[ 1 ].weapon && ( hands[ 1 ].weapon->IdentifyWeapon() == WEAPON_SOULCUBE || hands[ 1 ].weapon->IdentifyWeapon() == WEAPON_ARTIFACT ) )
        return hands[ 1 ].weapon;
    return GetMainWeapon();
}

/*
===============
idPlayer::GetMainWeapon
Carl: Dual wielding, when the code needs just one weapon, guess which one is the "main" one
===============
*/
idWeapon* idPlayer::GetMainWeapon()
{
    // Carl: TODO dual wielding
    return hands[ GetBestWeaponHand() ].weapon;
}

/*
===============
idPlayer::GetEyePosition
===============
*/
idVec3 idPlayer::GetEyePosition( void ) const {
	idVec3 org;

	// use the smoothed origin if spectating another player in multiplayer
	if ( gameLocal.isClient && entityNumber != gameLocal.localClientNum ) {
		org = smoothedOrigin;
	} else {
		org = GetPhysics()->GetOrigin();
	}
    return org + (GetPhysics()->GetGravityNormal() * -eyeOffset.z) + idVec3(0, 0, commonVr->headHeightDiff + vr_heightoffset.GetFloat());

	/*
	if (pVRClientInfo)
    {
		float eyeHeight = 0;
		float vrEyeHeight = (-(pVRClientInfo->hmdposition[1] +  vr_heightoffset.GetFloat()) * ((100.0f / 2.54f) * vr_scale.GetFloat()));

		//Add special handling for physical crouching at some point
		if (physicsObj.IsCrouching() && PHYSICAL_CROUCH) {
			eyeHeight = vrEyeHeight;
		}
		else
		{
			eyeHeight = vrEyeHeight - (eyeOffset.z - pm_normalviewheight.GetFloat());
		}

        return org + ( GetPhysics()->GetGravityNormal() * eyeHeight);
    } else{
        return org + ( GetPhysics()->GetGravityNormal() * -eyeOffset.z );
	}*/
}

void idPlayer::SetVRClientInfo(vrClientInfo *pVR)
{
	commonVr->pVRClientInfo = pVR;
	pVRClientInfo = pVR;
	//Need to do this at least once
	if(!commonVr->initialResetPerformed) {
        OrientHMDBody();
    }
	commonVr->FrameStart();
}

vrClientInfo*	idPlayer::GetVRClientInfo()
{
    return pVRClientInfo;
}



/*
===============
idPlayer::GetViewPos
===============
*/
void idPlayer::GetViewPos( idVec3 &origin, idMat3 &axis ) const {
	idAngles angles;

    if ( game->isVR && pVRClientInfo )
    {
        GetViewPosVR( origin, axis );
        return;
    }
    //GBFIX DrBeef Old Code
	/*
	// if dead, fix the angle and don't add any kick
	if ( health <= 0 ) {
		angles.yaw = viewAngles.yaw;
		//angles.roll = 40;
		//angles.pitch = -15;
		axis = angles.ToMat3();
		origin = GetEyePosition();
	} else {
		origin = GetEyePosition() + viewBob;
		if (pVRClientInfo)
		{
			//Use pitch and roll from HMD
			angles.Set(pVRClientInfo->hmdorientation[PITCH], viewAngles.yaw, pVRClientInfo->hmdorientation[ROLL]);
		} else{
			angles = viewAngles + viewBobAngles + playerView.AngleOffset();
		}

		axis = angles.ToMat3() * physicsObj.GetGravityAxis();

		// adjust the origin based on the camera nodal distance (eye distance from neck)
		origin += physicsObj.GetGravityNormal() * g_viewNodalZ.GetFloat();
		origin += axis[0] * g_viewNodalX.GetFloat() + axis[2] * g_viewNodalZ.GetFloat();
	}*/
}

/*
===============
idPlayer::GetWeaponInHand
Carl: Dual wielding. Returns NULL if no weapon in the hand or if hand < 0 (no hand).
===============
*/
idWeapon* idPlayer::GetWeaponInHand( int hand ) const
{
    if( hand < 0 )
        return NULL;
    else
        return hands[ hand ].weapon;
}

// Carl: for now, only one of the weapons we are holding can have a flashlight mounted to it
// because we haven't implemented dual wielding flashlights yet.
idWeapon * idPlayer::GetWeaponWithMountedFlashlight()
{
    // The pistol we start with in RoE for XBox has a flashlight mounted, so prefer that weapon first (for now)
    if( commonVr->currentFlashlightMode == FLASHLIGHT_PISTOL )
    {
        if( hands[0].currentWeapon == weapon_pistol && !hands[0].isTheDuplicate )
            return hands[0].weapon;
        if( hands[1].currentWeapon == weapon_pistol && !hands[1].isTheDuplicate )
            return hands[1].weapon;
    }
    // Carl: todo
    return GetMainWeapon();
}

/*
===============
idPlayer::GetViewPosVR
===============
*/
void idPlayer::GetViewPosVR( idVec3 &origin, idMat3 &axis ) const {

    idAngles angles;

    // if dead, fix the angle and don't add any kick
    if ( health <= 0 )
    {
        angles = viewAngles;
        axis = angles.ToMat3();
        origin = GetEyePosition();
        return;
    }

    //Carl: Use head and neck rotation model
    float eyeHeightAboveRotationPoint;
    float eyeShiftRight = 0;

    eyeHeightAboveRotationPoint = 5;//

    origin = GetEyePosition(); // +viewBob;
    // Carl: No view bobbing unless knockback is enabled. This isn't strictly a knockback, but close enough.
    // This is the bounce when you land after jumping

    // re-enabling this until a better method is implemented, as headbob is how vertical smoothing is implemented when going over stairs/bumps
    // bobbing can be disabled via the walkbob and runbob cvars until a better method devised.
    // if (vr_knockBack.GetBool())


    origin += viewBob;
    angles = viewAngles; // NO VIEW KICKING  +playerView.AngleOffset();
    axis = angles.ToMat3();// *physicsObj.GetGravityAxis();

    // Move pivot point down so looking straight ahead is a no-op on the Z
//		const idVec3 & gravityVector = physicsObj.GetGravityNormal();
    //origin += gravityVector * g_viewNodalZ.GetFloat();


    // adjust the origin based on the camera nodal distance (eye distance from neck)
    //origin += axis[0] * g_viewNodalX.GetFloat() - axis[1] * eyeShiftRight + axis[2] * eyeHeightAboveRotationPoint;
    //origin +=  axis[1] * -eyeShiftRight + axis[2] * eyeHeightAboveRotationPoint;
    origin += axis[2] * eyeHeightAboveRotationPoint;

    //common->Printf( "GetViewPosVr returning %s\n", origin.ToString() );

}


/*
===============
idPlayer::CalculateFirstPersonView
===============
*/
void idPlayer::CalculateFirstPersonView( void ) {
	if ( ( pm_modelView.GetInteger() == 1 ) || ( ( pm_modelView.GetInteger() == 2 ) && ( health <= 0 ) ) ) {
		//	Displays the view from the point of view of the "camera" joint in the player model

		idMat3 axis;
		idVec3 origin;
		idAngles ang;

		ang = playerView.AngleOffset();
		ang.yaw += viewAxis[ 0 ].ToYaw();

		jointHandle_t joint = animator.GetJointHandle( "camera" );
		animator.GetJointTransform( joint, gameLocal.time, origin, axis );
		firstPersonViewOrigin = ( origin + modelOffset ) * ( viewAxis * physicsObj.GetGravityAxis() ) + physicsObj.GetOrigin() + viewBob;
		firstPersonViewAxis = axis * ang.ToMat3() * physicsObj.GetGravityAxis();
	} else {
		// offset for local bobbing and kicks
		GetViewPos( firstPersonViewOrigin, firstPersonViewAxis );
#if 0
		// shakefrom sound stuff only happens in first person
		firstPersonViewAxis = firstPersonViewAxis * playerView.ShakeAxis();
#endif
	}
    CalculateLeftHand();
    CalculateRightHand();
    CalculateWaist();
}

void idPlayer::CalculateWaist()
{
    idMat3 & hmdAxis = commonVr->lastHMDViewAxis;

    waistOrigin = hmdAxis * neckOffset + commonVr->lastHMDViewOrigin;
    waistOrigin.z += waistZ;

    if ( hmdAxis[0].z < 0 ) // looking down
    {
        if ( hmdAxis[2].z > 0 )
        {
            // use a point between head forward and upward
            float h = hmdAxis[2].z - hmdAxis[0].z;
            float x = -hmdAxis[0].z / h;
            float y = hmdAxis[2].z / h;
            idVec3 i = hmdAxis[0] * y + hmdAxis[2] * x;
            float yaw = atan2( i.y, i.x ) * idMath::M_RAD2DEG;
            waistAxis = idAngles( 0, yaw, 0 ).ToMat3();
        }
        else
        {
            // use a point between head backward and upward
            float h = -hmdAxis[2].z - hmdAxis[0].z;
            float x = -hmdAxis[0].z / h;
            float y = hmdAxis[2].z / h;
            idVec3 i = hmdAxis[0] * y + hmdAxis[2] * x;
            float yaw = atan2( i.y, i.x ) * idMath::M_RAD2DEG;
            waistAxis = idAngles( 0, yaw, 0 ).ToMat3();
        }
    }
    else // fallback
    {
        waistAxis = idAngles( 0, hmdAxis.ToAngles().yaw, 0 ).ToMat3();
    }
}

void idPlayer::CalculateLeftHand()
{
    slotIndex_t oldSlot = hands[HAND_LEFT].handSlot;
    slotIndex_t slot = SLOT_NONE;
    if ( commonVr->hasHMD )
    {
        // remove pitch
        idMat3 axis = firstPersonViewAxis;
        //float pitch = idMath::M_RAD2DEG * asin(axis[0][2]);
        //idAngles angles(pitch, 0, 0);
        //axis = angles.ToMat3() * axis;
        //leftHandOrigin = hmdOrigin + (usercmd.vrLeftControllerOrigin - usercmd.vrHeadOrigin) * vrFaceForward * axis;
        //leftHandAxis = usercmd.vrLeftControllerAxis * vrFaceForward * axis;

        if( !vr_slotDisable.GetBool() )
        {
            for( int i = 0; i < SLOT_COUNT; i++ )
            {
                idVec3 slotOrigin = slots[i].origin;
                if ( vr_weaponHand.GetInteger() && i != SLOT_FLASHLIGHT_SHOULDER )
                    slotOrigin.y *= -1;
                idVec3 origin = waistOrigin + slotOrigin * waistAxis;
                if( ( hands[HAND_LEFT].handOrigin - origin ).LengthSqr() < slots[i].radiusSq )
                {
                    slot = (slotIndex_t)i;
                    break;
                }
            }
        }
    }
    else
    {
        //hands[HAND_LEFT].handOrigin = hmdOrigin + hmdAxis[2] * -5;
        //hands[HAND_LEFT].handAxis = hmdAxis;
    }
    if( oldSlot != slot )
    {
        hands[HAND_LEFT].SetControllerShake( vr_slotMag.GetFloat(), vr_slotDur.GetInteger(), vr_slotMag.GetFloat(), vr_slotDur.GetInteger() );
    }
    hands[HAND_LEFT].handSlot = slot;
}

void idPlayer::CalculateRightHand()
{
    slotIndex_t oldSlot = hands[HAND_RIGHT].handSlot;
    slotIndex_t slot = SLOT_NONE;
    if ( commonVr->hasHMD )
    {
        // remove pitch
        idMat3 axis = firstPersonViewAxis;
        //float pitch = idMath::M_RAD2DEG * asin(axis[0][2]);
        //idAngles angles(pitch, 0, 0);
        //axis = angles.ToMat3() * axis;
        //rightHandOrigin = hmdOrigin + (usercmd.vrRightControllerOrigin - usercmd.vrHeadOrigin) * vrFaceForward * axis;
        //rightHandAxis = usercmd.vrRightControllerAxis * vrFaceForward * axis;

        if( !vr_slotDisable.GetBool() )
        {
            for( int i = 0; i < SLOT_COUNT; i++ )
            {
                idVec3 slotOrigin = slots[i].origin;
                if ( vr_weaponHand.GetInteger() && i != SLOT_FLASHLIGHT_SHOULDER )
                    slotOrigin.y *= -1;
                idVec3 origin = waistOrigin + slotOrigin * waistAxis;
                if( (hands[HAND_RIGHT].handOrigin - origin).LengthSqr() < slots[i].radiusSq )
                {
                    slot = (slotIndex_t)i;
                    break;
                }
            }
        }
    }
    else
    {
        //rightHandOrigin = hmdOrigin + hmdAxis[2] * -5;
        //rightHandAxis = hmdAxis;
    }
    if( oldSlot != slot )
    {
        hands[HAND_RIGHT].SetControllerShake(vr_slotMag.GetFloat(), vr_slotDur.GetInteger(), vr_slotMag.GetFloat(), vr_slotDur.GetInteger() );
    }
    hands[HAND_RIGHT].handSlot = slot;
}


/*
==================
idPlayer::GetRenderView

Returns the renderView that was calculated for this tic
==================
*/
renderView_t *idPlayer::GetRenderView( void ) {
	return renderView;
}

bool idPlayer::GetTeleportBeamOrigin( idVec3 &beamOrigin, idMat3 &beamAxis ) // returns true if the teleport beam should be displayed
{
    //const idVec3 beamOff[2] = { idVec3( 2.5f, 0.0f, 1.0f ), idVec3( 2.5f, 0.0f, 1.5f ) };
    const idVec3 beamOff[2] = { idVec3( 4.5f, 0.0f, 1.0f ), idVec3( 4.5f, 0.0f, 1.5f ) };

    if ( gameLocal.inCinematic || AI_DEAD || game->IsPDAOpen() )
    {
        return false;
    }

    int teleportHand, hand;
    teleportHand = vr_teleport.GetInteger();
    if( teleportHand == TP_HAND_RIGHT )
        hand = HAND_RIGHT;
    else if( teleportHand == TP_HAND_LEFT )
        hand = HAND_LEFT;
    else if( teleportHand == TP_HAND_HEAD )
        hand = vr_weaponHand.GetInteger();
    else if( teleportHand == 1 )
        hand = vr_weaponHand.GetInteger();
    else
        hand = 1 - vr_weaponHand.GetInteger();

    if ( teleportHand <= 0 || ( teleportHand == 1 && commonVr->VR_USE_MOTION_CONTROLS ) )// teleport aim mode is to use the standard weaponsight, so just return.
    {
        return false;
    }
    else if( teleportHand == TP_HAND_HEAD || !commonVr->VR_USE_MOTION_CONTROLS ) // beam originates from in front of the head
    {
        beamAxis = commonVr->lastHMDViewAxis;
        beamOrigin = commonVr->lastHMDViewOrigin + 12 * beamAxis[ 0 ];
        beamOrigin = beamOrigin + 5 * beamAxis[ 2 ];
    }
    else // teleport aim origin from the hand
    {
        if ( !hands[hand].weapon->ShowCrosshair() ||
             hands[hand].weapon->IsHidden() ||
             hands[ hand ].weapon->hideOffset != 0 ||						// Koz - turn off lasersight If gun is lowered ( in gui ).
             commonVr->handInGui || 						// turn off lasersight if hand is in gui.
			 hands[ hand ].weapon.GetEntity()->GetGrabberState() >= 2 	// Koz turn off laser sight if grabber is dragging an entity
             )
        {
            return false;
        }

        if( hands[hand].holdingFlashlight() ) // flashlight is in the hand, so originate the beam slightly in front of the flashlight.
        {
            beamAxis = flashlight->GetRenderEntity()->axis;
            beamOrigin = flashlight->GetRenderEntity()->origin + 10 * beamAxis[ 0 ];
        }
        else if ( !hands[hand].controllingWeapon() ) // just send it from the hand.
        {
            if( animator.GetJointTransform( ik_hand[ hand ], gameLocal.time, beamOrigin, beamAxis ) )
            {
                beamAxis = ik_handCorrectAxis[ hand ][ 1 ].Inverse() * beamAxis;

                beamOrigin = beamOrigin * renderEntity.axis + renderEntity.origin;
                beamAxis = beamAxis * renderEntity.axis;
                beamOrigin += beamOff[ hand ] * beamAxis;
            }
            else
            {
                // we failed to get the joint for some reason, so just default to the weapon origin and axis
                beamOrigin = hands[ hand ].weapon->viewWeaponOrigin;
                beamAxis = hands[ hand ].weapon->viewWeaponAxis;
            }
        }
        else if ( !hands[ hand ].weapon->GetMuzzlePositionWithHacks( beamOrigin, beamAxis ) )
        {
            // weapon has no muzzle, so get the position and axis of the animated hand joint
            if ( animator.GetJointTransform( ik_hand[hand], gameLocal.time, beamOrigin, beamAxis ) )
            {
                beamAxis = ik_handCorrectAxis[hand][1].Inverse() * beamAxis;

                beamOrigin = beamOrigin * renderEntity.axis + renderEntity.origin;
                beamAxis = beamAxis * renderEntity.axis;
                beamOrigin += beamOff[hand] * beamAxis;
            }
            else
            {
                // we failed to get the joint for some reason, so just default to the weapon origin and axis
                beamOrigin = hands[ hand ].weapon->viewWeaponOrigin;
                beamAxis = hands[ hand ].weapon->viewWeaponAxis;
            }
        }
        else // had a valid muzzle;
        {
            beamOrigin -= 2 * beamAxis[1]; // if coming from the muzzle, move 2 in down, looks better when it doesn't interfere with the laser sight.
        }

        if ( hands[hand].weapon->IdentifyWeapon() == WEAPON_CHAINSAW )
        {
            beamOrigin += 6 * beamAxis[0]; // move the beam origin 6 inches forward
        }
        else
        {
            beamOrigin += 4 * beamAxis[0]; // move the beam origin 4 inches forward
        }
    }
    return true;
}

/*
==================
idPlayer::CalculateRenderView

create the renderView for the current tic
==================
*/
void idPlayer::CalculateRenderView( void ) {
	// Koz add headtracking
	static idAngles hmdAngles( 0.0, 0.0, 0.0 );
	static idVec3 headPositionDelta = vec3_zero;
	static idVec3 bodyPositionDelta = vec3_zero;

	static bool wasCinematic = false;
	static idVec3 cinematicOffset = vec3_zero;
	static float cineYawOffset = 0.0f;

	static bool wasThirdPerson = false;
	static idVec3 thirdPersonOffset = vec3_zero;
	static idVec3 thirdPersonOrigin = vec3_zero;
	static idMat3 thirdPersonAxis = mat3_identity;
	static float thirdPersonBodyYawOffset = 0.0f;

	int i;
	float range;

	if ( !renderView ) {
		renderView = new renderView_t;
	}
	memset( renderView, 0, sizeof( *renderView ) );

	// copy global shader parms
	for( i = 0; i < MAX_GLOBAL_SHADER_PARMS; i++ ) {
		renderView->shaderParms[ i ] = gameLocal.globalShaderParms[ i ];
	}
	renderView->globalMaterial = gameLocal.GetGlobalMaterial();
	renderView->time = gameLocal.slow.time;

	// calculate size of 3D view
	renderView->x = 0;
	renderView->y = 0;
	renderView->width = SCREEN_WIDTH;
	renderView->height = SCREEN_HEIGHT;
	renderView->viewID = 0;

	// check if we should be drawing from a camera's POV
	if ( !noclip && (gameLocal.GetCamera() || privateCameraView) ) {
		// get origin, axis, and fov
		if ( privateCameraView ) {
			privateCameraView->GetViewParms( renderView );
		} else {
			gameLocal.GetCamera()->GetViewParms( renderView );
		}
	} else {
		if ( g_stopTime.GetBool() || commonVr->VR_GAME_PAUSED) {
			renderView->vieworg = firstPersonViewOrigin;
			renderView->viewaxis = firstPersonViewAxis;

			if ( !pm_thirdPerson.GetBool() ) {
				// set the viewID to the clientNum + 1, so we can suppress the right player bodies and
				// allow the right player view weapons
				renderView->viewID = entityNumber + 1;
			}
		} else if ( pm_thirdPerson.GetBool() ) {
			OffsetThirdPersonView( pm_thirdPersonAngle.GetFloat(), pm_thirdPersonRange.GetFloat(), pm_thirdPersonHeight.GetFloat(), pm_thirdPersonClip.GetBool() );
		} else if ( pm_thirdPersonDeath.GetBool() ) {
			range = gameLocal.time < minRespawnTime ? ( gameLocal.time + RAGDOLL_DEATH_TIME - minRespawnTime ) * ( 120.0f / RAGDOLL_DEATH_TIME ) : 120.0f;
			OffsetThirdPersonView( 0.0f, 20.0f + range, 0.0f, false );
		} else {
			renderView->vieworg = firstPersonViewOrigin;
			renderView->viewaxis = firstPersonViewAxis;

			// set the viewID to the clientNum + 1, so we can suppress the right player bodies and
			// allow the right player view weapons
			renderView->viewID = entityNumber + 1;
		}

		// field of view
		gameLocal.CalcFov( CalcFov( true ), renderView->fov_x, renderView->fov_y );
	}

	if ( renderView->fov_y == 0 ) {
		common->Error( "renderView->fov_y == 0" );
	}

	if ( g_showviewpos.GetBool() ) {
		gameLocal.Printf( "%s : %s\n", renderView->vieworg.ToString(), renderView->viewaxis.ToAngles().ToString() );
	}

	if ( game->isVR && pVRClientInfo )
	{

		// Koz headtracker does not modify the model rotations
		// offsets to body rotation added here

		// body position based on neck model
		// Koz fixme fix this.

		// Koz begin : Add headtracking
		static idVec3 absolutePosition;

		hmdAngles = commonVr->poseHmdAngles;



		headPositionDelta = commonVr->poseHmdHeadPositionDelta;
		bodyPositionDelta = commonVr->poseHmdBodyPositionDelta;
		absolutePosition = commonVr->poseHmdAbsolutePosition;

		idVec3 origin = renderView->vieworg;
		idAngles angles = renderView->viewaxis.ToAngles();
		idMat3 axis = renderView->viewaxis;
		float yawOffset = commonVr->bodyYawOffset;

		if ( gameLocal.inCinematic || privateCameraView || (health <= 0) )
		{
			if ( wasCinematic == false )
			{
				wasCinematic = true;
				commonVr->cinematicStartViewYaw = hmdAngles.yaw + commonVr->trackingOriginYawOffset;
				commonVr->cinematicStartPosition = absolutePosition + (commonVr->trackingOriginOffset * idAngles( 0.0f, commonVr->trackingOriginYawOffset, 0.0f ).ToMat3());
				cineYawOffset = hmdAngles.yaw - yawOffset;
				//commonVr->cinematicStartPosition.x = -commonVr->hmdTrackingState.HeadPose.ThePose.Position.z;
				//commonVr->cinematicStartPosition.y = -commonVr->hmdTrackingState.HeadPose.ThePose.Position.x;
				//commonVr->cinematicStartPosition.z = commonVr->hmdTrackingState.HeadPose.ThePose.Position.y;

				if (health > 0) {
					playerView.Flash(colorWhite, 300);
				}

				if ( vr_cinematics.GetInteger() == 2)
				{
					cinematicOffset = vec3_zero;
				}
				else
				{
					cinematicOffset = absolutePosition;
				}
			}

			if (vr_cinematics.GetInteger() == 2)
			{
				headPositionDelta = bodyPositionDelta = vec3_zero;
			}
			else
			{
				headPositionDelta = absolutePosition - cinematicOffset;
				bodyPositionDelta = vec3_zero;
			}
		}
		else
		{
			wasCinematic = false;
			cineYawOffset = 0.0f;
		}


		if (!(gameLocal.inCinematic && vr_cinematics.GetInteger() == 2) && (health > 0))
		{

			//move the head in relation to the body.
			//bodyYawOffsets are external rotations of the body where the head remains looking in the same direction
			//e.g. when using movepoint and snapping the body to the view.

			idAngles bodyAng = axis.ToAngles();
			idMat3 bodyAx = idAngles( bodyAng.pitch, bodyAng.yaw - yawOffset, bodyAng.roll ).Normalize180().ToMat3();
			origin = origin + bodyAx[0] * headPositionDelta.x + bodyAx[1] * headPositionDelta.y + bodyAx[2] * headPositionDelta.z;

			origin += commonVr->leanOffset;

			// Koz to do clean up later - added to allow cropped cinematics with original camera movements.
			idQuat q1, q2;

			q1 = angles.ToQuat();
			q2 = idAngles( hmdAngles.pitch, ( hmdAngles.yaw - yawOffset ) - cineYawOffset, hmdAngles.roll ).ToQuat();

			angles = ( q2 * q1 ).ToAngles();

			angles.Normalize180();

			commonVr->lastHMDYaw = hmdAngles.yaw;
			commonVr->lastHMDPitch = hmdAngles.pitch;
			commonVr->lastHMDRoll = hmdAngles.roll;

			axis = angles.ToMat3(); // this sets the actual view axis, separate from the body axis.

			commonVr->lastHMDViewOrigin = origin;
			commonVr->lastHMDViewAxis = axis;

			commonVr->uncrouchedHMDViewOrigin = origin;
			commonVr->uncrouchedHMDViewOrigin.z -= commonVr->headHeightDiff;


			if (commonVr->thirdPersonMovement)
			{
				if (wasThirdPerson == false)
				{
					wasThirdPerson = true;
					thirdPersonOffset = absolutePosition;
					thirdPersonOrigin = commonVr->lastHMDViewOrigin;//origin;
					thirdPersonAxis = idAngles(0.0f, commonVr->lastHMDViewAxis.ToAngles().yaw, 0.0f).ToMat3();//axis;
					thirdPersonBodyYawOffset = hmdAngles.yaw;// -yawOffset;

				}
				origin = thirdPersonOrigin;
				axis = thirdPersonAxis;
				yawOffset = thirdPersonBodyYawOffset;
				angles = thirdPersonAxis.ToAngles();
				headPositionDelta = absolutePosition - thirdPersonOffset;
				bodyPositionDelta = vec3_zero;

				idAngles bodyAng = thirdPersonAxis.ToAngles();
				idMat3 bodyAx = idAngles(bodyAng.pitch, bodyAng.yaw - yawOffset, bodyAng.roll).Normalize180().ToMat3();
				origin = thirdPersonOrigin + bodyAx[0] * headPositionDelta.x + bodyAx[1] * headPositionDelta.y + bodyAx[2] * headPositionDelta.z;

				origin += commonVr->leanOffset;

				angles.yaw += hmdAngles.yaw - thirdPersonBodyYawOffset;    // add the current hmd orientation
				angles.pitch += hmdAngles.pitch;
				angles.roll += hmdAngles.roll;
				angles.Normalize180();
				axis = angles.ToMat3();
				commonVr->thirdPersonHudAxis = axis;
				commonVr->thirdPersonHudPos = origin;

				commonVr->thirdPersonDelta = (origin - firstPersonViewOrigin).LengthSqr();

			}
			else
			{
				if (wasThirdPerson)
				{
					commonVr->thirdPersonDelta = 0.0f;;
					playerView.Flash(colorBlack, 140);
				}
				wasThirdPerson = false;
			}

		}
		renderView->vieworg = origin;
		renderView->viewaxis = axis;



		// if leaning, check if the eye is in a wall
		if ( commonVr->isLeaning )
		{
			idBounds bounds = idBounds( idVec3( 0, -1, -1 ), idVec3( 0, 1, 1 ) );
			trace_t trace;

			gameLocal.clip.TraceBounds( trace, origin, origin, bounds, MASK_SHOT_RENDERMODEL, this);
			if ( trace.fraction != 1.0f )
			{
				commonVr->leanBlank = true;
				commonVr->leanBlankOffset = commonVr->leanOffset;
				commonVr->leanBlankOffsetLengthSqr = commonVr->leanOffset.LengthSqr();

			}

			else
			{
				commonVr->leanBlank = false;
				commonVr->leanBlankOffset = vec3_zero;
				commonVr->leanBlankOffsetLengthSqr = 0.0f;
			}

		}




		// Koz fixme pause - handle the PDA model if game is paused
		// really really need to move this somewhere else,

		if ( !commonVr->PDAforcetoggle && commonVr->PDAforced && hands[ 0 ].weapon->IdentifyWeapon() != WEAPON_PDA && hands[ 1 ].weapon->IdentifyWeapon() != WEAPON_PDA ) // PDAforced cannot be valid if the weapon is not the PDA
		{
			commonVr->PDAforced = false;
			commonVr->VR_GAME_PAUSED = false;
			idPlayer* player = gameLocal.GetLocalPlayer();
			player->SetupPDASlot( true );
			player->SetupHolsterSlot( vr_weaponHand.GetInteger(), true );
		}

		if ( commonVr->PDAforcetoggle )
		{
			int pdahand = 1 - vr_weaponHand.GetInteger();
			if ( !commonVr->PDAforced )
			{
				if ( hands[ 0 ].weapon->IdentifyWeapon() != WEAPON_PDA && hands[ 1 ].weapon->IdentifyWeapon() != WEAPON_PDA )
				{
					//common->Printf( "idPlayer::CalculateRenderView calling SelectWeapon for PDA\nPDA Forced = %i, PDAForceToggle = %i\n",commonVr->PDAforced,commonVr->PDAforcetoggle );
					//common->Printf( "CRV3 Calling SetupHolsterSlot( %i, %i ) \n", 1 - pdahand, commonVr->PDAforced );

					idPlayer* player = gameLocal.GetLocalPlayer();
					player->SetupPDASlot( commonVr->PDAforced );
					player->SetupHolsterSlot( 1 - pdahand, commonVr->PDAforced );

					hands[ pdahand ].SelectWeapon( weapon_pda, true, false );
					SetWeaponHandPose();
				}
				else
				{

					if ( (hands[ 0 ].weapon->IdentifyWeapon() == WEAPON_PDA && hands[ 0 ].weapon->status == WP_READY) || ( hands[ 1 ].weapon->IdentifyWeapon() == WEAPON_PDA && hands[ 1 ].weapon->status == WP_READY ) )
					{
						commonVr->PDAforced = true;
						commonVr->PDAforcetoggle = false;
					}
				}
			}
			else
			{ // pda has been already been forced active, put it away.

				TogglePDA( pdahand );
				commonVr->PDAforcetoggle = false;
				commonVr->PDAforced = false;
			}

		}
	}
}

/*
=============
idPlayer::AddAIKill
=============
*/
void idPlayer::AddAIKill( void ) {
	int max_souls;
	int ammo_souls;

	if ( ( weapon_soulcube < 0 ) || ( inventory.weapons & ( 1 << weapon_soulcube ) ) == 0 ) {
		return;
	}

	assert( hud );

	ammo_souls = idWeapon::GetAmmoNumForName( "ammo_souls" );
	max_souls = inventory.MaxAmmoForAmmoClass( this, "ammo_souls" );
	if ( inventory.ammo[ ammo_souls ] < max_souls ) {
		inventory.ammo[ ammo_souls ]++;
		if ( inventory.ammo[ ammo_souls ] >= max_souls ) {
			hud->HandleNamedEvent( "soulCubeReady" );
			StartSound( "snd_soulcube_ready", SND_CHANNEL_ANY, 0, false, NULL );
		}
	}
}

/*
=============
idPlayer::SetSoulCubeProjectile
=============
*/
void idPlayer::SetSoulCubeProjectile( idProjectile *projectile ) {
	soulCubeProjectile = projectile;
}

/*
=============
idPlayer::AddProjectilesFired
=============
*/
void idPlayer::AddProjectilesFired( int count ) {
	numProjectilesFired += count;
}

/*
=============
idPlayer::AddProjectileHites
=============
*/
void idPlayer::AddProjectileHits( int count ) {
	numProjectileHits += count;
}

/*
=============
idPlayer::SetLastHitTime
=============
*/
void idPlayer::SetLastHitTime( int time ) {
	idPlayer *aimed = NULL;

	if ( time && lastHitTime != time ) {
		lastHitToggle ^= 1;
	}
	lastHitTime = time;
	if ( !time ) {
		// level start and inits
		return;
	}
	if ( gameLocal.isMultiplayer && ( time - lastSndHitTime ) > 10 ) {
		lastSndHitTime = time;
		StartSound( "snd_hit_feedback", SND_CHANNEL_ANY, SSF_PRIVATE_SOUND, false, NULL );
	}
	if ( cursor ) {
		cursor->HandleNamedEvent( "hitTime" );
	}
	if ( hud ) {
		if ( MPAim != -1 ) {
			if ( gameLocal.entities[ MPAim ] && gameLocal.entities[ MPAim ]->IsType( idPlayer::Type ) ) {
				aimed = static_cast< idPlayer * >( gameLocal.entities[ MPAim ] );
			}
			assert( aimed );
			// full highlight, no fade till loosing aim
			hud->SetStateString( "aim_text", gameLocal.userInfo[ MPAim ].GetString( "ui_name" ) );
			if ( aimed ) {
				hud->SetStateFloat( "aim_color", aimed->colorBarIndex );
			}
			hud->HandleNamedEvent( "aim_flash" );
			MPAimHighlight = true;
			MPAimFadeTime = 0;
		} else if ( lastMPAim != -1 ) {
			if ( gameLocal.entities[ lastMPAim ] && gameLocal.entities[ lastMPAim ]->IsType( idPlayer::Type ) ) {
				aimed = static_cast< idPlayer * >( gameLocal.entities[ lastMPAim ] );
			}
			assert( aimed );
			// start fading right away
			hud->SetStateString( "aim_text", gameLocal.userInfo[ lastMPAim ].GetString( "ui_name" ) );
			if ( aimed ) {
				hud->SetStateFloat( "aim_color", aimed->colorBarIndex );
			}
			hud->HandleNamedEvent( "aim_flash" );
			hud->HandleNamedEvent( "aim_fade" );
			MPAimHighlight = false;
			MPAimFadeTime = gameLocal.realClientTime;
		}
	}
}

/*
=============
idPlayer::SetInfluenceLevel
=============
*/
void idPlayer::SetInfluenceLevel( int level ) {
	if ( level != influenceActive ) {
		if ( level ) {
			for ( idEntity *ent = gameLocal.spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next() ) {
				if ( ent->IsType( idProjectile::Type ) ) {
					// remove all projectiles
					ent->PostEventMS( &EV_Remove, 0 );
				}
			}
			for( int h = 0; h < 2; h++ )
			{
				if( weaponEnabled && hands[ h ].weapon.GetEntity() )
					hands[ h ].weapon.GetEntity()->EnterCinematic();
			}
		} else {
			physicsObj.SetLinearVelocity( vec3_origin );
			for( int h = 0; h < 2; h++ )
			{
				if( weaponEnabled && hands[ h ].weapon.GetEntity() )
					hands[ h ].weapon.GetEntity()->ExitCinematic();
			}
		}
		influenceActive = level;
	}
}

/*
=============
idPlayer::SetInfluenceView
=============
*/
void idPlayer::SetInfluenceView( const char *mtr, const char *skinname, float radius, idEntity *ent ) {
	influenceMaterial = NULL;
	influenceEntity = NULL;
	influenceSkin = NULL;
	if ( mtr && *mtr ) {
		influenceMaterial = declManager->FindMaterial( mtr );
	}
	if ( skinname && *skinname ) {
		influenceSkin = declManager->FindSkin( skinname );
		if ( head ) {
			head->GetRenderEntity()->shaderParms[ SHADERPARM_TIMEOFFSET ] = -MS2SEC( gameLocal.time );
		}
		UpdateVisuals();
	}
	influenceRadius = radius;
	if ( radius > 0.0f ) {
		influenceEntity = ent;
	}
}

/*
=============
idPlayer::SetInfluenceFov
=============
*/
void idPlayer::SetInfluenceFov( float fov ) {
	//Clamp this for VR
    influenceFov = (fov == 0.0f) ? 0.0f : idMath::ClampFloat(80.0f, 120.0f, fov);
}

/*
================
idPlayer::OnLadder
================
*/
bool idPlayer::OnLadder( void ) const {
	return physicsObj.OnLadder();
}

/*
==============
idPlayer::OrientHMDBody  Koz align the body with the view  ( move the body to point the same direction as the HMD view - does not change the view. )
==============
*/
void idPlayer::OrientHMDBody()
{
    SnapBodyToView();
    commonVr->bodyYawOffset = 0;
    commonVr->lastHMDYaw = 0;
    commonVr->HMDResetTrackingOriginOffset();
    commonVr->MotionControlSetOffset();
    commonVr->bodyMoveAng = 0.0f;
}

bool idPlayer::OtherHandImpulseSlot()
{
    int hand = 1 - vr_weaponHand.GetInteger();
    slotIndex_t otherHandSlot = hands[ hand ].handSlot;
    if( otherHandSlot == SLOT_PDA_HIP )
    {
        // we don't have a PDA, so toggle the menu instead
        if ( commonVr->PDAforced || inventory.pdas.Num() == 0 )
        {
            PerformImpulse( 40 );
        }
        else if( objectiveSystemOpen ) // if we're holding our PDA and we try to grab the PDA slot
        {
            // our hand is always full in this case, but that's only an issue we care and the holster contains the flashlight
            if( vr_mustEmptyHands.GetBool() && commonVr->currentFlashlightMode == FLASHLIGHT_HAND )
                return false;
            TogglePDA( hand );
        }
        else if( weapon_pda >= 0 ) // if the PDA is in the slot and we try to grab it
        {
            if( hands[ hand ].tooFullToInteract() )
                return false;
            SetupPDASlot( false );
            SetupHolsterSlot( 1 - hand, false );
            hands[ hand ].SelectWeapon( weapon_pda, true, false );
        }
        return true;
    }

    if( otherHandSlot == SLOT_FLASHLIGHT_HEAD && !commonVr->PDAforced && !objectiveSystemOpen
        && flashlight.IsValid() && !spectating && weaponEnabled && !hiddenWeapon && !gameLocal.world->spawnArgs.GetBool("no_Weapons") && !vr_flashlightStrict.GetBool() )
    {
        // swap flashlight between head and hand
        if ( commonVr->currentFlashlightPosition == FLASHLIGHT_HEAD && HasHoldableFlashlight() )
        {
            // Carl: move weapon from hand to inventory
            hands[hand].idealWeapon = weapon_fists;
            // move flashlight from head to hand
            vr_flashlightMode.SetInteger(FLASHLIGHT_HAND);
            vr_flashlightMode.SetModified();
        }
        else if ( commonVr->currentFlashlightPosition == FLASHLIGHT_HAND )
        {
            vr_flashlightMode.SetInteger(FLASHLIGHT_HEAD);
            vr_flashlightMode.SetModified();
        }
        return true;
    }
    if( otherHandSlot == SLOT_FLASHLIGHT_SHOULDER && !commonVr->PDAforced && !objectiveSystemOpen
        && flashlight.IsValid() && !spectating && weaponEnabled && !hiddenWeapon && !gameLocal.world->spawnArgs.GetBool("no_Weapons") && !vr_flashlightStrict.GetBool() )
    {
        // swap flashlight between body and hand
        if ( commonVr->currentFlashlightPosition == FLASHLIGHT_BODY && HasHoldableFlashlight() )
        {
            // Carl: move weapon from hand to inventory
            hands[hand].idealWeapon = weapon_fists;
            // move flashlight from shoulder to hand
            vr_flashlightMode.SetInteger(FLASHLIGHT_HAND);
            vr_flashlightMode.SetModified();
        }
        else if ( commonVr->currentFlashlightPosition == FLASHLIGHT_HAND )
        {
            vr_flashlightMode.SetInteger(FLASHLIGHT_BODY);
            vr_flashlightMode.SetModified();
        }
        return true;
    }
    if (otherHandSlot == SLOT_WEAPON_HIP)
    {
        // Holster the PDA we are holding on the other side
        if( commonVr->PDAforced )
        {
            SwapWeaponHand();
            PerformImpulse( 40 );
        }
        else if( objectiveSystemOpen )
        {
            SwapWeaponHand();
            if (hands[hand].previousWeapon == weapon_fists)
                hands[hand].previousWeapon = holsteredWeapon;
            TogglePDA( hand );
        }
        else
        {
            int h = 1 - vr_weaponHand.GetInteger();
            if( hands[ h ].tooFullToInteract() && holsteredWeapon != weapon_fists )
                return false;
            SwapWeaponHand();
            // pick up whatever weapon we have holstered, and magically holster our current weapon
            SetupHolsterSlot( vr_weaponHand.GetInteger() );
        }
        return true;
    }
    if ( otherHandSlot == SLOT_WEAPON_BACK_BOTTOM )
    {
        SwapWeaponHand();
        // Holster the PDA we are holding on the other side
        // we don't have a PDA, so toggle the menu instead
        if ( commonVr->PDAforced )
        {
            PerformImpulse( 40 );
        }
        else if( objectiveSystemOpen )
        {
            TogglePDA( hand );
        }

        hands[ hand ].PrevWeapon();
        return true;
    }
    if ( otherHandSlot == SLOT_WEAPON_BACK_TOP )
    {
        // SwapWeaponHand();
        // If we are holding a PDA, put it away.
        // if it's the pause menu "PDA", toggle the menu instead
        if ( commonVr->PDAforced )
        {
            PerformImpulse(40);
        }
        else if ( objectiveSystemOpen )
        {
            TogglePDA( hand );
        }
        hands[ hand ].NextWeapon();
        return true;
    }
    return false;
}

/*
=====================
idPlayer::PathToGoal
=====================
*/
bool idPlayer::PathToGoal(aasPath_t& path, int areaNum, const idVec3& origin, int goalAreaNum, const idVec3& goalOrigin) const
{
    idVec3 org;
    idVec3 goal;

    if (!aas)
    {
        return false;
    }

    org = origin;

    if (ai_debugMove.GetBool())
    {
        aas->DrawArea( areaNum );
        aas->DrawArea( goalAreaNum );
    }

    aas->PushPointIntoAreaNum(areaNum, org);
    if (!areaNum)
    {
        return false;
    }

    goal = goalOrigin;
    aas->PushPointIntoAreaNum(goalAreaNum, goal);
    if (!goalAreaNum)
    {
        return false;
    }

    if (ai_debugMove.GetBool())
    {
        aas->ShowWalkPath(org, goalAreaNum, goal, travelFlags);
    }
    return aas->WalkPathToGoal( path, areaNum, org, goalAreaNum, goal, travelFlags );
}

/*
==================
idPlayer::Event_GetButtons
==================
*/
void idPlayer::Event_GetButtons( void ) {
	idThread::ReturnInt( usercmd.buttons );
}

/*
==================
idPlayer::Event_GetMove
==================
*/
void idPlayer::Event_GetMove( void ) {
    idVec3 move( usercmd.forwardmove, usercmd.rightmove, usercmd.upmove );
	idThread::ReturnVector( move );
}

/*
================
idPlayer::Event_GetViewAngles
================
*/
void idPlayer::Event_GetViewAngles( void ) {
	idThread::ReturnVector( idVec3( viewAngles[0], viewAngles[1], viewAngles[2] ) );
}

/*
==================
idPlayer::Event_StopFxFov
==================
*/
void idPlayer::Event_StopFxFov( void ) {
	fxFov = false;
}

/*
==================
idPlayer::StartFxFov
==================
*/
void idPlayer::StartFxFov( float duration ) {
	fxFov = true;
	PostEventSec( &EV_Player_StopFxFov, duration );
}

/*
==================
idPlayer::Event_EnableWeapon
==================
*/
void idPlayer::Event_EnableWeapon( void ) {
	hiddenWeapon = gameLocal.world->spawnArgs.GetBool( "no_Weapons" );
	weaponEnabled = true;
    for( int h = 0; h < 2; h++ )
    {
        if( hands[ h ].weapon )
            hands[ h ].weapon->ExitCinematic();
    }
}

/*
==================
idPlayer::Event_DisableWeapon
==================
*/
void idPlayer::Event_DisableWeapon( void ) {
	hiddenWeapon = gameLocal.world->spawnArgs.GetBool( "no_Weapons" );
	weaponEnabled = false;
    if ( !gameLocal.isMultiplayer ) {
        for( int h = 0; h < 2; h++ ) {
            if( hands[ h ].weapon )
                hands[ h ].weapon->EnterCinematic();
        }
    }
}

/*
==================
idPlayer::Event_GiveInventoryItem
==================
*/
void idPlayer::Event_GiveInventoryItem(const char *name) {
    GiveInventoryItem(name);
}

/*
==================
idPlayer::Event_RemoveInventoryItem
==================
*/
void idPlayer::Event_RemoveInventoryItem(const char *name) {
    RemoveInventoryItem(name);
}

/*
==================
idPlayer::Event_SetPowerupTime
==================
*/
void idPlayer::Event_SetPowerupTime(int powerup, int time) {
    if (time > 0) {
        GivePowerUp(powerup, time);
    } else {
        ClearPowerup(powerup);
    }
}

/*
==================
idPlayer::Event_IsPowerupActive
==================
*/
void idPlayer::Event_IsPowerupActive(int powerup) {
    idThread::ReturnInt(this->PowerUpActive(powerup) ? 1 : 0);
}

/*
==================
idPlayer::Event_StartWarp
==================
*/
void idPlayer::Event_StartWarp() {
    playerView.AddWarp(idVec3(0, 0, 0), SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 100, 1000);
}

/*
==================
idPlayer::Event_StopHelltime
==================
*/
void idPlayer::Event_StopHelltime(int mode) {
    if (mode == 1) {
        StopHelltime(true);
    } else {
        StopHelltime(false);
    }
}

/*
==================
idPlayer::Event_GetCurrentWeapon
==================
*/
void idPlayer::Event_GetCurrentWeapon( void ) {
	const char *weapon;

    if( hands[vr_weaponHand.GetInteger()].currentWeapon >= 0 )
    {
        weapon = spawnArgs.GetString( va( "def_weapon%d", hands[ vr_weaponHand.GetInteger() ].currentWeapon ) );
        idThread::ReturnString( weapon );
    }
    else
    {
        idThread::ReturnString( "" );
    }
}

/*
==================
idPlayer::Event_GetFlashHand // get flashlight hand
==================
*/
void idPlayer::Event_GetFlashHand() // get flashlight hand
{
    static int flashlightHand = 1;
    //returns 0 for right, 1 for left
    flashlightHand = vr_weaponHand.GetInteger() == 0 ? 1 : 0;

    idThread::ReturnInt( flashlightHand );
}
/*
==================
idPlayer::Event_GetFlashState // get flashlight state
==================
*/
void idPlayer::Event_GetFlashState() // get flashlight state
{
    static int flashlighton;
    flashlighton = flashlight && flashlight->lightOn  ? 1 : 0 ;
    // Koz debug common->Printf( "Returning flashlight state = %d\n",flashlighton );
    idThread::ReturnInt( flashlighton );
}
/*
==================
idPlayer::Event_GetFlashHandState // get flashlight hand state
==================
*/
void idPlayer::Event_GetFlashHandState() // get flashlight hand state
{

    // this is for the flashlight hand
    // this is not for weapon hand animations like firing, this is to change hand pose if no weapon is present or if using guis interactively
    // 0 = hand empty no weapon
    // 1 = fist (no flashlight)
    // 2 = flashlight
    // 3 = normal weapon idle hand anim - used for holding PDA.


    int flashlightHand = 1;
    int hand = 1 - vr_weaponHand.GetInteger();

    if ( hands[hand].weapon && hands[hand].weapon->IdentifyWeapon() == WEAPON_PDA )
    {
        flashlightHand = 3;
    }
    else if ( spectating || !weaponEnabled || hiddenWeapon || gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) )
    {
        flashlightHand = 0;
    }
    else if( hands[hand].currentWeapon == weapon_fists && !hands[hand].holdingFlashlight() )
    {
        flashlightHand = 1;
    }

        //else if ( commonVr->currentFlashlightPosition == FLASHLIGHT_HAND && !spectating && weaponEnabled &&!hiddenWeapon && !gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) )
    else if ( commonVr->currentFlashlightPosition == FLASHLIGHT_HAND ) //&& !spectating && weaponEnabled &&!hiddenWeapon && !gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) )
    {
        flashlightHand = 2;
    }
    else if( hands[hand].floatingWeapon() )
    {
        flashlightHand = 0;
    }
    else if( hands[ hand ].holdingWeapon() || hands[ hand ].floatingWeapon() )
    {
        flashlightHand = 3;
    }

    if ( flashlightHand <= 1 && vr_useHandPoses.GetBool() )
    {
        int fingerPose;
        fingerPose = vr_weaponHand.GetInteger() != 0 ? commonVr->fingerPose[HAND_RIGHT] : commonVr->fingerPose[HAND_LEFT];
        fingerPose = fingerPose << 4;
        //common->Printf( "Flashlight hand finger pose = %d , %d\n ", flashlightHand, fingerPose );
        flashlightHand += fingerPose;
    }
    idThread::ReturnInt( flashlightHand );

}


/*
==================
idPlayer::Event_GetPreviousWeapon
==================
*/
void idPlayer::Event_GetPreviousWeapon( void ) {
	const char *weapon;

    if( hands[ vr_weaponHand.GetInteger() ].previousWeapon >= 0 )
    {
        int pw = ( gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) ) ? 0 : hands[ vr_weaponHand.GetInteger() ].previousWeapon;
        weapon = spawnArgs.GetString( va( "def_weapon%d", pw ) );
        idThread::ReturnString( weapon );
    }
    else
    {
        idThread::ReturnString( spawnArgs.GetString( "def_weapon0" ) );
    }
}

/*
==================
idPlayer::Event_SelectWeapon
==================
*/
void idPlayer::Event_SelectWeapon( const char *weaponName ) {

    if( vr_debugHands.GetBool() )
    {
        common->Printf( "Before Event_SelectWeapon(\"%s\"):\n", weaponName );
        hands[HAND_LEFT].debugPrint();
        hands[HAND_RIGHT].debugPrint();
    }
    int i;
    int weaponNum;

    if( gameLocal.isClient )
    {
        gameLocal.Warning( "Cannot switch weapons from script in multiplayer" );
        return;
    }

    // Carl: This function is called by item_pda::Lower() with the value returned from Event_GetPreviousWeapon,
    // in an attempt to put away the PDA and return both hands to what they were holding before.
    // That won't work when dual wielding, so check if we are in the PDA (before we change weapon) and put it away properly.
    for( int h = 0; h < 2; h++ )
    {
        if( hands[h].holdingPDA() )
        {
            //GB Good as place as any to try and get the flashlight back
            if(h != vr_weaponHand.GetInteger() && flashlightPreviouslyInHand)
            {
				flashlightPreviouslyInHand = false;
				commonVr->currentFlashlightMode = FLASHLIGHT_HAND;
            }
			hands[h].idealWeapon = hands[h].previousWeapon;
            //if ( hands[1 - h].previousWeapon >= 0 && hands[1 - h].previousWeapon != weapon_fists )
            //	UpdateHudWeapon( 1 - h );
            //else
            UpdateHudWeapon( h );
            return;
        }
    }

    // Carl: We're probably being called by weapon_bloodstone_passive::Fire() to switch back to the previous weapon
    /*
    for( int h = 0; h < 2; h++ )
    {
        if( hands[h].currentWeapon == weapon_bloodstone || hands[h].idealWeapon == weapon_bloodstone )
        {
            hands[h].idealWeapon = hands[h].previousWeapon;
            UpdateHudWeapon( h );
            return;
        }
    }*/

    if( hiddenWeapon && gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) )
    {
        hands[ vr_weaponHand.GetInteger() ].idealWeapon = weapon_fists;
        for( int h = 0; h < 2; h++ )
            hands[ h ].weapon->HideWeapon();
        return;
    }

    weaponNum = -1;
    for( i = 0; i < MAX_WEAPONS; i++ )
    {
        if( inventory.weapons & ( 1 << i ) )
        {
            const char* weap = spawnArgs.GetString( va( "def_weapon%d", i ) );
            if( !idStr::Cmp( weap, weaponName ) )
            {
                weaponNum = i;
                break;
            }
        }
    }

    if( weaponNum < 0 )
    {
        gameLocal.Warning( "%s is not carrying weapon '%s'", name.c_str(), weaponName );
        return;
    }

    hiddenWeapon = false;
    // Carl: check for dual wielding
    if( hands[ 1 - vr_weaponHand.GetInteger() ].idealWeapon != weaponNum )
    {
        hands[ vr_weaponHand.GetInteger() ].idealWeapon = weaponNum;
        UpdateHudWeapon( vr_weaponHand.GetInteger() );
    }
    else
    {
        UpdateHudWeapon( 1 - vr_weaponHand.GetInteger() );
    }
    if( vr_debugHands.GetBool() )
    {
        common->Printf( "After Event_SelectWeapon(\"%s\") (not BFG or artifact):\n", weaponName );
        hands[HAND_LEFT].debugPrint();
        hands[HAND_RIGHT].debugPrint();
    }
}

/*
==================
idPlayer::Event_GetWeaponEntity
==================
*/
void idPlayer::Event_GetWeaponEntity( void ) {
    int h = risingWeaponHand;
    if ( h < 0 )
        h = vr_weaponHand.GetInteger();
    if( hands[ 1 - h ].weapon->IdentifyWeapon() == WEAPON_ARTIFACT )
        h = 1 - h;
    idThread::ReturnEntity( hands[h].weapon );
}

// Koz begin
/*
==================
idPlayer::Event_GetWeaponHand
==================
*/
void idPlayer::Event_GetWeaponHand()
{
    //returns 0 for right hand 1 for left
    if ( vr_weaponHand.GetInteger() == 1 )
    {

        idThread::ReturnInt( 1 );
    }
    else
    {
        idThread::ReturnInt( 0 );
    }
}

/*
==================
idPlayer::Event_GetWeaponHandState
==================
*/
void idPlayer::Event_GetWeaponHandState()
{

    // weapon hand
    // not for animations like firing, this is to change hand pose if no weapon is present or if using guis interactively
    // 0 = no weapon
    // 1 = normal (weapon in hand)
    // 2 = pointy gui finger

    int handState = 0;
    int fingerPose = 0;
    int hand = vr_weaponHand.GetInteger();
    if ( commonVr->handInGui || commonVr->PDAforcetoggle || hands[ 1 - hand ].currentWeapon == weapon_pda )
    {
        handState = 2 ;
    }
    else if ( !spectating && weaponEnabled &&!hiddenWeapon && !gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) )
    {

        handState = 1 ;
    }


    if ( handState != 1 && vr_useHandPoses.GetBool() && !commonVr->PDAforcetoggle && !commonVr->PDAforced )
    {
        fingerPose = vr_weaponHand.GetInteger() == 0 ? commonVr->fingerPose[HAND_RIGHT] : commonVr->fingerPose[HAND_LEFT];
        fingerPose = fingerPose << 4;

        handState += fingerPose;
        //common->Printf( "Weapon hand finger pose = %d, %d\n", handState, fingerPose );
    }

    idThread::ReturnInt( handState );
}


/*
==================
idPlayer::Event_OpenPDA
==================
*/
void idPlayer::Event_OpenPDA( void ) {
	if ( !gameLocal.isMultiplayer ) {
        // Koz debug common->Printf( "idPlayer::Event_OpenPDA() calling TogglePDA\n" );
        TogglePDA( 1 - vr_weaponHand.GetInteger() );
	}
}

/*
==================
idPlayer::Event_InPDA
==================
*/
void idPlayer::Event_InPDA( void ) {
	idThread::ReturnInt( objectiveSystemOpen );
}

/*
==================
idPlayer::TeleportDeath
==================
*/
void idPlayer::TeleportDeath( int killer ) {
	teleportKiller = killer;
}

/* Carl: TouchTriggers() at every point in a pathfinding walk from the player's position to target, then teleport to target.
   It does so even if there is no path, or the current position and/or target aren't valid.
====================
idPlayer::TeleportPath
====================
*/
void idPlayer::TeleportPath( const idVec3& target )
{
    aasPath_t	path;
    int	originAreaNum, toAreaNum;
    idVec3 origin = physicsObj.GetOrigin();
    idVec3 trueOrigin = physicsObj.GetOrigin();
    idVec3 toPoint = target;
    idVec3 lastPos = origin;
    bool blocked = false;
    // Find path start and end areas and points
    originAreaNum = PointReachableAreaNum( origin );
    if ( aas )
        aas->PushPointIntoAreaNum( originAreaNum, origin );
    toAreaNum = PointReachableAreaNum( toPoint );
    if ( aas )
        aas->PushPointIntoAreaNum( toAreaNum, toPoint );
    // if there's no path, just go in a straight line (or should we just teleport straight there?)
    if ( !aas || !originAreaNum || !toAreaNum || !aas->WalkPathToGoal( path, originAreaNum, origin, toAreaNum, toPoint, travelFlags ) )
    {
        blocked = !TeleportPathSegment( physicsObj.GetOrigin(), target, lastPos );
    }
    else
    {
        // move from actual position to start of path
        blocked = !TeleportPathSegment( physicsObj.GetOrigin(), origin, lastPos );
        idVec3 currentPos = origin;
        int currentArea = originAreaNum;
        // Move along path
        while ( !blocked && currentArea && currentArea != toAreaNum )
        {
            if ( !TeleportPathSegment( currentPos, path.moveGoal, lastPos ) )
            {
                blocked = true;
                break;
            }
            currentPos = path.moveGoal;
            currentArea = path.moveAreaNum;
            // Find next path segment. Sometimes it tells us to go to the current location and gets stuck in a loop, so check for that.
            // TODO: Work out why it gets stuck in a loop, and fix it. Currently we just go in a straight line from stuck point to destination.
            if ( !aas->WalkPathToGoal( path, currentArea, currentPos, toAreaNum, toPoint, travelFlags ) || ( path.moveAreaNum == currentArea && path.moveGoal == currentPos ) )
            {
                path.moveGoal = toPoint;
                path.moveAreaNum = toAreaNum;
            }
        }
        // Is this needed? Doesn't hurt.
        blocked = blocked || !TeleportPathSegment( currentPos, toPoint, lastPos );
        // move from end of path to actual target
        blocked = blocked || !TeleportPathSegment( toPoint, target, lastPos );
    }

    if ( !blocked )
    {
        lastPos = target;
        // Check we didn't teleport inside a door that's not open.
        // It's OK to teleport THROUGH a closed but unlocked door, but we can't end up inside it.
        int				i, numClipModels;
        idClipModel* 	cm;
        idClipModel* 	clipModels[MAX_GENTITIES];
        idEntity* 		ent;
        trace_t			trace;

        memset( &trace, 0, sizeof(trace) );
        trace.endpos = target;
        trace.endAxis = GetPhysics()->GetAxis();

        numClipModels = gameLocal.clip.ClipModelsTouchingBounds( GetPhysics()->GetAbsBounds(), CONTENTS_SOLID, clipModels, MAX_GENTITIES );

        for ( i = 0; i < numClipModels; i++ )
        {
            cm = clipModels[i];

            // don't touch it if we're the owner
            if (cm->GetOwner() == this)
                continue;

            ent = cm->GetEntity();

            if ( !blocked && ent->IsType(idDoor::Type) )
            {
                idDoor *door = (idDoor *)ent;
                // A door that is in the process of opening falsely registers as open.
                // But we can rely on the fact that we're touching it, to know it's still partly closed.
                //if ( !door->IsOpen() )
                {
                    idVec3 away = door->GetPhysics()->GetOrigin() - target;
                    away.z = 0;
                    float dist = away.Length();
                    if (dist < 50.0f)
                    {
                        away /= dist;
                        lastPos = target + (away * (dist - 50));
                    }
                }
            }
        }
    }
    physicsObj.SetOrigin( trueOrigin );
    // Actually teleport

    if ( vr_teleportMode.GetInteger() == 0 )
    {
        Teleport( lastPos, viewAngles, NULL );
    }
    else
    {
        extern idCVar timescale;
        warpMove = true;
        noclip = true;
        warpDest = lastPos;
        //warpDest.z += 1;
        warpVel = ( warpDest - trueOrigin ) / 0.075f;  // 75 ms
        //warpVel[2] = warpVel[2] + 50; // add a small fixed upwards velocity to handle noclip problem
        warpTime = gameLocal.time + 75;
        timescale.SetFloat( 0.5f );
        //playerView.EnableBFGVision(true);
    }
}

/* Carl: TouchTriggers() at every point along straight line from start to end
====================
idPlayer::TeleportPathSegment
====================
*/
bool idPlayer::TeleportPathSegment( const idVec3& start, const idVec3& end, idVec3& lastPos )
{
    idVec3 total = end - start;
    float length = total.Length();
    if ( length >= 0.1f )
    {
        const float stepSize = 8.0f;
        int steps = (int)(length / stepSize);
        if (steps <= 0) steps = 1;
        idVec3 step = total / steps;
        idVec3 pos = start;
        for (int i = 0; i < steps; i++)
        {
            bool blocked = false;
            physicsObj.SetOrigin(pos);
            // Like TouchTriggers() but also checks for locked doors
            {
                int				i, numClipModels;
                idClipModel* 	cm;
                idClipModel* 	clipModels[MAX_GENTITIES];
                idEntity* 		ent;
                trace_t			trace;

                memset(&trace, 0, sizeof(trace));
                trace.endpos = pos;
                trace.endAxis = GetPhysics()->GetAxis();

                numClipModels = gameLocal.clip.ClipModelsTouchingBounds(GetPhysics()->GetAbsBounds(), CONTENTS_TRIGGER | CONTENTS_SOLID, clipModels, MAX_GENTITIES);

                for (i = 0; i < numClipModels; i++)
                {
                    cm = clipModels[i];

                    // don't touch it if we're the owner
                    if (cm->GetOwner() == this)
                    {
                        continue;
                    }

                    ent = cm->GetEntity();

                    if (!blocked && ent->IsType(idDoor::Type))
                    {
                        idDoor *door = (idDoor *)ent;
                        if (door->IsLocked() || ( !vr_teleportThroughDoors.GetBool() && (cm->GetContents() & CONTENTS_SOLID) ))
                        {
                            // check if we're moving toward the door
                            idVec3 away = door->GetPhysics()->GetOrigin() - pos;
                            away.z = 0;
                            float dist = away.Length();
                            if (dist < 60.0f)
                            {
                                away /= dist;
                                idVec3 my_dir = step;
                                my_dir.Normalize();
                                float angle = idMath::ACos(away * my_dir);
                                if (angle < DEG2RAD(45) || (angle < DEG2RAD(90) && dist < 20))
                                    blocked = true;
                                if (blocked && door->IsLocked())
                                {
                                    // Trigger the door to make the locked sound, if we're not close enough to happen naturally
                                    if (dist > 30)
                                    {
                                        physicsObj.SetOrigin(pos + (away * (dist - 30)));
                                        TouchTriggers();
                                    }
                                }
                            }
                        }
                    }

                    if (!ent->RespondsTo(EV_Touch) && !ent->HasSignal(SIG_TOUCH))
                    {
                        continue;
                    }

                    if (!GetPhysics()->ClipContents(cm))
                    {
                        continue;
                    }

                    SetTimeState ts(ent->timeGroup);

                    trace.c.contents = cm->GetContents();
                    trace.c.entityNum = cm->GetEntity()->entityNumber;
                    trace.c.id = cm->GetId();

                    ent->Signal(SIG_TOUCH);
                    ent->ProcessEvent(&EV_Touch, this, &trace);
                }
            }

            if (blocked)
                return false;
            lastPos = pos;
            pos += step;
        }
        // we don't call TouchTriggers after the final step because it's either
        // the start of the next path segment, or the teleport destination
    }
    return true;
}


/*
==================
idPlayer::Event_ExitTeleporter
==================
*/
void idPlayer::Event_ExitTeleporter( void ) {
	idEntity	*exitEnt;
	float		pushVel;

	// verify and setup
	exitEnt = teleportEntity;
	if ( !exitEnt ) {
		common->DPrintf( "Event_ExitTeleporter player %d while not being teleported\n", entityNumber );
		return;
	}

	pushVel = exitEnt->spawnArgs.GetFloat( "push", "300" );

	if ( gameLocal.isServer ) {
		ServerSendEvent( EVENT_EXIT_TELEPORTER, NULL, false, -1 );
	}

	SetPrivateCameraView( NULL );
	// setup origin and push according to the exit target
	SetOrigin( exitEnt->GetPhysics()->GetOrigin() + idVec3( 0, 0, CM_CLIP_EPSILON ) );
	SetViewAngles( exitEnt->GetPhysics()->GetAxis().ToAngles() );
	physicsObj.SetLinearVelocity( exitEnt->GetPhysics()->GetAxis()[ 0 ] * pushVel );
	physicsObj.ClearPushedVelocity();
	// teleport fx
	playerView.Flash( colorWhite, 120 );

	// clear the ik heights so model doesn't appear in the wrong place
	walkIK.EnableAll();

	UpdateVisuals();

	StartSound( "snd_teleport_exit", SND_CHANNEL_ANY, 0, false, NULL );

	if ( teleportKiller != -1 ) {
		// we got killed while being teleported
		Damage( gameLocal.entities[ teleportKiller ], gameLocal.entities[ teleportKiller ], vec3_origin, "damage_telefrag", 1.0f, INVALID_JOINT );
		teleportKiller = -1;
	} else {
		// kill anything that would have waited at teleport exit
		gameLocal.KillBox( this );
	}
	teleportEntity = NULL;
}
/*
==================
idPlayer::Event_ForceOrigin
==================
*/
void idPlayer::Event_ForceOrigin( idVec3& origin, idAngles& angles )
{
    SetOrigin( origin + idVec3( 0, 0, CM_CLIP_EPSILON ) );
    //SetViewAngles( angles );

    UpdateVisuals();
}

/*
================
idPlayer::ClientPredictionThink
================
*/
void idPlayer::ClientPredictionThink( void ) {
    UpdateSkinSetup();

    renderEntity_t *headRenderEnt;

	oldFlags = usercmd.flags;
	oldButtons = usercmd.buttons;

	usercmd = gameLocal.usercmds[ entityNumber ];

	if ( entityNumber != gameLocal.localClientNum ) {
		// ignore attack button of other clients. that's no good for predictions
		usercmd.buttons &= ~BUTTON_ATTACK;
	}

	buttonMask &= usercmd.buttons;
	usercmd.buttons &= ~buttonMask;

	if (mountedObject) {
		usercmd.forwardmove = 0;
		usercmd.rightmove = 0;
		usercmd.upmove = 0;
	}

	if ( objectiveSystemOpen ) {
		usercmd.forwardmove = 0;
		usercmd.rightmove = 0;
		usercmd.upmove = 0;
	}

	// clear the ik before we do anything else so the skeleton doesn't get updated twice
	walkIK.ClearJointMods();

    // Koz
    armIK.ClearJointMods();

	if ( gameLocal.isNewFrame ) {
		if ( ( usercmd.flags & UCF_IMPULSE_SEQUENCE ) != ( oldFlags & UCF_IMPULSE_SEQUENCE ) ) {
			PerformImpulse( usercmd.impulse );
		}
	}

	scoreBoardOpen = ( ( usercmd.buttons & BUTTON_SCORES ) != 0 || forceScoreBoard );

	AdjustSpeed();

	UpdateViewAngles();

	// update the smoothed view angles
	if ( gameLocal.framenum >= smoothedFrame && entityNumber != gameLocal.localClientNum ) {
		idAngles anglesDiff = viewAngles - smoothedAngles;
		anglesDiff.Normalize180();
		if ( idMath::Fabs( anglesDiff.yaw ) < 90.0f && idMath::Fabs( anglesDiff.pitch ) < 90.0f ) {
			// smoothen by pushing back to the previous angles
			viewAngles -= gameLocal.clientSmoothing * anglesDiff;
			viewAngles.Normalize180();
		}
		smoothedAngles = viewAngles;
	}
	smoothedOriginUpdated = false;

	if ( !af.IsActive() ) {
		AdjustBodyAngles();
	}

	if ( !isLagged ) {
		// don't allow client to move when lagged
		Move();
	}

	// update GUIs, Items, and character interactions
	UpdateFocus();

	// service animations
	if ( !spectating && !af.IsActive() ) {
		UpdateConditions();
		UpdateAnimState();
		CheckBlink();
	}

	// clear out our pain flag so we can tell if we recieve any damage between now and the next time we think
	AI_PAIN = false;

	// calculate the exact bobbed view position, which is used to
	// position the view weapon, among other things
	CalculateFirstPersonView();

	// this may use firstPersonView, or a thirdPerson / camera view
	CalculateRenderView();

	if ( !gameLocal.inCinematic && hands[ vr_weaponHand.GetInteger() ].weapon && ( health > 0 ) && !( gameLocal.isMultiplayer && spectating ) ) {
		UpdateWeapon();
	}

    UpdateNeckPose();

    UpdateFlashlight();

	UpdateHud();

	if ( gameLocal.isNewFrame ) {
		UpdatePowerUps();
	}

	UpdateDeathSkin( false );

	if ( head ) {
		headRenderEnt = head->GetRenderEntity();
	} else {
		headRenderEnt = NULL;
	}

	if ( headRenderEnt ) {
		if ( influenceSkin ) {
			headRenderEnt->customSkin = influenceSkin;
		} else {
			headRenderEnt->customSkin = NULL;
		}
	}

	if ( gameLocal.isMultiplayer || g_showPlayerShadow.GetBool() ) {
		renderEntity.suppressShadowInViewID	= 0;
		if ( headRenderEnt ) {
			headRenderEnt->suppressShadowInViewID = 0;
		}
	} else {
		renderEntity.suppressShadowInViewID	= entityNumber+1;
		if ( headRenderEnt ) {
            // Koz begin
            if ( game->isVR && pVRClientInfo )
            {
                headRenderEnt->suppressShadowInViewID = 0; //Carl:Draw the head's shadow when showing the body
            } else {
                headRenderEnt->suppressShadowInViewID = entityNumber + 1;
            }
		}
	}
	// never cast shadows from our first-person muzzle flashes
	renderEntity.suppressShadowInLightID = LIGHTID_VIEW_MUZZLE_FLASH + entityNumber;
	if ( headRenderEnt ) {
		headRenderEnt->suppressShadowInLightID = LIGHTID_VIEW_MUZZLE_FLASH + entityNumber;
	}

	if ( !gameLocal.inCinematic ) {
        if ( armIK.IsInitialized() ) armIK.Evaluate();
	    UpdateAnimation();
	}

	if (enviroSuitLight.IsValid()) {
		idAngles lightAng = firstPersonViewAxis.ToAngles();
		idVec3 lightOrg = firstPersonViewOrigin;
		const idDict *lightDef = gameLocal.FindEntityDefDict("envirosuit_light", false);

		idVec3 enviroOffset = lightDef->GetVector("enviro_offset");
		idVec3 enviroAngleOffset = lightDef->GetVector("enviro_angle_offset");

		lightOrg += (enviroOffset.x * firstPersonViewAxis[0]);
		lightOrg += (enviroOffset.y * firstPersonViewAxis[1]);
		lightOrg += (enviroOffset.z * firstPersonViewAxis[2]);
		lightAng.pitch += enviroAngleOffset.x;
		lightAng.yaw += enviroAngleOffset.y;
		lightAng.roll += enviroAngleOffset.z;

		enviroSuitLight.GetEntity()->GetPhysics()->SetOrigin(lightOrg);
		enviroSuitLight.GetEntity()->GetPhysics()->SetAxis(lightAng.ToMat3());
		enviroSuitLight.GetEntity()->UpdateVisuals();
		enviroSuitLight.GetEntity()->Present();
	}

	if ( gameLocal.isMultiplayer ) {
		DrawPlayerIcons();
	}

	Present();

	UpdateDamageEffects();

	LinkCombat();

    UpdateTeleportAim();

	if ( gameLocal.isNewFrame && entityNumber == gameLocal.localClientNum ) {
		playerView.CalculateShake();
	}

	// determine if portal sky is in pvs
	pvsHandle_t	clientPVS = gameLocal.pvs.SetupCurrentPVS(GetPVSAreas(), GetNumPVSAreas());
	gameLocal.portalSkyActive = gameLocal.pvs.CheckAreasForPortalSky(clientPVS, GetPhysics()->GetOrigin());
	gameLocal.pvs.FreeCurrentPVS(clientPVS);
}

/*
================
idPlayer::GetPhysicsToVisualTransform
================
*/
bool idPlayer::GetPhysicsToVisualTransform( idVec3 &origin, idMat3 &axis ) {
	if ( af.IsActive() ) {
		af.GetPhysicsToVisualTransform( origin, axis );
		return true;
	}

	// smoothen the rendered origin and angles of other clients
	// smooth self origin if snapshots are telling us prediction is off
	if ( gameLocal.isClient && gameLocal.framenum >= smoothedFrame && ( entityNumber != gameLocal.localClientNum || selfSmooth ) ) {
		// render origin and axis
		idMat3 renderAxis = viewAxis * GetPhysics()->GetAxis();
		idVec3 renderOrigin = GetPhysics()->GetOrigin() + modelOffset * renderAxis;

		// update the smoothed origin
		if ( !smoothedOriginUpdated ) {
			idVec2 originDiff = renderOrigin.ToVec2() - smoothedOrigin.ToVec2();
			if ( originDiff.LengthSqr() < Square( 100.0f ) ) {
				// smoothen by pushing back to the previous position
				if ( selfSmooth ) {
					assert( entityNumber == gameLocal.localClientNum );
					renderOrigin.ToVec2() -= net_clientSelfSmoothing.GetFloat() * originDiff;
				} else {
					renderOrigin.ToVec2() -= gameLocal.clientSmoothing * originDiff;
				}
			}
			smoothedOrigin = renderOrigin;

			smoothedFrame = gameLocal.framenum;
			smoothedOriginUpdated = true;
		}

		axis = idAngles( 0.0f, smoothedAngles.yaw, 0.0f ).ToMat3();
		origin = ( smoothedOrigin - GetPhysics()->GetOrigin() ) * axis.Transpose();

	} else {

		axis = viewAxis;
		origin = modelOffset;
	}
	return true;
}

/*
===============
idPlayer::GetQuickSlot
===============
*/
int idPlayer::GetQuickSlot( int index )
{

    if( index >= NUM_QUICK_SLOTS || index < 0 )
    {
        return -1;
    }

    return quickSlot[ index ];
}

/*
================
idPlayer::GetPhysicsToSoundTransform
================
*/
bool idPlayer::GetPhysicsToSoundTransform( idVec3 &origin, idMat3 &axis ) {
	idCamera *camera;

	if ( privateCameraView ) {
		camera = privateCameraView;
	} else {
		camera = gameLocal.GetCamera();
	}

	if ( camera ) {
		renderView_t view;

		memset( &view, 0, sizeof( view ) );
		camera->GetViewParms( &view );
		origin = view.vieworg;
		axis = view.viewaxis;
		return true;
	} else {
		return idActor::GetPhysicsToSoundTransform( origin, axis );
	}
}

/*
================
idPlayer::WriteToSnapshot
================
*/
void idPlayer::WriteToSnapshot( idBitMsgDelta &msg ) const {
	physicsObj.WriteToSnapshot( msg );
	WriteBindToSnapshot( msg );
	msg.WriteDeltaFloat( 0.0f, deltaViewAngles[0] );
	msg.WriteDeltaFloat( 0.0f, deltaViewAngles[1] );
	msg.WriteDeltaFloat( 0.0f, deltaViewAngles[2] );
	msg.WriteShort( health );
	msg.WriteBits( gameLocal.ServerRemapDecl( -1, DECL_ENTITYDEF, lastDamageDef ), gameLocal.entityDefBits );
	msg.WriteDir( lastDamageDir, 9 );
	msg.WriteShort( lastDamageLocation );
	msg.WriteBits( hands[vr_weaponHand.GetInteger()].idealWeapon, idMath::BitsForInteger( MAX_WEAPONS ) );
	msg.WriteBits( inventory.weapons, MAX_WEAPONS );
	msg.WriteBits( hands[vr_weaponHand.GetInteger()].weapon.GetSpawnId(), 32 ); // Carl dont change msg format
	msg.WriteBits( spectator, idMath::BitsForInteger( MAX_CLIENTS ) );
	msg.WriteBits( lastHitToggle, 1 );
	msg.WriteBits( hands[ vr_weaponHand.GetInteger() ].weaponGone, 1 );
	msg.WriteBits( isLagged, 1 );
	msg.WriteBits( isChatting, 1 );
	msg.WriteBits(enviroSuitLight.GetSpawnId(), 32);
}

/*
================
idPlayer::ReadFromSnapshot
================
*/
void idPlayer::ReadFromSnapshot( const idBitMsgDelta &msg ) {
	int		i, oldHealth, newIdealWeapon, weaponSpawnId;
    int		flashlightSpawnId;
	bool	newHitToggle, stateHitch;

	if ( snapshotSequence - lastSnapshotSequence > 1 ) {
		stateHitch = true;
	} else {
		stateHitch = false;
	}
	lastSnapshotSequence = snapshotSequence;

	oldHealth = health;

	physicsObj.ReadFromSnapshot( msg );
	ReadBindFromSnapshot( msg );
	deltaViewAngles[0] = msg.ReadDeltaFloat( 0.0f );
	deltaViewAngles[1] = msg.ReadDeltaFloat( 0.0f );
	deltaViewAngles[2] = msg.ReadDeltaFloat( 0.0f );
	health = msg.ReadShort();
	lastDamageDef = gameLocal.ClientRemapDecl( DECL_ENTITYDEF, msg.ReadBits( gameLocal.entityDefBits ) );
	lastDamageDir = msg.ReadDir( 9 );
	lastDamageLocation = msg.ReadShort();
	newIdealWeapon = msg.ReadBits( idMath::BitsForInteger( MAX_WEAPONS ) );
	inventory.weapons = msg.ReadBits( MAX_WEAPONS );
	weaponSpawnId = msg.ReadBits( 32 );
	spectator = msg.ReadBits( idMath::BitsForInteger( MAX_CLIENTS ) );
	newHitToggle = msg.ReadBits( 1 ) != 0;
	weaponGone = msg.ReadBits( 1 ) != 0;
	isLagged = msg.ReadBits( 1 ) != 0;
	isChatting = msg.ReadBits( 1 ) != 0;

	int enviroSpawnId;
	enviroSpawnId = msg.ReadBits(32);
	enviroSuitLight.SetSpawnId(enviroSpawnId);

	// no msg reading below this

    if( hands[vr_weaponHand.GetInteger()].weapon.SetSpawnId( weaponSpawnId ) )
    {
        if( hands[ vr_weaponHand.GetInteger() ].weapon )
        {
            // maintain ownership locally
            hands[ vr_weaponHand.GetInteger() ].weapon->SetOwner( this, vr_weaponHand.GetInteger() );
        }
        hands[ vr_weaponHand.GetInteger() ].currentWeapon = -1;
    }

    if( flashlight.SetSpawnId( flashlightSpawnId ) )
    {
        if( flashlight )
        {
            flashlight->SetFlashlightOwner( this );
        }
    }

	// if not a local client assume the client has all ammo types
	if ( entityNumber != gameLocal.localClientNum ) {
		for( i = 0; i < AMMO_NUMTYPES; i++ ) {
			inventory.ammo[ i ] = 999;
		}
	}

	if ( oldHealth > 0 && health <= 0 ) {
		if ( stateHitch ) {
			// so we just hide and don't show a death skin
			UpdateDeathSkin( true );
		}
		// die
		AI_DEAD = true;
		ClearPowerUps();
		SetAnimState( ANIMCHANNEL_LEGS, "Legs_Death", 4 );
		SetAnimState( ANIMCHANNEL_TORSO, "Torso_Death", 4 );
		SetWaitState( "" );
		animator.ClearAllJoints();
		if ( entityNumber == gameLocal.localClientNum ) {
			playerView.Fade( colorBlack, 12000 );
		}
		StartRagdoll();
		physicsObj.SetMovementType( PM_DEAD );
		if ( !stateHitch ) {
			StartSound( "snd_death", SND_CHANNEL_VOICE, 0, false, NULL );
		}
        for( int h = 0; h < 2; h++ )
        {
            if( hands[ h ].weapon )
                hands[ h ].weapon->OwnerDied();
        }
        if( flashlight )
        {
            FlashlightOff();
            flashlight->OwnerDied();
        }

        if( IsLocallyControlled() )
        {
            ControllerShakeFromDamage( oldHealth - health );
        }

		/*{
			common->Vibrate(250, 0, 1.0);
			common->Vibrate(250, 1, 1.0);
		}*/
	} else if ( oldHealth <= 0 && health > 0 ) {
		// respawn
		Init();
		StopRagdoll();
		SetPhysics( &physicsObj );
		physicsObj.EnableClip();
		SetCombatContents( true );
	} else if ( health < oldHealth && health > 0 ) {
		if ( stateHitch ) {
			lastDmgTime = gameLocal.time;
		} else {
			// damage feedback
			const idDeclEntityDef *def = static_cast<const idDeclEntityDef *>( declManager->DeclByIndex( DECL_ENTITYDEF, lastDamageDef, false ) );
			if ( def ) {
				playerView.DamageImpulse( lastDamageDir * viewAxis.Transpose(), &def->dict );
				AI_PAIN = Pain( NULL, NULL, oldHealth - health, lastDamageDir, lastDamageLocation );
				lastDmgTime = gameLocal.time;
			} else {
				common->Warning( "NET: no damage def for damage feedback '%d'\n", lastDamageDef );
			}

			if( IsLocallyControlled() )
			{
				ControllerShakeFromDamage( oldHealth - health );
			}
			/*{
				common->Vibrate(250, 0, 0.6);
				common->Vibrate(250, 1, 0.6);
			}*/
		}
	} else if ( health > oldHealth && PowerUpActive( MEGAHEALTH ) && !stateHitch ) {
		// just pulse, for any health raise
		healthPulse = true;
	}

	// If the player is alive, restore proper physics object
	if ( health > 0 && IsActiveAF() ) {
		StopRagdoll();
		SetPhysics( &physicsObj );
		physicsObj.EnableClip();
		SetCombatContents( true );
	}

    const int oldIdealWeapon = hands[ vr_weaponHand.GetInteger() ].idealWeapon;
    //GB Not using Predicted Values - seee if this causes any problems
	//hands[ vr_weaponHand.GetInteger() ].idealWeapon.UpdateFromSnapshot( newIdealWeapon, GetEntityNumber() );

    if( oldIdealWeapon != hands[ vr_weaponHand.GetInteger() ].idealWeapon )
    {
        if( snapshotStale )
        {
            weaponCatchup = true;
        }
        UpdateHudWeapon( vr_weaponHand.GetInteger() );
    }

	if ( lastHitToggle != newHitToggle ) {
		SetLastHitTime( gameLocal.realClientTime );
	}

	if ( msg.HasChanged() ) {
		UpdateVisuals();
	}
}

/*
================
idPlayer::WritePlayerStateToSnapshot
================
*/
void idPlayer::WritePlayerStateToSnapshot( idBitMsgDelta &msg ) const {
	int i;

	msg.WriteByte( bobCycle );
	msg.WriteInt( stepUpTime );
	msg.WriteFloat( stepUpDelta );
	msg.WriteInt( inventory.weapons );
	msg.WriteByte( inventory.armor );

	for( i = 0; i < AMMO_NUMTYPES; i++ ) {
		msg.WriteBits( inventory.ammo[i], ASYNC_PLAYER_INV_AMMO_BITS );
	}
	for( i = 0; i < MAX_WEAPONS; i++ ) {
		msg.WriteBits( inventory.clip[i], ASYNC_PLAYER_INV_CLIP_BITS );
	}
}

/*
================
idPlayer::ReadPlayerStateFromSnapshot
================
*/
void idPlayer::ReadPlayerStateFromSnapshot( const idBitMsgDelta &msg ) {
	int i, ammo;

	bobCycle = msg.ReadByte();
	stepUpTime = msg.ReadInt();
	stepUpDelta = msg.ReadFloat();
	inventory.weapons = msg.ReadInt();
	inventory.armor = msg.ReadByte();

	for( i = 0; i < AMMO_NUMTYPES; i++ ) {
		ammo = msg.ReadBits( ASYNC_PLAYER_INV_AMMO_BITS );
		if ( gameLocal.time >= inventory.ammoPredictTime ) {
			inventory.ammo[ i ] = ammo;
		}
	}
	for( i = 0; i < MAX_WEAPONS; i++ ) {
		inventory.clip[i] = msg.ReadBits( ASYNC_PLAYER_INV_CLIP_BITS );
	}
}

/*
==============
Koz idPlayer::RecreateCopyJoints()
After restoring from a savegame with different player models, the copyjoints for the head are wrong.
This will recreate a working copyJoint list.
==============
*/
void idPlayer::RecreateCopyJoints()
{
    idEntity* headEnt = head;
    idAnimator* headAnimator;
    const idKeyValue* kv;
    idStr jointName;
    copyJoints_t	copyJoint;

    copyJoints.Clear();
    if ( headEnt )
    {
        headAnimator = headEnt->GetAnimator();
    }
    else
    {
        headAnimator = &animator;
    }

    if ( headEnt )
    {
        // set up the list of joints to copy to the head
        for ( kv = spawnArgs.MatchPrefix( "copy_joint", NULL ); kv != NULL; kv = spawnArgs.MatchPrefix( "copy_joint", kv ) )
        {
            if ( kv->GetValue() == "" )
            {
                // probably clearing out inherited key, so skip it
                continue;
            }

            jointName = kv->GetKey();
            if ( jointName.StripLeadingOnce( "copy_joint_world " ) )
            {
                copyJoint.mod = JOINTMOD_WORLD_OVERRIDE;
            }
            else
            {
                jointName.StripLeadingOnce( "copy_joint " );
                copyJoint.mod = JOINTMOD_LOCAL_OVERRIDE;
            }

            copyJoint.from = animator.GetJointHandle( jointName );
            if ( copyJoint.from == INVALID_JOINT )
            {
                gameLocal.Warning( "Unknown copy_joint '%s' on entity %s", jointName.c_str(), name.c_str() );
                continue;
            }

            jointName = kv->GetValue();
            copyJoint.to = headAnimator->GetJointHandle( jointName );
            if ( copyJoint.to == INVALID_JOINT )
            {
                gameLocal.Warning( "Unknown copy_joint '%s' on head of entity %s", jointName.c_str(), name.c_str() );
                continue;
            }

            copyJoints.Append( copyJoint );
        }
    }
}


/*
================
idPlayer::ServerReceiveEvent
================
*/
bool idPlayer::ServerReceiveEvent( int event, int time, const idBitMsg &msg ) {

	if ( idEntity::ServerReceiveEvent( event, time, msg ) ) {
		return true;
	}

	// client->server events
	switch( event ) {
		case EVENT_IMPULSE: {
			PerformImpulse( msg.ReadBits( 6 ) );
			return true;
		}
		default: {
			return false;
		}
	}
}

/*
================
idPlayer::ClientReceiveEvent
================
*/
bool idPlayer::ClientReceiveEvent( int event, int time, const idBitMsg &msg ) {
	int powerup;
	bool start;

	switch ( event ) {
		case EVENT_EXIT_TELEPORTER:
			Event_ExitTeleporter();
			return true;
		case EVENT_ABORT_TELEPORTER:
			SetPrivateCameraView( NULL );
			return true;
		case EVENT_POWERUP: {
			powerup = msg.ReadShort();
			start = msg.ReadBits( 1 ) != 0;
			if ( start ) {
				GivePowerUp( powerup, 0 );
			} else {
				ClearPowerup( powerup );
			}
			return true;
		}
		case EVENT_PICKUPNAME: {
			char buf[MAX_EVENT_PARAM_SIZE];
			msg.ReadString(buf, MAX_EVENT_PARAM_SIZE);
			inventory.AddPickupName(buf, "", this); //_D3XP
			return true;
		}
		case EVENT_SPECTATE: {
			bool spectate = ( msg.ReadBits( 1 ) != 0 );
			Spectate( spectate );
			return true;
		}
		case EVENT_ADD_DAMAGE_EFFECT: {
			if ( spectating ) {
				// if we're spectating, ignore
				// happens if the event and the spectate change are written on the server during the same frame (fraglimit)
				return true;
			}
			break;
		}
		default:
			break;
	}

	return idActor::ClientReceiveEvent( event, time, msg );
}

/*
================
idPlayer::Hide
================
*/
void idPlayer::Hide( void ) {
	idWeapon *weap;

	idActor::Hide();
    for( int h = 0; h < 2; h++ )
    {
        weap = hands[ h ].weapon;
        if( weap )
            weap->HideWorldModel();
    }
    idWeapon* flashlight_weapon = flashlight;
    if( flashlight_weapon )
    {
        flashlight_weapon->HideWorldModel();
    }
}

/*
================
idPlayer::Show
================
*/
void idPlayer::Show( void ) {
	idWeapon *weap;

	idActor::Show();
    for( int h = 0; h < 2; h++ )
    {
        weap = hands[ h ].weapon;
        if( weap )
            weap->ShowWorldModel();
    }
    idWeapon* flashlight_weapon = flashlight;
    if( flashlight_weapon )
    {
        flashlight_weapon->ShowWorldModel();
    }
}

/*
===============
idPlayer::StartAudioLog
===============
*/
void idPlayer::StartAudioLog( void ) {
	if ( hud ) {
		hud->HandleNamedEvent( "audioLogUp" );
	}
}

/*
===============
idPlayer::StopAudioLog
===============
*/
void idPlayer::StopAudioLog( void ) {
	if ( hud ) {
		hud->HandleNamedEvent( "audioLogDown" );
	}
}

/*
===============
idPlayer::ShowTip
===============
*/
void idPlayer::ShowTip( const char *title, const char *tip, bool autoHide ) {
	if ( tipUp ) {
		return;
	}
	hud->SetStateString( "tip", tip );
	hud->SetStateString( "tiptitle", title );
	hud->HandleNamedEvent( "tipWindowUp" );
	if ( autoHide ) {
		PostEventSec( &EV_Player_HideTip, 5.0f );
	}
	tipUp = true;
}

/*
===============
idPlayer::HideTip
===============
*/
void idPlayer::HideTip( void ) {
	hud->HandleNamedEvent( "tipWindowDown" );
	tipUp = false;
}

/*
===============
idPlayer::Event_HideTip
===============
*/
void idPlayer::Event_HideTip( void ) {
	HideTip();
}

/*
===============
idPlayer::ShowObjective
===============
*/
void idPlayer::ShowObjective( const char *obj ) {
	hud->HandleNamedEvent( obj );
	objectiveUp = true;
}

/*
===============
idPlayer::HideObjective
===============
*/
void idPlayer::HideObjective( void ) {
	hud->HandleNamedEvent( "closeObjective" );
	objectiveUp = false;
}

/*
===============
idPlayer::Event_StopAudioLog
===============
*/
void idPlayer::Event_StopAudioLog( void ) {
	StopAudioLog();
}

/*
===============
idPlayer::SetSpectateOrigin
===============
*/
void idPlayer::SetSpectateOrigin( void ) {
	idVec3 neworig;

	neworig = GetPhysics()->GetOrigin();
	neworig[ 2 ] += EyeHeight();
	neworig[ 2 ] += 25;
	SetOrigin( neworig );
}

/*
==============
idPlayer::SetWeaponHandPose()
Updates the pose of the player model weapon hand
======
*/
void idPlayer::SetWeaponHandPose()
{
    const function_t* func;
    func = scriptObject.GetFunction( "SetWeaponHandPose" );
    if ( func )
    {
        // use the frameCommandThread since it's safe to use outside of framecommands
        //common->Printf( "Calling SetWeaponHandPose\n" );g
        gameLocal.frameCommandThread->CallFunction( this, func, true );
        gameLocal.frameCommandThread->Execute();

    }
    else
    {
        common->Warning( "Can't find function 'SetWeaponHandPose' in object '%s'", scriptObject.GetTypeName() );
        return;
    }
}

/*
==============
idPlayer::SetupHolsterSlot

stashed: -1 = switch weapons, 1 = empty holster of stashed weapon, 0 = stash current weapon in holster but don't switch
==============
*/
void idPlayer::SetupHolsterSlot( int hand, int stashed )
{
    // if there's nothing to stash because we were already using fists or PDA
    if ( stashed == 0 && (hands[hand].currentWeapon == weapon_pda || hands[ hand ].currentWeapon == weapon_fists) )
        return;
    // if we were using fists before activating pda, we didn't stash anything in our holster, so don't unstash anything
    if( stashed == 1 && hands[ hand ].previousWeapon == weapon_fists )
        return;
    // if we want to read or switch the current weapon but it's not ready
    if( !hands[ hand ].weapon->IsReady() && stashed != 1 )
    {
        return;
    }

    const char * modelname;
    idRenderModel* renderModel;

    if ( stashed == 0 )
    {
        extraHolsteredWeapon = holsteredWeapon;
        if ( holsterRenderEntity.hModel )
            extraHolsteredWeaponModel = holsterRenderEntity.hModel->Name();
        else
            extraHolsteredWeaponModel = NULL;
    }

    FreeHolsterSlot();
    if( vr_slotDisable.GetBool() )
    {
        return;
    }

    if ( stashed == 1 )
    {
        modelname = extraHolsteredWeaponModel;
        extraHolsteredWeaponModel = NULL;
    }
    else
        modelname = hands[hand].weapon->weaponDef->dict.GetString("model");

    // can we holster?
    if( !modelname ||
        strcmp(modelname, "models/weapons/soulcube/w_soulcube.lwo") == 0 ||
        strcmp(modelname, "_DEFAULT") == 0 ||
        strcmp(modelname, "models/items/grenade_ammo/grenade.lwo") == 0 ||
        strcmp(modelname, "models/items/pda/pda_world.lwo") == 0 ||
        !(renderModel = renderModelManager->FindModel( modelname )) )
    {
        // can't holster, just unholster
        if( holsteredWeapon != weapon_fists )
        {
            if ( stashed != 0 )
                SelectWeapon(holsteredWeapon, false, true);
            holsteredWeapon = weapon_fists;
            memset(&holsterRenderEntity, 0, sizeof(holsterRenderEntity));
        }
        return;
    }

    // we can holster! so unholster or change weapons
    if (stashed < 0)
    {
        int prevWeapon = hands[hand].currentWeapon;
        hands[hand].SelectWeapon(holsteredWeapon, false, true);
        holsteredWeapon = prevWeapon;
    }
    else
    {
        if (stashed == 0) // stash current weapon, holstered weapon moves to invisible "extra" slot
        {
            holsteredWeapon = hands[hand].currentWeapon;
        }
        else // unstash holstered weapon, extra weapon moves back to holster
        {
            hands[ hand ].SelectWeapon(holsteredWeapon, true, true);
            holsteredWeapon = extraHolsteredWeapon;
            extraHolsteredWeapon = weapon_fists;
        }
    }

    memset( &holsterRenderEntity, 0, sizeof( holsterRenderEntity ) );

    holsterRenderEntity.hModel = renderModel;
    if( holsterRenderEntity.hModel )
    {
        holsterRenderEntity.hModel->Reset();
        holsterRenderEntity.bounds = holsterRenderEntity.hModel->Bounds( &holsterRenderEntity );
    }
    holsterRenderEntity.shaderParms[ SHADERPARM_RED ]	= 1.0f;
    holsterRenderEntity.shaderParms[ SHADERPARM_GREEN ] = 1.0f;
    holsterRenderEntity.shaderParms[ SHADERPARM_BLUE ]	= 1.0f;
    holsterRenderEntity.shaderParms[3] = 1.0f;
    holsterRenderEntity.shaderParms[ SHADERPARM_TIMEOFFSET ] = 0.0f;
    holsterRenderEntity.shaderParms[5] = 0.0f;
    holsterRenderEntity.shaderParms[6] = 0.0f;
    holsterRenderEntity.shaderParms[7] = 0.0f;

    if( strcmp(modelname, "models/weapons/pistol/w_pistol.lwo") == 0 )
    {
        holsterAxis = idAngles(90, 0, 0).ToMat3() * 0.75f;
    }
    else if( strcmp(modelname, "models/weapons/shotgun/w_shotgun2.lwo") == 0 ||
             strcmp(modelname, "models/weapons/bfg/bfg_world.lwo") == 0)
    {
        holsterAxis = idAngles(0, -90, -90).ToMat3();
    }
	else if( strcmp(modelname, "models/weapons/grabber/grabber_world.ase") == 0 )
	{
		holsterAxis = idAngles(-90, 180, 0).ToMat3() * 0.5f;
	}
    else if (strcmp(modelname, "models/weapons/machinegun/w_machinegun.lwo") == 0)
    {
        holsterAxis = idAngles(0, 90, 90).ToMat3() * 0.75f;
    }
    else if (strcmp(modelname, "models/weapons/plasmagun/plasmagun_world.lwo") == 0)
    {
        holsterAxis = idAngles(0, 90, 90).ToMat3() * 0.75f;
    }
    else if (strcmp(modelname, "models/weapons/chainsaw/w_chainsaw.lwo") == 0)
    {
        holsterAxis = idAngles(0, 90, 90).ToMat3() * 0.9f;
    }
    else if (strcmp(modelname, "models/weapons/chaingun/w_chaingun.lwo") == 0)
    {
        holsterAxis = idAngles(0, 90, 90).ToMat3() * 0.9f;
    }
    else
    {
        holsterAxis = idAngles(0, 90, 90).ToMat3();
    }
}

/*
==============
idPlayer::SetupPDASlot
==============
*/
void idPlayer::SetupPDASlot( bool holsterPDA )
{
    const char * modelname;
    idRenderModel* renderModel;

    FreePDASlot();

    if( vr_slotDisable.GetBool() )
    {
        return;
    }

    if ( holsterPDA )
    {
        // we will holster the PDA
        modelname = "models/items/pda/pda_world.lwo";
        pdaHolsterAxis = (pdaAngle1.ToMat3() * pdaAngle2.ToMat3() * pdaAngle3.ToMat3()) * 0.6f;
    }
    else
    {
        // we will holster the flashlight if carrying it
        if ( commonVr->currentFlashlightMode == FLASHLIGHT_HAND && flashlight->IsLinked() && !spectating && weaponEnabled && !hiddenWeapon && !gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) )
        {
            flashlightPreviouslyInHand = true;
            modelname = flashlight->weaponDef->dict.GetString("model");
            pdaHolsterAxis = idAngles(0, 90, 90).ToMat3();
        }
        else modelname = "";
    }

    memset( &pdaRenderEntity, 0, sizeof( pdaRenderEntity ) );

    // can we holster?
    if ( !(renderModel = renderModelManager->FindModel(modelname)) )
    {
        // can't holster, just unholster
        return;
    }

    pdaRenderEntity.hModel = renderModel;
    if (pdaRenderEntity.hModel)
    {
        pdaRenderEntity.hModel->Reset();
        pdaRenderEntity.bounds = pdaRenderEntity.hModel->Bounds( &pdaRenderEntity );
    }
    pdaRenderEntity.shaderParms[ SHADERPARM_RED ]	= 1.0f;
    pdaRenderEntity.shaderParms[ SHADERPARM_GREEN ] = 1.0f;
    pdaRenderEntity.shaderParms[ SHADERPARM_BLUE ]	= 1.0f;
    pdaRenderEntity.shaderParms[3] = 1.0f;
    pdaRenderEntity.shaderParms[ SHADERPARM_TIMEOFFSET ] = 0.0f;
    pdaRenderEntity.shaderParms[5] = 0.0f;
    pdaRenderEntity.shaderParms[6] = 0.0f;
    pdaRenderEntity.shaderParms[7] = 0.0f;
}


/*
===============
idPlayer::RemoveWeapon
===============
*/
void idPlayer::RemoveWeapon( const char *weap ) {
	if ( weap && *weap ) {
		inventory.Drop( spawnArgs, spawnArgs.GetString( weap ), -1 );
	}
}

/*
========================
idPlayer::ResetControllerShake
========================
*/
void idPlayer::ResetControllerShake()
{
    for( int h = 0; h < 2; h++ )
        hands[h].ResetControllerShake();
}

/*
===============
idPlayer::CanShowWeaponViewmodel
===============
*/
bool idPlayer::CanShowWeaponViewmodel( void ) const {
	return showWeaponViewModel;
}

bool idPlayer::CheckTeleportPathSegment(const idVec3& start, const idVec3& end, idVec3& lastPos)
{
    idVec3 total = end - start;
    float length = total.Length();
    if ( length >= 0.1f )
    {
        const float stepSize = 15.0f; // We have a radius of 16, so this should catch everything
        int steps = (int)( length / stepSize );
        if ( steps <= 0 ) steps = 1;
        idVec3 step = total / steps;
        idVec3 pos = start;
        for ( int i = 0; i < steps; i++ )
        {
            physicsObj.SetOrigin( pos );
            // Check for doors
            {
                int				i, numClipModels;
                idClipModel* 	cm;
                idClipModel* 	clipModels[MAX_GENTITIES];
                idEntity* 		ent;
                trace_t			trace;

                memset( &trace, 0, sizeof(trace) );
                trace.endpos = pos;
                trace.endAxis = GetPhysics()->GetAxis();

                numClipModels = gameLocal.clip.ClipModelsTouchingBounds( GetPhysics()->GetAbsBounds(), CONTENTS_SOLID, clipModels, MAX_GENTITIES );

                for ( i = 0; i < numClipModels; i++ )
                {
                    cm = clipModels[i];

                    // don't touch it if we're the owner
                    if (cm->GetOwner() == this)
                    {
                        continue;
                    }

                    ent = cm->GetEntity();

                    // check if it's a closed or locked door
                    if ( ent->IsType(idDoor::Type) )
                    {
                        idDoor *door = (idDoor *)ent;
                        if ( door->IsLocked() || ( !vr_teleportThroughDoors.GetBool() && ( cm->GetContents() & CONTENTS_SOLID ) ) )
                        {
                            // check if we're moving toward the door
                            idVec3 away = door->GetPhysics()->GetOrigin() - pos;
                            away.z = 0;
                            float dist = away.Length();
                            if ( dist < 60.0f )
                            {
                                away /= dist;
                                idVec3 my_dir = step;
                                my_dir.Normalize();
                                float angle = idMath::ACos(away * my_dir);
                                if ( angle < DEG2RAD( 45 ) || ( angle < DEG2RAD( 90 ) && dist < 20 ) )
                                    return false;
                            }
                        }
                    }
                        // Check if it's a glass window. func_static with textures/glass/glass2
                    else if ( ent->IsType( idStaticEntity::Type ) )
                    {
                        renderEntity_t *rent = ent->GetRenderEntity();
                        if ( rent )
                        {
                            const idMaterial *mat = rent->customShader;
                            if ( !mat )
                                mat = rent->referenceShader;
                            if ( !mat && rent->hModel )
                            {
                                for ( int i = 0; i < rent->hModel->NumSurfaces(); i++ )
                                    if ( rent->hModel->Surface(i)->shader )
                                    {
                                        mat = rent->hModel->Surface(i)->shader;
                                        break;
                                    }
                            }
                            if ( mat )
                            {
                                const char* name = mat->GetName();
                                // trying to teleport through glass: textures/glass/glass2 or textures/glass/glass1
                                if ( name && idStr::Cmpn( name, "textures/glass/glass", 20 ) == 0 )
                                    return false;
                                //else if (name)
                                //	common->Printf("teleporting through \"%s\"\n", name);
                                //else
                                //	common->Printf("teleporting through NULL\n");
                            }
                            else if ( ent->name )
                            {
                                //common->Printf("teleporting through entity %s", ent->name);
                            }

                        }
                    }
                }
            }

            lastPos = pos;
            pos += step;
        }
    }
    return true;
}

/* Carl: Check if we are trying to teleport through a locked or closed door
====================
idPlayer::CheckTeleportPath
====================
*/
bool idPlayer::CheckTeleportPath(const idVec3& target, int toAreaNum)
{
    aasPath_t	path;
    int	originAreaNum;
    idVec3 origin = physicsObj.GetOrigin();
    idVec3 trueOrigin = origin;
    idVec3 toPoint = target;
    idVec3 lastPos = origin;
    bool blocked = false;
    // Find path start and end areas and points
    originAreaNum = PointReachableAreaNum( origin );
    if ( aas )
        aas->PushPointIntoAreaNum( originAreaNum, origin );
    if ( !toAreaNum )
        toAreaNum = PointReachableAreaNum( toPoint );
    if ( aas )
        aas->PushPointIntoAreaNum( toAreaNum, toPoint );
    // if there's no path, just go in a straight line
    if ( !aas || !originAreaNum || !toAreaNum || !aas->WalkPathToGoal(path, originAreaNum, origin, toAreaNum, toPoint, travelFlags ) )
    {
        if ( !CheckTeleportPathSegment( physicsObj.GetOrigin(), target, lastPos ) )
        {
            physicsObj.SetOrigin( trueOrigin );
            return false;
        }
    }
    else
    {
        // move from actual position to start of path
        if ( !CheckTeleportPathSegment( trueOrigin, origin, lastPos ) )
        {
            physicsObj.SetOrigin( trueOrigin );
            return false;
        }
        idVec3 currentPos = origin;
        int currentArea = originAreaNum;

        int lastAreas[4], lastAreaIndex;
        lastAreas[0] = lastAreas[1] = lastAreas[2] = lastAreas[3] = currentArea;
        lastAreaIndex = 0;


        // Move along path
        while ( currentArea && currentArea != toAreaNum )
        {
            if ( !CheckTeleportPathSegment( currentPos, path.moveGoal, lastPos ) )
            {
                physicsObj.SetOrigin( trueOrigin );
                return false;
            }

            lastAreas[lastAreaIndex] = currentArea;
            lastAreaIndex = ( lastAreaIndex + 1 ) & 3;

            currentPos = path.moveGoal;
            currentArea = path.moveAreaNum;

            // Sometimes it tells us to go to the current location and gets stuck in a loop, so check for that.
            // TODO: Work out why it gets stuck in a loop, and fix it. Currently we just go in a straight line from stuck point to destination.
            if ( currentArea == lastAreas[0] || currentArea == lastAreas[1] ||
                 currentArea == lastAreas[2] || currentArea == lastAreas[3] )
            {
                common->Warning( "CheckTeleportPath: local routing minimum going from area %d to area %d", currentArea, toAreaNum );
                if ( !CheckTeleportPathSegment( currentPos, toPoint, lastPos ) )
                {
                    physicsObj.SetOrigin( trueOrigin );
                    return false;
                }
                currentPos = toPoint;
                currentArea = toAreaNum;
                break;
            }

            // Find next path segment.
            if ( !aas->WalkPathToGoal(path, currentArea, currentPos, toAreaNum, toPoint, travelFlags) )
            {
                path.moveGoal = toPoint;
                path.moveAreaNum = toAreaNum;
                currentPos = toPoint;
                currentArea = toAreaNum;
            }
        }
        // Is this needed? Doesn't hurt.
        if ( !CheckTeleportPathSegment( currentPos, toPoint, lastPos ) )
        {
            physicsObj.SetOrigin( trueOrigin );
            return false;
        }
        // move from end of path to actual target
        if ( !CheckTeleportPathSegment( toPoint, target, lastPos ) )
        {
            physicsObj.SetOrigin( trueOrigin );
            return false;
        }
    }

    physicsObj.SetOrigin( trueOrigin );
    return true;
}

/*
===============
idPlayer::SetLevelTrigger
===============
*/
void idPlayer::SetLevelTrigger( const char *levelName, const char *triggerName ) {
	if ( levelName && *levelName && triggerName && *triggerName ) {
		idLevelTriggerInfo lti;
		lti.levelName = levelName;
		lti.triggerName = triggerName;
		inventory.levelTriggers.Append( lti );
	}
}

/*
===============
idPlayer::Event_LevelTrigger
===============
*/
void idPlayer::Event_LevelTrigger( void ) {
	idStr mapName = gameLocal.GetMapName();
	mapName.StripPath();
	mapName.StripFileExtension();
	for ( int i = inventory.levelTriggers.Num() - 1; i >= 0; i-- ) {
		if ( idStr::Icmp( mapName, inventory.levelTriggers[i].levelName) == 0 ){
			idEntity *ent = gameLocal.FindEntity( inventory.levelTriggers[i].triggerName );
			if ( ent ) {
				ent->PostEventMS( &EV_Activate, 1, this );
			}
		}
	}
}

/*
===============
idPlayer::Event_Gibbed
===============
*/
void idPlayer::Event_Gibbed( void ) {
}

/*
==================
idPlayer::Event_GetIdealWeapon
==================
*/
void idPlayer::Event_GetIdealWeapon( void ) {
       const char *weapon;

    if( hands[vr_weaponHand.GetInteger()].idealWeapon >= 0 )
    {
        weapon = spawnArgs.GetString( va( "def_weapon%d", hands[ vr_weaponHand.GetInteger() ].idealWeapon ) );
        idThread::ReturnString( weapon );
    }
    else
    {
        idThread::ReturnString( "" );
    }
}

/*
===============
idPlayer::UpdatePlayerIcons
===============
*/
void idPlayer::UpdatePlayerIcons( void ) {
	int time = networkSystem->ServerGetClientTimeSinceLastPacket( entityNumber );
	if ( time > cvarSystem->GetCVarInteger( "net_clientMaxPrediction" ) ) {
		isLagged = true;
	} else {
		isLagged = false;
	}
}

/*
==============
idPlayer::UpdatePlayerSkinsPoses()
Updates the skins of the weapon and flashlight to hide/show arms/watch and updates poses of player model hands.
======
*/
void idPlayer::UpdatePlayerSkinsPoses()
{
    for( int h = 0; h < 2; h++ )
    {
        if( hands[ h ].weapon )
            hands[ h ].weapon->UpdateSkin();
    }
    if ( flashlight )
    {
        flashlight->UpdateSkin();
    }
    SetFlashHandPose(); // Call set flashlight hand pose script function
    SetWeaponHandPose();
}

/*
===============
idPlayer::DrawPlayerIcons
===============
*/
void idPlayer::DrawPlayerIcons( void ) {
	if ( !NeedsIcon() ) {
		playerIcon.FreeIcon();
		return;
	}
	playerIcon.Draw( this, headJoint );
}

/*
===============
idPlayer::HidePlayerIcons
===============
*/
void idPlayer::HidePlayerIcons( void ) {
	playerIcon.FreeIcon();
}

/*
===============
idPlayer::NeedsIcon
==============
*/
bool idPlayer::NeedsIcon( void ) {
	// local clients don't render their own icons... they're only info for other clients
	return entityNumber != gameLocal.localClientNum && ( isLagged || isChatting );
}

/*
=================
idPlayer::UpdateClickables

Searches nearby entities for clickables and adds text markers
=================
*/
void idPlayer::UpdateClickables(void) {
  int i, numListedClipModels;
  idClipModel *clipModel;
  idClipModel *clipModelList[MAX_GENTITIES];
  idVec3 eyePos;
  idMat3 viewAxis;
  idBounds bounds;
  trace_t trace;
  idEntity *ent;

  if (gameLocal.GetCamera()) {
    if (!gameLocal.GetCamera()->spawnArgs.GetInt("AllowClickers"))
      return;

    eyePos = gameLocal.GetCamera()->GetRenderView()->vieworg;
    viewAxis = gameLocal.GetCamera()->GetRenderView()->viewaxis;
  } else {
    eyePos = GetEyePosition();
    viewAxis = viewAngles.ToMat3();
  }

  bounds = idBounds(eyePos).Expand(80.f);

  // get all clip models touching the bounds
  numListedClipModels = gameLocal.clip.ClipModelsTouchingBounds(
      bounds, -1, clipModelList, MAX_GENTITIES);

  for (i = 0; i < numListedClipModels; i++) {
    clipModel = clipModelList[i];
    ent = clipModel->GetEntity();

    if (ent->IsType(idEntity::Type) && ent->spawnArgs.GetInt("clickable") &&
        ent->spawnArgs.GetInt("click_item") &&
        !ent->spawnArgs.GetInt("item_disabled")) {
      gameLocal.clip.TracePoint(trace, eyePos, clipModel->GetOrigin(),
                                MASK_SHOT_RENDERMODEL, this);
      if ((trace.fraction < 1.0f) && (trace.c.entityNum == ent->entityNumber)) {
        idVec3 center = ent->GetPhysics()->GetAbsBounds().GetCenter();
        gameRenderWorld->DrawText(
            ent->spawnArgs.GetString("item_name", "Object"),
            idVec3(center.x, center.y, center.z + 1.5f), 0.075f, colorBlue,
            viewAxis, 1);
      }
      continue;
    }
  }
}

/*
=================
idPlayer::UpdateObjectiveSystem

Closes the PDA after 5 seconds
=================
*/
void idPlayer::UpdateObjectiveSystem(void) {
  if (objectiveSystem == NULL) {
    return;
  }

  if (objectiveSystemOpen &&
      gameLocal.time > objectiveSystemOpenTime + SEC2MS(5)) {
    TogglePDA( 1 - vr_weaponHand.GetInteger() );
  }
}

/*
=================
idPlayer::UpdateTextMessages

Removes old text messages
=================
*/
void idPlayer::UpdateTextMessages(void) {
  if (textMessageSystem == NULL) {
    return;
  }

  int numVisible = 0;

  for (int i = 1; i <= 5; i++) {
    int typingVisible =
        textMessageSystem->State().GetInt("text_message_typing_visible");
    int messageVisible =
        textMessageSystem->State().GetInt(va("text_message_%i_visible", i));
    int messageReceivedTime =
        textMessageSystem->State().GetInt(va("text_message_%i_timestamp", i));
    int messageCleared =
        textMessageSystem->State().GetInt(va("text_message_%i_cleared", i));

    if (typingVisible || messageVisible) {
      numVisible++;
    }

    if (gameLocal.time >= messageReceivedTime + SEC2MS(20) && messageVisible &&
        !messageCleared) {
      textMessageSystem->HandleNamedEvent(va("Text_Message_%i_Anim_Out", i));
      textMessageSystem->SetStateInt(va("text_message_%i_cleared", i), 1);
    }

    if (gameLocal.time >= messageReceivedTime + SEC2MS(20.75) &&
        messageVisible && messageCleared) {
      textMessageSystem->SetStateInt(va("text_message_%i_visible", i), 0);
    }
  }

  if (!numVisible) {
    textMessageSystem->SetStateInt("text_message_linepos", 0);
    textMessageSystem->Activate(false, gameLocal.time);
    textMessageSystemOpen = false;
  }
}

/*
=================
idPlayer::UpdateCameraGui

Updates camera overlay GUI
=================
*/
void idPlayer::UpdateCameraGui(void) {
  if ((!gameLocal.GetCamera() ||
       idStr::Icmp(gameLocal.GetCamera()->spawnArgs.GetString("name"),
                   cameraGuiCamName)) &&
      cameraGuiSystemOpen) {
    cameraGuiSystemOpen = false;
    cameraGuiSystem->Activate(false, gameLocal.time);
    cameraGuiSystem = NULL;
  }

  if (gameLocal.GetCamera() &&
      gameLocal.GetCamera()->spawnArgs.FindKey("cameragui") &&
      !cameraGuiSystemOpen) {
    cameraGuiSystem = uiManager->FindGui(
        gameLocal.GetCamera()->spawnArgs.GetString("cameragui"), true, false,
        true);
    cameraGuiCamName = gameLocal.GetCamera()->spawnArgs.GetString("name");
    gameLocal.SetUIAspectRatio(cameraGuiSystem);

    if (cameraGuiSystem) {
      cameraGuiSystem->Activate(true, gameLocal.time);
      cameraGuiSystemOpen = true;
    }
  }
 }
