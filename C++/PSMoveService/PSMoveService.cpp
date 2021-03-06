#include "stdafx.h"
#include <windows.h>
#include <thread>
#include "PSMoveService/PSMoveClient_CAPI.h"

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

typedef struct
{
	double yaw, pitch, roll;
} DeviceRotation;

DeviceRotation hmdOffset, ctrl1Offset, ctrl2Offset;
bool hmdInitCentring = false, controllersInitCentring = false;

PSMControllerList controllerList;
PSMHmdList hmdList;
PSMVector3f hmdPos, ctrl1Pos, ctrl2Pos;
PSMQuatf hmdRot, ctrl1Rot, ctrl2Rot;
PSMController *ctrl1;
PSMController *ctrl2;

static const float k_fScalePSMoveAPIToMeters = 0.01f; // psmove driver in cm

bool PSMConnected = false, PSMoveServiceInit = false;
std::thread *pPSMUpdatethread = NULL;

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

double RadToDeg(double r) {
	return r * (180 / 3.14159265358979323846); //180 / PI
}

DeviceRotation QuatToYPR(double QuatW, double QuatX, double QuatY, double QuatZ) {
	// roll (x-axis rotation)
	DeviceRotation OutRot;
	double sinr_cosp = 2.0 * (QuatW * QuatX + QuatY * QuatZ);
	double cosr_cosp = 1.0 - 2.0 * (QuatX * QuatX + QuatY * QuatY);
	OutRot.roll = RadToDeg(atan2(sinr_cosp, cosr_cosp));

	// pitch (y-axis rotation)
	double sinp = 2.0 * (QuatW * QuatY - QuatZ * QuatX);
	if (fabs(sinp) >= 1)
		OutRot.pitch = RadToDeg(copysign(3.14159265358979323846 / 2, sinp)); // use 90 degrees if out of range
	else
		OutRot.pitch = RadToDeg(asin(sinp));

	// yaw (z-axis rotation)
	double siny_cosp = 2.0 * (QuatW * QuatZ + QuatX * QuatY);
	double cosy_cosp = 1.0 - 2.0 * (QuatY * QuatY + QuatZ * QuatZ);
	OutRot.yaw = RadToDeg(atan2(siny_cosp, cosy_cosp));

	return OutRot;
}

void PSMoveServiceUpdate()
{
	while (PSMConnected) {
		PSM_Update();

		if (hmdList.count > 0) {
			PSM_GetHmdPosition(hmdList.hmd_id[0], &hmdPos);
			PSM_GetHmdOrientation(hmdList.hmd_id[0], &hmdRot);
		}

		if (controllerList.count > 0) {
			PSM_GetControllerPosition(controllerList.controller_id[0], &ctrl1Pos);
			PSM_GetControllerOrientation(controllerList.controller_id[0], &ctrl1Rot);
			ctrl1 = PSM_GetController(controllerList.controller_id[0]);
		}
			
		if (controllerList.count > 1) {
			PSM_GetControllerPosition(controllerList.controller_id[1], &ctrl2Pos);
			PSM_GetControllerOrientation(controllerList.controller_id[1], &ctrl2Rot);
			ctrl2 = PSM_GetController(controllerList.controller_id[1]);
		}
	}
}

void ConnectToPSMoveService()
{
	if (PSM_Initialize(PSMOVESERVICE_DEFAULT_ADDRESS, PSMOVESERVICE_DEFAULT_PORT, PSM_DEFAULT_TIMEOUT) == PSMResult_Success)
	{
		memset(&controllerList, 0, sizeof(PSMControllerList));
		PSM_GetControllerList(&controllerList, PSM_DEFAULT_TIMEOUT);

		memset(&hmdList, 0, sizeof(PSMHmdList));
		PSM_GetHmdList(&hmdList, PSM_DEFAULT_TIMEOUT);
	}

	unsigned int data_stream_flags =
		PSMControllerDataStreamFlags::PSMStreamFlags_includePositionData |
		PSMControllerDataStreamFlags::PSMStreamFlags_includePhysicsData |
		PSMControllerDataStreamFlags::PSMStreamFlags_includeCalibratedSensorData |
		PSMControllerDataStreamFlags::PSMStreamFlags_includeRawTrackerData;

	//HMD
	if (hmdList.count > 0) 
		if (PSM_AllocateHmdListener(hmdList.hmd_id[0]) == PSMResult_Success && PSM_StartHmdDataStream(hmdList.hmd_id[0], data_stream_flags, PSM_DEFAULT_TIMEOUT) == PSMResult_Success)
			PSMConnected = true;
	
	//Controller1
	if (controllerList.count > 0) 
		if (PSM_AllocateControllerListener(controllerList.controller_id[0]) == PSMResult_Success && PSM_StartControllerDataStream(controllerList.controller_id[0], data_stream_flags, PSM_DEFAULT_TIMEOUT) == PSMResult_Success)
			PSMConnected = true;
	
	//Controller2
	if (controllerList.count > 1)
		if (PSM_AllocateControllerListener(controllerList.controller_id[1]) == PSMResult_Success && PSM_StartControllerDataStream(controllerList.controller_id[1], data_stream_flags, PSM_DEFAULT_TIMEOUT) == PSMResult_Success)
			PSMConnected = true;

	if (PSMConnected)
		pPSMUpdatethread = new std::thread(PSMoveServiceUpdate);
}

DLLEXPORT DWORD __stdcall GetHMDData(__out THMD *HMD)
{
	if (PSMoveServiceInit == false) {
		PSMoveServiceInit = true;
		ConnectToPSMoveService();
	}

	if (PSMConnected && hmdList.count > 0) {

		HMD->X = hmdPos.x * k_fScalePSMoveAPIToMeters;
		HMD->Y = hmdPos.y * k_fScalePSMoveAPIToMeters;
		HMD->Z = hmdPos.z * k_fScalePSMoveAPIToMeters;
		
		DeviceRotation OutRot = { 0, 0, 0 };
		OutRot = QuatToYPR(hmdRot.w, hmdRot.x, hmdRot.y, hmdRot.z);

		if (hmdInitCentring == false)
		{
			hmdOffset.yaw = HMD->Yaw;
			hmdOffset.pitch = HMD->Pitch;
			hmdOffset.roll = HMD->Roll;
			hmdInitCentring = true;
		}
		
		HMD->Yaw = OffsetYPR(OutRot.yaw, hmdOffset.yaw);
		HMD->Pitch = OffsetYPR(OutRot.pitch, hmdOffset.pitch);
		HMD->Roll = OffsetYPR(OutRot.roll, hmdOffset.roll);

		return TOVR_SUCCESS;
	}
	else {
		HMD->X = 0;
		HMD->Y = 0;
		HMD->Z = 0;
		HMD->Yaw = 0;
		HMD->Pitch = 0;
		HMD->Roll = 0;

		return TOVR_FAILURE;
	}
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
	if (PSMoveServiceInit == false) {
		PSMoveServiceInit = true;
		ConnectToPSMoveService();
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

	if (PSMConnected && controllerList.count > 0) {
		FirstController->X = ctrl1Pos.x * k_fScalePSMoveAPIToMeters;
		FirstController->Y = ctrl1Pos.y * k_fScalePSMoveAPIToMeters;
		FirstController->Z = ctrl1Pos.z * k_fScalePSMoveAPIToMeters;

		DeviceRotation OutRot = { 0, 0, 0 };
		OutRot = QuatToYPR(ctrl1Rot.w, ctrl1Rot.x, ctrl1Rot.y, ctrl1Rot.z);

		if (controllersInitCentring == false)
		{
			ctrl1Offset.yaw = OutRot.yaw;
			ctrl1Offset.pitch = OutRot.pitch;
			ctrl1Offset.roll = OutRot.roll;
		}

		FirstController->Yaw = OffsetYPR(OutRot.yaw, ctrl1Offset.yaw);
		FirstController->Pitch = OffsetYPR(OutRot.pitch, ctrl1Offset.pitch);
		FirstController->Roll = OffsetYPR(OutRot.roll, ctrl1Offset.roll);

		if (ctrl1->ControllerState.PSMoveState.MoveButton == PSMButtonState_PRESSED)
			FirstController->Buttons += THUMB_BTN;
		if (ctrl1->ControllerState.PSMoveState.StartButton == PSMButtonState_PRESSED)
			FirstController->Buttons += GRIP_BTN;
		if (ctrl1->ControllerState.PSMoveState.SelectButton == PSMButtonState_PRESSED)
			FirstController->Buttons += MENU_BTN;

		if (ctrl1->ControllerState.PSMoveState.SquareButton == PSMButtonState_PRESSED) //UP
			FirstController->AxisX = 1;
		if (ctrl1->ControllerState.PSMoveState.TriangleButton == PSMButtonState_PRESSED) //Down
			FirstController->AxisX = -1;

		if (ctrl1->ControllerState.PSMoveState.CrossButton == PSMButtonState_PRESSED) //Left
			FirstController->AxisY = -1;

		if (ctrl1->ControllerState.PSMoveState.CircleButton == PSMButtonState_PRESSED) //Right
			FirstController->AxisY = 1;

		FirstController->Trigger = ConvAxis(ctrl1->ControllerState.PSMoveState.TriggerValue * 0.003921568627451);
	}
	
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
	

	//Controller 2
	if (PSMConnected && controllerList.count > 1) {
		SecondController->X = ctrl2Pos.x * k_fScalePSMoveAPIToMeters;
		SecondController->Y = ctrl2Pos.y * k_fScalePSMoveAPIToMeters;
		SecondController->Z = ctrl2Pos.z * k_fScalePSMoveAPIToMeters;

		DeviceRotation OutRot = { 0, 0, 0 };
		OutRot = QuatToYPR(ctrl2Rot.w, ctrl2Rot.x, ctrl2Rot.y, ctrl2Rot.z);

		if (controllersInitCentring == false)
		{
			ctrl2Offset.yaw = OutRot.yaw;
			ctrl2Offset.pitch = OutRot.pitch;
			ctrl2Offset.roll = OutRot.roll;
			controllersInitCentring = true;
		}

		SecondController->Yaw = OffsetYPR(OutRot.yaw, ctrl2Offset.yaw);
		SecondController->Pitch = OffsetYPR(OutRot.pitch, ctrl2Offset.pitch);
		SecondController->Roll = OffsetYPR(OutRot.roll, ctrl2Offset.roll);

		if (ctrl2->ControllerState.PSMoveState.MoveButton == PSMButtonState_PRESSED)
			SecondController->Buttons += THUMB_BTN;
		if (ctrl2->ControllerState.PSMoveState.StartButton == PSMButtonState_PRESSED)
			SecondController->Buttons += MENU_BTN;
		if (ctrl2->ControllerState.PSMoveState.SelectButton == PSMButtonState_PRESSED)
			SecondController->Buttons += GRIP_BTN;

		if (ctrl2->ControllerState.PSMoveState.SquareButton == PSMButtonState_PRESSED) //UP
			SecondController->AxisX = 1;
		if (ctrl2->ControllerState.PSMoveState.TriangleButton == PSMButtonState_PRESSED) //Down
			SecondController->AxisX = -1;

		if (ctrl2->ControllerState.PSMoveState.CrossButton == PSMButtonState_PRESSED) //Left
			SecondController->AxisY = -1;

		if (ctrl2->ControllerState.PSMoveState.CircleButton == PSMButtonState_PRESSED) //Right
			SecondController->AxisY = 1;

		SecondController->Trigger = ConvAxis(ctrl2->ControllerState.PSMoveState.TriggerValue * 0.003921568627451);
	}
	

	if (PSMConnected) {
		return TOVR_SUCCESS;
	} 
	else 
	{
		return TOVR_FAILURE;
	}
}

DLLEXPORT DWORD __stdcall SetControllerData(__in int dwIndex, __in unsigned char MotorSpeed)
{
	//Soon
	return TOVR_SUCCESS;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_DETACH:
		if (PSMConnected) {
			PSMConnected = false;
			if (pPSMUpdatethread) {
				pPSMUpdatethread->join();
				delete pPSMUpdatethread;
				pPSMUpdatethread = nullptr;
			}

			if (hmdList.count > 0)
			{
				PSM_StopHmdDataStream(hmdList.hmd_id[0], PSM_DEFAULT_TIMEOUT);
				PSM_FreeHmdListener(hmdList.hmd_id[0]);
			}

			if (controllerList.count > 0)
			{
				PSM_StopControllerDataStream(controllerList.controller_id[0], PSM_DEFAULT_TIMEOUT);
				PSM_FreeControllerListener(controllerList.controller_id[0]);
			}

			if (controllerList.count > 1)
			{
				PSM_StopControllerDataStream(controllerList.controller_id[1], PSM_DEFAULT_TIMEOUT);
				PSM_FreeControllerListener(controllerList.controller_id[1]);
			}

			PSM_Shutdown();
		}
		break;
	}
	return TRUE;
}