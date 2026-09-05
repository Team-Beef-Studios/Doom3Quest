
#include "renderer/tr_local.h"
#include "sys/platform.h"

extern "C"
{

int Android_GetCVarInteger(const char* cvar)
{
	if (cvarSystem != nullptr) {
		return cvarSystem->GetCVarInteger(cvar);
	}

	return 0;
}

float Android_GetCVarFloat(const char* cvar)
{
	if (cvarSystem != nullptr) {
		return cvarSystem->GetCVarFloat(cvar);
	}

	return 0.0f;
}

static const char *cmd_to_run = NULL;
void Android_SetCommand(const char * cmd)
{
	cmd_to_run = cmd;
}

// Can only set one impulse per frame, this should be fine
static int nextImpulse = 0;
void Android_SetImpulse(int impulse)
{
	nextImpulse = impulse;
}

typedef enum {
	UB_NONE,

	UB_UP,
	UB_DOWN,
	UB_LEFT,
	UB_RIGHT,
	UB_FORWARD,
	UB_BACK,
	UB_LOOKUP,
	UB_LOOKDOWN,
	UB_STRAFE,
	UB_MOVELEFT,
	UB_MOVERIGHT,

	UB_BUTTON0,
	UB_BUTTON1,
	UB_BUTTON2,
	UB_BUTTON3,
	UB_BUTTON4,
	UB_BUTTON5,
	UB_BUTTON6,
	UB_BUTTON7,

	UB_ATTACK,
	UB_SPEED,
	UB_ZOOM,
	UB_SHOWSCORES,
	UB_MLOOK,

	UB_IMPULSE0,
	UB_IMPULSE1,
	UB_IMPULSE2,
	UB_IMPULSE3,
	UB_IMPULSE4,
	UB_IMPULSE5,
	UB_IMPULSE6,
	UB_IMPULSE7,
	UB_IMPULSE8,
	UB_IMPULSE9,
	UB_IMPULSE10,
	UB_IMPULSE11,
	UB_IMPULSE12,
	UB_IMPULSE13,
	UB_IMPULSE14,
	UB_IMPULSE15,
	UB_IMPULSE16,
	UB_IMPULSE17,
	UB_IMPULSE18,
	UB_IMPULSE19,
	UB_IMPULSE20,
	UB_IMPULSE21,
	UB_IMPULSE22,
	UB_IMPULSE23,
	UB_IMPULSE24,
	UB_IMPULSE25,
	UB_IMPULSE26,
	UB_IMPULSE27,
	UB_IMPULSE28,
	UB_IMPULSE29,
	UB_IMPULSE30,
	UB_IMPULSE31,
	UB_IMPULSE32,
	UB_IMPULSE33,
	UB_IMPULSE34,
	UB_IMPULSE35,
	UB_IMPULSE36,
	UB_IMPULSE37,
	UB_IMPULSE38,
	UB_IMPULSE39,
	UB_IMPULSE40,
	UB_IMPULSE41,
	UB_IMPULSE42,
	UB_IMPULSE43,
	UB_IMPULSE44,
	UB_IMPULSE45,
	UB_IMPULSE46,
	UB_IMPULSE47,
	UB_IMPULSE48,
	UB_IMPULSE49,
	UB_IMPULSE50,
	UB_IMPULSE51,
	UB_IMPULSE52,
	UB_IMPULSE53,
	UB_IMPULSE54,
	UB_IMPULSE55,
	UB_IMPULSE56,
	UB_IMPULSE57,
	UB_IMPULSE58,
	UB_IMPULSE59,
	UB_IMPULSE60,
	UB_IMPULSE61,
	UB_IMPULSE62,
	UB_IMPULSE63,

	UB_MAX_BUTTONS
} usercmdButton_t;

static int cmdButtons[UB_MAX_BUTTONS];

void Android_ButtonChange(int key, int state)
{
	cmdButtons[key] = !!state;
}

int Android_GetButton(int key)
{
	return cmdButtons[key];
}

const char * Android_GetCommand()
{
	// Potential race condition here to miss a command, however extremely unlikely to happen
	const char *cmd = cmd_to_run;
	cmd_to_run = NULL;
	return cmd;
}

int Android_GetNextImpulse()
{
	int impulse = nextImpulse;
	nextImpulse = 0;
	return impulse;
}


}