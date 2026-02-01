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

#ifndef __GAME_PLAYER_H__
#define __GAME_PLAYER_H__

#include <idlib/math/Quat.h>
#include "idlib/math/Interpolate.h"

#include "physics/Physics_Player.h"
#include "Item.h"
#include "Actor.h"
#include "Weapon.h"
#include "Projectile.h"
#include "PlayerIcon.h"
#include "GameEdit.h"
#include "PredictedValue.h"
#include "Vr.h"
#include "gamesys/SysCvar.h"

class idAI;

/*
===============================================================================

	Player entity.

===============================================================================
*/

extern const idEventDef EV_Player_GetButtons;
extern const idEventDef EV_Player_GetMove;
extern const idEventDef EV_Player_GetViewAngles;
extern const idEventDef EV_Player_EnableWeapon;
extern const idEventDef EV_Player_DisableWeapon;
extern const idEventDef EV_Player_ExitTeleporter;
extern const idEventDef EV_Player_SelectWeapon;
extern const idEventDef EV_SpectatorTouch;

const float THIRD_PERSON_FOCUS_DISTANCE	= 512.0f;
const int	LAND_DEFLECT_TIME = 150;
const int	LAND_RETURN_TIME = 300;
const int	FOCUS_TIME = 300;
const int	FOCUS_GUI_TIME = 200; // Koz fixme, only change in VR. Previously 500, reduced to 200 to drop weapon out of guis faster.
const int	NUM_QUICK_SLOTS = 4;

const int MAX_WEAPONS = 32;

const int weapon_empty_hand = -2; // Carl: todo, maybe a different constant

const int DEAD_HEARTRATE = 0;			// fall to as you die
const int LOWHEALTH_HEARTRATE_ADJ = 20; //
const int DYING_HEARTRATE = 30;			// used for volumen calc when dying/dead
const int BASE_HEARTRATE = 70;			// default
const int ZEROSTAMINA_HEARTRATE = 115;  // no stamina
const int MAX_HEARTRATE = 130;			// maximum
const int ZERO_VOLUME = -40;			// volume at zero
const int DMG_VOLUME = 5;				// volume when taking damage
const int DEATH_VOLUME = 15;			// volume at death

const int SAVING_THROW_TIME = 5000;		// maximum one "saving throw" every five seconds

extern const int ASYNC_PLAYER_INV_AMMO_BITS;
extern const int ASYNC_PLAYER_INV_CLIP_BITS;

struct idItemInfo {
	idStr name;
	idStr icon;
};

struct idObjectiveInfo {
	idStr title;
	idStr text;
	idStr screenshot;
};

struct idLevelTriggerInfo {
	idStr levelName;
	idStr triggerName;
};

// powerups - the "type" in item .def must match
enum {
	BERSERK = 0,
	INVISIBILITY,
	MEGAHEALTH,
	ADRENALINE,
    INVULNERABILITY,
    HELLTIME,
    ENVIROSUIT,
    ENVIROTIME,
    MAX_POWERUPS
};

// powerup modifiers
enum {
	SPEED = 0,
	PROJECTILE_DAMAGE,
	MELEE_DAMAGE,
	MELEE_DISTANCE
};

// influence levels
enum {
	INFLUENCE_NONE = 0,			// none
	INFLUENCE_LEVEL1,			// no gun or hud
	INFLUENCE_LEVEL2,			// no gun, hud, movement
	INFLUENCE_LEVEL3,			// slow player movement
};

enum gameExpansionType_t
{
	GAME_BASE,
	GAME_D3XP,
	GAME_D3LE,
	GAME_UNKNOWN
};

typedef struct
{
    int ammo;
    int rechargeTime;
    char ammoName[128];
} RechargeAmmo_t;

typedef struct
{
    char		name[64];
    idList<int>	toggleList;
    int			lastUsed;
} WeaponToggle_t;

class idInventory {
public:
	int						maxHealth;
    int						weapons, duplicateWeapons, foundWeapons;
	int						powerups;
	int						armor;
	int						maxarmor;
	int						ammo[ AMMO_NUMTYPES ];
	int						clip[ MAX_WEAPONS ];
	int						clipDuplicate[ MAX_WEAPONS ];
	int						powerupEndTime[ MAX_POWERUPS ];
	RechargeAmmo_t			rechargeAmmo[ AMMO_NUMTYPES ];

	// mp
	int						ammoPredictTime;

	int						deplete_armor;
	float					deplete_rate;
	int						deplete_ammount;
	int						nextArmorDepleteTime;

	int						pdasViewed[4]; // 128 bit flags for indicating if a pda has been viewed

	int						selPDA;
	int						selEMail;
	int						selVideo;
	int						selAudio;
	bool					pdaOpened;
	bool					turkeyScore;
	idList<idDict *>		items;
	idStrList				pdas;
	idStrList				pdaSecurity;
	idStrList				videos;
	idStrList				emails;

	bool					ammoPulse;
	bool					weaponPulse;
	bool					armorPulse;
	int						lastGiveTime;

	idList<idLevelTriggerInfo> levelTriggers;

							idInventory() { Clear(); }
							~idInventory() { Clear(); }

	// save games
	void					Save( idSaveGame *savefile ) const;					// archives object for save game file
	void					Restore( idRestoreGame *savefile );					// unarchives object from save game file

	void					Clear( void );
	void					GivePowerUp( idPlayer *player, int powerup, int msec );
	void					ClearPowerUps( void );
	void					GetPersistantData( idDict &dict );
	void					RestoreInventory( idPlayer *owner, const idDict &dict );
	bool					Give( idPlayer *owner, const idDict &spawnArgs, const char *statname, const char *value, int *idealWeapon, bool updateHud );
	void					Drop( const idDict &spawnArgs, const char *weapon_classname, int weapon_index );
	ammo_t					AmmoIndexForAmmoClass( const char *ammo_classname ) const;
	int						MaxAmmoForAmmoClass( idPlayer *owner, const char *ammo_classname ) const;
	int						WeaponIndexForAmmoClass( const idDict & spawnArgs, const char *ammo_classname ) const;
	ammo_t					AmmoIndexForWeaponClass( const char *weapon_classname, int *ammoRequired );
	const char *			AmmoPickupNameForIndex( ammo_t ammonum ) const;
	void					AddPickupName(const char *name, const char *icon, idPlayer *owner);   //_D3XP

	int						HasAmmo( ammo_t type, int amount );
	bool					UseAmmo( ammo_t type, int amount );
	int						HasAmmo(const char *weapon_classname, bool includeClip = false, idPlayer *owner = NULL);			// _D3XP

	bool					HasEmptyClipCannotRefill(const char *weapon_classname, idPlayer *owner);

	void					UpdateArmor( void );
	int						GetClipAmmoForWeapon( const int weapon, const bool duplicate ) const;

	bool					CanGive( idPlayer* owner, const idDict& spawnArgs, const char* statname, const char* value );

	int						nextItemPickup;
	int						nextItemNum;
	int						onePickupTime;
	idList<idItemInfo>		pickupItemNames;
	idList<idObjectiveInfo>	objectiveNames;

	void					InitRechargeAmmo(idPlayer *owner);
	void					RechargeAmmo(idPlayer *owner);
	bool					CanGive(idPlayer *owner, const idDict &spawnArgs, const char *statname, const char *value, int *idealWeapon);
};

typedef struct {
	int		time;
	idVec3	dir;		// scaled larger for running
} loggedAccel_t;

typedef struct {
	int		areaNum;
	idVec3	pos;
} aasLocation_t;

enum slotIndex_t
{
    SLOT_NONE = -1,
    SLOT_PDA_HIP,
    SLOT_WEAPON_HIP,
    SLOT_WEAPON_BACK_BOTTOM,
    SLOT_WEAPON_BACK_TOP,
    SLOT_FLASHLIGHT_SHOULDER,
    SLOT_FLASHLIGHT_HEAD,
    SLOT_COUNT
};

struct slot_t
{
    idVec3 origin;
    float radiusSq;
};

class idWeaponHolder
{
public:
    idPlayer* owner;
    int currentWeapon;
    bool isTheDuplicate; // Carl: Dual wielding, is weapon the duplicate copy or the original? (ie. Which ammo clip does it use?)
public:
    idWeaponHolder();
    virtual	~idWeaponHolder();
    void Init( idPlayer* player );
    virtual bool isEmpty();
};

class idHolster : public idWeaponHolder
{
public:
    slotIndex_t slot;
    idVec3 origin;
    float radiusSq;
    renderEntity_t			renderEntity;					// used to present a model to the renderer
    qhandle_t				modelDefHandle;					// handle to static renderer model
    idMat3					holsterAxis;
public:
    idHolster();
    virtual	~idHolster();
    void Init( idPlayer* player );
    void FreeSlot();
    void HolsterModelByName( const char* modelname, idRenderModel* renderModel = NULL );
    void HolsterPDA();
    void HolsterFlashlight();
    void EmptyHolster();
    void HolsterCurrentWeapon( int stashed, int hand );
    void StashToExtraHolster();
    void RestoreFromExtraHolster();
    void UpdateSlot();
};

class idPlayerHand : public idWeaponHolder {
public:
	int whichHand;

	// laser sight - technically every weapon instance could have its own laser sight,
	// but laser sights are only active when in a player's hand, so make it one per hand
	renderEntity_t laserSightRenderEntity;    // replace crosshair for 3DTV
	qhandle_t laserSightHandle;
	renderEntity_t crosshairEntity; // Koz add a model to place the crosshair into the world
	qhandle_t crosshairHandle;
	int lastCrosshairMode;
	bool laserSightActive; // Koz allow lasersight toggle

	// throwing is per hand
	idVec3 throwDirection; // for motion control throwing actions e.g. grenade
	float throwVelocity;
	int frameTime[10];
	idVec3 position[10];
	int frameNum;
	int curTime;
	int timeDelta;
	int startFrameNum;

	// slots
	idVec3 handOrigin;
	idMat3 handAxis;
	slotIndex_t handSlot;

	bool grabbingWorld, oldGrabbingWorld; // currently this corresponds more to the state of the grip trigger/button which may actually act as a toggle
	bool virtualGrabDown, oldVirtualGrabDown; // this is the actual virtual grab state, when this goes false you should drop what you're holding
	bool triggerDown, oldTriggerDown, oldFlashlightTriggerDown;
	bool thumbDown, oldThumbDown;

	idEntityPtr<idWeapon> weapon;
	bool weaponGone;            // force stop firing

	// currentWeapon is inherited from the idWeaponHolder superclass
	int idealWeapon;
	int previousWeapon;
	int weaponSwitchTime;

	// Weapon positioning
	bool PDAfixed; // Koz has the PDA been fixed in space?
	bool lastPdaFixed;
	idVec3 playerPdaPos; // position player was at when pda fixed in space
	idVec3 motionPosition;
	idQuat motionRotation;// = idQuat_zero;
	bool wasPDA;

	idStr animPrefix; // player hand anims

	//-----------------------------------------------------------------
	// controller shake parms
	//-----------------------------------------------------------------

	const static int MAX_SHAKE_BUFFER = 3;
	float controllerShakeHighMag[MAX_SHAKE_BUFFER];        // magnitude of the high frequency controller shake
	float controllerShakeLowMag[MAX_SHAKE_BUFFER];        // magnitude of the low frequency controller shake
	int controllerShakeHighTime[MAX_SHAKE_BUFFER];    // time the controller shake ends for high frequency.
	int controllerShakeLowTime[MAX_SHAKE_BUFFER];        // time the controller shake ends for low frequency.

public:
	idPlayerHand();

	virtual                    ~idPlayerHand();

	void Init(idPlayer *player, int hand);

	void TrackWeaponDirection(idVec3 origin);

	virtual bool holdingFlashlight();

	virtual bool holdingWeapon(); // physically holding it, not just levitating or using fists
	virtual bool
	floatingWeapon(); // the soul cube and the artifact float next to your hand rather than being held
	virtual bool controllingWeapon(); // holding or floating
	virtual bool holdingPDA();

	virtual bool holdingPhysics();

	virtual bool holdingItem();

	virtual bool holdingSomethingDroppable();

	bool isOverMountedFlashlight();

	bool tooFullToInteract();

	bool
	handExists(); // false if our character is missing a hand or has something mounted on their forearm instead

	bool contextToggleVirtualGrab();

	bool startVirtualGrab();

	bool releaseVirtualGrab(); // will drop whatever you're holding

	idStr GetCurrentWeaponString();

	void NextWeapon(int dir = 1);

	void PrevWeapon();

	void NextBestWeapon();

	void SelectWeapon(int num, bool force, bool specific);

	void DropWeapon(bool died);

	// Controller Shake
	void
	SetControllerShake(float highMagnitude, int highDuration, float lowMagnitude, int lowDuration);

	void ResetControllerShake();

	void GetControllerShake(int &highMagnitude, int &lowMagnitude) const;

	void debugPrint();
};

class idPlayer : public idActor {
public:
	enum {
		EVENT_IMPULSE = idEntity::EVENT_MAXEVENTS,
		EVENT_EXIT_TELEPORTER,
		EVENT_ABORT_TELEPORTER,
		EVENT_POWERUP,
		EVENT_SPECTATE,
        EVENT_PICKUPNAME,
        EVENT_FORCE_ORIGIN,
        EVENT_KNOCKBACK,
		EVENT_MAXEVENTS
	};

    static const int MAX_PLAYER_PDA = 100;
    static const int MAX_PLAYER_VIDEO = 100;
    static const int MAX_PLAYER_AUDIO = 100;
    static const int MAX_PLAYER_AUDIO_ENTRIES = 2;

    usercmd_t				oldCmd;
    usercmd_t				usercmd;

	class idPlayerView		playerView;			// handles damage kicks and effects

    idPlayerHand hands[2];

	renderEntity_t			flashlightRenderEntity;					// used to present a model to the renderer
	qhandle_t				flashlightModelDefHandle;					// handle to static renderer model

	// Koz begin
    const idDeclSkin*		skinHeadingSolid;
    const idDeclSkin*		skinHeadingArrows;
    const idDeclSkin*		skinHeadingArrowsScroll;

    renderEntity_t			pdaRenderEntity;					// used to present a model to the renderer
    qhandle_t				pdaModelDefHandle;					// handle to static renderer model
    idMat3					pdaHolsterAxis;

    renderEntity_t			holsterRenderEntity;					// used to present a model to the renderer
    qhandle_t				holsterModelDefHandle;					// handle to static renderer model
    idMat3					holsterAxis;
    int						holsteredWeapon, extraHolsteredWeapon;
    const char*				extraHolsteredWeaponModel;

    renderEntity_t			hudEntity; // Koz add a model to place the hud into the world
    qhandle_t				hudHandle;
    bool					hudActive;

    bool resetHUDYaw;
    float hud_yaw_x = 0.0f;
    float hud_yaw_y = 0.0f;


    const idDeclSkin*		skinCrosshairDot;
    const idDeclSkin*		skinCrosshairCircleDot;
    const idDeclSkin*		skinCrosshairCross;

    const idDeclSkin*		skinTelepadCrouch;


    idEntityPtr<idAnimatedEntity>	teleportTarget;
    idAnimator*						teleportTargetAnimator;

    jointHandle_t			teleportBeamJoint[24];
    jointHandle_t			teleportPadJoint;
    jointHandle_t			teleportCenterPadJoint;

    idVec3					teleportPoint; // Carl: used for teleporting
    idVec3					teleportAimPoint; // Carl: used for teleporting
    idVec3					teleportDir; // direction of teleport movement - needed for scripts where entities check player movement.

    float					teleportAimPointPitch;

    bool					aimValidForTeleport;

    //GB
    bool					velocityPunched;

    bool                    flashlightPreviouslyInHand;

    idVec3					PDAorigin; // Koz
    idMat3					PDAaxis; // Koz

    idMat3					chestPivotCorrectAxis; //made these public so could be accessed by hmdgetorientation;
    idVec3					chestPivotDefaultPos;
    jointHandle_t			chestPivotJoint;
    //float					independentWeaponPitch; // deltas to provide aim independent of body/view orientation
    //float					independentWeaponYaw;


    // Koz end

	bool					noclip;
	bool					godmode;
    bool					warpMove, warpAim;
    idVec3					warpVel, warpDest;
    int						warpTime;

    bool					jetMove;
    idVec3					jetMoveVel;
    int						jetMoveTime;
    int						jetMoveCoolDownTime;

	bool					spawnAnglesSet;		// on first usercmd, we must set deltaAngles
	idAngles				spawnAngles;
	idAngles				viewAngles;			// player view angles
	idAngles				cmdAngles;			// player cmd angles

	int						buttonMask;
	int						oldButtons;
	int						oldFlags;

	int						lastHitTime;			// last time projectile fired by player hit target
	int						lastSndHitTime;			// MP hit sound - != lastHitTime because we throttle
	int						lastSavingThrowTime;	// for the "free miss" effect

    bool					pdaHasBeenRead[ MAX_PLAYER_PDA ];
    bool					videoHasBeenViewed[ MAX_PLAYER_VIDEO ];
    bool					audioHasBeenHeard[ MAX_PLAYER_AUDIO ][ MAX_PLAYER_AUDIO_ENTRIES ];

	idScriptBool			AI_FORWARD;
	idScriptBool			AI_BACKWARD;
	idScriptBool			AI_STRAFE_LEFT;
	idScriptBool			AI_STRAFE_RIGHT;
	idScriptBool			AI_ATTACK_HELD;
	idScriptBool			AI_WEAPON_FIRED;
	idScriptBool			AI_JUMP;
	idScriptBool			AI_CROUCH;
	idScriptBool			AI_ONGROUND;
	idScriptBool			AI_ONLADDER;
	idScriptBool			AI_DEAD;
	idScriptBool			AI_RUN;
	idScriptBool			AI_PAIN;
	idScriptBool			AI_HARDLANDING;
	idScriptBool			AI_SOFTLANDING;
	idScriptBool			AI_RELOAD;
	idScriptBool			AI_TELEPORT;
	idScriptBool			AI_TURN_LEFT;
	idScriptBool			AI_TURN_RIGHT;

	// inventory
	idInventory				inventory;

    int						flashlightBattery;
    idEntityPtr<idWeapon>   flashlight;

	idUserInterface *		hud;				// MP: is NULL if not local player
	idUserInterface *		objectiveSystem;
	bool					objectiveSystemOpen;
	int						objectiveSystemOpenTime;
	idUserInterface *		itemSystem;
	bool					itemSystemOpen;
	idUserInterface *		textMessageSystem;
	bool					textMessageSystemOpen;
	idUserInterface *		subtitleSystem;
	bool					subtitleSystemOpen;
	idUserInterface *		cameraGuiSystem;
	bool					cameraGuiSystemOpen;
	idStr					cameraGuiCamName;
    int						quickSlot[ NUM_QUICK_SLOTS ];

	vrClientInfo *pVRClientInfo;

	//Gb added this to match upto the enums
	int						weapon_none;
	int						weapon_soulcube;
	int						weapon_pda;
	int						weapon_fists;
	int						weapon_flashlight;
	int						weapon_chainsaw;
	int						weapon_bloodstone;
	int						weapon_bloodstone_active1;
	int						weapon_bloodstone_active2;
	int						weapon_bloodstone_active3;
	bool					harvest_lock;
	// Koz begin
	int						weapon_pistol;
	int						weapon_shotgun;
	int						weapon_shotgun_double;
	int						weapon_machinegun;
	int						weapon_chaingun;
	int						weapon_handgrenade;
	int						weapon_plasmagun;
	int						weapon_rocketlauncher;
	int						weapon_bfg;
	int						weapon_flashlight_new;
	int						weapon_grabber;


	int						heartRate;
	idInterpolate<float>	heartInfo;
	int						lastHeartAdjust;
	int						lastHeartBeat;
	int						lastDmgTime;
	int						deathClearContentsTime;
	bool					doingDeathSkin;
	int						lastArmorPulse;		// lastDmgTime if we had armor at time of hit
	float					stamina;
	float					healthPool;			// amount of health to give over time
	int						nextHealthPulse;
	bool					healthPulse;
	bool					healthTake;
	int						nextHealthTake;


	bool					hiddenWeapon;		// if the weapon is hidden ( in noWeapons maps )
	idEntityPtr<idProjectile> soulCubeProjectile;

	// mp stuff
	static idVec3			colorBarTable[ 8 ];
	int						spectator;
	idVec3					colorBar;			// used for scoreboard and hud display
	int						colorBarIndex;
	bool					scoreBoardOpen;
	bool					forceScoreBoard;
	bool					forceRespawn;
	bool					spectating;
	int						lastSpectateTeleport;
	bool					lastHitToggle;
	bool					forcedReady;
	bool					wantSpectate;		// from userInfo
	bool					weaponGone;			// force stop firing
	bool					useInitialSpawns;	// toggled by a map restart to be active for the first game spawn
	int						latchedTeam;		// need to track when team gets changed
	int						tourneyRank;		// for tourney cycling - the higher, the more likely to play next - server
	int						tourneyLine;		// client side - our spot in the wait line. 0 means no info.
	int						spawnedTime;		// when client first enters the game

	idEntityPtr<idEntity>	teleportEntity;		// while being teleported, this is set to the entity we'll use for exit
	int						teleportKiller;		// entity number of an entity killing us at teleporter exit
	bool					lastManOver;		// can't respawn in last man anymore (srv only)
	bool					lastManPlayAgain;	// play again when end game delay is cancelled out before expiring (srv only)
	bool					lastManPresent;		// true when player was in when game started (spectators can't join a running LMS)
	bool					isLagged;			// replicated from server, true if packets haven't been received from client.
	bool					isChatting;			// replicated from server, true if the player is chatting.

	// timers
	int						minRespawnTime;		// can respawn when time > this, force after g_forcerespawn
	int						maxRespawnTime;		// force respawn after this time

    renderEntity_t			laserSightRenderEntity;
    qhandle_t				laserSightHandle;


    // the first person view values are always calculated, even
	// if a third person view is used
	idVec3					firstPersonViewOrigin;
	idMat3					firstPersonViewAxis;

    idVec3					waistOrigin;
    idMat3					waistAxis;

    idDragEntity			dragEntity;

    idFuncMountedObject*		mountedObject;
    idEntityPtr<idLight>	enviroSuitLight;

    bool					healthRecharge;
    int						lastHealthRechargeTime;
    int						rechargeSpeed;

    float					new_g_damageScale;

    bool					bloomEnabled;
    float					bloomSpeed;
    float					bloomIntensity;

public:
	CLASS_PROTOTYPE( idPlayer );

							idPlayer();
	virtual					~idPlayer();

	void 					SetVRClientInfo(vrClientInfo *pVRClientInfo);
    vrClientInfo*			GetVRClientInfo();

	void					Spawn( void );
	void					Think( void );

    void					SetupPDASlot( bool holsterPDA );
    void					FreePDASlot();
    void					UpdatePDASlot();

    void					SetupHolsterSlot( int hand, int stashed = -1 );
    void					FreeHolsterSlot();
    void					UpdateHolsterSlot();

    void					UpdateLaserSight( int hand );
    bool					GetHandOrHeadPositionWithHacks( int hand, idVec3& origin, idMat3& axis );

    // Koz begin
    void					UpdateTeleportAim();
    bool					GetTeleportBeamOrigin( idVec3 &beamOrigin, idMat3 &beamAxis);
    void					UpdatePlayerSkinsPoses();
    void					SetWeaponHandPose();
    void					SetFlashHandPose(); // Set flashlight hand pose
    void					ToggleLaserSight();
    void					UpdateVrHud();
    void					ToggleHud();
    void					RecreateCopyJoints();
    void					UpdateNeckPose();
    bool					IsCrouching()
    {
        return physicsObj.IsCrouching();
    }
    void					InitTeleportTarget();
    // Koz end
    //GB Begin
	float					GetHudAlpha();
	//GB End

    // Carl begin
    bool					HasHoldableFlashlight();
    // Carl end

	// save games
	void					Save( idSaveGame *savefile ) const;					// archives object for save game file
	void					Restore( idRestoreGame *savefile );					// unarchives object from save game file

	virtual void			Hide( void );
	virtual void			Show( void );

	void					Init( void );
    void					InitPlayerBones(); // Koz
	void					PrepareForRestart( void );
	virtual void			Restart( void );
	void					LinkScriptVariables( void );
	void					SetupWeaponEntity( void );
	void					SelectInitialSpawnPoint( idVec3 &origin, idAngles &angles );
	void					SpawnFromSpawnSpot( void );
	void					SpawnToPoint( const idVec3	&spawn_origin, const idAngles &spawn_angles );
	void					SetClipModel( void );	// spectator mode uses a different bbox size

	void					SavePersistantInfo( void );
	void					RestorePersistantInfo( void );
	void					SetLevelTrigger( const char *levelName, const char *triggerName );

	bool					UserInfoChanged( bool canModify );
	idDict *				GetUserInfo( void );
	bool					BalanceTDM( void );

	void					CacheWeapons( void );

	void					EnterCinematic( void );
	void					ExitCinematic( void );
	bool					HandleESC( void );
	bool					SkipCinematic( void );

	void					UpdateConditions( void );
	void					SetViewAngles( const idAngles &angles );

    void					ControllerShakeFromDamage( int damage );
    void					ControllerShakeFromDamage( int damage, const idVec3 &direction );
    void					SetControllerShake( float magnitude, int duration, const idVec3 &direction );
    void					ResetControllerShake();

							// delta view angles to allow movers to rotate the view of the player
	void					UpdateDeltaViewAngles( const idAngles &angles );

	virtual bool			Collide( const trace_t &collision, const idVec3 &velocity );

	// Carl : Teleporting
	int						PointReachableAreaNum(const idVec3& pos, const float boundsScale = 2.0f) const;
	bool					PathToGoal(aasPath_t& path, int areaNum, const idVec3& origin, int goalAreaNum, const idVec3& goalOrigin) const;
	bool					CanReachPosition( const idVec3& pos, idVec3& betterPos );

	virtual void			GetAASLocation( idAAS *aas, idVec3 &pos, int &areaNum ) const;
	virtual void			GetAIAimTargets( const idVec3 &lastSightPos, idVec3 &headPos, idVec3 &chestPos );
	virtual void			DamageFeedback( idEntity *victim, idEntity *inflictor, int &damage );
	void					CalcDamagePoints(  idEntity *inflictor, idEntity *attacker, const idDict *damageDef,
							   const float damageScale, const int location, int *health, int *armor );
	virtual	void			Damage( idEntity *inflictor, idEntity *attacker, const idVec3 &dir, const char *damageDefName, const float damageScale, const int location );

							// use exitEntityNum to specify a teleport with private camera view and delayed exit
	virtual void			Teleport( const idVec3 &origin, const idAngles &angles, idEntity *destination );
    virtual bool			TeleportPathSegment( const idVec3& start, const idVec3& end, idVec3& lastPos );
    virtual void			TeleportPath( const idVec3& target );
    virtual bool			CheckTeleportPathSegment(const idVec3& start, const idVec3& end, idVec3& lastPos);
    virtual bool			CheckTeleportPath(const idVec3& target, int toAreaNum = 0);

    virtual void            SetupFlashlightHolster();
    virtual void            UpdateFlashlightHolster();
    virtual void            SetupLaserSight();
    //virtual void            UpdateLaserSight( );

	void					Kill( bool delayRespawn, bool nodamage );
	virtual void			Killed( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location );
	void					StartFxOnBone(const char *fx, const char *bone);

	bool					ShouldBlink();
	renderView_t *			GetRenderView( void );
	void					CalculateRenderView( void );	// called every tic by player code
	void					CalculateFirstPersonView( void );
    void					CalculateWaist();
    void					CalculateLeftHand();
    void					CalculateRightHand();

	void					DrawHUD( idUserInterface *hud );
	void					DrawWeaponWheel( idUserInterface *hud );

	bool					IsWeaponReady( int weapon );
    void					WeaponFireFeedback( int hand, const idDict* weaponDef );

	float					DefaultFov( void ) const;
	float					CalcFov( bool honorZoom );
    void					CalculateViewWeaponPos( int hand, idVec3& origin, idMat3& axis );
    void					CalculateViewWeaponPosVR( int hand, idVec3& origin, idMat3& axis );
    void					SetHandIKPos( int hand, idVec3 handOrigin, idMat3 handAxis, idQuat rotation, bool isFlashlight = false );

    // Koz begin
    void					CalculateViewFlashlightPos( idVec3 &origin, idMat3 &axis, idVec3 flashlightOffset ); // Koz aim the flashlight with motion controls
    // Koz end

	idVec3					GetEyePosition( void ) const;
	void					GetViewPos( idVec3 &origin, idMat3 &axis ) const;
    void					GetViewPosVR( idVec3& origin, idMat3& axis ) const; // Koz fixme

	void					OffsetThirdPersonView( float angle, float range, float height, bool clip );

	//bool					Give( const char *statname, const char *value );
    bool					Give( const char* statname, const char* value, int hand );
	bool					GiveItem( idItem *item );
	void					GiveItem( const char *name );
	void					GiveHealthPool( float amt );

	bool					GiveInventoryItem( idDict *item );
	void					RemoveInventoryItem( idDict *item );
	bool					GiveInventoryItem( const char *name );
	void					RemoveInventoryItem( const char *name );
	idDict *				FindInventoryItem( const char *name );

    void					SetQuickSlot( int index, int val );
    int						GetQuickSlot( int index );

	void					GivePDA( const char *pdaName, idDict *item, bool toggle  );
	void					GiveVideo( const char *videoName, idDict *item );
	void					GiveEmail( const char *emailName );
	void					GiveSecurity( const char *security );
	void					GiveObjective( const char *title, const char *text, const char *screenshot );
	void					CompleteObjective( const char *title );

	bool					GivePowerUp( int powerup, int time );
	void					ClearPowerUps( void );
	bool					PowerUpActive( int powerup ) const;
	float					PowerUpModifier( int type );

	bool					OtherHandImpulseSlot();
	bool					WeaponHandImpulseSlot();
	bool					GrabWorld( int hand, bool pressed ); // 0 = right hand, 1 = left hand; true if pressed, false if released; returns true if handled as grab
    bool					TriggerClickWorld( int hand, bool pressed ); // 0 = right hand, 1 = left hand; true if pressed, false if released; returns true if handled as trigger
    bool					ThumbClickWorld( int hand, bool pressed ); // 0 = right hand, 1 = left hand; true if pressed, false if released; returns true if handled as thumb click

	int						SlotForWeapon( const char *weaponName );
	void					Reload( void );
	void					NextWeapon( void );
	void					NextBestWeapon( void );
	void					PrevWeapon( void );
	void					SetPreviousWeapon( int num )
	{
		hands[ vr_weaponHand.GetInteger() ].previousWeapon = num;
	}
    void					SelectWeapon( int num, bool force, bool specific = false );
	void					DropWeapons( bool died ) ;
	void					StealWeapon( idPlayer *player );
	void					AddProjectilesFired( int count );
	void					AddProjectileHits( int count );
	void					SetLastHitTime( int time );
	void					WeaponLoweringCallback( void );
	void					WeaponRisingCallback( void );
	void					RemoveWeapon( const char *weap );
	bool					CanShowWeaponViewmodel( void ) const;
	// Carl: Dual wielding
	bool					CanDualWield( int num ) const;
    idWeapon*				GetWeaponInHand( int hand ) const;
    // Carl: when the code needs just one weapon, guess which one is the "main" one
    idWeapon*				GetMainWeapon();
	idWeapon*				GetGrabberWeapon() const;
    idWeapon*				GetPDAWeapon() const;
    idWeapon*				GetWeaponWithMountedFlashlight();
    int						GetBestWeaponHand();
    int						GetBestWeaponHandToSteal( idPlayer* thief );
    // Carl: get the specific weapon used to harvest souls (Soul Cube or Artifact)
    // Returns the required one if you're holding it in a hand, or the other one, or the main weapon
    idWeapon*				GetHarvestWeapon( idStr requiredWeapons );
    idStr					GetCurrentHarvestWeapon( idStr requiredWeapons );
    virtual int				GetAnim( int channel, const char* name );
    // Carl end

	void					AddAIKill( void );
	void					SetSoulCubeProjectile( idProjectile *projectile );

	void					AdjustHeartRate( int target, float timeInSecs, float delay, bool force );
	void					SetCurrentHeartRate( void );
	int						GetBaseHeartRate( void );
	void					UpdateAir( void );
	void					UpdatePowerupHud();

	virtual bool			HandleSingleGuiCommand( idEntity *entityGui, idLexer *src );
	bool					GuiActive( void ) { return focusGUIent != NULL; }
    bool					HandleGuiEvents( const sysEvent_t* ev );

	void					PerformImpulse( int impulse );
	void					Spectate( bool spectate );
	int 					MapWeaponHudId( int inGame );
	void					TogglePDA( int hand );
	void					ToggleScoreboard( void );
	void					RouteGuiMouse( idUserInterface *gui );
	void					UpdateHud( void );
	const idDeclPDA *		GetPDA( void ) const;
	const idDeclVideo *		GetVideo( int index );
	void					SetInfluenceFov( float fov );
	void					SetInfluenceView( const char *mtr, const char *skinname, float radius, idEntity *ent );
	void					SetInfluenceLevel( int level );
	int						GetInfluenceLevel( void ) { return influenceActive; };

	void					SetPrivateCameraView( idCamera *camView );
	idCamera *				GetPrivateCameraView( void ) const { return privateCameraView; }

	void					StartFxFov( float duration  );
	void					UpdateHudWeapon( int flashWeaponHand );
	void					UpdateHudStats( idUserInterface *hud );
	void					UpdateHudAmmo( idUserInterface *hud, int hand );
	void					Event_StopAudioLog( void );
    bool					IsSoundChannelPlaying( const s_channelType channel = SND_CHANNEL_ANY );
	void					StartAudioLog( void );
	void					StopAudioLog( void );
	void					ShowTip( const char *title, const char *tip, bool autoHide );
	void					HideTip( void );

	bool					IsTipVisible( void ) { return tipUp; };

	void					ShowObjective( const char *obj );
	void					HideObjective( void );

	virtual void			ClientPredictionThink( void );
	virtual void			WriteToSnapshot( idBitMsgDelta &msg ) const;
	virtual void			ReadFromSnapshot( const idBitMsgDelta &msg );
	void					WritePlayerStateToSnapshot( idBitMsgDelta &msg ) const;
	void					ReadPlayerStateFromSnapshot( const idBitMsgDelta &msg );

	virtual bool			ServerReceiveEvent( int event, int time, const idBitMsg &msg );

	virtual bool			GetPhysicsToVisualTransform( idVec3 &origin, idMat3 &axis );
	virtual bool			GetPhysicsToSoundTransform( idVec3 &origin, idMat3 &axis );

	virtual bool			ClientReceiveEvent( int event, int time, const idBitMsg &msg );
	bool					IsReady( void );
	bool					IsRespawning( void );
	bool					IsInTeleport( void );

	idEntity				*GetInfluenceEntity( void ) { return influenceEntity; };
	const idMaterial		*GetInfluenceMaterial( void ) { return influenceMaterial; };
	float					GetInfluenceRadius( void ) { return influenceRadius; };

	// server side work for in/out of spectate. takes care of spawning it into the world as well
	void					ServerSpectate( bool spectate );
	// for very specific usage. != GetPhysics()
	idPhysics				*GetPlayerPhysics( void );
	void					TeleportDeath( int killer );
	void					SetLeader( bool lead );
	bool					IsLeader( void );

	void					UpdateSkinSetup();

	bool					OnLadder( void ) const;

	virtual	void			UpdatePlayerIcons( void );
	virtual	void			DrawPlayerIcons( void );
	virtual	void			HidePlayerIcons( void );
	bool					NeedsIcon( void );


	void					StartHealthRecharge(int speed);
	void					StopHealthRecharge();

	bool					SelfSmooth( void );
	void					SetSelfSmooth( bool b );
	idStr					GetCurrentWeapon();
    int					    GetCurrentWeaponId();
	int						GetCurrentWeaponSlot()
	{
		return hands[ vr_weaponHand.GetInteger() ].currentWeapon;
	}
	int						GetIdealWeapon()
	{
		return hands[ vr_weaponHand.GetInteger() ].idealWeapon;
	}
	idHashTable<WeaponToggle_t>	GetWeaponToggles() const
	{
		return weaponToggles;
	}

	bool					CanGive(const char *statname, const char *value);

	void					StopHelltime(bool quick = true);
	void					PlayHelltimeStopSound();

    virtual void			FreeModelDef();

    const idAngles& 		GetViewBobAngles()
    {
        return viewBobAngles;
    }
    const idVec3& 			GetViewBob()
    {
        return viewBob;
    }

    bool					IsLocallyControlled() const
    {
        return entityNumber == gameLocal.localClientNum;
    }

    gameExpansionType_t		GetExpansionType() const;

    friend class idWeaponHolder;
    friend class idHolster;
    friend class idPlayerHand;

private:
	jointHandle_t			hipJoint;
	jointHandle_t			chestJoint;
	jointHandle_t			headJoint;

    // Koz begin
    jointHandle_t			neckJoint;

    jointHandle_t			ik_hand[2];
    jointHandle_t			ik_elbow[2];
    jointHandle_t			ik_shoulder[2];
    jointHandle_t			ik_handAttacher[2];
    bool					handLowered;
    bool					handRaised;

    //idMat3					chestPivotCorrectAxis; //made these public so could be accessed by hmdge
    //idVec3					chestPivotDefaultPos;
    idMat3					ik_elbowCorrectAxis[2];
    idMat3					ik_handCorrectAxis[2][32];
    idVec3					handWeaponAttachertoWristJointOffset[2][32];
    idVec3					handWeaponAttacherToDefaultOffset[2][32];

    int						aasState;
    // Koz end

	bool					blink;

	idPhysics_Player		physicsObj;			// player physics

	idList<aasLocation_t>	aasLocation;		// for AI tracking the player

	int						bobFoot;
	float					bobFrac;
	float					bobfracsin;
	int						bobCycle;			// for view bobbing and footstep generation
	float					xyspeed;
	int						stepUpTime;
	float					stepUpDelta;
	float					idealLegsYaw;
	float					legsYaw;
	bool					legsForward;
	float					oldViewYaw;
	idAngles				viewBobAngles;
	idVec3					viewBob;
	int						landChange;
	int						landTime;

	bool					weaponEnabled;
    int						risingWeaponHand; // Carl: getWeaponEntity script function assumes there is only one weapon and assigns it during raise
	bool					showWeaponViewModel;

	const idDeclSkin *		skin;
	const idDeclSkin *		powerUpSkin;
	idStr					baseSkinName;

	int						numProjectilesFired;	// number of projectiles fired
	int						numProjectileHits;		// number of hits on mobs

	bool					airless;
	int						airTics;				// set to pm_airTics at start, drops in vacuum
	int						lastAirDamage;

	bool					gibDeath;
	bool					gibsLaunched;
	idVec3					gibsDir;

	idInterpolate<float>	zoomFov;
	idInterpolate<float>	centerView;
	bool					fxFov;

	float					influenceFov;
	int						influenceActive;		// level of influence.. 1 == no gun or hud .. 2 == 1 + no movement
	idEntity *				influenceEntity;
	const idMaterial *		influenceMaterial;
	float					influenceRadius;
	const idDeclSkin *		influenceSkin;

	idCamera *				privateCameraView;

	static const int		NUM_LOGGED_VIEW_ANGLES = 64;		// for weapon turning angle offsets
	idAngles				loggedViewAngles[NUM_LOGGED_VIEW_ANGLES];	// [gameLocal.framenum&(LOGGED_VIEW_ANGLES-1)]
	static const int		NUM_LOGGED_ACCELS = 16;			// for weapon turning angle offsets
	loggedAccel_t			loggedAccel[NUM_LOGGED_ACCELS];	// [currentLoggedAccel & (NUM_LOGGED_ACCELS-1)]
	int						currentLoggedAccel;

	// if there is a focusGUIent, the attack button will be changed into mouse clicks
	idEntity *				focusGUIent;
	idUserInterface *		focusUI;				// focusGUIent->renderEntity.gui, gui2, or gui3
	idAI *					focusCharacter;
	idEntity *				focusClickable;
	int						talkCursor;				// show the state of the focusCharacter (0 == can't talk/dead, 1 == ready to talk, 2 == busy talking)
	int						focusTime;
	idAFEntity_Vehicle *	focusVehicle;
	idUserInterface *		cursor;

	// full screen guis track mouse movements directly
	int						oldMouseX;
	int						oldMouseY;

	idStr					pdaAudio;
	idStr					pdaVideo;
	idStr					pdaVideoWave;

	bool					tipUp;
	bool					objectiveUp;

	int						lastDamageDef;
	idVec3					lastDamageDir;
	int						lastDamageLocation;
	int						smoothedFrame;
	bool					smoothedOriginUpdated;
	idVec3					smoothedOrigin;
	idAngles				smoothedAngles;

    idHashTable<WeaponToggle_t>	weaponToggles;

	int						hudPowerup;
	int						lastHudPowerup;
	int						hudPowerupDuration;

	// mp
	bool					ready;					// from userInfo
	bool					respawning;				// set to true while in SpawnToPoint for telefrag checks
	bool					leader;					// for sudden death situations
	int						lastSpectateChange;
	int						lastTeleFX;
	unsigned int			lastSnapshotSequence;	// track state hitches on clients
	bool					weaponCatchup;			// raise up the weapon silently ( state catchups )
	int						MPAim;					// player num in aim
	int						lastMPAim;
	int						lastMPAimTime;			// last time the aim changed
	int						MPAimFadeTime;			// for GUI fade
	bool					MPAimHighlight;
	bool					isTelefragged;			// proper obituaries

	idPlayerIcon			playerIcon;

	bool					selfSmooth;

	idStr					itemSystemCallExit;
	int						nextTriggerTime;

	void					LookAtKiller( idEntity *inflictor, idEntity *attacker );

	void					StopFiring( void );
    void					FireWeapon( int hand, idWeapon* weap );
	void					Weapon_Combat( void );
	void					Weapon_NPC( void );
	void					Weapon_GUI( void );
	bool					ClickableCallScript(idStr funcname, int delay);
	void					ToggleItemSystem(void);
	void					Weapon_Clickable(void);
	void					UpdateWeapon( void );
    void					UpdateFlashlight();
    void					FlashlightOn();
    void					FlashlightOff();
	void					UpdateSpectating( void );
	void					SpectateFreeFly( bool force );	// ignore the timeout to force when followed spec is no longer valid
	void					SpectateCycle( void );
	idAngles				GunTurningOffset( void );
	idVec3					GunAcceleratingOffset( void );


	void					UseObjects( void );
	void					CrashLand( const idVec3 &oldOrigin, const idVec3 &oldVelocity );
	void					BobCycle( const idVec3 &pushVelocity );
	void					UpdateViewAngles( void );
	void					EvaluateControls( void );
	void					AdjustSpeed( void );
	void					AdjustBodyAngles( void );

	void					SnapBodyToView(); // Koz align body to current view;
    void					OrientHMDBody(); // Koz reset hmd/body orientations

    void					SetAAS( bool forceAAS48 = false );
    void					InitAASLocation( void );
	void					SetAASLocation( void );
	void					Move( void );
	void					Move_Interpolated( float fraction );
	void					UpdatePowerUps( void );
	void					UpdateDeathSkin( bool state_hitch );
	void					ClearPowerup( int i );
	void					SetSpectateOrigin( void );

	void					ClearFocus( void );
	void					UpdateFocus( void );
    void					SendPDAEvent( const sysEvent_t* sev );
    bool					UpdateFocusPDA( void );
	void					UpdateLocation( void );
	void					UpdateClickables(void);
	void					UpdateObjectiveSystem(void);
	void					UpdateTextMessages(void);
	void					UpdateCameraGui(void);
	idUserInterface *		ActiveGui( void );
	void					UpdatePDAInfo( bool updatePDASel );
	int						AddGuiPDAData( const declType_t dataType, const char *listName, const idDeclPDA *src, idUserInterface *gui );
	void					ExtractEmailInfo( const idStr &email, const char *scan, idStr &out );
	void					UpdateObjectiveInfo( void );

    bool					WeaponAvailable( const char* name );

	void					UseVehicle( void );

	void					Event_GetButtons( void );
	void					Event_GetMove( void );
	void					Event_GetViewAngles( void );
	void					Event_StopFxFov( void );
	void					Event_EnableWeapon( void );
	void					Event_DisableWeapon( void );
	void					Event_GetCurrentWeapon( void );
	void					Event_GetPreviousWeapon( void );
	void					Event_SelectWeapon( const char *weaponName );
	void					Event_GetWeaponEntity( void );
	void					Event_OpenPDA( void );
	void					Event_PDAAvailable( void );
	void					Event_InPDA( void );
	void					Event_ExitTeleporter( void );
	void					Event_HideTip( void );
	void					Event_LevelTrigger( void );
	void					Event_Gibbed( void );

    //BSM: Event to remove inventory items. Useful with powercells.
    void					Event_GiveInventoryItem(const char *name);
    void					Event_RemoveInventoryItem(const char *name);

    void					Event_ForceOrigin( idVec3& origin, idAngles& angles );
	void					Event_GetIdealWeapon( void );
    void					Event_WeaponAvailable( const char* name );
    void					Event_SetPowerupTime( int powerup, int time );
    void					Event_IsPowerupActive( int powerup );
    void					Event_StartWarp();
    void					Event_StopHelltime(int mode);
    void					Event_ToggleBloom(int on);
    void					Event_SetBloomParms(float speed, float intensity);
    void					Event_ShowConsequences(void);
    void					Event_SetViewAngles(const idVec3 &angles);
    void					Event_GetEyeHeight(void);

    // Koz
    void					Event_GetWeaponHand();
    void					Event_GetWeaponHandState();
    void					Event_GetFlashHand(); // get flashlight hand
    void					Event_GetFlashHandState(); // get flashlight hand state
    void					Event_GetFlashState(); // get flashlight state
};

ID_INLINE bool idPlayer::IsReady( void ) {
	return ready || forcedReady;
}

ID_INLINE bool idPlayer::IsRespawning( void ) {
	return respawning;
}

ID_INLINE idPhysics* idPlayer::GetPlayerPhysics( void ) {
	return &physicsObj;
}

ID_INLINE bool idPlayer::IsInTeleport( void ) {
	return ( teleportEntity.GetEntity() != NULL );
}

ID_INLINE void idPlayer::SetLeader( bool lead ) {
	leader = lead;
}

ID_INLINE bool idPlayer::IsLeader( void ) {
	return leader;
}

ID_INLINE bool idPlayer::SelfSmooth( void ) {
	return selfSmooth;
}

ID_INLINE void idPlayer::SetSelfSmooth( bool b ) {
	selfSmooth = b;
}

#endif /* !__GAME_PLAYER_H__ */
