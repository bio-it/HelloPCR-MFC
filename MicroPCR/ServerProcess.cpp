#pragma once
#include "stdafx.h"
#include "ServerProcess.h"
#include "FileManager.h"

using namespace std;
ServerProcess::ServerProcess()
{
	_port = 0;
	memset(&rx_packet, 0, sizeof(RX_PACKET));
	memset(&tx_packet, 0, sizeof(TX_PACKET));

	int result = WSAStartup(MAKEWORD(2, 2), &_wsdata);
}
ServerProcess::~ServerProcess()
{
	Disconnect();
	WSACleanup();
}
bool ServerProcess::FindPort()
{
	_port = 0;
	_socket = socket(AF_INET, SOCK_STREAM, 0);
	socklen_t len = sizeof(_sockaddr);

	ZeroMemory(&_sockaddr, sizeof(_sockaddr)); // initialize structure
	_sockaddr.sin_family = AF_INET;
	_sockaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // set host ip address

	_sockaddr.sin_port = htons(_port); // set port 
	bool result = bind(_socket, (SOCKADDR*)&_sockaddr, sizeof(_sockaddr));

	getsockname(_socket, (SOCKADDR*)&_sockaddr, &len);
	_port = ntohs(_sockaddr.sin_port);
	closesocket(_socket);

	return result;
}
bool ServerProcess::Connect()
{
	if (_port == 0) return true;
	_socket = socket(AF_INET, SOCK_STREAM, 0);

	ZeroMemory(&_sockaddr, sizeof(_sockaddr)); // initialize structure
	_sockaddr.sin_family = AF_INET;
	_sockaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); // set host ip address
	_sockaddr.sin_port = htons(_port); // set port 

	if (connect(_socket, (SOCKADDR*)&_sockaddr, sizeof(_sockaddr)) == -1) return true;
	
	// set socket keep alive 
	bool opt_val = TRUE;
	if (setsockopt(_socket, SOL_SOCKET, SO_KEEPALIVE, (const char*)&opt_val, sizeof(char)) == -1) return true;
	
	// set Timeout 2000ms 
	int timeout_val = 2000; 
	if (setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_val, sizeof(int)) == -1) return true;
	if (setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout_val, sizeof(int)) == -1) return true;
	
	return false;
}

void ServerProcess::Disconnect()
{
	if (_socket != NULL) {
		closesocket(_socket);
		_socket = NULL;
	}
	_port = 0;
}

bool ServerProcess::Send()
{
	if (_port == 0) return true;
	int result = send(_socket, (const char*)&tx_packet, BUF_SIZE, 0);
	memset(&tx_packet, 0, sizeof(Tx_Packet));
	return result == -1;
}

bool ServerProcess::Recv()
{
	if (_port == 0) return true;
	memset(&rx_packet, 0, sizeof(Rx_Packet));
	int result = recv(_socket, (char*)&rx_packet, BUF_SIZE, 0);
	return result == -1;
}

bool ServerProcess::Shot(int filter_index, int current_cycle, CString experiment_date)
{
	memset(&tx_packet, 0, sizeof(Tx_Packet));
	tx_packet.Command = CMD_SHOT;
	tx_packet.FilterIndex = filter_index;
	tx_packet.CurrentCycle = current_cycle;
	strcpy(tx_packet.ExperimentDate, ((string)CT2A(experiment_date)).c_str());
	return Send();
}

bool ServerProcess::UpdateStatus()
{
	bool result_send, result_recv;
	memset(&tx_packet, 0, sizeof(Tx_Packet));
	tx_packet.Command = CMD_STATUS;
	result_send = Send();
	result_recv = Recv();
	return result_send || result_recv;
}

bool ServerProcess::SetIndicatorLED(Command command)
{
	memset(&tx_packet, 0, sizeof(Tx_Packet));
	tx_packet.Command = command;
	return Send();
}

int ServerProcess::GetIntensity()
{
	int intensity;
	memcpy(&intensity, &rx_packet.Intensity_1, sizeof(int));
	return intensity;
}

int ServerProcess::GetErrorCode()
{
	return rx_packet.code;
}

string ServerProcess::GetErrorMessage()
{
	return string(rx_packet.message);
}
bool ServerProcess::StartProcess(long serial_number)
{
	int mode = SW_HIDE;

	if (FindPort() == -1) {
		FileManager::log(L"[ERROR] Find unused TCP port", serial_number);
		return true;
	}

	CString arguments;
	arguments.Format(L"-p %d %05ld", _port, serial_number);

#ifdef EMULATOR
	mode = SW_SHOW;
	arguments.Format(L"-E -p %d %05ld", port, serial_number);
#endif
	
	ShellExecute(NULL, L"open", L"HelloPCR-Runner.exe", arguments, NULL, mode);
	if (Connect()) {
		FileManager::log(L"[ERROR] Server Connect Failed", serial_number);
		return true;
	}
	return false;
	
}

void ServerProcess::StopProcess()
{
	memset(&tx_packet, 0, sizeof(Tx_Packet));
	tx_packet.Command = CMD_EXIT;
	Send();
	Disconnect();
}


string ServerProcess::CStringToUTF8(CString cstr)
{
	int utf8Length = WideCharToMultiByte(CP_UTF8, 0, cstr, -1, nullptr, 0, nullptr, nullptr);
	if (utf8Length == 0) {
		// Error occurred
		return "";
	}

	std::string utf8Str(utf8Length - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, cstr, -1, &utf8Str[0], utf8Length, nullptr, nullptr);

	return utf8Str;
}
