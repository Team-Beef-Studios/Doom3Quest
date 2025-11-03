/************************************************************************************

Filename	:	Q2VR_SurfaceView.c based on VrCubeWorld_SurfaceView.c
Content		:	This sample uses a plain Android SurfaceView and handles all
				Activity and Surface life cycle events in native code.
Created		:	March, 2015
Authors		:	J.M.P. van Waveren, Simon Brown, Lubos Vonasek

Copyright	:	Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h>					// for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/native_window_jni.h>	// for native window JNI
#include <android/input.h>

#include "VrInput.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include <SDL2/SDL_mutex.h>

#include "VrInput.h"
#include "VrCommon.h"
#include "VrMath.h"
#include "VrRenderer.h"


// EXT_texture_border_clamp
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER			0x812D
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR		0x1004
#endif

vrClientInfo vr;
vrClientInfo *pVRClientInfo;
XrQuaternionf quatHmd;
XrVector3f positionHmd[2];

char **argv;
int argc=0;
bool shutdown;

bool forceVirtualScreen = false;
bool inMenu = true;
bool inGameGuiActive = false;
bool objectiveSystemActive = false;
bool inCinematic = false;
bool loading = false;

void Doom3Quest_setUseScreenLayer(int screen)
{
	inMenu = screen & 0x1;
	inGameGuiActive = !!(screen & 0x2);
	objectiveSystemActive = !!(screen & 0x4);
	inCinematic = !!(screen & 0x8);
	loading = !!(screen & 0x10);

	pVRClientInfo->inMenu = inMenu;
}

bool Doom3Quest_useScreenLayer()
{
	return inMenu || forceVirtualScreen || inCinematic || loading || pVRClientInfo->consoleShown/*Lubos*/;
}

static void UnEscapeQuotes( char *arg )
{
	char *last = NULL;
	while( *arg ) {
		if( *arg == '"' && *last == '\\' ) {
			char *c_curr = arg;
			char *c_last = last;
			while( *c_curr ) {
				*c_last = *c_curr;
				c_last = c_curr;
				c_curr++;
			}
			*c_last = '\0';
		}
		last = arg;
		arg++;
	}
}

static int ParseCommandLine(char *cmdline, char **argv)
{
	char *bufp;
	char *lastp = NULL;
	int argc, last_argc;
	argc = last_argc = 0;
	for ( bufp = cmdline; *bufp; ) {
		while ( isspace(*bufp) ) {
			++bufp;
		}
		if ( *bufp == '"' ) {
			++bufp;
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			while ( *bufp && ( *bufp != '"' || *lastp == '\\' ) ) {
				lastp = bufp;
				++bufp;
			}
		} else {
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			while ( *bufp && ! isspace(*bufp) ) {
				++bufp;
			}
		}
		if ( *bufp ) {
			if ( argv ) {
				*bufp = '\0';
			}
			++bufp;
		}
		if( argv && last_argc != argc ) {
			UnEscapeQuotes( argv[last_argc] );
		}
		last_argc = argc;
	}
	if ( argv ) {
		argv[argc] = NULL;
	}
	return(argc);
}

#ifndef EPSILON
#define EPSILON 0.001f
#endif

static XrVector3f normalizeVec(XrVector3f vec) {
    //NOTE: leave w-component untouched
    float xxyyzz = vec.x*vec.x + vec.y*vec.y + vec.z*vec.z;

	XrVector3f result;
    float invLength = 1.0f / sqrtf(xxyyzz);
    result.x = vec.x * invLength;
    result.y = vec.y * invLength;
    result.z = vec.z * invLength;
    return result;
}

void NormalizeAngles(vec3_t angles)
{
	while (angles[0] >= 90) angles[0] -= 180;
	while (angles[1] >= 180) angles[1] -= 360;
	while (angles[2] >= 180) angles[2] -= 360;
	while (angles[0] < -90) angles[0] += 180;
	while (angles[1] < -180) angles[1] += 360;
	while (angles[2] < -180) angles[2] += 360;
}

void GetAnglesFromVectors(const XrVector3f forward, const XrVector3f right, const XrVector3f up, vec3_t angles)
{
	float sr, sp, sy, cr, cp, cy;

	sp = -forward.z;

	float cp_x_cy = forward.x;
	float cp_x_sy = forward.y;
	float cp_x_sr = -right.z;
	float cp_x_cr = up.z;

	float yaw = atan2(cp_x_sy, cp_x_cy);
	float roll = atan2(cp_x_sr, cp_x_cr);

	cy = cos(yaw);
	sy = sin(yaw);
	cr = cos(roll);
	sr = sin(roll);

	if (fabs(cy) > EPSILON)
	{
	cp = cp_x_cy / cy;
	}
	else if (fabs(sy) > EPSILON)
	{
	cp = cp_x_sy / sy;
	}
	else if (fabs(sr) > EPSILON)
	{
	cp = cp_x_sr / sr;
	}
	else if (fabs(cr) > EPSILON)
	{
	cp = cp_x_cr / cr;
	}
	else
	{
	cp = cos(asin(sp));
	}

	float pitch = atan2(sp, cp);

	angles[0] = pitch / (M_PI*2.f / 360.f);
	angles[1] = yaw / (M_PI*2.f / 360.f);
	angles[2] = roll / (M_PI*2.f / 360.f);

	NormalizeAngles(angles);
}

void QuatToYawPitchRoll(XrQuaternionf q, vec3_t rotation, vec3_t out) {

    ovrMatrix4f mat = ovrMatrix4f_CreateFromQuaternion( &q );

    if (rotation[0] != 0.0f || rotation[1] != 0.0f || rotation[2] != 0.0f)
	{
		ovrMatrix4f rot = ovrMatrix4f_CreateRotation(ToRadians(rotation[0]), ToRadians(rotation[1]), ToRadians(rotation[2]));
		mat = ovrMatrix4f_Multiply(&mat, &rot);
	}

    XrVector4f v1 = {0, 0, -1, 0};
    XrVector4f v2 = {1, 0, 0, 0};
    XrVector4f v3 = {0, 1, 0, 0};

    XrVector4f forwardInVRSpace = XrVector4f_MultiplyMatrix4f(&mat, &v1);
    XrVector4f rightInVRSpace = XrVector4f_MultiplyMatrix4f(&mat, &v2);
    XrVector4f upInVRSpace = XrVector4f_MultiplyMatrix4f(&mat, &v3);

	XrVector3f forward = {-forwardInVRSpace.z, -forwardInVRSpace.x, forwardInVRSpace.y};
	XrVector3f right = {-rightInVRSpace.z, -rightInVRSpace.x, rightInVRSpace.y};
	XrVector3f up = {-upInVRSpace.z, -upInVRSpace.x, upInVRSpace.y};

	XrVector3f forwardNormal = normalizeVec(forward);
	XrVector3f rightNormal = normalizeVec(right);
	XrVector3f upNormal = normalizeVec(up);

	GetAnglesFromVectors(forwardNormal, rightNormal, upNormal, out);
}

int Android_GetCVarInteger(const char* cvar);

double GetTimeInMilliSeconds()
{
    struct timespec now;
    clock_gettime( CLOCK_MONOTONIC, &now );
    return ( now.tv_sec * 1e9 + now.tv_nsec ) * (double)(1e-6);
}

//0 = left, 1 = right
float vibration_channel_intensity[2][2] = {{0.0f,0.0f},{0.0f,0.0f}};

void Doom3Quest_Vibrate(int channel, float low, float high)
{
    vibration_channel_intensity[channel][0] = low;
    vibration_channel_intensity[channel][1] = high;
}

void Doom3Quest_processHaptics() {//Handle haptics
    float beat;
    for (int h = 0; h < 2; ++h) {
        beat = fabs( vibration_channel_intensity[h][0] - vibration_channel_intensity[h][1] ) / 65535;
        if(beat > 0.0f)
            INVR_Vibrate(10, 1 - h, beat);
        else
            INVR_Vibrate(0, 1 - h, 0);
    }
}

void jni_haptic_event(const char* event, int position, int flags, int intensity, float angle, float yHeight);
void jni_haptic_updateevent(const char* event, int intensity, float angle);
void jni_haptic_stopevent(const char* event);
void jni_haptic_endframe();
void jni_haptic_enable();
void jni_haptic_disable();

void Doom3Quest_HapticEvent(const char* event, int position, int flags, int intensity, float angle, float yHeight )
{
    jni_haptic_event(event, position, flags, intensity, angle, yHeight);
}

void Doom3Quest_HapticUpdateEvent(const char* event, int intensity, float angle )
{
    jni_haptic_updateevent(event, intensity, angle);
}

void Doom3Quest_HapticEndFrame()
{
	jni_haptic_endframe();
}

void Doom3Quest_HapticStopEvent(const char* event)
{
	jni_haptic_stopevent(event);
}

void Doom3Quest_HapticEnable()
{
    jni_haptic_enable();
}

void Doom3Quest_HapticDisable()
{
    jni_haptic_disable();
}

void VR_Doom3Main(int argc, char** argv);

//Lubos BEGIN
float hmdposition_last[3] = {};
extern float remote_movementSideways;
extern float remote_movementForward;

void VR_GetMove( float *joy_forward, float *joy_side, float *hmd_forward, float *hmd_side, float *up, float *yaw, float *pitch, float *roll ) {
	float dx = pVRClientInfo->hmdposition_last[0] - hmdposition_last[0];
	float dy = pVRClientInfo->hmdposition_last[1] - hmdposition_last[1];
	float dz = pVRClientInfo->hmdposition_last[2] - hmdposition_last[2];
	if (fabs(dx) + fabs(dy) + fabs(dz) > 1) {
		dx = 0; dy = 0; dz = 0;
	}

	vec2_t v;
	rotateAboutOrigin(dx,-dz, -pVRClientInfo->hmdorientation_temp[YAW], v);
	*hmd_forward = v[0] * 100.0f;
	*hmd_side = v[1] * 100.0f;
	*joy_side = remote_movementSideways;
	*joy_forward = remote_movementForward;
	*up = pVRClientInfo->hmdposition_last[1];

	if (fabs(vr.hmdorientation_diff[PITCH]) > 1) vr.hmdorientation_offset[PITCH] += vr.hmdorientation_diff[PITCH] * 0.1f;
	if (fabs(vr.hmdorientation_diff[ROLL]) > 1) vr.hmdorientation_offset[ROLL] += vr.hmdorientation_diff[ROLL] * 0.1f;
	//*pitch = vr.hmdorientation_temp[PITCH] - vr.hmdorientation_offset[PITCH];
	//*roll = vr.hmdorientation_temp[ROLL] - vr.hmdorientation_offset[ROLL];
	//*yaw = vr.hmdorientation_temp[YAW] + vr.snapTurn;
    *yaw = vr.snapTurn;

	hmdposition_last[0] = pVRClientInfo->hmdposition_last[0];
	hmdposition_last[1] = pVRClientInfo->hmdposition_last[1];
	hmdposition_last[2] = pVRClientInfo->hmdposition_last[2];
}

extern XrVector2f pPrimaryJoystick;

void VR_GetJoystick( float *x, float *y ) {
	*x = -pPrimaryJoystick.x;
	*y = -pPrimaryJoystick.y;
}
//Lubos END

static ovrEgl Egl;
static ANativeWindow *NativeWindow;
static JavaVM *jVM;
static bool destroyed = false;

time_t seconds;
int lastRefresh = 0;
int currentRefresh = 60;

float Doom3Quest_GetFOV()
{
	return VR_GetConfigFloat(VR_CONFIG_VIEWPORT_FOVY);
}

int Doom3Quest_GetRefresh()
{
	return currentRefresh;
}

void Doom3Quest_GetScreenRes(int *width, int *height)
{
	VR_GetResolution(VR_GetEngine(), width, height);
}

void Doom3Quest_prepareEyeBuffer( )
{
	//Recreate framebuffer if needed
	if (!VR_GetConfig(VR_CONFIG_VIEWPORT_VALID)) {
		VR_InitRenderer(VR_GetEngine(), true);
		VR_SetConfig(VR_CONFIG_VIEWPORT_VALID, true);
	}

	VR_SetConfigFloat(VR_CONFIG_CANVAS_ASPECT, inMenu ? 1 : 0.85f);
	VR_SetConfigFloat(VR_CONFIG_CANVAS_DISTANCE, 4);
	VR_SetConfig(VR_CONFIG_MODE, Doom3Quest_useScreenLayer() ? VR_MODE_STEREO_SCREEN : VR_MODE_STEREO_6DOF);

	VR_BeginFrame(VR_GetEngine());
	VR_BindFramebuffer(VR_GetEngine());
}

void Doom3Quest_finishEyeBuffer( )
{
	VR_EndFrame(VR_GetEngine());
	VR_FinishFrame(VR_GetEngine());
	Doom3Quest_HapticEndFrame();

	if (Doom3Quest_useScreenLayer()) {
		VR_InitFrame(VR_GetEngine());
	}
}

void shutdownVR() {
	VR_LeaveVR(VR_GetEngine());
}

void jni_shutdown();

/* Called before SDL_main() to initialize JNI bindings in SDL library */
extern void SDL_Android_Init(JNIEnv* env, jclass cls);

//Calld on the main thread before the rendering thread is started
void DeactivateContext()
{
    eglMakeCurrent( Egl.Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
}

//Caled by the rendering thread to take charge of the conteSUxt
void ActivateContext()
{
    eglMakeCurrent( Egl.Display, Egl.TinySurface, Egl.TinySurface, Egl.Context );
}

void * AppThreadFunction(void * parm) {
	//Initialise all our variables
	vr.snapTurn = 0.0f;
	vr.visible_hud = true;

	//init randomiser
	srand(time(NULL));

    pVRClientInfo = &vr;

	// Note that AttachCurrentThread will reset the thread name.
	prctl(PR_SET_NAME, (long) "OVR::Main", 0, 0, 0);

	shutdown = false;

	ovrEgl_Clear( &Egl );
	ovrEgl_CreateContext(&Egl, NULL);

    chdir("/sdcard/Doom3Quest");

	VR_SetConfig(VR_CONFIG_NEED_RECENTER, true);
	VR_EnterVR(VR_GetEngine(), Egl);
	IN_VRInit(VR_GetEngine());

    //Should now be all set up and ready - start the Doom3 main loop
    VR_Doom3Main(argc, argv);

    //Take the context back
    ActivateContext();

	//We are done, shutdown cleanly
	shutdownVR();

	//Ask Java to shut down
    jni_shutdown();

	return NULL;
}

//All the stuff we want to do each frame
void Doom3Quest_FrameSetup(int controlscheme, int switch_sticks, int refresh, float msaa, float supersampling)
{
	//Inform GL thread about required framebuffer parameters.
	if (Doom3Quest_useScreenLayer()) {
		supersampling = 1.3f;
	}
	if (fabs(VR_GetConfigFloat(VR_CONFIG_VIEWPORT_SUPERSAMPLING) - supersampling) > 0.01) {
		VR_SetConfigFloat(VR_CONFIG_VIEWPORT_SUPERSAMPLING, supersampling);
		VR_SetConfig(VR_CONFIG_VIEWPORT_VALID, false);
	}
	if (fabs((float)VR_GetConfig(VR_CONFIG_VIEWPORT_MSAA) - msaa) > 0.01) {
		VR_SetConfig(VR_CONFIG_VIEWPORT_MSAA, (int)msaa);
		VR_SetConfig(VR_CONFIG_VIEWPORT_VALID, false);
	}

	if (lastRefresh != refresh) {
		lastRefresh = refresh;
		VR_SetRefreshRate(refresh);
		currentRefresh = VR_GetRefreshRate();
	}

	if (!Doom3Quest_useScreenLayer()) {
		VR_InitFrame(VR_GetEngine());
	}
	Doom3Quest_processHaptics();
	Doom3Quest_getHMDOrientation();
	pVRClientInfo->right_handed = !controlscheme;
	HandleInput_Default(controlscheme, switch_sticks);
}

void Doom3Quest_getHMDOrientation() {
	quatHmd = VR_GetView(0).orientation;
	positionHmd[0] = VR_GetView(0).position;
	positionHmd[1] = VR_GetView(1).position;
	vec3_t rotation = {0};
	QuatToYawPitchRoll(quatHmd, rotation, vr.hmdorientation_temp);

	vr.hmdposition[0] = (positionHmd[0].x + positionHmd[1].x) / 2.0f;
	vr.hmdposition[1] = (positionHmd[0].y + positionHmd[1].y) / 2.0f;
	vr.hmdposition[2] = (positionHmd[0].z + positionHmd[1].z) / 2.0f;
	Vector4Set(vr.hmdorientation_quat, quatHmd.x, quatHmd.y, quatHmd.z, quatHmd.w);
	VectorSubtract(vr.hmdposition_last, vr.hmdposition, vr.hmdposition_delta);
	VectorCopy(vr.hmdposition, vr.hmdposition_last);
}

/*
================================================================================

Activity lifecycle

================================================================================
*/

jmethodID android_shutdown;
jmethodID android_haptic_event;
jmethodID android_haptic_updateevent;
jmethodID android_haptic_stopevent;
jmethodID android_haptic_endframe;
jmethodID android_haptic_enable;
jmethodID android_haptic_disable;
static jobject jniCallbackObj=0;

void jni_shutdown()
{
    ALOGV("Calling: jni_shutdown");
    JNIEnv *env;
    jobject tmp;
    if (((*jVM)->GetEnv(jVM, (void**) &env, JNI_VERSION_1_4))<0)
    {
        (*jVM)->AttachCurrentThread(jVM,&env, NULL);
    }
    return (*env)->CallVoidMethod(env, jniCallbackObj, android_shutdown);
}

void jni_haptic_event(const char* event, int position, int flags, int intensity, float angle, float yHeight)
{
    JNIEnv *env;
    jobject tmp;
    if (((*jVM)->GetEnv(jVM, (void**) &env, JNI_VERSION_1_4))<0)
    {
        (*jVM)->AttachCurrentThread(jVM,&env, NULL);
    }

    jstring StringArg1 = (*env)->NewStringUTF(env, event);

    return (*env)->CallVoidMethod(env, jniCallbackObj, android_haptic_event, StringArg1, position, flags, intensity, angle, yHeight);
}

void jni_haptic_updateevent(const char* event, int intensity, float angle)
{
    JNIEnv *env;
    jobject tmp;
    if (((*jVM)->GetEnv(jVM, (void**) &env, JNI_VERSION_1_4))<0)
    {
        (*jVM)->AttachCurrentThread(jVM,&env, NULL);
    }

    jstring StringArg1 = (*env)->NewStringUTF(env, event);

    return (*env)->CallVoidMethod(env, jniCallbackObj, android_haptic_updateevent, StringArg1, intensity, angle);
}

void jni_haptic_stopevent(const char* event)
{
    ALOGV("Calling: jni_haptic_stopevent");
    JNIEnv *env;
    jobject tmp;
    if (((*jVM)->GetEnv(jVM, (void**) &env, JNI_VERSION_1_4))<0)
    {
        (*jVM)->AttachCurrentThread(jVM,&env, NULL);
    }

    jstring StringArg1 = (*env)->NewStringUTF(env, event);

    return (*env)->CallVoidMethod(env, jniCallbackObj, android_haptic_stopevent, StringArg1);
}

void jni_haptic_endframe()
{
    JNIEnv *env;
    jobject tmp;
    if (((*jVM)->GetEnv(jVM, (void**) &env, JNI_VERSION_1_4))<0)
    {
        (*jVM)->AttachCurrentThread(jVM,&env, NULL);
    }

    return (*env)->CallVoidMethod(env, jniCallbackObj, android_haptic_endframe);
}

void jni_haptic_enable()
{
    ALOGV("Calling: jni_haptic_enable");
    JNIEnv *env;
    jobject tmp;
    if (((*jVM)->GetEnv(jVM, (void**) &env, JNI_VERSION_1_4))<0)
    {
        (*jVM)->AttachCurrentThread(jVM,&env, NULL);
    }

    return (*env)->CallVoidMethod(env, jniCallbackObj, android_haptic_enable);
}

void jni_haptic_disable()
{
    ALOGV("Calling: jni_haptic_disable");
    JNIEnv *env;
    jobject tmp;
    if (((*jVM)->GetEnv(jVM, (void**) &env, JNI_VERSION_1_4))<0)
    {
        (*jVM)->AttachCurrentThread(jVM,&env, NULL);
    }

    return (*env)->CallVoidMethod(env, jniCallbackObj, android_haptic_disable);
}

JNIEXPORT jint JNICALL SDL_JNI_OnLoad(JavaVM* vm, void* reserved);

int JNI_OnLoad(JavaVM* vm, void* reserved)
{
	JNIEnv *env;
    jVM = vm;
	if((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK)
	{
		ALOGE("Failed JNI_OnLoad");
		return -1;
	}

	return SDL_JNI_OnLoad(vm, reserved);
}

JNIEXPORT void JNICALL Java_com_drbeef_doom3quest_GLES3JNILib_onCreate( JNIEnv * env, jclass activityClass, jobject activity,
																	   jstring commandLineParams, jstring device)
{
	ALOGV( "    GLES3JNILib::onCreate()" );

	jboolean iscopy;
	const char *arg = (*env)->GetStringUTFChars(env, commandLineParams, &iscopy);


	char *cmdLine = NULL;
	if (arg && strlen(arg))
	{
		cmdLine = strdup(arg);
	}

	(*env)->ReleaseStringUTFChars(env, commandLineParams, arg);

	ALOGV("Command line %s", cmdLine);
	argv = malloc(sizeof(char*) * 255);
	argc = ParseCommandLine(strdup(cmdLine), argv);


	//Get device vendor (uppercase)
	const char *devicename = (*env)->GetStringUTFChars(env, device, &iscopy);
	char vendor[64];
	sscanf(devicename, "%[^:]", vendor);
	for (unsigned int i = 0; i < strlen(vendor); i++) {
		if ((vendor[i] >= 'a') && (vendor[i] <= 'z')) {
			vendor[i] = vendor[i] - 'a' + 'A';
		}
	}

	//Set platform flags
	if (strcmp(vendor, "PICO") == 0) {
		VR_SetPlatformFLag(VR_PLATFORM_CONTROLLER_PICO, true);
		VR_SetPlatformFLag(VR_PLATFORM_EXTENSION_INSTANCE, true);
		VR_SetPlatformFLag(VR_PLATFORM_EXTENSION_PERFORMANCE, true);
		VR_SetPlatformFLag(VR_PLATFORM_EXTENSION_REFRESH, true);
	} else {
		VR_SetPlatformFLag(VR_PLATFORM_CONTROLLER_QUEST, true);
		VR_SetPlatformFLag(VR_PLATFORM_EXTENSION_FOVEATION, true);
		VR_SetPlatformFLag(VR_PLATFORM_EXTENSION_PERFORMANCE, true);
		VR_SetPlatformFLag(VR_PLATFORM_EXTENSION_REFRESH, true);
		VR_SetPlatformFLag(VR_PLATFORM_VIEWPORT_UNCENTERED, true);
	}
	VR_SetPlatformFLag(VR_PLATFORM_TRACKING_FLOOR, true);
	VR_SetPlatformFLag(VR_PLATFORM_VIEWPORT_SQUARE, true);
	VR_SetConfigFloat(VR_CONFIG_VIEWPORT_SUPERSAMPLING, 1.3f);

	//Init VR
	ovrJava java;
	java.Vm = (JavaVM*)jVM;
	java.ActivityObject = (*env)->NewGlobalRef( env, activity );
	(*java.Vm)->AttachCurrentThread(java.Vm, &java.Env, NULL);
	VR_Init(&java, "Doom3Quest", "1");
	VR_InitRenderer(VR_GetEngine(), true);
}


JNIEXPORT void JNICALL Java_com_drbeef_doom3quest_GLES3JNILib_onStart( JNIEnv * env, jobject obj, jobject obj1)
{
	ALOGV( "    GLES3JNILib::onStart()" );

    jniCallbackObj = (jobject)(*env)->NewGlobalRef(env, obj1);
    jclass callbackClass = (*env)->GetObjectClass(env, jniCallbackObj);

    android_shutdown = (*env)->GetMethodID(env,callbackClass,"shutdown","()V");
	android_haptic_event = (*env)->GetMethodID(env, callbackClass, "haptic_event", "(Ljava/lang/String;IIIFF)V");
	android_haptic_updateevent = (*env)->GetMethodID(env, callbackClass, "haptic_updateevent", "(Ljava/lang/String;IF)V");
	android_haptic_stopevent = (*env)->GetMethodID(env, callbackClass, "haptic_stopevent", "(Ljava/lang/String;)V");
	android_haptic_endframe = (*env)->GetMethodID(env, callbackClass, "haptic_endframe", "()V");
    android_haptic_enable = (*env)->GetMethodID(env, callbackClass, "haptic_enable", "()V");
    android_haptic_disable = (*env)->GetMethodID(env, callbackClass, "haptic_disable", "()V");
}

JNIEXPORT void JNICALL Java_com_drbeef_doom3quest_GLES3JNILib_onDestroy( JNIEnv * env, jobject obj )
{
	ALOGV( "    GLES3JNILib::onDestroy()" );
	NativeWindow = NULL;
	destroyed = true;
	shutdown = true;
}

/*
================================================================================

Surface lifecycle

================================================================================
*/

JNIEXPORT void JNICALL Java_com_drbeef_doom3quest_GLES3JNILib_onSurfaceCreated( JNIEnv * env, jobject obj, jobject surface )
{
	ALOGV( "    GLES3JNILib::onSurfaceCreated()" );
	ANativeWindow * newNativeWindow = ANativeWindow_fromSurface( env, surface );
	if ( ANativeWindow_getWidth( newNativeWindow ) < ANativeWindow_getHeight( newNativeWindow ) )
	{
		// An app that is relaunched after pressing the home button gets an initial surface with
		// the wrong orientation even though android:screenOrientation="landscape" is set in the
		// manifest. The choreographer callback will also never be called for this surface because
		// the surface is immediately replaced with a new surface with the correct orientation.
		ALOGE( "        Surface not in landscape mode!" );
	}

	ALOGV( "        NativeWindow = ANativeWindow_fromSurface( env, surface )" );
	if (!NativeWindow) {
		pthread_t thread = 0;
		const int createErr = pthread_create( &thread, NULL, AppThreadFunction, NULL );
		if ( createErr != 0 )
		{
			ALOGE( "pthread_create returned %i", createErr );
		}
	}
	NativeWindow = newNativeWindow;
}

JNIEXPORT void JNICALL Java_com_drbeef_doom3quest_GLES3JNILib_onSurfaceChanged( JNIEnv * env, jobject obj, jobject surface )
{
	ALOGV( "    GLES3JNILib::onSurfaceChanged()" );
	ANativeWindow * newNativeWindow = ANativeWindow_fromSurface( env, surface );
	if ( ANativeWindow_getWidth( newNativeWindow ) < ANativeWindow_getHeight( newNativeWindow ) )
	{
		// An app that is relaunched after pressing the home button gets an initial surface with
		// the wrong orientation even though android:screenOrientation="landscape" is set in the
		// manifest. The choreographer callback will also never be called for this surface because
		// the surface is immediately replaced with a new surface with the correct orientation.
		ALOGE( "        Surface not in landscape mode!" );
	}
	NativeWindow = newNativeWindow;
}
