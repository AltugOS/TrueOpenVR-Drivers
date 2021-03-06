#include "stdafx.h"
#include <thread> 
#include <winsock2.h>
#pragma comment (lib, "WSock32.Lib")
#include <vector>
#include <math.h>
#include <sstream>
#pragma warning(disable:4996) 

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

struct TControllerPacket {
	BYTE Index;
	double Yaw;
	double Pitch;
	double Roll;
	double JoystickX;
	double JoystickY;
	WORD Buttons;
	BYTE Trigger;
};

SOCKET socketS, socketS2;
int bytes_read;
struct sockaddr_in from;
int fromlen;
bool SocketsActivated = false, ControllersInit = true;
bool bKeepReading = false;

double Ctrl1Offset[3] = { 0, 0, 0 }; 
double Ctrl2Offset[3] = { 0, 0, 0 };
TControllerPacket Ctrl1Packet, Ctrl2Packet;

std::thread *pCtrlsReadThread = NULL;

char *sp;
char DataBuff[1024];
std::string item;
std::vector<std::string> parsed;

double OffsetYPR(double f, double f2)
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

void __stdcall SetCentering(__in int dwIndex)
{
	if (SocketsActivated) {
		if (dwIndex == 1) {
			if (Ctrl1Packet.Index == 1) {
				Ctrl1Offset[0] = Ctrl1Packet.Yaw;
				Ctrl1Offset[1] = Ctrl1Packet.Pitch;
				Ctrl1Offset[2] = Ctrl1Packet.Roll;
			}
			else {
				Ctrl2Offset[0] = Ctrl1Packet.Yaw;
				Ctrl2Offset[1] = Ctrl1Packet.Pitch;
				Ctrl2Offset[2] = Ctrl1Packet.Roll;
			}
		}

		if (dwIndex == 2) {
			if (Ctrl2Packet.Index == 1) {
				Ctrl1Offset[0] = Ctrl2Packet.Yaw;
				Ctrl1Offset[1] = Ctrl2Packet.Pitch;
				Ctrl1Offset[2] = Ctrl2Packet.Roll;
			}
			else {
				Ctrl2Offset[0] = Ctrl2Packet.Yaw;
				Ctrl2Offset[1] = Ctrl2Packet.Pitch;
				Ctrl2Offset[2] = Ctrl2Packet.Roll;
			}
		}
	}
}

void ReadControllersData() {
	memset(&Ctrl1Packet, 0, sizeof(Ctrl1Packet));
	memset(&Ctrl2Packet, 0, sizeof(Ctrl2Packet));
	memset(&DataBuff, 0, sizeof(DataBuff));
	memset(&parsed, 0, sizeof(parsed));

	while (SocketsActivated) {

			//Controller 1
			bKeepReading = true;
			while (bKeepReading) {

				bytes_read = recvfrom(socketS, DataBuff, sizeof(DataBuff), 0, (sockaddr*)&from, &fromlen);

				if (bytes_read > 0) {

					sp = strtok(DataBuff, ";");
					while (sp) {
						item = std::string(sp);
						parsed.push_back(item);
						sp = strtok(NULL, ";");
					}

					Ctrl1Packet.Yaw = std::stof(parsed[5]);
					Ctrl1Packet.Pitch = std::stof(parsed[4]) * -1;
					Ctrl1Packet.Roll = std::stof(parsed[3]) * -1;

					Ctrl1Packet.Buttons = 0;
					Ctrl1Packet.Trigger = 0;

					if (parsed[14] == "1") Ctrl1Packet.Buttons = GRIP_BTN;
					if (parsed[14] == "2") Ctrl1Packet.Buttons = SYS_BTN;
					if (parsed[14] == "4") Ctrl1Packet.Buttons = MENU_BTN;
					if (parsed[14] == "8") Ctrl1Packet.Buttons = THUMB_BTN;
					if (parsed[14] == "16") Ctrl1Packet.Trigger = 1;

					Ctrl1Packet.JoystickX = 0;
					Ctrl1Packet.JoystickY = 0;
					Ctrl1Packet.JoystickX = std::stof(parsed[17]);
					Ctrl1Packet.JoystickY = std::stof(parsed[18]) * -1;

					if (parsed[0] == "L#TMST") {
						Ctrl1Packet.Index = 1; 
					} else { 
						Ctrl1Packet.Index = 2; 
					}

					if (parsed[12] == "1")
						SetCentering(1);

					parsed.clear();
				}
				else {
					bKeepReading = false;
				}
			}

			//Controller 2
			bKeepReading = true;
			while (bKeepReading) {

				bytes_read = recvfrom(socketS2, DataBuff, sizeof(DataBuff), 0, (sockaddr*)&from, &fromlen);

				if (bytes_read > 0) {

					sp = strtok(DataBuff, ";");
					while (sp) {
						item = std::string(sp);
						parsed.push_back(item);
						sp = strtok(NULL, ";");
					}

					Ctrl2Packet.Yaw = std::stof(parsed[5]);
					Ctrl2Packet.Pitch = std::stof(parsed[4]) * -1;
					Ctrl2Packet.Roll = std::stof(parsed[3]) * -1;

					Ctrl2Packet.Buttons = 0;
					Ctrl2Packet.Trigger = 0;

					if (parsed[14] == "1") Ctrl2Packet.Buttons = GRIP_BTN;
					if (parsed[14] == "2") Ctrl2Packet.Buttons = SYS_BTN;
					if (parsed[14] == "4") Ctrl2Packet.Buttons = MENU_BTN;
					if (parsed[14] == "8") Ctrl2Packet.Buttons = THUMB_BTN;
					if (parsed[14] == "16") Ctrl2Packet.Trigger = 1;

					Ctrl2Packet.JoystickX = 0;
					Ctrl2Packet.JoystickY = 0;
					Ctrl2Packet.JoystickX = std::stof(parsed[17]);
					Ctrl2Packet.JoystickY = std::stof(parsed[18]) * -1;

					if (parsed[0] == "L#TMST") {
						Ctrl2Packet.Index = 1;
					}
					else {
						Ctrl2Packet.Index = 2;
					}

					if (parsed[12] == "1")
						SetCentering(2);

					parsed.clear();
				}
				else {
					bKeepReading = false;
				}
			}
	}
}

void ControllersStart() {
	Ctrl1Offset[0] = 0;
	Ctrl1Offset[1] = 0;
	Ctrl1Offset[2] = 0;

	Ctrl2Offset[0] = 0;
	Ctrl2Offset[1] = 0;
	Ctrl2Offset[2] = 0;

	//Controller 1
	WSADATA wsaData;
	int iResult;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult == 0) {
		struct sockaddr_in local;
		fromlen = sizeof(from);
		local.sin_family = AF_INET;
		local.sin_port = htons(5555);
		local.sin_addr.s_addr = INADDR_ANY;

		socketS = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

		u_long nonblocking_enabled = TRUE;
		ioctlsocket(socketS, FIONBIO, &nonblocking_enabled);

		if (socketS != INVALID_SOCKET) {

			iResult = bind(socketS, (sockaddr*)&local, sizeof(local));

			if (iResult != SOCKET_ERROR) {
				SocketsActivated = true;
				//
			}
			else {
				WSACleanup();
				SocketsActivated = false;
			}

		}
		else {
			WSACleanup();
			SocketsActivated = false;
		}
	}
	else
	{
		WSACleanup();
		SocketsActivated = false;
	}

	//Controller 2
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult == 0) {
		struct sockaddr_in local;
		fromlen = sizeof(from);
		local.sin_family = AF_INET;
		local.sin_port = htons(5556);
		local.sin_addr.s_addr = INADDR_ANY;

		socketS2 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

		u_long nonblocking_enabled = TRUE;
		ioctlsocket(socketS2, FIONBIO, &nonblocking_enabled);

		if (socketS2 != INVALID_SOCKET) {

			iResult = bind(socketS2, (sockaddr*)&local, sizeof(local));

			if (iResult != SOCKET_ERROR) {
				SocketsActivated = true;
				//
			}
			else {
				WSACleanup();
				SocketsActivated = false;
			}

		}
		else {
			WSACleanup();
			SocketsActivated = false;
		}
	}
	else
	{
		WSACleanup();
		SocketsActivated = false;
	}


	if (SocketsActivated) {
		pCtrlsReadThread = new std::thread(ReadControllersData);
	}
}

DLLEXPORT DWORD __stdcall GetControllersData(__out TController *FirstController, __out TController *SecondController)
{
	if (ControllersInit) {
		ControllersInit = false;
		ControllersStart();
	}


	//Controller 1
	FirstController->X = -0.02;
	FirstController->Y = -0.05;
	FirstController->Z = -0.1;

	FirstController->Yaw = 0;
	FirstController->Pitch = 0;
	FirstController->Roll = 0;

	FirstController->Buttons = 0;
	FirstController->Trigger = 0;
	FirstController->AxisX = 0;
	FirstController->AxisY = 0;

	//Controller 2
	SecondController->X = 0.02;
	SecondController->Y = -0.05;
	SecondController->Z = -0.1;

	SecondController->Yaw = 0;
	SecondController->Pitch = 0;
	SecondController->Roll = 0;

	SecondController->Buttons = 0;
	SecondController->Trigger = 0;
	SecondController->AxisX = 0;
	SecondController->AxisY = 0;

	if (SocketsActivated) {
		//Controller 1

		if (Ctrl1Packet.Index == 1) {
			FirstController->Yaw = OffsetYPR(Ctrl1Packet.Yaw, Ctrl1Offset[0]);
			FirstController->Pitch = OffsetYPR(Ctrl1Packet.Pitch, Ctrl1Offset[1]);
			FirstController->Roll = OffsetYPR(Ctrl1Packet.Roll, Ctrl1Offset[2]);

			FirstController->Buttons = Ctrl1Packet.Buttons;
			FirstController->Trigger = Ctrl1Packet.Trigger;
			FirstController->AxisX = Ctrl1Packet.JoystickX;
			FirstController->AxisY = Ctrl1Packet.JoystickY;
		}
		if (Ctrl1Packet.Index == 2) {
			SecondController->Yaw = OffsetYPR(Ctrl1Packet.Yaw, Ctrl1Offset[0]);
			SecondController->Pitch = OffsetYPR(Ctrl1Packet.Pitch, Ctrl1Offset[1]);
			SecondController->Roll = OffsetYPR(Ctrl1Packet.Roll, Ctrl1Offset[2]);

			SecondController->Buttons = Ctrl1Packet.Buttons;
			SecondController->Trigger = Ctrl1Packet.Trigger;
			SecondController->AxisX = Ctrl1Packet.JoystickX;
			SecondController->AxisY = Ctrl1Packet.JoystickY;
		}
		//if index == 0 then none using

		//Controller2
		if (Ctrl2Packet.Index == 1) {
			FirstController->Yaw = OffsetYPR(Ctrl2Packet.Yaw, Ctrl1Offset[0]);
			FirstController->Pitch = OffsetYPR(Ctrl2Packet.Pitch, Ctrl1Offset[1]);
			FirstController->Roll = OffsetYPR(Ctrl2Packet.Roll, Ctrl1Offset[2]);

			FirstController->Buttons = Ctrl2Packet.Buttons;
			FirstController->Trigger = Ctrl2Packet.Trigger;
			FirstController->AxisX = Ctrl2Packet.JoystickX;
			FirstController->AxisY = Ctrl2Packet.JoystickY;
		}
		if (Ctrl2Packet.Index == 2) {
			SecondController->Yaw = OffsetYPR(Ctrl2Packet.Yaw, Ctrl2Offset[0]);
			SecondController->Pitch = OffsetYPR(Ctrl2Packet.Pitch, Ctrl2Offset[1]);
			SecondController->Roll = OffsetYPR(Ctrl2Packet.Roll, Ctrl2Offset[2]);

			SecondController->Buttons = Ctrl2Packet.Buttons;
			SecondController->Trigger = Ctrl2Packet.Trigger;
			SecondController->AxisX = Ctrl2Packet.JoystickX;
			SecondController->AxisY = Ctrl2Packet.JoystickY;
		}
	}

	if (SocketsActivated) {
		return TOVR_SUCCESS;
	}else {
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
){
	switch (ul_reason_for_call)
	{
		//case DLL_PROCESS_ATTACH:
		//case DLL_THREAD_ATTACH:
		//case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH: {
			if (SocketsActivated) {
				SocketsActivated = false;

				if (pCtrlsReadThread) {
					pCtrlsReadThread->join();
					delete pCtrlsReadThread;
					pCtrlsReadThread = nullptr;
				}

				closesocket(socketS);
				WSACleanup();
			}
			break;
		}
	}
		

	return TRUE;
}