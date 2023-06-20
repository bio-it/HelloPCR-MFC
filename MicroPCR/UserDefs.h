#pragma once		// once compile

#include "stdafx.h"
#include <windows.h>
#include <vector>
using namespace std;

// Debug mode 를 동작시킬 경우, UI 에 Edit Box 에 특정 값들을 보여주도록 한다.
// #define DEBUG_MODE

// Grpah 를 보여주는 모드 설정
#define GRAPH_MODE

// Magneto 에 대한 EMULATOR 설정
//#define EMULATOR  //KJD


// Device VID, PID
#define LS4550EK_VID			0x04D8
#define LS4550EK_PID			0x0041

// Capacity max device number

#define DEVICE_MAX				0x08

// Message Map
#define WM_SET_SERIAL			WM_USER + 0x01

#define TIMER_MAIN				0x01

// Timer Duration
#define TIMER_DURATION			50

#define CONSTANTS_PATH			L"PCRConstants"

// Recent Protocol File Path
#define RECENT_PATH				L"Recent_Protocol.txt"

#define PID_CONSTANTS_MAX		5

static const CString PID_TABLE_COLUMNS[5] = { L"Start Temp", L"Target Temp", L"Kp", L"Kd", L"Ki" };
static const CString RESULT_TABLE_COLUMNS[2] = { L"Target", L"Result" };
static const CString HISTORY_TABLE_COLUMNS[5] = { L"Date Time", L"Target", L"Filter", L"CT Value", L"Result" };
static const CString PROTOCOL_TABLE_COLUMNS[3] = { L"No", L"Temp", L"Time"};

// MAX Protocol Name
#define MAX_PROTOCOL_LENGTH		50


// #define USE_CHART

// Database 버전
#define VERSION_CONSTANTS	L"V1"
#define VERSION_HISTORY		L"V1"
#define VERSION_PROTOCOL	L"V1"
#define VERSION_MAGNETO		L"V1"


// 현재 이 값을 사용을 하고 있지만, 기기로부터 저장된 값이 받아오게 되면
// 지울 예정
#define	A_VAL					8.314242955712037e-004
#define B_VAL					2.620794578770541e-004
#define C_VAL					1.368534674434736e-007

#define Rref					1800.0
#define K						273.15


#define SET_BUTTON_IMAGE(button, img)	((CButton*)GetDlgItem(button))->SetBitmap((HBITMAP)img)

#define FAN_STOP_TEMPDIF		-2.0

#define INTGRALMAX				2600.0
#define FL_RELATIVE_MAX			4000.0

class PID{
public:
	float startTemp;
	float targetTemp;
	float kp;
	float ki;
	float kd;

	PID()
		: startTemp(0.)
		, targetTemp(0.)
		, kp(0.)
		, ki(0.)
		, kd(0.)
	{
	}

	PID(float startTemp, float targetTemp, float kp,
		float ki, float kd)
		: startTemp(startTemp)
		, targetTemp(targetTemp)
		, kp(kp)
		, ki(ki)
		, kd(kd)			
	{
	}
};

typedef struct _ACTION
{
	CString Label;			// Action Label or GOTO or SHOT
	double Temp;			// Target Temperature
	double Time;			// Duration that the temperature is stable over

} Action;

class Protocol {
public:
	CString protocolName;
	bool useFam;
	bool useHex;
	bool useRox;
	bool useCY5;

	CString labelFam;
	CString labelHex;
	CString labelRox;
	CString labelCY5;

	float ctFam;
	float ctHex;
	float ctRox;
	float ctCY5;

	// 220221 KBH filter threshold values
	float thresholdFam;
	float thresholdHex;
	float thresholdRox;
	float thresholdCY5;

	CString magnetoData;

	vector<Action> actionList;

	Protocol()
		: useFam(false)
		, useHex(false)
		, useRox(false)
		, useCY5(false)
		, protocolName(L"")
		, labelFam(L"")
		, labelHex(L"")
		, labelRox(L"")
		, labelCY5(L"")
		, ctFam(38.0)
		, ctHex(38.0)
		, ctRox(38.0)
		, ctCY5(38.0)
		, thresholdFam(5.05)
		, thresholdHex(4.47)
		, thresholdRox(5.0) // 220221 KBH defulat value
		, thresholdCY5(4.3)
		, magnetoData(L"")
	{
	}
};

class MagnetoProtocol {
public:
	CString protocolName;
	CString protocolData;

	MagnetoProtocol() 
		: protocolName(L"")
		, protocolData(L"")
	{
	}

	MagnetoProtocol(CString protocolName, CString protocolData)
		: protocolName(protocolName)
		, protocolData(protocolData)
	{
	}
};

class History {
public:
	CString date;
	CString target;
	CString filter;
	CString ctValue;
	CString result;

	History() 
		: date(L"")
		, target(L"")
		, filter(L"")
		, ctValue(L"")
		, result(L"")
	{
	}

	History(CString date, CString target, CString filter, CString ctValue, CString result)
		: date(date)
		, target(target)
		, filter(filter)
		, ctValue(ctValue)
		, result(result)
	{
	}
};

typedef enum _CMD
{
	CMD_READY = 0x00,
	CMD_PCR_RUN,
	CMD_PCR_STOP,
	CMD_FAN_ON,
	CMD_FAN_OFF,
	CMD_BOOTLOADER = 0x55
} CMD;

typedef enum _STATE
{
	STATE_READY = 0x00,
	STATE_RUNNING,
	STATE_PAN_RUNNING,
} STATE;

typedef struct _RxBuffer
{
	BYTE reserved;
	BYTE state;
	BYTE chamber_h;
	BYTE chamber_l;

	// calculated temperature
	BYTE chamber_temp_1;
	BYTE chamber_temp_2;
	BYTE chamber_temp_3;
	BYTE chamber_temp_4;

	BYTE photodiode_h;
	BYTE photodiode_l;
	BYTE currentError;
	
	// For request command
	BYTE request_data;

	BYTE targetArrival;

	BYTE reserved_for_64byte[52];
} RxBuffer;

typedef struct _TxBuffer
{
	BYTE reserved;

	BYTE cmd;

	// 올릴 온도 값
	BYTE currentTargetTemp;
	
	// for pid params
	BYTE startTemp;
	BYTE targetTemp;

	BYTE pid_p1;
	BYTE pid_p2;
	BYTE pid_p3;
	BYTE pid_p4;
	
	BYTE pid_i1;
	BYTE pid_i2;
	BYTE pid_i3;
	BYTE pid_i4;

	BYTE pid_d1;
	BYTE pid_d2;
	BYTE pid_d3;
	BYTE pid_d4;

	BYTE integralMax_1;
	BYTE integralMax_2;
	BYTE integralMax_3;
	BYTE integralMax_4;

	BYTE ledControl;
	BYTE led_wg;
	BYTE led_r;
	BYTE led_g;
	BYTE led_b;

	BYTE compensation;

	BYTE led_wg_pwm;
	BYTE led_r_pwm;
	BYTE led_g_pwm;
	BYTE led_b_pwm;

	BYTE currentCycle;

	BYTE reserved_for_64byte[33];
} TxBuffer;

typedef enum _ERROR
{
	ERROR_NO = 0x00,
	ERROR_ASSERT,
	ERROR_OVERHEAT,
} Error;

#define BACKGROUND_COLOR		RGB(2,130,200)
