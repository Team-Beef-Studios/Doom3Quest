#include "VrBase.h"
#include "VrInput.h"
#include "VrRenderer.h"

#include <assert.h>
#include <string.h>
#include <pthread.h>

XrFovf fov;
XrPosef pose[ovrMaxNumEyes];
XrView* projections;
bool initialized = false;
bool stageBoundsDirty = true;
static bool menuYawNeedsUpdate = true;
bool stageSupported = false;
int vrConfig[VR_CONFIG_MAX] = {};
float vrConfigFloat[VR_CONFIG_FLOAT_MAX] = {};
PFN_xrGetDisplayRefreshRateFB pfnGetDisplayRefreshRate = NULL;
PFN_xrRequestDisplayRefreshRateFB pfnRequestDisplayRefreshRate = NULL;
PFN_xrEnumerateDisplayRefreshRatesFB pfnEnumerateDisplayRefreshRates = NULL;

typedef struct {
	XrTime displayTime;
	XrView views[ovrMaxNumEyes];
	XrFovf fov;
} vrFrameState_t;

#define VR_FRAME_SLOTS 4

static pthread_mutex_t frameLock = PTHREAD_MUTEX_INITIALIZER;
static vrFrameState_t frameSlots[VR_FRAME_SLOTS];
static int slotActive = 0;
static int64_t publishCount = 0;
static int64_t consumeCount = 0;
static XrTime lastWaitTime = 0;
static XrTime lastWaitPeriod = 0;
static int pendingRefreshRate = 0;

static XrTime renderDisplayTime = 0;
static bool renderFrameValid = false;
static bool renderShouldRender = false;

XrTime vrGameDisplayTime = 0;

void VR_UpdateStageBounds(ovrApp* pappState) {
	XrExtent2Df stageBounds = {};

	XrResult result;
	OXR(result = xrGetReferenceSpaceBoundsRect(pappState->Session, XR_REFERENCE_SPACE_TYPE_STAGE, &stageBounds));
	if (result != XR_SUCCESS) {
		stageBounds.width = 1.0f;
		stageBounds.height = 1.0f;

		pappState->CurrentSpace = pappState->FakeStageSpace;
	}
}

void VR_GetResolution(engine_t* engine, int *pWidth, int *pHeight) {
	static int width = 0;
	static int height = 0;

	if (engine) {
		// Enumerate the viewport configurations.
		uint32_t viewportConfigTypeCount = 0;
		OXR(xrEnumerateViewConfigurations(
				engine->appState.Instance, engine->appState.SystemId, 0, &viewportConfigTypeCount, NULL));

		XrViewConfigurationType* viewportConfigurationTypes =
				(XrViewConfigurationType*)malloc(viewportConfigTypeCount * sizeof(XrViewConfigurationType));

		OXR(xrEnumerateViewConfigurations(
				engine->appState.Instance,
				engine->appState.SystemId,
				viewportConfigTypeCount,
				&viewportConfigTypeCount,
				viewportConfigurationTypes));

		ALOGV("Available Viewport Configuration Types: %d", viewportConfigTypeCount);

		for (uint32_t i = 0; i < viewportConfigTypeCount; i++) {
			const XrViewConfigurationType viewportConfigType = viewportConfigurationTypes[i];

			ALOGV(
					"Viewport configuration type %d : %s",
					viewportConfigType,
					viewportConfigType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO ? "Selected" : "");

			XrViewConfigurationProperties viewportConfig;
			viewportConfig.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;
			OXR(xrGetViewConfigurationProperties(
					engine->appState.Instance, engine->appState.SystemId, viewportConfigType, &viewportConfig));
			ALOGV(
					"FovMutable=%s ConfigurationType %d",
					viewportConfig.fovMutable ? "true" : "false",
					viewportConfig.viewConfigurationType);

			uint32_t viewCount;
			OXR(xrEnumerateViewConfigurationViews(
					engine->appState.Instance, engine->appState.SystemId, viewportConfigType, 0, &viewCount, NULL));

			if (viewCount > 0) {
				XrViewConfigurationView* elements =
						(XrViewConfigurationView*)malloc(viewCount * sizeof(XrViewConfigurationView));

				for (uint32_t e = 0; e < viewCount; e++) {
					elements[e].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
					elements[e].next = NULL;
				}

				OXR(xrEnumerateViewConfigurationViews(
						engine->appState.Instance,
						engine->appState.SystemId,
						viewportConfigType,
						viewCount,
						&viewCount,
						elements));

				// Cache the view config properties for the selected config type.
				if (viewportConfigType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
					assert(viewCount == ovrMaxNumEyes);
					for (uint32_t e = 0; e < viewCount; e++) {
						engine->appState.ViewConfigurationView[e] = elements[e];
					}
				}

				free(elements);
			} else {
				ALOGE("Empty viewport configuration type: %d", viewCount);
			}
		}

		free(viewportConfigurationTypes);

		*pWidth = width = engine->appState.ViewConfigurationView[0].recommendedImageRectWidth;
		*pHeight = height = engine->appState.ViewConfigurationView[0].recommendedImageRectHeight;
	} else {
		//use cached values
		*pWidth = width;
		*pHeight = height;
	}

	//Apply supersampling
	float supersampling = VR_GetConfigFloat(VR_CONFIG_VIEWPORT_SUPERSAMPLING);
	if (supersampling > 0) {
		*pWidth *= supersampling;
		*pHeight *= supersampling;
	}

	//Force square resolution
	if (VR_GetPlatformFlag(VR_PLATFORM_VIEWPORT_SQUARE)) {
		*pHeight = *pWidth;
	}
}

void VR_Recenter(engine_t* engine) {

	// Calculate recenter reference
	XrReferenceSpaceCreateInfo spaceCreateInfo = {};
	spaceCreateInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
	spaceCreateInfo.poseInReferenceSpace = XrPosef_Identity();
	if (engine->appState.CurrentSpace != XR_NULL_HANDLE) {
		XrSpaceLocation loc = {};
		loc.type = XR_TYPE_SPACE_LOCATION;
		OXR(xrLocateSpace(engine->appState.HeadSpace, engine->appState.CurrentSpace, renderDisplayTime, &loc));
		XrVector3f hmdangles = XrQuaternionf_ToEulerAngles(loc.pose.orientation);

		VR_SetConfigFloat(VR_CONFIG_RECENTER_YAW, VR_GetConfigFloat(VR_CONFIG_RECENTER_YAW) + hmdangles.y);
		float recenterYaw = ToRadians(VR_GetConfigFloat(VR_CONFIG_RECENTER_YAW));
		spaceCreateInfo.poseInReferenceSpace.orientation.x = 0;
		spaceCreateInfo.poseInReferenceSpace.orientation.y = sinf(recenterYaw / 2);
		spaceCreateInfo.poseInReferenceSpace.orientation.z = 0;
		spaceCreateInfo.poseInReferenceSpace.orientation.w = cosf(recenterYaw / 2);
	}

	// Delete previous space instances
	if (engine->appState.StageSpace != XR_NULL_HANDLE) {
		OXR(xrDestroySpace(engine->appState.StageSpace));
	}
	if (engine->appState.FakeStageSpace != XR_NULL_HANDLE) {
		OXR(xrDestroySpace(engine->appState.FakeStageSpace));
	}

	// Create a default stage space to use if SPACE_TYPE_STAGE is not
	// supported, or calls to xrGetReferenceSpaceBoundsRect fail.
	spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	spaceCreateInfo.poseInReferenceSpace = XrPosef_Identity();
	if (VR_GetPlatformFlag(VR_PLATFORM_TRACKING_FLOOR)) {
		spaceCreateInfo.poseInReferenceSpace.position.y = -1.6750f;
	}
	OXR(xrCreateReferenceSpace(engine->appState.Session, &spaceCreateInfo, &engine->appState.FakeStageSpace));
	ALOGV("Created fake stage space from local space with offset");
	engine->appState.CurrentSpace = engine->appState.FakeStageSpace;

	if (stageSupported) {
		spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
		spaceCreateInfo.poseInReferenceSpace.position.y = 0.0;
		OXR(xrCreateReferenceSpace(engine->appState.Session, &spaceCreateInfo, &engine->appState.StageSpace));
		ALOGV("Created stage space");
		if (VR_GetPlatformFlag(VR_PLATFORM_TRACKING_FLOOR)) {
			engine->appState.CurrentSpace = engine->appState.StageSpace;
		}
	}

	// Update menu orientation on next frame with valid head tracking
	menuYawNeedsUpdate = true;
	stageBoundsDirty = true;
}

void VR_InitRenderer( engine_t* engine, bool multiview ) {
	if (initialized) {
		VR_DestroyRenderer(engine);
	}

	int eyeW, eyeH;
	VR_GetResolution(engine, &eyeW, &eyeH);
	VR_SetConfig(VR_CONFIG_VIEWPORT_WIDTH, eyeW);
	VR_SetConfig(VR_CONFIG_VIEWPORT_HEIGHT, eyeH);

	// Get the viewport configuration info for the chosen viewport configuration type.
	engine->appState.ViewportConfig.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;
	OXR(xrGetViewConfigurationProperties(engine->appState.Instance, engine->appState.SystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, &engine->appState.ViewportConfig));

	uint32_t numOutputSpaces = 0;
	OXR(xrEnumerateReferenceSpaces(engine->appState.Session, 0, &numOutputSpaces, NULL));
	XrReferenceSpaceType* referenceSpaces = (XrReferenceSpaceType*)malloc(numOutputSpaces * sizeof(XrReferenceSpaceType));
	OXR(xrEnumerateReferenceSpaces(engine->appState.Session, numOutputSpaces, &numOutputSpaces, referenceSpaces));

	for (uint32_t i = 0; i < numOutputSpaces; i++) {
		if (referenceSpaces[i] == XR_REFERENCE_SPACE_TYPE_STAGE) {
			stageSupported = true;
			break;
		}
	}

	free(referenceSpaces);

	if (engine->appState.CurrentSpace == XR_NULL_HANDLE) {
		VR_Recenter(engine);
	}

	projections = (XrView*)(malloc(ovrMaxNumEyes * sizeof(XrView)));
	for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
		memset(&projections[eye], 0, sizeof(XrView));
		projections[eye].type = XR_TYPE_VIEW;
		projections[eye].pose = XrPosef_Identity();
	}

	for (int i = 0; i < VR_FRAME_SLOTS; i++) {
		memset(&frameSlots[i], 0, sizeof(vrFrameState_t));
		for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
			frameSlots[i].views[eye].type = XR_TYPE_VIEW;
			frameSlots[i].views[eye].pose = XrPosef_Identity();
		}
	}

	int msaa = VR_GetConfig(VR_CONFIG_VIEWPORT_MSAA);
	ovrRenderer_Create(engine->appState.Session, &engine->appState.Renderer, multiview, eyeW, eyeH, msaa > 0 ? msaa : 1);
#ifdef ANDROID
	if (VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_FOVEATION)) {
		ovrRenderer_SetFoveation(&engine->appState.Instance, &engine->appState.Session, &engine->appState.Renderer, XR_FOVEATION_LEVEL_HIGH_FB, 0, XR_FOVEATION_DYNAMIC_LEVEL_ENABLED_FB);
	}
#endif
	initialized = true;
}

void VR_DestroyRenderer( engine_t* engine ) {
	ovrRenderer_Destroy(&engine->appState.Renderer);
	free(projections);
	initialized = false;
}

bool VR_PollInput( engine_t* engine ) {
	if (engine->appState.SessionActive == false) {
		return false;
	}

	pthread_mutex_lock(&frameLock);
	XrTime baseTime = lastWaitTime;
	XrTime basePeriod = lastWaitPeriod;
	int64_t framesAhead = 1 + (publishCount - consumeCount);
	pthread_mutex_unlock(&frameLock);

	if (baseTime == 0) {
		return false;
	}

	if (framesAhead < 1) {
		framesAhead = 1;
	} else if (framesAhead > VR_FRAME_SLOTS) {
		framesAhead = VR_FRAME_SLOTS;
	}
	vrGameDisplayTime = baseTime + basePeriod * framesAhead;

	// Update HMD
	XrViewLocateInfo projectionInfo = {};
	projectionInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
	projectionInfo.viewConfigurationType = engine->appState.ViewportConfig.viewConfigurationType;
	projectionInfo.displayTime = vrGameDisplayTime;
	projectionInfo.space = engine->appState.CurrentSpace;
	XrViewState viewState = {XR_TYPE_VIEW_STATE, NULL};
	uint32_t projectionCapacityInput = ovrMaxNumEyes;
	uint32_t projectionCountOutput = projectionCapacityInput;
	OXR(xrLocateViews(
			engine->appState.Session,
			&projectionInfo,
			&viewState,
			projectionCapacityInput,
			&projectionCountOutput,
			projections));

	// Update controllers
	IN_VRInputFrame(engine);

	float fovx = 0;
	float fovy = 0;
	for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
		fovx += fabs(projections[eye].fov.angleDown - projections[eye].fov.angleUp) / 2.0f;
		fovy += fabs(projections[eye].fov.angleRight - projections[eye].fov.angleLeft) / 2.0f;
	}

	if (VR_GetPlatformFlag(VR_PLATFORM_VIEWPORT_UNCENTERED)) {
		fovy *= 1.1f;
	}

	if (VR_GetPlatformFlag(VR_PLATFORM_VIEWPORT_SQUARE)) {
		VR_SetConfigFloat(VR_CONFIG_VIEWPORT_FOVX, ToDegrees(fovy));
		fov.angleLeft = -fovy / 2.0f;
		fov.angleRight = fovy / 2.0f;
	} else {
	   VR_SetConfigFloat(VR_CONFIG_VIEWPORT_FOVX, ToDegrees(fovx));
	   fov.angleLeft = -fovx / 2.0f;
	   fov.angleRight = fovx / 2.0f;
	}
	VR_SetConfigFloat(VR_CONFIG_VIEWPORT_FOVY, ToDegrees(fovy));
	fov.angleDown = -fovy / 2.0f;
	fov.angleUp = fovy / 2.0f;

	vrFrameState_t* slot = &frameSlots[publishCount % VR_FRAME_SLOTS];
	slot->displayTime = vrGameDisplayTime;
	slot->fov = fov;
	memcpy(slot->views, projections, sizeof(XrView) * ovrMaxNumEyes);

	pthread_mutex_lock(&frameLock);
	publishCount++;
	pthread_mutex_unlock(&frameLock);

	return true;
}

bool VR_WaitFrame( engine_t* engine ) {
	if (ovrApp_HandleXrEvents(&engine->appState)) {
		VR_Recenter(engine);
	}
	if (engine->appState.SessionActive == false) {
		renderFrameValid = false;
		return false;
	}

	if (stageBoundsDirty) {
		VR_UpdateStageBounds(&engine->appState);
		stageBoundsDirty = false;
	}

	XrFrameState frameState = {};
	frameState.type = XR_TYPE_FRAME_STATE;
	frameState.next = NULL;
	OXR(xrWaitFrame(engine->appState.Session, 0, &frameState));

	renderDisplayTime = frameState.predictedDisplayTime;
	renderShouldRender = frameState.shouldRender;
	renderFrameValid = true;

	pthread_mutex_lock(&frameLock);
	lastWaitTime = frameState.predictedDisplayTime;
	lastWaitPeriod = frameState.predictedDisplayPeriod;
	if (consumeCount < publishCount) {
		if (publishCount - consumeCount > VR_FRAME_SLOTS - 1) {
			consumeCount = publishCount - (VR_FRAME_SLOTS - 1);
		}
		slotActive = (int)(consumeCount % VR_FRAME_SLOTS);
		consumeCount++;
	}
	pthread_mutex_unlock(&frameLock);

	return true;
}

void VR_BeginFrame( engine_t* engine ) {
	if (!renderFrameValid) {
		return;
	}

	XrFrameBeginInfo beginFrameDesc = {};
	beginFrameDesc.type = XR_TYPE_FRAME_BEGIN_INFO;
	beginFrameDesc.next = NULL;
	OXR(xrBeginFrame(engine->appState.Session, &beginFrameDesc));

	const vrFrameState_t* slot = &frameSlots[slotActive];
	for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
		memcpy(&pose[eye], &slot->views[eye].pose, sizeof(XrPosef));
	}

	ovrFramebuffer_Acquire(&engine->appState.Renderer.FrameBuffer);
	ovrFramebuffer_SetCurrent(&engine->appState.Renderer.FrameBuffer);
}

void VR_EndFrame( engine_t* engine ) {
	if (!renderFrameValid) {
		return;
	}

	VR_BindFramebuffer(engine);

	// Show mouse cursor
	int vrMode = vrConfig[VR_CONFIG_MODE];
	bool screenMode = (vrMode == VR_MODE_MONO_SCREEN) || (vrMode == VR_MODE_STEREO_SCREEN);
	if (screenMode && (vrConfig[VR_CONFIG_MOUSE_SIZE] > 0)) {
		int x = vrConfig[VR_CONFIG_MOUSE_X];
		int y = vrConfig[VR_CONFIG_MOUSE_Y];
		int sx = vrConfig[VR_CONFIG_MOUSE_SIZE];
		int sy = (int)((float)sx * VR_GetConfigFloat(VR_CONFIG_CANVAS_ASPECT));
		ovrRenderer_MouseCursor(&engine->appState.Renderer, x, y, sx, sy);
	}

	ovrFramebuffer_Resolve(&engine->appState.Renderer.FrameBuffer);
	ovrFramebuffer_Release(&engine->appState.Renderer.FrameBuffer);
	ovrFramebuffer_SetNone();
}

void VR_FinishFrame( engine_t* engine ) {
	if (!renderFrameValid) {
		return;
	}
	renderFrameValid = false;

	int layerCount = 0;
	ovrCompositorLayer_Union layerUnion[ovrMaxLayerCount];
	memset(layerUnion, 0, sizeof(ovrCompositorLayer_Union) * ovrMaxLayerCount);

	const XrFovf layerFov = frameSlots[slotActive].fov;
	int vrMode = vrConfig[VR_CONFIG_MODE];
	XrCompositionLayerProjectionView projection_layer_elements[2] = {};
	if ((vrMode == VR_MODE_MONO_6DOF) || (vrMode == VR_MODE_STEREO_6DOF)) {
		VR_SetConfigFloat(VR_CONFIG_MENU_YAW, XrQuaternionf_ToEulerAngles(pose[0].orientation).y);
		menuYawNeedsUpdate = false;

		for (int eye = 0; eye < ovrMaxNumEyes; eye++) {
			ovrFramebuffer* frameBuffer = &engine->appState.Renderer.FrameBuffer;
			memset(&projection_layer_elements[eye], 0, sizeof(XrCompositionLayerProjectionView));
			projection_layer_elements[eye].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
			projection_layer_elements[eye].pose = pose[eye];
			projection_layer_elements[eye].fov = layerFov;

			memset(&projection_layer_elements[eye].subImage, 0, sizeof(XrSwapchainSubImage));
			projection_layer_elements[eye].subImage.swapchain = frameBuffer->ColorSwapChain.Handle;
			projection_layer_elements[eye].subImage.imageRect.offset.x = 0;
			projection_layer_elements[eye].subImage.imageRect.offset.y = 0;
			projection_layer_elements[eye].subImage.imageRect.extent.width = frameBuffer->ColorSwapChain.Width;
			projection_layer_elements[eye].subImage.imageRect.extent.height = frameBuffer->ColorSwapChain.Height;
			projection_layer_elements[eye].subImage.imageArrayIndex = vrMode == VR_MODE_MONO_6DOF ? 0 : eye;
		}

		XrCompositionLayerProjection projection_layer = {};
		projection_layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
		projection_layer.space = engine->appState.CurrentSpace;
		projection_layer.viewCount = ovrMaxNumEyes;
		projection_layer.views = projection_layer_elements;

		layerUnion[layerCount++].Projection = projection_layer;
	} else if ((vrMode == VR_MODE_MONO_SCREEN) || (vrMode == VR_MODE_STEREO_SCREEN)) {

		if (menuYawNeedsUpdate) {
			// Guard against uninitialized pose (first frame before xrLocateViews)
			float qLenSq = pose[0].orientation.x * pose[0].orientation.x
			              + pose[0].orientation.y * pose[0].orientation.y
			              + pose[0].orientation.z * pose[0].orientation.z
			              + pose[0].orientation.w * pose[0].orientation.w;
			if (qLenSq > 0.5f) {
				VR_SetConfigFloat(VR_CONFIG_MENU_YAW, XrQuaternionf_ToEulerAngles(pose[0].orientation).y);
				menuYawNeedsUpdate = false;
			}
		}

		// Flat screen pose
		float distance = VR_GetConfigFloat(VR_CONFIG_CANVAS_DISTANCE);
		float menuYaw = ToRadians(VR_GetConfigFloat(VR_CONFIG_MENU_YAW));
		XrVector3f pos = {
				pose[0].position.x - sinf(menuYaw) * distance,
				pose[0].position.y - 1.5f,
				pose[0].position.z - cosf(menuYaw) * distance
		};
		XrVector3f yawAxis = {0, 1, 0};
		XrQuaternionf yaw = XrQuaternionf_CreateFromVectorAngle(yawAxis, menuYaw);

		// Setup the cylinder layer
		XrCompositionLayerCylinderKHR cylinder_layer = {};
		cylinder_layer.type = XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR;
		cylinder_layer.space = engine->appState.CurrentSpace;
		memset(&cylinder_layer.subImage, 0, sizeof(XrSwapchainSubImage));
		cylinder_layer.subImage.imageRect.offset.x = 0;
		cylinder_layer.subImage.imageRect.offset.y = 0;
		cylinder_layer.subImage.imageRect.extent.width = engine->appState.Renderer.FrameBuffer.ColorSwapChain.Width;
		cylinder_layer.subImage.imageRect.extent.height = engine->appState.Renderer.FrameBuffer.ColorSwapChain.Height;
		cylinder_layer.subImage.swapchain = engine->appState.Renderer.FrameBuffer.ColorSwapChain.Handle;
		cylinder_layer.subImage.imageArrayIndex = 0;
		cylinder_layer.pose.orientation = yaw;
		cylinder_layer.pose.position = pos;
		cylinder_layer.radius = 12.0f;
		cylinder_layer.centralAngle = (float)(M_PI * 0.5);
		cylinder_layer.aspectRatio = VR_GetConfigFloat(VR_CONFIG_CANVAS_ASPECT);

		// Build the cylinder layer
		if (vrMode == VR_MODE_MONO_SCREEN) {
			cylinder_layer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
			layerUnion[layerCount++].Cylinder = cylinder_layer;
		} else {
			cylinder_layer.eyeVisibility = XR_EYE_VISIBILITY_LEFT;
			layerUnion[layerCount++].Cylinder = cylinder_layer;
			cylinder_layer.eyeVisibility = XR_EYE_VISIBILITY_RIGHT;
			cylinder_layer.subImage.imageArrayIndex = 0;
			layerUnion[layerCount++].Cylinder = cylinder_layer;
		}
	} else {
		assert(false);
	}

	if (!renderShouldRender) {
		layerCount = 0;
	}

	// Compose the layers for this frame.
	const XrCompositionLayerBaseHeader* layers[ovrMaxLayerCount] = {};
	for (int i = 0; i < layerCount; i++) {
		layers[i] = (const XrCompositionLayerBaseHeader*)&layerUnion[i];
	}

	XrFrameEndInfo endFrameInfo = {};
	endFrameInfo.type = XR_TYPE_FRAME_END_INFO;
	endFrameInfo.displayTime = renderDisplayTime;
	endFrameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
	endFrameInfo.layerCount = layerCount;
	endFrameInfo.layers = layers;
	OXR(xrEndFrame(engine->appState.Session, &endFrameInfo));

	if (VR_GetConfig(VR_CONFIG_NEED_RECENTER)) {
		VR_SetConfig(VR_CONFIG_NEED_RECENTER, false);
		VR_Recenter(engine);
	}
}

int VR_GetConfig(enum VRConfig config ) {
	return vrConfig[config];
}

void VR_SetConfig(enum VRConfig config, int value) {
	vrConfig[config] = value;
}

float VR_GetConfigFloat(enum VRConfigFloat config) {
	return vrConfigFloat[config];
}

void VR_SetConfigFloat(enum VRConfigFloat config, float value) {
	vrConfigFloat[config] = value;
}

void VR_BindFramebuffer(engine_t *engine) {
	if (!initialized) return;
	ovrFramebuffer_SetCurrent(&engine->appState.Renderer.FrameBuffer);
}

XrPosef VR_GetView(int eye) {
	return projections[eye].pose;
}

int VR_GetRefreshRate() {
	if (VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_REFRESH)) {
		if (!pfnGetDisplayRefreshRate) {
			OXR(xrGetInstanceProcAddr(
					VR_GetEngine()->appState.Instance,
					"xrGetDisplayRefreshRateFB",
					(PFN_xrVoidFunction*)(&pfnGetDisplayRefreshRate)));
		}

		float currentDisplayRefreshRate = 0.0f;
		OXR(pfnGetDisplayRefreshRate(VR_GetEngine()->appState.Session, &currentDisplayRefreshRate));
		return (int)currentDisplayRefreshRate;
	}
	return 72;
}

static bool VR_IsRefreshRateSupported(int refresh) {
	if (!pfnEnumerateDisplayRefreshRates) {
		OXR(xrGetInstanceProcAddr(
				VR_GetEngine()->appState.Instance,
				"xrEnumerateDisplayRefreshRatesFB",
				(PFN_xrVoidFunction*)(&pfnEnumerateDisplayRefreshRates)));
	}
	if (!pfnEnumerateDisplayRefreshRates) {
		return true;
	}

	uint32_t count = 0;
	OXR(pfnEnumerateDisplayRefreshRates(VR_GetEngine()->appState.Session, 0, &count, NULL));
	if (count == 0) {
		return true;
	}

	float* rates = (float*)malloc(count * sizeof(float));
	OXR(pfnEnumerateDisplayRefreshRates(VR_GetEngine()->appState.Session, count, &count, rates));

	bool supported = false;
	for (uint32_t i = 0; i < count; i++) {
		if ((int)rates[i] == refresh) {
			supported = true;
			break;
		}
	}
	free(rates);

	if (!supported) {
		ALOGE("Display refresh rate %d Hz is not supported, keeping the current rate", refresh);
	}
	return supported;
}

void VR_SetRefreshRate(int refresh) {
	if (VR_GetPlatformFlag(VR_PLATFORM_EXTENSION_REFRESH)) {
		if (!pfnRequestDisplayRefreshRate) {
			OXR(xrGetInstanceProcAddr(
					VR_GetEngine()->appState.Instance,
					"xrRequestDisplayRefreshRateFB",
					(PFN_xrVoidFunction*)(&pfnRequestDisplayRefreshRate)));
		}
		if (!VR_IsRefreshRateSupported(refresh)) {
			return;
		}
		OXR(pfnRequestDisplayRefreshRate(VR_GetEngine()->appState.Session, 72.0f));
		OXR(pfnRequestDisplayRefreshRate(VR_GetEngine()->appState.Session, (float)refresh));
	}
}

void VR_NotifyRefreshRateChanged( int refresh ) {
	pthread_mutex_lock(&frameLock);
	pendingRefreshRate = refresh;
	pthread_mutex_unlock(&frameLock);
}

int VR_ConsumePendingRefreshRate( void ) {
	pthread_mutex_lock(&frameLock);
	int refresh = pendingRefreshRate;
	pendingRefreshRate = 0;
	pthread_mutex_unlock(&frameLock);
	return refresh;
}