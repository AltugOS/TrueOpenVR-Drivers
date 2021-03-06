#include "stdafx.h"
#include <windows.h>
#include <atlstr.h> 

#define DLLEXPORT extern "C" __declspec(dllexport)

typedef struct _HMDData
{
	double	X;
	double	Y;
	double	Z;
	double	Yaw;
	double	Pitch;
	double	Roll;
} THMD, *PHMD;

typedef struct _Controller
{
	double	X;
	double	Y;
	double	Z;
	double	Yaw;
	double	Pitch;
	double	Roll;
	unsigned short	Buttons;
	float	Trigger;
	float	AxisX;
	float	AxisY;
} TController, *PController;

#define TOVR_SUCCESS 0
#define TOVR_FAILURE 1

#define GRIP_BTN	0x0001
#define THUMB_BTN	0x0002
#define A_BTN		0x0004
#define B_BTN		0x0008
#define MENU_BTN	0x0010
#define SYS_BTN		0x0020

#define XINPUT_GAMEPAD_DPAD_UP          0x0001
#define XINPUT_GAMEPAD_DPAD_DOWN        0x0002
#define XINPUT_GAMEPAD_DPAD_LEFT        0x0004
#define XINPUT_GAMEPAD_DPAD_RIGHT       0x0008
#define XINPUT_GAMEPAD_START            0x0010
#define XINPUT_GAMEPAD_BACK             0x0020
#define XINPUT_GAMEPAD_LEFT_THUMB       0x0040
#define XINPUT_GAMEPAD_RIGHT_THUMB      0x0080
#define XINPUT_GAMEPAD_LEFT_SHOULDER    0x0100
#define XINPUT_GAMEPAD_RIGHT_SHOULDER   0x0200
#define XINPUT_GAMEPAD_A                0x1000
#define XINPUT_GAMEPAD_B                0x2000
#define XINPUT_GAMEPAD_X                0x4000
#define XINPUT_GAMEPAD_Y				0x8000

#define XUSER_MAX_COUNT                 4
#define XUSER_INDEX_ANY					0x000000FF

#define ERROR_DEVICE_NOT_CONNECTED		1167
#define ERROR_SUCCESS					0

//
// Structures used by XInput APIs
//
typedef struct _XINPUT_GAMEPAD
{
	WORD                                wButtons;
	BYTE                                bLeftTrigger;
	BYTE                                bRightTrigger;
	SHORT                               sThumbLX;
	SHORT                               sThumbLY;
	SHORT                               sThumbRX;
	SHORT                               sThumbRY;
} XINPUT_GAMEPAD, *PXINPUT_GAMEPAD;

typedef struct _XINPUT_STATE
{
	DWORD                               dwPacketNumber;
	XINPUT_GAMEPAD                      Gamepad;
} XINPUT_STATE, *PXINPUT_STATE;

typedef struct _XINPUT_VIBRATION
{
	WORD                                wLeftMotorSpeed;
	WORD                                wRightMotorSpeed;
} XINPUT_VIBRATION, *PXINPUT_VIBRATION;

typedef DWORD(__stdcall *_XInputGetState)(_In_ DWORD dwUserIndex, _Out_ XINPUT_STATE *pState);
typedef DWORD(__stdcall *_XInputSetState)(_In_ DWORD dwUserIndex, _In_ XINPUT_VIBRATION *pVibration);

_XInputGetState MyXInputGetState;
_XInputSetState MyXInputSetState;

HMODULE hDll;
bool GamePadConnected = false, GamePadInit = false;
XINPUT_STATE myPState;

void GamePadStart() {
	hDll = LoadLibrary(_T("C:\\Windows\\System32\\xinput1_3.dll"));

	if (hDll != NULL) {

		MyXInputGetState = (_XInputGetState)GetProcAddress(hDll, "XInputGetState");
		MyXInputSetState = (_XInputSetState)GetProcAddress(hDll, "XInputSetState"); 

		if (MyXInputGetState != NULL && MyXInputSetState != NULL)
			if (MyXInputGetState(0, &myPState) == ERROR_SUCCESS)
				GamePadConnected = true;
	}
}

DLLEXPORT DWORD __stdcall GetHMDData(__out THMD *HMD)
{
	HMD->X = 0;
	HMD->Y = 0;
	HMD->Z = 0;
	HMD->Yaw = 0;
	HMD->Pitch = 0;
	HMD->Roll = 0;

	return TOVR_FAILURE;
}

double ConvAxis(double n) {
	if (n > 1) {
		return 1;
	}
	else if (n < -1)
	{
		return -1;
	}
	else
	{
		return n;
	}
}

DLLEXPORT DWORD __stdcall GetControllersData(__out TController *FirstController, __out TController *SecondController)
{
	if (GamePadInit == false) {
		GamePadInit = true;
		GamePadStart();
	}

	//Controller 1
	FirstController->X = 0;
	FirstController->Y = 0;
	FirstController->Z = 0;

	FirstController->Yaw = 0;
	FirstController->Pitch = 0;
	FirstController->Roll = 0;

	FirstController->Trigger = 0;
	FirstController->AxisX = 0;
	FirstController->AxisY = 0;

	FirstController->Buttons = 0;

	//Controller 2
	SecondController->X = 0;
	SecondController->Y = 0;
	SecondController->Z = 0;

	SecondController->Yaw = 0;
	SecondController->Pitch = 0;
	SecondController->Roll = 0;

	SecondController->Trigger = 0;
	SecondController->AxisX = 0;
	SecondController->AxisY = 0;

	SecondController->Buttons = 0;

	if (GamePadConnected) {
		MyXInputGetState(0, &myPState);

		//Controller 1
		FirstController->Trigger = ConvAxis(myPState.Gamepad.bLeftTrigger * 0.003921568627451);
		FirstController->AxisX = ConvAxis(myPState.Gamepad.sThumbLX * 0.00003051758);
		FirstController->AxisY = ConvAxis(myPState.Gamepad.sThumbLY * 0.00003051758);

		if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_BACK)
			FirstController->Buttons += SYS_BTN;

		if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)
			FirstController->Buttons += GRIP_BTN;

		if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB)
			FirstController->Buttons += THUMB_BTN;

		if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
			FirstController->Buttons += MENU_BTN;

		if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
			FirstController->Buttons += A_BTN;

		if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
			FirstController->Buttons += B_BTN;

		//Controller 2
		SecondController->Trigger = ConvAxis(myPState.Gamepad.bRightTrigger * 0.003921568627451);
		SecondController->AxisX = ConvAxis(myPState.Gamepad.sThumbRX * 0.00003051758);
		SecondController->AxisY = ConvAxis(myPState.Gamepad.sThumbRY * 0.00003051758);

		if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_START)
			SecondController->Buttons += SYS_BTN;

		if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)
			SecondController->Buttons += GRIP_BTN;

		if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB)
			SecondController->Buttons += THUMB_BTN;

		if (myPState.Gamepad.wButtons &  XINPUT_GAMEPAD_Y)
			SecondController->Buttons += MENU_BTN;

		if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_X)
			SecondController->Buttons += A_BTN;

		if (myPState.Gamepad.wButtons & XINPUT_GAMEPAD_B)
			SecondController->Buttons += B_BTN;

		return TOVR_SUCCESS;
	}
	else {
		return TOVR_FAILURE;
	}
}

DLLEXPORT DWORD __stdcall SetControllerData(__in int dwIndex, __in unsigned char MotorSpeed)
{
	if (GamePadConnected) {
		XINPUT_VIBRATION Vibration;
		Vibration.wLeftMotorSpeed = 0;
		Vibration.wRightMotorSpeed = 0;
		switch (dwIndex)
		{
		case 1: {Vibration.wLeftMotorSpeed = MotorSpeed * 257; break; }
		case 2: {Vibration.wRightMotorSpeed = MotorSpeed * 257; break; }
			default: break;
		}

 		MyXInputSetState(0, &Vibration);
		return TOVR_SUCCESS;
	}
	else {
		return TOVR_FAILURE;
	}
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_DETACH:
		if (hDll != NULL) {
			FreeLibrary(hDll);
			hDll = nullptr;
		}
		break;
	}
	return TRUE;
}
