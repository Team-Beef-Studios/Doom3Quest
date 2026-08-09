#include "VrBase.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#define VR_DEVICE_VERSION_UNKNOWN	2
#define VR_DEVICE_VERSION_NEWEST	3

static bool vr_platform[VR_PLATFORM_MAX];
static engine_t vr_engine;
static int vr_device_version = VR_DEVICE_VERSION_UNKNOWN;
static char vr_system_name[XR_MAX_SYSTEM_NAME_SIZE] = "";
int vr_initialized = 0;

/*
Maps an OpenXR system name onto a Quest generation, which selects questN_default.cfg.
Meta reports names such as "Oculus Quest2" and "Meta Quest 3". Quest Pro carries the same
SoC as Quest 2, so it takes the Quest 2 profile. Anything not recognised, Pico included,
takes the Quest 2 profile because that is the safe middle setting.
*/
static int VR_ParseDeviceVersion(const char* systemName) {
	char name[XR_MAX_SYSTEM_NAME_SIZE];
	size_t i;

	for (i = 0; i < sizeof(name) - 1 && systemName[i] != '\0'; i++) {
		name[i] = (char)tolower((unsigned char)systemName[i]);
	}
	name[i] = '\0';

	const char* model = strstr(name, "quest");
	if (model == NULL) {
		return VR_DEVICE_VERSION_UNKNOWN;
	}

	model += strlen("quest");
	while (*model == ' ') {
		model++;
	}

	if (*model >= '1' && *model <= '9') {
		int version = *model - '0';
		// A newer headset than we ship a config for gets the newest profile, not a missing file.
		return (version > VR_DEVICE_VERSION_NEWEST) ? VR_DEVICE_VERSION_NEWEST : version;
	}
	if (strncmp(model, "pro", 3) == 0) {
		return 2;
	}
	return 1;
}

void VR_Init( void* system, const char* name, int version ) {
	if (vr_initialized)
		return;

	ovrApp_Clear(&vr_engine.appState);

#ifdef ANDROID
	PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
	xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&xrInitializeLoaderKHR);
	if (xrInitializeLoaderKHR != NULL) {
		ovrJava* java = (ovrJava*)system;
		XrLoaderInitInfoAndroidKHR loaderInitializeInfo;
		memset(&loaderInitializeInfo, 0, sizeof(loaderInitializeInfo));
		loaderInitializeInfo.type = XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR;
		loaderInitializeInfo.next = NULL;
		loaderInitializeInfo.applicationVM = java->Vm;
		loaderInitializeInfo.applicationContext = java->ActivityObject;
		xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&loaderInitializeInfo);
	}
#endif

	int extensionsCount = 0;
	const char* extensions[32];
	extensions[extensionsCount++] = XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME;
#ifdef ANDROID
	extensions[extensionsCount++] = XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME;
	if (VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_FOVEATION)) {
		extensions[extensionsCount++] = XR_FB_SWAPCHAIN_UPDATE_STATE_EXTENSION_NAME;
		extensions[extensionsCount++] = XR_FB_SWAPCHAIN_UPDATE_STATE_OPENGL_ES_EXTENSION_NAME;
		extensions[extensionsCount++] = XR_FB_FOVEATION_EXTENSION_NAME;
		extensions[extensionsCount++] = XR_FB_FOVEATION_CONFIGURATION_EXTENSION_NAME;
	}
	if (VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_INSTANCE)) {
		extensions[extensionsCount++] = XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME;
	}
	if (VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_PERFORMANCE)) {
		extensions[extensionsCount++] = XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME;
		extensions[extensionsCount++] = XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME;
	}
	if (VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_REFRESH)) {
		extensions[extensionsCount++] = XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME;
	}
#endif

	// Create the OpenXR instance.
	XrApplicationInfo appInfo;
	memset(&appInfo, 0, sizeof(appInfo));
	strcpy(appInfo.applicationName, name);
	strcpy(appInfo.engineName, name);
	appInfo.applicationVersion = version;
	appInfo.engineVersion = version;
	appInfo.apiVersion = XR_API_VERSION_1_0;

	XrInstanceCreateInfo instanceCreateInfo;
	memset(&instanceCreateInfo, 0, sizeof(instanceCreateInfo));
	instanceCreateInfo.type = XR_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.next = NULL;
	instanceCreateInfo.createFlags = 0;
	instanceCreateInfo.applicationInfo = appInfo;
	instanceCreateInfo.enabledApiLayerCount = 0;
	instanceCreateInfo.enabledApiLayerNames = NULL;
	instanceCreateInfo.enabledExtensionCount = (uint32_t)extensionsCount;
	instanceCreateInfo.enabledExtensionNames = extensions;

#ifdef ANDROID
	XrInstanceCreateInfoAndroidKHR instanceCreateInfoAndroid = {XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};
	if (VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_INSTANCE)) {
		ovrJava* java = (ovrJava*)system;
		instanceCreateInfoAndroid.applicationVM = java->Vm;
		instanceCreateInfoAndroid.applicationActivity = java->ActivityObject;
		instanceCreateInfo.next = (XrBaseInStructure*)&instanceCreateInfoAndroid;
	}
#endif

	XrResult initResult;
	OXR(initResult = xrCreateInstance(&instanceCreateInfo, &vr_engine.appState.Instance));
	if (initResult != XR_SUCCESS) {
		ALOGE("Failed to create XR instance: %d.", initResult);
		exit(1);
	}

	XrInstanceProperties instanceInfo;
	instanceInfo.type = XR_TYPE_INSTANCE_PROPERTIES;
	instanceInfo.next = NULL;
	OXR(xrGetInstanceProperties(vr_engine.appState.Instance, &instanceInfo));
	ALOGV(
			"Runtime %s: Version : %u.%u.%u",
			instanceInfo.runtimeName,
			XR_VERSION_MAJOR(instanceInfo.runtimeVersion),
			XR_VERSION_MINOR(instanceInfo.runtimeVersion),
			XR_VERSION_PATCH(instanceInfo.runtimeVersion));

	XrSystemGetInfo systemGetInfo;
	memset(&systemGetInfo, 0, sizeof(systemGetInfo));
	systemGetInfo.type = XR_TYPE_SYSTEM_GET_INFO;
	systemGetInfo.next = NULL;
	systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

	XrSystemId systemId;
	OXR(initResult = xrGetSystem(vr_engine.appState.Instance, &systemGetInfo, &systemId));
	if (initResult != XR_SUCCESS) {
		ALOGE("Failed to get system.");
		exit(1);
	}

	XrSystemProperties systemProperties;
	memset(&systemProperties, 0, sizeof(systemProperties));
	systemProperties.type = XR_TYPE_SYSTEM_PROPERTIES;
	// On failure systemName stays empty and the parse falls back to the Quest 2 profile.
	OXR(xrGetSystemProperties(vr_engine.appState.Instance, systemId, &systemProperties));
	vr_device_version = VR_ParseDeviceVersion(systemProperties.systemName);
	strncpy(vr_system_name, systemProperties.systemName, sizeof(vr_system_name) - 1);

	// Get the graphics requirements.
#ifdef ANDROID
	PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetOpenGLESGraphicsRequirementsKHR = NULL;
	OXR(xrGetInstanceProcAddr(
			vr_engine.appState.Instance,
			"xrGetOpenGLESGraphicsRequirementsKHR",
			(PFN_xrVoidFunction*)(&pfnGetOpenGLESGraphicsRequirementsKHR)));

	XrGraphicsRequirementsOpenGLESKHR graphicsRequirements = {};
	graphicsRequirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR;
	OXR(pfnGetOpenGLESGraphicsRequirementsKHR(vr_engine.appState.Instance, systemId, &graphicsRequirements));
#endif

#ifdef ANDROID
	vr_engine.appState.MainThreadTid = gettid();
#endif
	vr_engine.appState.SystemId = systemId;
	vr_initialized = 1;
}

void VR_Destroy( engine_t* engine ) {
	if (engine == &vr_engine) {
		xrDestroyInstance(engine->appState.Instance);
		ovrApp_Destroy(&engine->appState);
	}
}

void VR_EnterVR( engine_t* engine, ovrEgl egl ) {

	if (engine->appState.Session) {
		ALOGE("VR_EnterVR called with existing session");
		return;
	}

	// Create the OpenXR Session.
	XrSessionCreateInfo sessionCreateInfo = {};
#ifdef ANDROID
	XrGraphicsBindingOpenGLESAndroidKHR graphicsBindingGL = {};
	memset(&sessionCreateInfo, 0, sizeof(sessionCreateInfo));
	graphicsBindingGL.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR;
	graphicsBindingGL.next = NULL;
	graphicsBindingGL.display = egl.Display;
	graphicsBindingGL.config = egl.Config;
	graphicsBindingGL.context = egl.Context;
	sessionCreateInfo.next = &graphicsBindingGL;
#endif
	sessionCreateInfo.type = XR_TYPE_SESSION_CREATE_INFO;
	sessionCreateInfo.createFlags = 0;
	sessionCreateInfo.systemId = engine->appState.SystemId;

	XrResult initResult;
	OXR(initResult = xrCreateSession(engine->appState.Instance, &sessionCreateInfo, &engine->appState.Session));
	if (initResult != XR_SUCCESS) {
		ALOGE("Failed to create XR session: %d.", initResult);
		exit(1);
	}

	// Create a space to the first path
	XrReferenceSpaceCreateInfo spaceCreateInfo = {};
	spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
	OXR(xrCreateReferenceSpace(engine->appState.Session, &spaceCreateInfo, &engine->appState.HeadSpace));
	engine->appState.RenderThreadTid = gettid();

#ifdef ANDROID
	if (VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_PERFORMANCE)) {
		XrPerfSettingsLevelEXT cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_BOOST_EXT;
		XrPerfSettingsLevelEXT gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_BOOST_EXT;

		PFN_xrPerfSettingsSetPerformanceLevelEXT pfnPerfSettingsSetPerformanceLevelEXT = NULL;
		OXR(xrGetInstanceProcAddr(
				engine->appState.Instance,
				"xrPerfSettingsSetPerformanceLevelEXT",
				(PFN_xrVoidFunction*)(&pfnPerfSettingsSetPerformanceLevelEXT)));

		OXR(pfnPerfSettingsSetPerformanceLevelEXT(engine->appState.Session, XR_PERF_SETTINGS_DOMAIN_CPU_EXT, cpuPerfLevel));
		OXR(pfnPerfSettingsSetPerformanceLevelEXT(engine->appState.Session, XR_PERF_SETTINGS_DOMAIN_GPU_EXT, gpuPerfLevel));

		PFN_xrSetAndroidApplicationThreadKHR pfnSetAndroidApplicationThreadKHR = NULL;
		OXR(xrGetInstanceProcAddr(
				engine->appState.Instance,
				"xrSetAndroidApplicationThreadKHR",
				(PFN_xrVoidFunction*)(&pfnSetAndroidApplicationThreadKHR)));

		OXR(pfnSetAndroidApplicationThreadKHR(engine->appState.Session, XR_ANDROID_THREAD_TYPE_APPLICATION_MAIN_KHR, engine->appState.MainThreadTid));
		OXR(pfnSetAndroidApplicationThreadKHR(engine->appState.Session, XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR, engine->appState.RenderThreadTid));
	}
#endif
}

void VR_LeaveVR( engine_t* engine ) {
	if (engine->appState.Session) {
		OXR(xrDestroySpace(engine->appState.HeadSpace));
		// StageSpace is optional.
		if (engine->appState.StageSpace != XR_NULL_HANDLE) {
			OXR(xrDestroySpace(engine->appState.StageSpace));
		}
		OXR(xrDestroySpace(engine->appState.FakeStageSpace));
		engine->appState.CurrentSpace = XR_NULL_HANDLE;
		OXR(xrDestroySession(engine->appState.Session));
		engine->appState.Session = XR_NULL_HANDLE;
	}
}

engine_t* VR_GetEngine( void ) {
	return &vr_engine;
}

int VR_GetDeviceVersion( void ) {
	return vr_device_version;
}

const char* VR_GetSystemName( void ) {
	return vr_system_name;
}

bool VR_GetPlatformFlag(enum VRPlatformFlag flag) {
	return vr_platform[flag];
}

void VR_SetPlatformFLag(enum VRPlatformFlag flag, bool value) {
	vr_platform[flag] = value;
}
