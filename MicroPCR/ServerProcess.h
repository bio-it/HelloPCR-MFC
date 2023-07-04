#pragma once
#define HOST L"127.0.0.1"

typedef enum _Command
{
	CMD_STATUS = 0x00,
	CMD_SHOT = 0x01,
	CMD_LED_OFF = 0x02,
	CMD_LED_READY = 0x03,
	CMD_LED_RUN = 0x04,
	CMD_LED_ERROR = 0x05,
	CMD_EXIT = 0xFF
} Command;

typedef enum _FILTERINDEX
{
	FAM = 1,
	HEX = 2,
	ROX = 3,
	CY5 = 4
} FILTERINDEX;

typedef struct TX_PACKET
{
	Command Command;
	int FilterIndex;
	int CurrentCycle;
	char StartTime[500];
} Tx_Packet;

typedef struct RX_PACKET
{
	int Intensity;
	char reserved[508];
} Rx_Packet;

class ServerProcess
{
private:
	int port;
	Tx_Packet tx_packet;
	Rx_Packet rx_packet;
	void FindUnusedPort();
	string CStringToUTF8(CString cstring);
	Rx_Packet TCP_xfer(Tx_Packet tx_packet);

public:
	ServerProcess();
	int Shot(int filter_index, int current_cycle, CString start_time);
	int Status();
	void SetIndicatorLED(Command command);
	void StartProcess(long serial_number);
	void StopProcess();
};
