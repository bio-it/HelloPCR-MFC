#pragma once
#define HOST L"127.0.0.1"
#define BUF_SIZE 128
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
	BYTE Command;
	BYTE FilterIndex;
	BYTE CurrentCycle;
	char ExperimentDate[16];
	BYTE reserved[109];
} Tx_Packet;

typedef struct RX_PACKET
{
	BYTE code;
	BYTE Intensity_1;
	BYTE Intensity_2;
	BYTE Intensity_3;
	BYTE Intensity_4;
	char message[100];
	BYTE reserved[23];
} Rx_Packet;

class ServerProcess
{
private:
	WSADATA _wsdata;
	SOCKET _socket;
	SOCKADDR_IN _sockaddr;
	int _port;
	
	Tx_Packet tx_packet;
	Rx_Packet rx_packet;
	bool Connect();
	void Disconnect();
	bool Send();
	bool Recv();
	string CStringToUTF8(CString cstring);
	Rx_Packet TCP_xfer(Tx_Packet tx_packet);

public:
	bool FindPort();

	ServerProcess();
	~ServerProcess();
	bool StartProcess(long serial_number);
	void StopProcess();
	
	bool Shot(int filter_index, int current_cycle, CString experiment_date);
	bool SetIndicatorLED(Command command);
	bool UpdateStatus();
	int GetIntensity();
	int GetErrorCode();
	string GetErrorMessage();
};