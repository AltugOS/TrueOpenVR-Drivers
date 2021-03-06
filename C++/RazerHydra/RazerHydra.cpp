#include "stdafx.h"
#include <Windows.h>
#include <Math.h>

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

DLLEXPORT DWORD __stdcall GetHMDData(__out THMD *HMD);
DLLEXPORT DWORD __stdcall GetControllersData(__out TController *FirstController, __out TController *SecondController);
DLLEXPORT DWORD __stdcall SetControllerData(__in int dwIndex, __in unsigned char MotorSpeed);

#define TOVR_SUCCESS 0
#define TOVR_FAILURE 1

#define GRIP_BTN	0x0001
#define THUMB_BTN	0x0002
#define A_BTN		0x0004
#define B_BTN		0x0008
#define MENU_BTN	0x0010
#define SYS_BTN		0x0020

#define SIXENSE_BUTTON_BUMPER   (0x01<<7)
#define SIXENSE_BUTTON_JOYSTICK (0x01<<8)
#define SIXENSE_BUTTON_1        (0x01<<5)
#define SIXENSE_BUTTON_2        (0x01<<6)
#define SIXENSE_BUTTON_3        (0x01<<3)
#define SIXENSE_BUTTON_4        (0x01<<4)
#define SIXENSE_BUTTON_START    (0x01<<0)

#define SIXENSE_SUCCESS 0
#define SIXENSE_FAILURE -1

typedef struct _sixenseControllerData {
	float pos[3];
	float rot_mat[3][3];
	float joystick_x;
	float joystick_y;
	float trigger;
	unsigned int buttons;
	unsigned char sequence_number;
	float rot_quat[4];
	unsigned short firmware_revision;
	unsigned short hardware_revision;
	unsigned short packet_type;
	unsigned short magnetic_frequency;
	int enabled;
	int controller_index;
	unsigned char is_docked;
	unsigned char which_hand;
	unsigned char hemi_tracking_enabled;
} sixenseControllerData;

typedef int(__cdecl *_sixenseInit)(void);
typedef int(__cdecl *_sixenseGetData)(int which, int index_back, sixenseControllerData *);

_sixenseInit sixenseInit;
_sixenseGetData sixenseGetData;

HMODULE hDll;
bool HydraInit = false, HydraConnected = false;

//Yaw, Pitch, Roll
float Ctrl1YPR[3], Ctrl2YPR[3];
//Offset YPR
float Ctrl1YPROffset[3] = { 0, 0, 0 }; float Ctrl2YPROffset[3] = { 0, 0, 0 };
//Pos calibration
double Ctrl1POSOffset[3] = { 0, 0, 0 }; double Ctrl2POSOffset[3] = {0, 0, 0};

//double HydraYawOffset = 0; 
double HydraPosYOffset = 0;

double RadToDeg(double r) {
	return r * (180 / 3.14159265358979323846); //180 / PI
}

double OffsetYPR(float f, float f2)
{
	f -= f2;
	if (f < -180) {
		f += 360;
	}
	else if (f > 180) {
		f -= 360;
	}

	return f;
}

void HydraStart()
{
	#ifdef _WIN64
		hDll = LoadLibrary("C:\\Program Files\\Sixence\\bin\\win64\\sixense_x64.dll");
	#else
		hDll = LoadLibrary("C:\\Program Files\\Sixence\\bin\\win32\\sixense.dll");
	#endif

		if (hDll != NULL) {

			sixenseInit = (_sixenseInit)GetProcAddress(hDll, "sixenseInit");
			sixenseGetData = (_sixenseGetData)GetProcAddress(hDll, "sixenseGetData");

			if (sixenseInit != NULL && sixenseGetData != NULL)
			{
				sixenseInit();
				HydraConnected = true;
			}
			else
			{
				hDll = NULL;
			}
			
	}
}

void SetCentering(int dwIndex)
{
	if (HydraConnected) {
		if (dwIndex == 1) {
			Ctrl1YPROffset[0] = Ctrl1YPR[0];
			Ctrl1YPROffset[1] = Ctrl1YPR[1];
			Ctrl1YPROffset[2] = Ctrl1YPR[2];
		}

		if (dwIndex == 2) {
			Ctrl2YPROffset[0] = Ctrl2YPR[0];
			Ctrl2YPROffset[1] = Ctrl2YPR[1];
			Ctrl2YPROffset[2] = Ctrl2YPR[2];
		}
	}
}

DLLEXPORT DWORD __stdcall GetHMDData(__out THMD *HMD)
{
	
	HMD->X = 0;
	//HMD->Y = HydraPosYOffset;
	HMD->Y = 0;
	HMD->Z = 0;

	//Load library with rotation for HMD
	HMD->Yaw = 0;
	HMD->Pitch = 0;
	//HMD->Pitch = OffsetYPR(0, HydraYawOffset); //Rotation with buttons like on PavlovVR. For games without button rotation
	HMD->Roll = 0;

	if (HydraConnected) {
		return TOVR_SUCCESS;
	}
	else {
		return TOVR_FAILURE;
	}
}

sixenseControllerData HydraController;
double SinR, CosR, SinP, SinY, CosY;
bool ctrlsInitCentring = false;


DLLEXPORT DWORD __stdcall GetControllersData(__out TController *FirstController, __out TController *SecondController)
{
	if (HydraInit == false)
	{
		HydraInit = true;
		HydraStart();
	}

	//Controller 1
	FirstController->X = 0;
	FirstController->Y = 0;
	FirstController->Z = 0;

	FirstController->Yaw = 0;
	FirstController->Pitch = 0;
	FirstController->Roll = 0;

	FirstController->Buttons = 0;
	FirstController->Trigger = 0;
	FirstController->AxisX = 0;
	FirstController->AxisY = 0;

	//Controller 2
	SecondController->X = 0;
	SecondController->Y = 0;
	SecondController->Z = 0;

	SecondController->Yaw = 0;
	SecondController->Pitch = 0;
	SecondController->Roll = 0;

	SecondController->Buttons = 0;
	SecondController->Trigger = 0;
	SecondController->AxisX = 0;
	SecondController->AxisY = 0;

	if (HydraConnected) {
		
		
		//Controller1
		sixenseGetData(1, 0, &HydraController);

		if (HydraController.buttons & SIXENSE_BUTTON_3)
			HydraPosYOffset += 0.0033;

		if (HydraController.buttons & SIXENSE_BUTTON_1)
			HydraPosYOffset -= 0.0033;

		if ((HydraController.buttons & SIXENSE_BUTTON_1 || HydraController.buttons & SIXENSE_BUTTON_3) && (HydraController.buttons & SIXENSE_BUTTON_BUMPER))
			HydraPosYOffset = 0;

		FirstController->X = HydraController.pos[0] * 0.001 - Ctrl1POSOffset[0];
		FirstController->Y = HydraController.pos[1] * 0.001 - Ctrl1POSOffset[1] + HydraPosYOffset;
		FirstController->Z = HydraController.pos[2] * 0.001 - Ctrl1POSOffset[2];

		//Convert quaternion to yaw, pitch, roll
		//Roll
		SinR = 2.0 * (HydraController.rot_quat[0] * HydraController.rot_quat[1] + HydraController.rot_quat[2] * HydraController.rot_quat[3]);
		CosR = 1.0 - 2.0 * (HydraController.rot_quat[1] * HydraController.rot_quat[1] + HydraController.rot_quat[2] * HydraController.rot_quat[2]);
		FirstController->Yaw = RadToDeg(atan2(SinR, CosR));
		//Pitch
		SinP = 2.0 * (HydraController.rot_quat[0] * HydraController.rot_quat[2] - HydraController.rot_quat[3] * HydraController.rot_quat[1]);
		if (fabs(SinP) >= 1)
			FirstController->Pitch = RadToDeg(copysign(3.14159265358979323846 / 2, SinP)); // use 90 degrees if out of range
		else
			FirstController->Pitch = RadToDeg(asin(SinP));
		//Yaw
		SinY = 2.0 * (HydraController.rot_quat[0] * HydraController.rot_quat[3] + HydraController.rot_quat[1] * HydraController.rot_quat[2]);
		CosY = 1.0 - 2.0 * (HydraController.rot_quat[2] * HydraController.rot_quat[2] + HydraController.rot_quat[3] * HydraController.rot_quat[3]);
		FirstController->Roll = RadToDeg(atan2(SinY, CosY));
		//end convert

		//For centring
		Ctrl1YPR[0] = FirstController->Yaw;
		Ctrl1YPR[1] = FirstController->Pitch;
		Ctrl1YPR[2] = FirstController->Roll;

		//Offset YPR
		FirstController->Yaw = OffsetYPR(FirstController->Yaw, Ctrl1YPROffset[0]);
		FirstController->Pitch = OffsetYPR(FirstController->Pitch, Ctrl1YPROffset[1]) * -1; //HydraYawOffset 
		FirstController->Roll = OffsetYPR(FirstController->Roll, Ctrl1YPROffset[2]) * -1;

		//Buttons
		if (HydraController.buttons & SIXENSE_BUTTON_START)
			FirstController->Buttons += SYS_BTN;
		if (HydraController.buttons & SIXENSE_BUTTON_JOYSTICK)
			FirstController->Buttons += THUMB_BTN;
		if (HydraController.buttons & SIXENSE_BUTTON_4)
			FirstController->Buttons += MENU_BTN;
		if (HydraController.buttons & SIXENSE_BUTTON_BUMPER)
			FirstController->Buttons += GRIP_BTN;

		//Trigger
		FirstController->Trigger = HydraController.trigger;

		//Stick
		FirstController->AxisX = HydraController.joystick_x;
		FirstController->AxisY = HydraController.joystick_y;
		//end controller 1


		//Controller 2
		sixenseGetData(0, 1, &HydraController);
		SecondController->X = HydraController.pos[0] * 0.001 - Ctrl2POSOffset[0];
		SecondController->Y = HydraController.pos[1] * 0.001 - Ctrl2POSOffset[1] + HydraPosYOffset;
		SecondController->Z = HydraController.pos[2] * 0.001 - Ctrl2POSOffset[2];

		//Convert quaternion to yaw, pitch, roll
		//Roll
		SinR = 2.0 * (HydraController.rot_quat[0] * HydraController.rot_quat[1] + HydraController.rot_quat[2] * HydraController.rot_quat[3]);
		CosR = 1.0 - 2.0 * (HydraController.rot_quat[1] * HydraController.rot_quat[1] + HydraController.rot_quat[2] * HydraController.rot_quat[2]);
		SecondController->Yaw = RadToDeg(atan2(SinR, CosR));
		//Pitch
		SinP = 2.0 * (HydraController.rot_quat[0] * HydraController.rot_quat[2] - HydraController.rot_quat[3] * HydraController.rot_quat[1]);
		if (fabs(SinP) >= 1)
			SecondController->Pitch = RadToDeg(copysign(3.14159265358979323846 / 2, SinP)); // use 90 degrees if out of range
		else
			SecondController->Pitch = RadToDeg(asin(SinP));
		//Yaw
		SinY = 2.0 * (HydraController.rot_quat[0] * HydraController.rot_quat[3] + HydraController.rot_quat[1] * HydraController.rot_quat[2]);
		CosY = 1.0 - 2.0 * (HydraController.rot_quat[2] * HydraController.rot_quat[2] + HydraController.rot_quat[3] * HydraController.rot_quat[3]);
		SecondController->Roll = RadToDeg(atan2(SinY, CosY));
		//end convert
		
		//For centring
		Ctrl2YPR[0] = SecondController->Yaw;
		Ctrl2YPR[1] = SecondController->Pitch;
		Ctrl2YPR[2] = SecondController->Roll;

		//Offset YPR
		SecondController->Yaw = OffsetYPR(SecondController->Yaw, Ctrl2YPROffset[0]);
		SecondController->Pitch = OffsetYPR(SecondController->Pitch, Ctrl2YPROffset[1]) * -1; // HydraYawOffset
		SecondController->Roll = OffsetYPR(SecondController->Roll, Ctrl2YPROffset[2]) * -1;

		//Buttons
		if (HydraController.buttons & SIXENSE_BUTTON_START)
			SecondController->Buttons += SYS_BTN;
		if (HydraController.buttons & SIXENSE_BUTTON_JOYSTICK)
			SecondController->Buttons += THUMB_BTN;
		if (HydraController.buttons & SIXENSE_BUTTON_4)
			SecondController->Buttons += MENU_BTN;
		if (HydraController.buttons & SIXENSE_BUTTON_BUMPER)
			SecondController->Buttons += GRIP_BTN;

		/*if (HydraController.buttons & SIXENSE_BUTTON_1)
			if (HydraYawOffset >= -177)
				HydraYawOffset -= 3;

		if (HydraController.buttons & SIXENSE_BUTTON_2)
			if (HydraYawOffset < 177)
				HydraYawOffset += 3;


		if (((HydraController.buttons & SIXENSE_BUTTON_1) || (HydraController.buttons & SIXENSE_BUTTON_2)) && (HydraController.buttons & SIXENSE_BUTTON_BUMPER))
			HydraYawOffset = 0;*/

		//Trigger
		SecondController->Trigger = HydraController.trigger;

		//Stick
		SecondController->AxisX = HydraController.joystick_x;
		SecondController->AxisY = HydraController.joystick_y;
		//end controller 2

		//Bad POS calibration, it will be necessary to do as here - https://github.com/ValveSoftware/driver_hydra/blob/master/drivers/driver_hydra/driver_hydra.cpp
		if (!((FirstController->Buttons & GRIP_BTN) && (SecondController->Buttons & GRIP_BTN)))
			if ((FirstController->Buttons & SYS_BTN) && (SecondController->Buttons & SYS_BTN))
			{
				THMD MyHMD;
				GetHMDData(&MyHMD);
				Ctrl1POSOffset[0] = FirstController->X - 0.185; //37 cm between hands   0.0185
				if (SecondController->Y > MyHMD.Y)
				{
					Ctrl1POSOffset[1] = FirstController->Y - (FirstController->Y - MyHMD.Y) - 0.015;
				}
				else {
					Ctrl1POSOffset[1] = FirstController->Y - 0.015;
				}

				Ctrl1POSOffset[2] = FirstController->Z; //  - 0.01


				Ctrl2POSOffset[0] = SecondController->X + 0.185;
				if (SecondController->Y > MyHMD.Y)
				{
					Ctrl2POSOffset[1] = SecondController->Y - (SecondController->Y - MyHMD.Y) - 0.015;
				}
				else {
					Ctrl2POSOffset[1] = SecondController->Y - 0.015;
				}

				Ctrl2POSOffset[2] = SecondController->Z;
			}

		//Centring on start
		if (ctrlsInitCentring == false)
			if (FirstController->Yaw != 0 || FirstController->Pitch != 0 || FirstController->Roll != 0) {
				SetCentering(1);
				SetCentering(2);
				ctrlsInitCentring = true;
		}
		
		return TOVR_SUCCESS;
	}
	else {
		return TOVR_FAILURE;
	}
}

DLLEXPORT DWORD __stdcall SetControllerData(__in int dwIndex, __in unsigned char MotorSpeed)
{
	return TOVR_SUCCESS;
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	//case DLL_PROCESS_ATTACH:
	//case DLL_THREAD_ATTACH:
	//case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		if (hDll != NULL) {
			//sixenseExit();//?
			FreeLibrary(hDll);
			hDll = nullptr;
		}
		break;
	}
	return TRUE;
}

