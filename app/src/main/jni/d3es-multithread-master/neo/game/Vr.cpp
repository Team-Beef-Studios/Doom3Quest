//#pragma hdrstop

//#include"precompiled.h"

#include "../sys/platform.h"
#include "../framework/CVarSystem.h"
#include "GameBase.h"
#include "MultiplayerGame.h"
#include "Vr.h"
//#include "Voice.h"
#include "Game_local.h"
#include "physics/Clip.h"
#include "../sys/sys_public.h"
#include "../framework/Game.h"
#include "../framework/Common.h"
#include "../renderer/tr_local.h"
#include "../idlib/Lib.h"
#include "gamesys/SysCvar.h"
#include "../cm/CollisionModel.h"
#include "../framework/Licensee.h"

//#include "libs\LibOVR\Include\OVR_CAPI_GL.h"
//#include "../renderer/Framebuffer.h"

#define RADIANS_TO_DEGREES(rad) ((float) rad * (float) (180.0 / idMath::PI))

idCVar vr_d3qversion( "vr_d3qversion", ENGINE_VERSION, CVAR_ROM | CVAR_GUI, "Version" );

// *** Oculus HMD Variables
idCVar vr_scale( "vr_scale", "1.0", CVAR_FLOAT | CVAR_ARCHIVE | CVAR_GAME, "World scale. Everything virtual is this times as big." );
//In Menu - added virtual function - needs testing
idCVar vr_useFloorHeight( "vr_useFloorHeight", "1", CVAR_INTEGER | CVAR_ARCHIVE | CVAR_GAME, "0 = Custom eye height. 1 = Marine Eye Height. 2 = Normal View Height. 3 = make floor line up by Doomguy crouching. 4 = make everything line up by scaling world to your height.", 0, 4 );
idCVar vr_normalViewHeight( "vr_normalViewHeight", "73", CVAR_FLOAT | CVAR_ARCHIVE | CVAR_GAME, "Height of player's view while standing, in real world inches." );

//In Menu - don't think gun is working - investigate
idCVar vr_flashlightMode( "vr_flashlightMode", "3", CVAR_INTEGER | CVAR_ARCHIVE | CVAR_GAME, "Flashlight mount.\n0 = Body\n1 = Head\n2 = Gun\n3= Hand ( if motion controls available.)" );
idCVar vr_flashlightStrict( "vr_flashlightStrict", "0", CVAR_BOOL | CVAR_ARCHIVE | CVAR_GAME, "Flashlight location should be strictly enforced." ); // Carl

idCVar vr_flashlightBodyPosX( "vr_flashlightBodyPosX", "0", CVAR_FLOAT | CVAR_ARCHIVE | CVAR_GAME, "Flashlight vertical offset for body mount." );
idCVar vr_flashlightBodyPosY( "vr_flashlightBodyPosY", "0", CVAR_FLOAT | CVAR_ARCHIVE | CVAR_GAME, "Flashlight horizontal offset for body mount." );
idCVar vr_flashlightBodyPosZ( "vr_flashlightBodyPosZ", "0", CVAR_FLOAT | CVAR_ARCHIVE | CVAR_GAME, "Flashlight forward offset for body mount." );

idCVar vr_flashlightHelmetPosX( "vr_flashlightHelmetPosX", "6", CVAR_FLOAT | CVAR_ARCHIVE | CVAR_GAME, "Flashlight vertical offset for helmet mount." );
idCVar vr_flashlightHelmetPosY( "vr_flashlightHelmetPosY", "-6", CVAR_FLOAT | CVAR_ARCHIVE | CVAR_GAME, "Flashlight horizontal offset for helmet mount." );
idCVar vr_flashlightHelmetPosZ( "vr_flashlightHelmetPosZ", "-20", CVAR_FLOAT | CVAR_ARCHIVE | CVAR_GAME, "Flashlight forward offset for helmet mount." );

idCVar vr_forward_keyhole( "vr_forward_keyhole", "11.25", CVAR_FLOAT | CVAR_ARCHIVE | CVAR_GAME, "Forward movement keyhole in deg. If view is inside body direction +/- this value, forward movement is in view direction, not body direction" );

//In Menu - Needs testing
idCVar vr_PDAfixLocation( "vr_PDAfixLocation", "0", CVAR_BOOL | CVAR_ARCHIVE | CVAR_GAME, "Fix PDA position in space in front of player\n instead of holding in hand." );

idCVar vr_weaponPivotOffsetForward( "vr_weaponPivotOffsetForward", "3", CVAR_GAME | CVAR_ARCHIVE | CVAR_FLOAT, "" );
idCVar vr_weaponPivotOffsetHorizontal( "vr_weaponPivotOffsetHorizontal", "0", CVAR_GAME | CVAR_ARCHIVE | CVAR_FLOAT, "" );
idCVar vr_weaponPivotOffsetVertical( "vr_weaponPivotOffsetVertical", "0", CVAR_GAME | CVAR_ARCHIVE | CVAR_FLOAT, "" );
idCVar vr_weaponPivotForearmLength( "vr_weaponPivotForearmLength", "16", CVAR_GAME | CVAR_ARCHIVE | CVAR_FLOAT, "" );

//Possible want this - why is it not used
idCVar vr_guiScale( "vr_guiScale", "1", CVAR_FLOAT | CVAR_RENDERER | CVAR_ARCHIVE, "scale reduction factor for full screen menu/pda scale in VR", 0.0001f, 1.0f ); // Koz allow scaling of full screen guis/pda
//Is this what Bummser was saying about 3d - not used currently
idCVar vr_guiSeparation( "vr_guiSeparation", ".01", CVAR_FLOAT | CVAR_ARCHIVE, " Screen separation value for fullscreen guis." );

//In menu - retest
idCVar vr_guiMode( "vr_guiMode", "2", CVAR_INTEGER | CVAR_GAME | CVAR_ARCHIVE, "Gui interaction mode.\n 0 = Weapon aim as cursor\n 1 = Look direction as cursor\n 2 = Touch screen\n" );

//In menu - test
idCVar vr_hudScale( "vr_hudScale", "1.3", CVAR_FLOAT | CVAR_GAME | CVAR_ARCHIVE, "Hud scale", 0.1f, 2.0f );

idCVar vr_hudPosHorz( "vr_hudPosHorz", "0", CVAR_FLOAT | CVAR_GAME | CVAR_ARCHIVE, "Hud Horizontal offset in inches" );
idCVar vr_hudPosVert( "vr_hudPosVert", "4", CVAR_FLOAT | CVAR_GAME | CVAR_ARCHIVE, "Hud Vertical offset in inches" );
idCVar vr_hudPosDist( "vr_hudPosDist", "24", CVAR_FLOAT | CVAR_GAME | CVAR_ARCHIVE, "Hud Distance from view in inches" );
idCVar vr_hudPosAngle( "vr_hudPosAngle", "30", CVAR_FLOAT | CVAR_GAME | CVAR_ARCHIVE, "Hud View Angle" );

//In Menu - working
idCVar vr_hudPosLock( "vr_hudPosLock", "1", CVAR_INTEGER | CVAR_GAME | CVAR_ARCHIVE, "Lock Hud to:  0 = Face, 1 = Body" );
//In Menu - working but look activated is still not good IMO
idCVar vr_hudType( "vr_hudType", "1", CVAR_INTEGER | CVAR_GAME | CVAR_ARCHIVE, "VR Hud Type. 0 = Disable.\n1 = Full\n2=Look Activate", 0, 2 );
idCVar vr_hudRevealAngle( "vr_hudRevealAngle", "40", CVAR_FLOAT | CVAR_GAME | CVAR_ARCHIVE, "HMD pitch to reveal HUD in look activate mode." );

//In Menu - Working
idCVar vr_hudTransparency( "vr_hudTransparency", "1", CVAR_FLOAT | CVAR_GAME | CVAR_ARCHIVE, " Hud transparency. 0.0 = Invisible thru 1.0 = full", 0.0, 100.0 );

idCVar vr_hudOcclusion( "vr_hudOcclusion", "1", CVAR_BOOL | CVAR_GAME | CVAR_ARCHIVE, " Hud occlusion. 0 = Objects occlude HUD, 1 = No occlusion " );
idCVar vr_hudLowHealth( "vr_hudLowHealth", "20", CVAR_INTEGER | CVAR_GAME | CVAR_ARCHIVE, " 0 = Disable, otherwise force hud if heath below this value." );

//GB Why is this never used?! / Also not in menu
idCVar vr_wristStatMon( "vr_wristStatMon", "2", CVAR_INTEGER | CVAR_ARCHIVE, "Use wrist status monitor. 0 = Disable 1 = Right Wrist 2 = Left Wrist " );

idCVar vr_disableWeaponAnimation( "vr_disableWeaponAnimation", "1", CVAR_BOOL | CVAR_ARCHIVE | CVAR_GAME, "Disable weapon animations in VR. ( 1 = disabled )" );
idCVar vr_headKick( "vr_headKick", "0", CVAR_BOOL | CVAR_ARCHIVE | CVAR_GAME, "Damage can 'kick' the players view. 0 = Disabled in VR." );

idCVar vr_joystickMenuMapping( "vr_joystickMenuMapping", "1", CVAR_BOOL | CVAR_ARCHIVE | CVAR_GAME, " Use alternate joy mapping\n in menus/PDA.\n 0 = D3 Standard\n 1 = VR Mode.\n(Both joys can nav menus,\n joy r/l to change\nselect area in PDA." );

idCVar	vr_deadzonePitch( "vr_deadzonePitch", "90", CVAR_FLOAT | CVAR_GAME | CVAR_ARCHIVE, "Vertical Aim Deadzone", 0, 180 );
idCVar	vr_deadzoneYaw( "vr_deadzoneYaw", "30", CVAR_FLOAT | CVAR_GAME | CVAR_ARCHIVE, "Horizontal Aim Deadzone", 0, 180 );

//In menu - Working fine
idCVar	vr_weaponSight( "vr_weaponSight", "3", CVAR_INTEGER | CVAR_ARCHIVE, "Weapon Sight.\n 0 = Lasersight\n 1 = Red dot\n 2 = Circle dot\n 3 = Crosshair\n 4 = Beam + Dot\n" );
idCVar	vr_weaponSightToSurface( "vr_weaponSightToSurface", "1", CVAR_INTEGER | CVAR_ARCHIVE, "Map sight to surface. 0 = Disabled 1 = Enabled\n" );

//Added to menu - needs testing
idCVar	vr_motionWeaponPitchAdj( "vr_motionWeaponPitchAdj", "40", CVAR_FLOAT | CVAR_ARCHIVE, "Weapon controller pitch adjust" );
idCVar	vr_motionFlashPitchAdj( "vr_motionFlashPitchAdj", "40", CVAR_FLOAT | CVAR_ARCHIVE, "Flashlight controller pitch adjust" );

idCVar	vr_nodalX( "vr_nodalX", "-3", CVAR_FLOAT | CVAR_ARCHIVE, "Forward offset from eyes to neck" );
idCVar	vr_nodalZ( "vr_nodalZ", "-6", CVAR_FLOAT | CVAR_ARCHIVE, "Vertical offset from neck to eye height" );

idCVar vr_controllerOffsetX( "vr_controllerOffsetX", "-1.4", CVAR_FLOAT | CVAR_ARCHIVE, "Controller X offset to handle center" ); // these values work for steam
idCVar vr_controllerOffsetY( "vr_controllerOffsetY", "-1.32", CVAR_FLOAT | CVAR_ARCHIVE, "Controller Y offset to handle center" );
idCVar vr_controllerOffsetZ( "vr_controllerOffsetZ", "-0.9", CVAR_FLOAT | CVAR_ARCHIVE, "Controller Z offset to handle center" );
idCVar vr_vcx( "vr_vcx", "0", CVAR_FLOAT | CVAR_ARCHIVE, "Controller X offset to handle center" ); // these values work for steam
idCVar vr_vcy( "vr_vcy", "0", CVAR_FLOAT | CVAR_ARCHIVE, "Controller Y offset to handle center" );
idCVar vr_vcz( "vr_vcz", "0", CVAR_FLOAT | CVAR_ARCHIVE, "Controller Z offset to handle center" );
//idCVar vr_vcx( "vr_vcx", "-3.5", CVAR_FLOAT | CVAR_ARCHIVE, "Controller X offset to handle center" ); // these values work for steam
//idCVar vr_vcy( "vr_vcy", "0", CVAR_FLOAT | CVAR_ARCHIVE, "Controller Y offset to handle center" );
//idCVar vr_vcz( "vr_vcz", "-.5", CVAR_FLOAT | CVAR_ARCHIVE, "Controller Z offset to handle center" );

idCVar vr_mountx( "vr_mountx", "0", CVAR_FLOAT | CVAR_ARCHIVE, "If motion controller mounted on object, X offset from controller to object handle.\n (Eg controller mounted on Topshot)" );
idCVar vr_mounty( "vr_mounty", "0", CVAR_FLOAT | CVAR_ARCHIVE, "If motion controller mounted on object, Y offset from controller to object handle.\n (Eg controller mounted on Topshot)" );
idCVar vr_mountz( "vr_mountz", "0", CVAR_FLOAT | CVAR_ARCHIVE, "If motion controller mounted on object, Z offset from controller to object handle.\n (Eg controller mounted on Topshot)" );

idCVar vr_mountedWeaponController( "vr_mountedWeaponController", "0", CVAR_BOOL | CVAR_ARCHIVE, "If physical controller mounted on object (eg topshot), enable this to apply mounting offsets\n0=disabled 1 = enabled" );

//Why is this not used??
idCVar vr_3dgui( "vr_3dgui", "1", CVAR_BOOL | CVAR_ARCHIVE, "3d effects for in game guis. 0 = disabled 1 = enabled\n" );

//In Menu - Needs Testing
idCVar vr_shakeAmplitude( "vr_shakeAmplitude", "1.0", CVAR_FLOAT | CVAR_ARCHIVE, "Screen shake amplitude 0.0 = disabled to 1.0 = full\n", 0.0f, 1.0f );

idCVar vr_knockBack( "vr_knockBack", "0", CVAR_BOOL | CVAR_ARCHIVE | CVAR_GAME, "Enable damage knockback in VR. 0 = Disabled, 1 = Enabled" );
idCVar vr_jumpBounce( "vr_jumpBounce", "0", CVAR_FLOAT | CVAR_ARCHIVE | CVAR_GAME, "Enable view bounce after jumping. 0 = Disabled, 1 = Full", 0.0f, 1.0f ); // Carl

//Why is this not used?? Does Dr Beef do this already
idCVar vr_stepSmooth( "vr_stepSmooth", "1", CVAR_FLOAT | CVAR_ARCHIVE | CVAR_GAME, "Enable smoothing when climbing stairs. 0 = Disabled, 1 = Full", 0.0f, 1.0f ); // Carl

//Added to menu - needs testing
idCVar vr_walkSpeedAdjust( "vr_walkSpeedAdjust", "20", CVAR_FLOAT | CVAR_ARCHIVE, "Player walk speed adjustment for VR" );

//What is this??
idCVar vr_headbbox( "vr_headbbox", "10.0", CVAR_FLOAT | CVAR_ARCHIVE, "" );

//GB I've re-used this for both fixed position and hand. Check fixed and if worth keeping separate the two
idCVar vr_pdaTuning( "vr_pdaTuning", "0", CVAR_BOOL | CVAR_GAME, "Hold the off-hand grip to adjust vr_pdaPosX/Y/Z with the sticks, and log them once a second." );
idCVar vr_pdaPosX( "vr_pdaPosX", "-2.0", CVAR_FLOAT | CVAR_ARCHIVE, "" );
idCVar vr_pdaPosY( "vr_pdaPosY", "0.0", CVAR_FLOAT | CVAR_ARCHIVE, "" );
idCVar vr_pdaPosZ( "vr_pdaPosZ", "-1.0", CVAR_FLOAT | CVAR_ARCHIVE, "" );
idCVar vr_pdaPitch( "vr_pdaPitch", "30", CVAR_FLOAT | CVAR_ARCHIVE, "" );

idCVar vr_comfortRepeat( "vr_comfortRepeat", "100", CVAR_ARCHIVE | CVAR_INTEGER, "Delay in MS between repeating comfort snap turns." );

idCVar vr_movePoint( "vr_movePoint", "1", CVAR_INTEGER | CVAR_ARCHIVE, "0: Standard Stick Move, 1: Off Hand = Forward, 2: Look = forward, 3: Weapon Hand = Forward, 4: Left Hand = Forward, 5: Right Hand = Forward", 0, 5 );
idCVar vr_moveClick( "vr_moveClick", "0", CVAR_INTEGER | CVAR_ARCHIVE, " 0 = Normal movement.\n 1 = Click and hold to walk, run button to run.\n 2 = Click to start walking, then touch only. Run btn to run.\n 3 = Click to start walking, hold click to run.\n 4 = Click to start walking, then click toggles run\n" );

//In Menu - working
idCVar vr_playerBodyMode( "vr_playerBodyMode", "0", CVAR_INTEGER | CVAR_GAME | CVAR_ARCHIVE, "Player body mode:\n0 = Display full body\n1 = Just Hands \n2 = Weapons only\n" );

idCVar vr_moveThirdPerson( "vr_moveThirdPerson", "1", CVAR_BOOL | CVAR_GAME | CVAR_ARCHIVE, "Artifical movement will user 3rd person perspective." );
//needs to be added to menu - cant be bothered now
idCVar vr_crouchMode( "vr_crouchMode", "0", CVAR_INTEGER | CVAR_GAME | CVAR_ARCHIVE, "Crouch Mode:\n 0 = Full motion crouch (In game matches real life)\n 1 = Crouch anim triggered by smaller movement." );
idCVar vr_crouchTriggerDist( "vr_crouchTriggerDist", "7", CVAR_FLOAT | CVAR_ARCHIVE, " Distance ( in real-world inches ) player must crouch in real life to toggle crouch\n" );
idCVar vr_crouchHideBody( "vr_crouchHideBody", "1", CVAR_FLOAT | CVAR_ARCHIVE, "Hide body ( if displayed )  when crouching. 0 = Dont hide, 1 = hide." );

idCVar vr_frameCheck( "vr_frameCheck", "1", CVAR_INTEGER | CVAR_ARCHIVE, "0 = bypass frame check" );

idCVar vr_teleportSkipHandrails( "vr_teleportSkipHandrails", "0", CVAR_INTEGER | CVAR_ARCHIVE, "Teleport aim ingnores handrails. 1 = true" );
idCVar vr_teleportShowAimAssist( "vr_teleportShowAimAssist", "0", CVAR_INTEGER | CVAR_ARCHIVE, "Move telepad target to reflect aim assist. 1 = true" );
idCVar vr_teleportButtonMode( "vr_teleportButtonMode", "0", CVAR_BOOL | CVAR_ARCHIVE, "0 = Press aim, release teleport.\n1 = 1st press aim, 2nd press teleport" );
idCVar vr_teleportHint( "vr_teleportHint", "0", CVAR_BOOL | CVAR_ARCHIVE, "" ); // Koz blech hack - used for now to keep track if the game has issued the player the hint about ducking when the teleport target is red.

//GB See if we can get this from the API
idCVar vr_useHandPoses( "vr_useHandPoses", "0", CVAR_BOOL | CVAR_ARCHIVE, "If using oculus touch, enable finger poses when hands are empty or in guis" );
// Koz end
// Carl
idCVar vr_teleport( "vr_teleport", "2", CVAR_INTEGER | CVAR_ARCHIVE, "Player can teleport at will. 0 = disabled, 1 = gun sight, 2 = right hand, 3 = left hand, 4 = head", 0, 4 );
idCVar vr_teleportMode("vr_teleportMode", "0", CVAR_INTEGER | CVAR_ARCHIVE, "Teleport Mode. 0 = Blink (default), 1 = Doom VFR style (slow time and warp speed), 2 = Doom VFR style + jet strafe)", 0, 2);
idCVar vr_teleportMaxTravel( "vr_teleportMaxTravel", "950", CVAR_INTEGER | CVAR_ARCHIVE, "Maximum teleport path length/complexity/time. About 250 or 500 are good choices, but must be >= about 950 to use tightrope in MC Underground.", 150, 5000 );
idCVar vr_teleportThroughDoors( "vr_teleportThroughDoors", "0", CVAR_BOOL | CVAR_ARCHIVE, "Player can teleport somewhere visible even if the path to get there takes them through closed (but not locked) doors." );

//GB Check these
idCVar vr_motionSickness( "vr_motionSickness", "0", CVAR_INTEGER | CVAR_ARCHIVE, "Motion sickness prevention aids. 0 = None, 1 = Chaperone, 2 = Reduce FOV, 3 = Black Screen, 4 = Black & Chaperone, 5 = Reduce FOV & Chaperone, 6 = Slow Mo, 7 = Slow Mo & Chaperone, 8 = Slow Mo & Reduce FOV, 9 = Slow Mo, Chaperone, Reduce FOV, 10 = Third Person, 11 = Particles, 12 = Particles & Chaperone", 0, 12 );

idCVar vr_strobeTime( "vr_strobeTime", "500", CVAR_INTEGER | CVAR_ARCHIVE, "Time in ms between flashes when blacking screen. 0 = no strobe" );

idCVar vr_handSwapsAnalogs( "vr_handSwapsAnalogs", "0", CVAR_BOOL | CVAR_ARCHIVE, "Should swapping the weapon hand affect analog controls (stick or touchpad) or just buttons/triggers? 0 = only swap buttons, 1 = swap all controls" );
idCVar vr_autoSwitchControllers( "vr_autoSwitchControllers", "1", CVAR_BOOL | CVAR_ARCHIVE, "Automatically switch to/from gamepad mode when using gamepad/motion controller. Should be true unless you're trying to use both together, or you get false detections. 0 = no, 1 = yes." );

//In Menu - Test
idCVar vr_cinematics("vr_cinematics", "0", CVAR_INTEGER | CVAR_ARCHIVE, "Cinematic type. 0 = Immersive, 1 = Cropped, 2 = Projected");

idCVar vr_instantAccel( "vr_instantAccel", "1", CVAR_BOOL | CVAR_ARCHIVE, "Instant Movement Acceleration. 0 = Disabled 1 = Enabled" );
idCVar vr_shotgunChoke( "vr_shotgunChoke", "90", CVAR_FLOAT | CVAR_ARCHIVE, "% To choke shotgun. 0 = None, 100 = Full Choke\n" );
idCVar vr_headshotMultiplier( "vr_headshotMultiplier", "2.5", CVAR_FLOAT | CVAR_ARCHIVE, "Damage multiplier for headshots when using Fists,Pistol,Shotgun,Chaingun or Plasmagun.", 1, 5 );
idCVar vr_headshotMultiplierRecruit( "vr_headshotMultiplierRecruit", "2.0", CVAR_FLOAT | CVAR_ARCHIVE, "Recruit Level Damage multiplier for headshots when using Fists,Pistol,Shotgun,Chaingun or Plasmagun.", 1, 5 );
idCVar vr_headshotMultiplierNormal( "vr_headshotMultiplierNormal", "1.5", CVAR_FLOAT | CVAR_ARCHIVE, "Normal Level Damage multiplier for headshots when using Fists,Pistol,Shotgun,Chaingun or Plasmagun.", 1, 5 );
idCVar vr_headshotMultiplierHard( "vr_headshotMultiplierHard", "1.25", CVAR_FLOAT | CVAR_ARCHIVE, "Hard Level Damage multiplier for headshots when using Fists,Pistol,Shotgun,Chaingun or Plasmagun.", 1, 5 );
idCVar vr_headshotMultiplierNightmare( "vr_headshotMultiplierNightmare", "1.0", CVAR_FLOAT | CVAR_ARCHIVE, "Nightmare Level Damage multiplier for headshots when using Fists,Pistol,Shotgun,Chaingun or Plasmagun.", 1, 5 );

// Carl
idCVar vr_weaponCycleMode( "vr_weaponCycleMode", "0", CVAR_INTEGER | CVAR_GAME | CVAR_ARCHIVE, "When cycling through weapons\n0 = skip holstered weapons, 1 = include holstered weapons, 2 = flashlight but not holstered, 3 = holstered+flashlight, 4 = holstered+flashlight+pda" );
idCVar vr_gripMode( "vr_gripMode", "0", CVAR_INTEGER | CVAR_GAME | CVAR_ARCHIVE, "How the grip button works\n0 = context sensitive toggle, 1 = context sensitive toggle no surface, 2 = toggle for weapons/items hold for physics objects, 3 = toggle for weapons hold for physics/items, 4 = always toggle (can drop), 5 = Dead and Burried, 6 = hold to hold, 7 = hold to hold squeeze for action" );
idCVar vr_mustEmptyHands( "vr_mustEmptyHands", "0", CVAR_BOOL | CVAR_GAME | CVAR_ARCHIVE, "Do you need to have an empty hand to interact with things?\n0 = no, it works automatically; 1 = yes, your hand must be empty" );
idCVar vr_contextSensitive( "vr_contextSensitive", "1", CVAR_BOOL | CVAR_GAME | CVAR_ARCHIVE, "Are buttons context sensitive?\n0 = no just map the buttons in the binding window, 1 = yes, context sensitive buttons (default)" );
idCVar vr_dualWield( "vr_dualWield", "2", CVAR_INTEGER | CVAR_GAME | CVAR_ARCHIVE, "Can you use two weapons at once?\n0 = not even fists, 1 = nothing, 2 = only flashlight, 3 = only grenades (VFR), 4 = only grenades/flashlight, 5 = only pistols, 6 = only pistols/flashlight, 7 = only pistols/grenades/flashlight, 8 = yes" );

idCVar vr_debugHands( "vr_debugHands", "0", CVAR_BOOL | CVAR_GAME, "Enable hand/weapon/dual wielding debugging" );
idCVar vr_rumbleChainsaw( "vr_rumbleChainsaw", "1", CVAR_BOOL | CVAR_GAME | CVAR_ARCHIVE, "Enable weapon (currently chainsaw only) constant haptic feedback in VR. Not recommended for wireless VR controllers." );

//===================================================================

int fboWidth;
int fboHeight;

iVr vrCom;
iVr* commonVr = &vrCom;

//iVoice _voice; //avoid nameclash with timidity
//iVoice* commonVoice = &_voice;

void SwapBinding(int Old, int New)
{
    /*idStr s = idKeyInput::GetBinding(New);
    idKeyInput::SetBinding(New, idKeyInput::GetBinding(Old));
    idKeyInput::SetBinding(Old, s.c_str());*/
}


void SwapWeaponHand()
{
    vr_weaponHand.SetInteger(1 - vr_weaponHand.GetInteger());
    // swap teleport hand
    if (vr_teleport.GetInteger() == 2)
        vr_teleport.SetInteger( 3 );
    else if (vr_teleport.GetInteger() == 3)
        vr_teleport.SetInteger( 2 );
    // swap motion controller bindings to other hand
    //GBCHANGE
    /*for (int k = K_JOY17; k <= K_JOY18; k++)
        SwapBinding(k, k + 7);
    // JOY19 is the Touch menu button, which only exists on the left hand
    for (int k = K_JOY20; k <= K_JOY23; k++)
        SwapBinding(k, k + 7);
    for (int k = K_JOY31; k <= K_JOY48; k++)
        SwapBinding(k, k + 18);
    SwapBinding(K_L_TOUCHTRIG, K_R_TOUCHTRIG);
    SwapBinding(K_L_STEAMVRTRIG, K_R_STEAMVRTRIG);*/
    /*
    if (vr_handSwapsAnalogs.GetBool())
    {
        for (int k = K_TOUCH_LEFT_STICK_UP; k <= K_TOUCH_LEFT_STICK_RIGHT; k++)
            SwapBinding(k, k + 4);
        for (int k = K_STEAMVR_LEFT_PAD_UP; k <= K_STEAMVR_LEFT_PAD_RIGHT; k++)
            SwapBinding(k, k + 4);
        for (int k = K_STEAMVR_LEFT_JS_UP; k <= K_STEAMVR_LEFT_JS_RIGHT; k++)
            SwapBinding(k, k + 4);
    }*/
}

/*
==============
iVr::iVr()
==============
*/
iVr::iVr()
{
    lastComfortTime = 0;
    lastFrame = -1;
    initialResetPerformed = false;
    hasHMD = true;
    hasOculusRift = true;
    VR_GAME_PAUSED = false;
    PDAforcetoggle = false;
    PDAforced = false;
    PDArising = false;
    gameSavingLoading = false;
    showingIntroVideo = true;
    forceLeftStick = true;	// start the PDA in the left menu.
    pdaToggleTime = 0;
    lastSaveTime = 0;
    wasSaved = false;
    wasLoaded = false;
    shouldRecenter = false;
    restoreFlashlightMode = false;
    PDAclipModelSet = false;
    useFBO = false;

    VR_USE_MOTION_CONTROLS = true;
    motionControlType = MOTION_OCULUS;

    scanningPDA = false;

    vrIsBackgroundSaving = false;

    VRScreenSeparation = 0.0f;

    officialIPD = 64.0f;
    officialHeight = 72.0f;

    manualIPD = 64.0f;
    manualHeight = 72.0f;

    hmdPositionTracked = false;

    vrFrameNumber = 0;
    lastPostFrame = 0;


    lastViewOrigin = vec3_zero;
    lastViewAxis = mat3_identity;

    lastCenterEyeOrigin = vec3_zero;
    lastCenterEyeAxis = mat3_identity;

    bodyYawOffset = 0.0f;
    lastHMDYaw = 0.0f;
    lastHMDPitch = 0.0f;
    lastHMDRoll = 0.0f;
    lastHMDViewOrigin = vec3_zero;
    lastHMDViewAxis = mat3_identity;
    headHeightDiff = 0;

    motionMoveDelta = vec3_zero;
    motionMoveVelocity = vec3_zero;
    leanOffset = vec3_zero;
    leanBlankOffset = vec3_zero;
    leanBlankOffsetLengthSqr = 0.0f;
    leanBlank = false;
    isLeaning = false;

    thirdPersonMovement = false;
    thirdPersonDelta = 0.0f;
    thirdPersonHudAxis = mat3_identity;
    thirdPersonHudPos = vec3_zero;

    chestDefaultDefined = false;

    currentFlashlightPosition = FLASHLIGHT_BODY;

    handInGui = false;

    handRoll[0] = 0.0f;
    handRoll[1] = 0.0f;

    fingerPose[0] = 0;
    fingerPose[1] = 0;

    angles[3] = { 0.0f };

    swfRenderMode = RENDERING_NORMAL;

    isWalking = false;

    forceRun = false;

    hmdBodyTranslation = vec3_zero;

    VR_AAmode = 0;

    independentWeaponYaw = 0;
    independentWeaponPitch = 0;

    playerDead = false;

    hmdWidth = 0;
    hmdHeight = 0;

    primaryFBOWidth = 0;
    primaryFBOHeight = 0;
    hmdHz = 90;

    hmdFovX = 0.0f;
    hmdFovY = 0.0f;

    hmdPixelScale = 1.0f;
    hmdAspect = 1.0f;


    /*
    hmdSession = nullptr;
	ovrLuid.Reserved[0] = { 0 };

	oculusSwapChain[0] = nullptr;
	oculusSwapChain[1] = nullptr;
	oculusFboId = 0;
	ocululsDepthTexID = 0;
	oculusMirrorFboId = 0;
	oculusMirrorTexture = 0;
	mirrorTexId = 0;
	mirrorW = 0;
	mirrorH = 0;
    */


    hmdEyeImage[0] = 0;
    hmdEyeImage[1] = 0;
    hmdCurrentRender[0] = 0;
    hmdCurrentRender[1] = 0;

    // wip stuff
    // wip stuff
    wipNumSteps = 0;
    wipStepState = 0;
    wipLastPeriod = 0;
    wipCurrentDelta = 0.0f;
    wipCurrentVelocity = 0.0f;

    wipTotalDelta = 0.0f;
    wipLastAcces = 0.0f;
    wipAvgPeriod = 0.0f;
    wipTotalDeltaAvg = 0.0f;

    hmdFrameTime = 0;

    lastRead = 0;
    currentRead = 0;
    updateScreen = false;

    bodyMoveAng = 0.0f;
    teleportButtonCount = 0;


    currentFlashlightMode = vr_flashlightMode.GetInteger();
    renderingSplash = true;

    currentBindingDisplay = "";

    cinematicStartViewYaw = 0.0f;
    cinematicStartPosition = vec3_zero;

    didTeleport = false;
    teleportDir = 0.0f;

    currentHandWorldPosition[0] = vec3_zero;
    currentHandWorldPosition[1] = vec3_zero;



}

/*
==============
iVr::HMDInit
==============
*/

void iVr::HMDInit( void )
{
    hasHMD = false;
    game->isVR = false;
    common->Printf( "\n\n HMD Initialized\n" );
    hasHMD = true;
    game->isVR = true;
    common->Printf( "VR_USE_MOTION_CONTROLS Final = %d\n", VR_USE_MOTION_CONTROLS );
}





/*
==============
iVr::HMDShutdown
==============
*/

void iVr::HMDShutdown( void )
{
#ifdef USE_OVR
    if ( hasOculusRift )
	{
		ovr_DestroyTextureSwapChain( hmdSession, oculusSwapChain[0] );
		ovr_DestroyTextureSwapChain( hmdSession, oculusSwapChain[1] );

		ovr_Destroy( hmdSession );
		ovr_Shutdown();
		hmdSession = NULL;
	}
	else
#endif
    {
        //vr::VR_Shutdown();
    }
    //m_pHMD = NULL;
    return;
}

void iVr::HMDGetOrientation( idAngles &hmdAngles, idVec3 &headPositionDelta, idVec3 &bodyPositionDelta, idVec3 &absolutePosition, bool resetTrackingOffset )
{
    static double time = 0.0;
    static int currentlyTracked;
    static int lastFrameReturned = -1;
    static uint lastIdFrame = -1;
    static float lastRoll = 0.0f;
    static float lastPitch = 0.0f;
    static float lastYaw = 0.0f;
    static idVec3 lastHmdPosition = vec3_zero;

    static idVec3 hmdPosition;
    static idVec3 lastHmdPos2 = vec3_zero;
    static idMat3 hmdAxis = mat3_identity;

    static bool	neckInitialized = false;
    static idVec3 initialNeckPosition = vec3_zero;
    static idVec3 currentNeckPosition = vec3_zero;
    static idVec3 lastNeckPosition = vec3_zero;

    static idVec3 currentChestPosition = vec3_zero;
    static idVec3 lastChestPosition = vec3_zero;

    static float chestLength = 0;
    static bool chestInitialized = false;

    idVec3 neckToChestVec = vec3_zero;
    idMat3 neckToChestMat = mat3_identity;
    idAngles neckToChestAng = ang_zero;

    static idVec3 lastHeadPositionDelta = vec3_zero;
    static idVec3 lastBodyPositionDelta = vec3_zero;
    static idVec3 lastAbsolutePosition = vec3_zero;

    //static vr::TrackedDevicePose_t lastTrackedPoseOpenVR = { 0.0f };


    if ( !pVRClientInfo )
    {
        hmdAngles.roll = 0.0f;
        hmdAngles.pitch = 0.0f;
        hmdAngles.yaw = 0.0f;
        headPositionDelta = vec3_zero;
        bodyPositionDelta = vec3_zero;
        absolutePosition = vec3_zero;
        return;
    }

    lastBodyYawOffset = bodyYawOffset;
    poseLastHmdAngles = poseHmdAngles;
    poseLastHmdHeadPositionDelta = poseHmdHeadPositionDelta;
    poseLastHmdBodyPositionDelta = poseHmdBodyPositionDelta;
    poseLastHmdAbsolutePosition = poseHmdAbsolutePosition;


    if ( vr_frameCheck.GetInteger() == 1 && common->GetFrameNumber() == lastFrame )//&& !commonVr->renderingSplash )
    {
        //make sure to return the same values for this frame.
        hmdAngles.roll = lastRoll;
        hmdAngles.pitch = lastPitch;
        hmdAngles.yaw = lastYaw;
        headPositionDelta = lastHeadPositionDelta;
        bodyPositionDelta = lastBodyPositionDelta;

        if ( resetTrackingOffset == true )
        {

            trackingOriginOffset = lastHmdPosition;
            trackingOriginHeight = trackingOriginOffset.z;
            /*if (vr_useFloorHeight.GetInteger() == 0)
                trackingOriginOffset.z += pm_normalviewheight.GetFloat() + 5 + CM_CLIP_EPSILON - vr_normalViewHeight.GetFloat() / vr_scale.GetFloat();
            else if (vr_useFloorHeight.GetInteger() == 2)
                trackingOriginOffset.z += 5;
            else if (vr_useFloorHeight.GetInteger() == 3)
                trackingOriginOffset.z = pm_normalviewheight.GetFloat() + 5 + CM_CLIP_EPSILON;
            else if (vr_useFloorHeight.GetInteger() == 4)
            {
                float oldScale = vr_scale.GetFloat();
                float h = trackingOriginHeight * oldScale;
                float newScale = h / 73.0f;
                trackingOriginHeight *= oldScale / newScale;
                trackingOriginOffset *= oldScale / newScale;
                vr_scale.SetFloat( newScale );
            }*/
            common->Printf( "Resetting tracking yaw offset.\n Yaw = %f old offset = %f ", hmdAngles.yaw, trackingOriginYawOffset );
            trackingOriginYawOffset = hmdAngles.yaw;
            common->Printf( "New Tracking yaw offset %f\n", hmdAngles.yaw, trackingOriginYawOffset );
            neckInitialized = false;

            cinematicStartViewYaw = trackingOriginYawOffset;

        }
        //common->Printf( "HMDGetOrientation FramCheck Bail == idLib:: framenumber  lf %d  ilfn %d  rendersplash = %d\n", lastFrame, common->GetFrameNumber(), commonVr->renderingSplash );
        return;
    }

    lastFrame = common->GetFrameNumber();

    static idPosef translationPose;
    static idPosef	orientationPose;
    static idPosef cameraPose;
    static idPosef lastTrackedPoseOculus;
    lastTrackedPoseOculus.Position = idVec3(0.0,0.0,0.0);
    lastTrackedPoseOculus.Orientation = idQuat(0.0,0.0,0.0,0.0);

    if ( hasOculusRift )
    {
        //hmdFrameTime = ovr_GetPredictedDisplayTime( hmdSession, lastFrame );
        //common->Printf( "HMDGetOrientation lastframe idLib::framenumber = %d\n", lastFrame );
        //time = hmdFrameTime;

        //hmdTrackingState = ovr_GetTrackingState( hmdSession, time, false );

        //currentlyTracked = hmdTrackingState.StatusFlags & ( ovrStatus_PositionTracked );
        //GBCHANGE
        currentlyTracked = true;

        if (currentlyTracked)
        {
            idVec3 _hmdPosition = idVec3(pVRClientInfo->hmdposition[0],pVRClientInfo->hmdposition[1],pVRClientInfo->hmdposition[2]);
            //static idVec3 _hmdTranslation = idVec3(pVRClientInfo->hmdtranslation[0],pVRClientInfo->hmdtranslation[1],pVRClientInfo->hmdtranslation[2]);
            idQuat _hmdOrientation = idQuat(pVRClientInfo->hmdorientation_quat[0],pVRClientInfo->hmdorientation_quat[1],pVRClientInfo->hmdorientation_quat[2],pVRClientInfo->hmdorientation_quat[3]);
            translationPose.Position = _hmdPosition;
            translationPose.Orientation = _hmdOrientation;
            //translationPose.Translation = _hmdTranslation;
            //translationPose = hmdTrackingState.HeadPose.ThePose;
            lastTrackedPoseOculus = translationPose;
        }
        else
        {
            translationPose = lastTrackedPoseOculus;
        }

        //GB Get all hand poses
        idVec3 _lhandPosition = idVec3(pVRClientInfo->lhandposition[0],
                                       pVRClientInfo->lhandposition[1],
                                       pVRClientInfo->lhandposition[2]);
        idQuat _lhandOrientation = idQuat(pVRClientInfo->lhand_orientation_quat[0],
                                          pVRClientInfo->lhand_orientation_quat[1],
                                          pVRClientInfo->lhand_orientation_quat[2],
                                          pVRClientInfo->lhand_orientation_quat[3]);
        idVec3 _rhandPosition = idVec3(pVRClientInfo->rhandposition[0],
                                       pVRClientInfo->rhandposition[1],
                                       pVRClientInfo->rhandposition[2]);
        idQuat _rhandOrientation = idQuat(pVRClientInfo->rhand_orientation_quat[0],
                                          pVRClientInfo->rhand_orientation_quat[1],
                                          pVRClientInfo->rhand_orientation_quat[2],
                                          pVRClientInfo->rhand_orientation_quat[3]);


        handPose[1].Orientation = _lhandOrientation;
        handPose[1].Position = _lhandPosition;
        handPose[0].Orientation = _rhandOrientation;
        handPose[0].Position = _rhandPosition;

        for (int i = 0; i < 2; i++)
        {
            MotionControlGetHand(i, poseHandPos[i], poseHandRotationQuat[i]);
            poseHandRotationMat3[i] = poseHandRotationQuat[i].ToMat3();
            poseHandRotationAngles[i] = poseHandRotationQuat[i].ToAngles();
        }
    }

    if (hasOculusRift)
    {
        hmdPosition.x = -translationPose.Position.z * (100.0f / 2.54f) / vr_scale.GetFloat(); // Koz convert position (in meters) to inch (1 id unit = 1 inch).
        hmdPosition.y = -translationPose.Position.x * (100.0f / 2.54f) / vr_scale.GetFloat();
        hmdPosition.z = translationPose.Position.y * (100.0f / 2.54f) / vr_scale.GetFloat();
    }

    lastHmdPosition = hmdPosition;

    static idQuat poseRot;// = idQuat_zero;
    static idAngles poseAngles = ang_zero;

    if (hasOculusRift)
    {
        static idPosef	orientationPose;
        orientationPose.Orientation = idQuat(pVRClientInfo->hmdorientation_quat[0],pVRClientInfo->hmdorientation_quat[1],pVRClientInfo->hmdorientation_quat[2],pVRClientInfo->hmdorientation_quat[3]);

        poseRot.x = orientationPose.Orientation.z;	// x;
        poseRot.y = orientationPose.Orientation.x;	// y;
        poseRot.z = -orientationPose.Orientation.y;	// z;
        poseRot.w = orientationPose.Orientation.w;
    }

    poseAngles = poseRot.ToAngles();

    hmdAngles.yaw = poseAngles.yaw;
    hmdAngles.roll = poseAngles.roll;
    hmdAngles.pitch = poseAngles.pitch;

    lastRoll = hmdAngles.roll;
    lastPitch = hmdAngles.pitch;
    lastYaw = hmdAngles.yaw;

    hmdPosition += hmdForwardOffset * poseAngles.ToMat3()[0];

    if ( resetTrackingOffset == true )
    {

        trackingOriginOffset = lastHmdPosition;
        trackingOriginHeight = trackingOriginOffset.z;
        /*if (vr_useFloorHeight.GetInteger() == 0)
            trackingOriginOffset.z += pm_normalviewheight.GetFloat() + 5 + CM_CLIP_EPSILON - vr_normalViewHeight.GetFloat() / vr_scale.GetFloat();
        else if (vr_useFloorHeight.GetInteger() == 2)
            trackingOriginOffset.z += 5;
        else if (vr_useFloorHeight.GetInteger() == 3)
            trackingOriginOffset.z = pm_normalviewheight.GetFloat() + 5 + CM_CLIP_EPSILON;
        else if (vr_useFloorHeight.GetInteger() == 4)
        {
            float oldScale = vr_scale.GetFloat();
            float h = trackingOriginHeight * oldScale;
            float newScale = h / (pm_normalviewheight.GetFloat() + 5 + CM_CLIP_EPSILON);
            trackingOriginHeight *= oldScale / newScale;
            trackingOriginOffset *= oldScale / newScale;
            vr_scale.SetFloat(newScale);
        }*/
        common->Printf("Resetting tracking yaw offset.\n Yaw = %f old offset = %f ", hmdAngles.yaw, trackingOriginYawOffset);
        trackingOriginYawOffset = hmdAngles.yaw;
        common->Printf( "New Tracking yaw offset %f\n", hmdAngles.yaw, trackingOriginYawOffset );
        neckInitialized = false;
        cinematicStartViewYaw = trackingOriginYawOffset;
        initialResetPerformed = true;
        return;
    }

    hmdPosition -= trackingOriginOffset;

    hmdPosition *= idAngles( 0.0f, -trackingOriginYawOffset, 0.0f ).ToMat3();

    absolutePosition = hmdPosition;

    hmdAngles.yaw -= trackingOriginYawOffset;
    hmdAngles.Normalize360();

    //	common->Printf( "Hmdangles yaw = %f pitch = %f roll = %f\n", poseAngles.yaw, poseAngles.pitch, poseAngles.roll );
    //	common->Printf( "Trans x = %f y = %f z = %f\n", hmdPosition.x, hmdPosition.y, hmdPosition.z );

    lastRoll = hmdAngles.roll;
    lastPitch = hmdAngles.pitch;
    lastYaw = hmdAngles.yaw;
    lastAbsolutePosition = absolutePosition;
    hmdPositionTracked = true;

    commonVr->hmdBodyTranslation = absolutePosition;

    idAngles hmd2 = hmdAngles;
    hmd2.yaw -= commonVr->bodyYawOffset;

    //hmdAxis = hmd2.ToMat3();
    hmdAxis = hmdAngles.ToMat3();

    currentNeckPosition = hmdPosition + hmdAxis[0] * vr_nodalX.GetFloat() / vr_scale.GetFloat() /*+ hmdAxis[1] * 0.0f */ + hmdAxis[2] * vr_nodalZ.GetFloat() / vr_scale.GetFloat();

//	currentNeckPosition.z = pm_normalviewheight.GetFloat() - (vr_nodalZ.GetFloat() + currentNeckPosition.z);

    /*
    if ( !chestInitialized )
    {
        if ( chestDefaultDefined )
        {

            neckToChestVec = currentNeckPosition - gameLocal.GetLocalPlayer()->chestPivotDefaultPos;
            chestLength = neckToChestVec.Length();
            chestInitialized = true;
            common->Printf( "Chest Initialized, length %f\n", chestLength );
            common->Printf( "Chest default position = %s\n", gameLocal.GetLocalPlayer()->chestPivotDefaultPos.ToString() );
        }
    }

    if ( chestInitialized )
    {
        neckToChestVec = currentNeckPosition - gameLocal.GetLocalPlayer()->chestPivotDefaultPos;
        neckToChestVec.Normalize();

        idVec3 chesMove = chestLength * neckToChestVec;
        currentChestPosition = currentNeckPosition - chesMove;

        common->Printf( "Chest length %f angles roll %f pitch %f yaw %f \n", chestLength, neckToChestVec.ToAngles().roll, neckToChestVec.ToAngles().pitch, neckToChestVec.ToAngles().yaw );
        common->Printf( "CurrentNeckPos = %s\n", currentNeckPosition.ToString() );
        common->Printf( "CurrentChestPos = %s\n", currentChestPosition.ToString() );
        common->Printf( "ChestMove = %s\n", chesMove.ToString() );

        idAngles chestAngles = ang_zero;
        chestAngles.roll = neckToChestVec.ToAngles().yaw + 90.0f;
        chestAngles.pitch = 0;// neckToChestVec.ToAngles().yaw;            //chest angles.pitch rotates the chest.
        chestAngles.yaw = 0;


        //lastView = commonVr->lastHMDViewAxis.ToAngles();
        //headAngles.roll = lastView.pitch;
        //headAngles.pitch = commonVr->lastHMDYaw - commonVr->bodyYawOffset;
        //headAngles.yaw = lastView.roll;
        //headAngles.Normalize360();
        //gameLocal.GetLocalPlayer()->GetAnimator()->SetJointAxis( gameLocal.GetLocalPlayer()->chestPivotJoint, JOINTMOD_LOCAL, chestAngles.ToMat3() );
    }
    */
    if ( !neckInitialized )
    {
        lastNeckPosition = currentNeckPosition;
        initialNeckPosition = currentNeckPosition;
        if ( vr_useFloorHeight.GetInteger() != 1 )
            initialNeckPosition.z = vr_nodalZ.GetFloat() / vr_scale.GetFloat();
        neckInitialized = true;
    }

    bodyPositionDelta = currentNeckPosition - lastNeckPosition; // use this to base movement on neck model
    bodyPositionDelta.z = currentNeckPosition.z - initialNeckPosition.z;

    //bodyPositionDelta = currentChestPosition - lastChestPosition;
    lastBodyPositionDelta = bodyPositionDelta;

    lastNeckPosition = currentNeckPosition;
    lastChestPosition = currentChestPosition;

    headPositionDelta = hmdPosition - currentNeckPosition; // use this to base movement on neck model
    //headPositionDelta = hmdPosition - currentChestPosition;
    headPositionDelta.z = hmdPosition.z;
    //bodyPositionDelta.z = 0;
    // how many game units the user has physically ducked in real life from their calibrated position
    userDuckingAmount = (trackingOriginHeight - trackingOriginOffset.z) - hmdPosition.z;

    lastBodyPositionDelta = bodyPositionDelta;
    lastHeadPositionDelta = headPositionDelta;
}

/*
==============
iVr::HMDGetOrientation
==============
*/

void iVr::HMDGetOrientationAbsolute( idAngles &hmdAngles, idVec3 &position )
{

}

int iVr::Sys_Milliseconds( void ) {
    struct timeval tp;
    struct timezone tzp;

    gettimeofday( &tp, &tzp );

    if ( !sys_timeBase ) {
        sys_timeBase = tp.tv_sec;
        return tp.tv_usec / 1000;
    }

    curtime = ( tp.tv_sec - sys_timeBase ) * 1000 + tp.tv_usec / 1000;

    return curtime;
}

/*
==============
iVr::HMDResetTrackingOriginOffset
==============
*/
void iVr::HMDResetTrackingOriginOffset( void )
{
    static idVec3 body = vec3_zero;
    static idVec3 head = vec3_zero;
    static idVec3 absPos = vec3_zero;
    static idAngles rot = ang_zero;

    common->Printf( "HMDResetTrackingOriginOffset called\n " );

    HMDGetOrientation( rot, head, body, absPos, true );

    common->Printf( "New Yaw offset = %f\n", commonVr->trackingOriginYawOffset );
}

/*
==============
iVr::MotionControllSetRotationOffset;
==============
*/
void iVr::MotionControlSetRotationOffset()
{

    /*
    switch ( motionControlType )
    {

    default:
        break;
    }
    */
}

/*
==============
iVr::MotionControllSetOffset;
==============
*/
void iVr::MotionControlSetOffset()
{
    /*
    switch ( motionControlType )
    {

        default:
            break;
    }
    */
    return;
}

void iVr::MotionControlGetTouchController( int hand, idVec3 &motionPosition, idQuat &motionRotation )
{
    static idQuat poseRot;
	static idAngles poseAngles = ang_zero;
	static idAngles angTemp = ang_zero;

    poseRot.x = handPose[hand].Orientation.z;    // x;
    poseRot.y = handPose[hand].Orientation.x;    // y;
    poseRot.z = -handPose[hand].Orientation.y;    // z;
    poseRot.w = handPose[hand].Orientation.w;

    motionPosition.x = -handPose[hand].Position.z * (100.0f / 2.54f) / vr_scale.GetFloat();// Koz convert position (in meters) to inch (1 id unit = 1 inch).
	motionPosition.y = -handPose[hand].Position.x * (100.0f / 2.54f) / vr_scale.GetFloat();
	motionPosition.z = handPose[hand].Position.y * (100.0f / 2.54f) / vr_scale.GetFloat();
	motionPosition -= trackingOriginOffset;
	motionPosition *= idAngles( 0.0f, (-trackingOriginYawOffset), 0.0f ).ToMat3();
    motionPosition -= commonVr->hmdBodyTranslation;

    if (GetWeaponStabilised() &&
            hand == vr_weaponHand.GetInteger()) {
        idVec3 offHandPosition;
        offHandPosition.x = -handPose[1 - hand].Position.z * (100.0f / 2.54f) / vr_scale.GetFloat();// Koz convert position (in meters) to inch (1 id unit = 1 inch).
        offHandPosition.y = -handPose[1 - hand].Position.x * (100.0f / 2.54f) / vr_scale.GetFloat();
        offHandPosition.z = handPose[1 - hand].Position.y * (100.0f / 2.54f) / vr_scale.GetFloat();
        offHandPosition -= trackingOriginOffset;
        offHandPosition *= idAngles(0.0f, (-trackingOriginYawOffset), 0.0f ).ToMat3();
        offHandPosition -= commonVr->hmdBodyTranslation;

        poseAngles = (offHandPosition - motionPosition).ToAngles();

        angTemp.Set(poseAngles.pitch, poseAngles.yaw, poseRot.ToAngles().roll);
    } else {
        poseAngles = poseRot.ToAngles();

        angTemp.Set(poseAngles.pitch, poseAngles.yaw, poseAngles.roll);
        angTemp.yaw -= trackingOriginYawOffset;// + bodyYawOffset;
    }

    angTemp.Normalize360();
    motionRotation = angTemp.ToQuat();

}
/*
==============
iVr::MotionControllGetHand;
==============
*/
void iVr::MotionControlGetHand( int hand, idVec3 &motionPosition, idQuat &motionRotation )
{
    if ( hand == HAND_LEFT )
    {
        MotionControlGetLeftHand( motionPosition, motionRotation );
    }
    else
    {
        MotionControlGetRightHand( motionPosition, motionRotation );
    }

    // apply weapon mount offsets

    if ( hand == vr_weaponHand.GetInteger() && vr_mountedWeaponController.GetBool() )
    {
        idVec3 controlToHand = idVec3( vr_mountx.GetFloat() / vr_scale.GetFloat(), vr_mounty.GetFloat() / vr_scale.GetFloat(), vr_mountz.GetFloat()  / vr_scale.GetFloat() );
        idVec3 controlCenter = idVec3( vr_controllerOffsetX.GetFloat() / vr_scale.GetFloat(), vr_controllerOffsetY.GetFloat() / vr_scale.GetFloat(), vr_controllerOffsetZ.GetFloat()  / vr_scale.GetFloat() );

        motionPosition += ( controlToHand - controlCenter ) * motionRotation; // pivot around the new point
    }
    else
    {
        motionPosition += idVec3( vr_controllerOffsetX.GetFloat()  / vr_scale.GetFloat(), vr_controllerOffsetY.GetFloat() / vr_scale.GetFloat(), vr_controllerOffsetZ.GetFloat() / vr_scale.GetFloat() ) * motionRotation;
    }
}


/*
==============
iVr::MotionControllGetLeftHand;
==============
*/
void iVr::MotionControlGetLeftHand( idVec3 &motionPosition, idQuat &motionRotation )
{
    static idAngles angles = ang_zero;
    MotionControlGetTouchController(1, motionPosition, motionRotation);
    /*switch ( motionControlType )
    {
        case MOTION_OCULUS:
        {

            MotionControlGetTouchController(1, motionPosition, motionRotation);
            break;
        }

        default:
            break;
    }*/
}

/*
==============
iVr::MotionControllGetRightHand;
==============
*/
void iVr::MotionControlGetRightHand( idVec3 &motionPosition, idQuat &motionRotation )
{
    static idAngles angles = ang_zero;
    MotionControlGetTouchController(0, motionPosition, motionRotation);
    /*switch ( motionControlType )
    {

        case MOTION_OCULUS:
        {

            MotionControlGetTouchController(0, motionPosition, motionRotation);
            break;
        }

        default:
            break;
    }*/
}

/*
==============
iVr::MotionControllSetHaptic
==============
*/
void iVr::MotionControllerSetHapticOculus( float low, float hi )
{

    float beat;
    float enable;

    beat = fabs( low - hi ) / 65535;

    enable = ( beat > 0.0f) ? 1.0f : 0.0f;

    if ( vr_weaponHand.GetInteger() == HAND_RIGHT )
	{
		//ovr_SetControllerVibration( hmdSession, ovrControllerType_RTouch, beat, enable );
	}
	else
	{
		//ovr_SetControllerVibration( hmdSession, ovrControllerType_LTouch, beat, enable );
	}


    return;
}

/*
==============
iVr::CalcAimMove
Pass the controller yaw & pitch changes.
Indepent weapon view angles will be updated,
and the correct yaw & pitch movement values will
be returned based on the current user aim mode.
==============
*/

void iVr::CalcAimMove( float &yawDelta, float &pitchDelta )
{

    if ( commonVr->VR_USE_MOTION_CONTROLS ) // no independent aim or joystick pitch when using motion controllers.
    {
        pitchDelta = 0.0f;
        return;
    }


    float pitchDeadzone = vr_deadzonePitch.GetFloat();
    float yawDeadzone = vr_deadzoneYaw.GetFloat();

    commonVr->independentWeaponPitch += pitchDelta;

    if ( commonVr->independentWeaponPitch >= pitchDeadzone ) commonVr->independentWeaponPitch = pitchDeadzone;
    if ( commonVr->independentWeaponPitch < -pitchDeadzone ) commonVr->independentWeaponPitch = -pitchDeadzone;
    pitchDelta = 0;

    // if moving the character in third person, just turn immediately, no deadzones.
    if ( commonVr->thirdPersonMovement ) return;


    commonVr->independentWeaponYaw += yawDelta;

    if ( commonVr->independentWeaponYaw >= yawDeadzone )
    {
        yawDelta = commonVr->independentWeaponYaw - yawDeadzone;
        commonVr->independentWeaponYaw = yawDeadzone;
        return;
    }

    if ( commonVr->independentWeaponYaw < -yawDeadzone )
    {
        yawDelta = commonVr->independentWeaponYaw + yawDeadzone;
        commonVr->independentWeaponYaw = -yawDeadzone;
        return;
    }

    yawDelta = 0.0f;

}

bool iVr::GetWeaponStabilised()
{
    if (pVRClientInfo)
    {
        return pVRClientInfo->weapon_stabilised;
    }

    return false;
}



/*
==============
iVr::FrameStart
==============
*/
void iVr::FrameStart( void )
{
    //common->Printf( "Framestart called from frame %d\n", idLib::frameNumber );
    HMDGetOrientation( poseHmdAngles, poseHmdHeadPositionDelta, poseHmdBodyPositionDelta, poseHmdAbsolutePosition, false );
    remainingMoveHmdBodyPositionDelta = poseHmdBodyPositionDelta;
    return;
}

/*
==============
iVr::GetCurrentFlashlightMode();
==============
*/

int iVr::GetCurrentFlashlightMode()
{
    //common->Printf( "Returning flashlightmode %d\n", currentFlashlightMode );
    return currentFlashlightMode;
}

/*
==============
iVr::GetCurrentFlashlightMode();
==============
*/
void iVr::NextFlashlightMode()
{
    currentFlashlightMode++;
    if ( currentFlashlightMode >= FLASHLIGHT_MAX ) currentFlashlightMode = 0;
}

bool iVr::ShouldQuit()
{
    /*
    if (hasOculusRift)
	{
		ovrSessionStatus ss;
		ovrResult result = ovr_GetSessionStatus(hmdSession, &ss);
		if (ss.ShouldQuit)
			return true;
		if (ss.ShouldRecenter)
			shouldRecenter = true;
	}
    */
    return false;
}

void iVr::ForceChaperone(int which, bool force)
{
    static bool chaperones[2] = {};
    chaperones[which] = force;
    force = chaperones[0] || chaperones[1];

    if (hasOculusRift)
	{
		//ovr_RequestBoundaryVisible(hmdSession, force);
	}
}

