#if !defined(vrcommon_h)
#define vrcommon_h

#include <android/log.h>

#include "mathlib.h"
#include "VrClientInfo.h"
#include "openxr.h"

#define LOG_TAG "D3QUESTVR"

#ifndef NDEBUG
#define DEBUG 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ALOGE(...) __android_log_print( ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )

#if DEBUG
#define ALOGV(...) __android_log_print( ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__ )
#else
#define ALOGV(...) __android_log_print( ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__ )
#endif

void HandleInput_Default(int controlscheme, int switchsticks);

bool isMultiplayer();

double GetTimeInMilliSeconds();

float length(float x, float y);

float nonLinearFilter(float in);

bool between(float min, float val, float max);

void rotateAboutOrigin(float v1, float v2, float rotation, vec2_t out);

void QuatToYawPitchRoll(XrQuaternionf q, vec3_t rotation, vec3_t out);

void handleTrackedControllerButton_AsButton(uint32_t buttonsNew, uint32_t buttonsOld,
                                   bool mouse, uint32_t button, int key);

void handleTrackedControllerButton_AsKey(uint32_t buttonsNew, uint32_t buttonsOld,
                                   uint32_t button, int key);

void handleTrackedControllerButton_AsToggleButton(uint32_t buttonsNew, uint32_t buttonsOld,
                                   uint32_t button, int key);


void controlMouse(bool reset);


//Called from engine code
int Doom3Quest_GetDeviceVersion();

int Doom3Quest_GetRefresh();

bool Doom3Quest_useScreenLayer();

void Doom3Quest_GetScreenRes(int *width, int *height);

void Doom3Quest_Vibrate(int channel, float low, float high);

void Doom3Quest_FrameSetup(int controlscheme, int switch_sticks, int refresh, float msaa, float supersampling);

void Doom3Quest_setUseScreenLayer(int screen);

void Doom3Quest_getHMDOrientation();

void Doom3Quest_prepareEyeBuffer();

void Doom3Quest_finishEyeBuffer();

#ifdef __cplusplus
}
#endif

#endif //vrcommon_h
