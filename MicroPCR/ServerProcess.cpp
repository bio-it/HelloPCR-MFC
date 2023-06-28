#pragma once
#include "stdafx.h"
#include "ServerProcess.h"

ServerProcess::ServerProcess()
{
	port = 0;
	memset(&rx_packet, 0, sizeof(RX_PACKET));
	memset(&tx_packet, 0, sizeof(TX_PACKET));
}

void ServerProcess::FindUnusedPort()
{
	UINT UINT_PORT;	CString temp; CSocket socket;
	if (!AfxSocketInit()) return;
	if (!socket.Create()) return;
	socket.GetSockName((CString&)temp, (UINT&)UINT_PORT);
	socket.Close();
	port = (int)UINT_PORT;
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

Rx_Packet ServerProcess::TCP_xfer(Tx_Packet tx_packet)
{
	int bytes = 0;
	memset(&rx_packet, 0, sizeof(RX_PACKET));
	if (!AfxSocketInit())
	{
		AfxMessageBox(L"Socket initialize failed...");
		return rx_packet;
	}

	CSocket socket;
	if (!socket.Create())
	{
		AfxMessageBox(L"Socket Create failed...");
		return rx_packet;
	}

	if (!socket.Connect((const wchar_t*)HOST, port))
	{
		AfxMessageBox(L"Socket Connect failed...");
		return rx_packet;
	}


	bytes = socket.Send((Tx_Packet*)&tx_packet, sizeof(Tx_Packet));
	bytes = socket.Receive((Rx_Packet*)&rx_packet, sizeof(Rx_Packet));

	socket.Close();
	return rx_packet;
}

int ServerProcess::Shot(int filter_index, int current_cycle, CString start_time)
{
	int len;
	Tx_Packet tx_packet;
	memset(&tx_packet, 0, sizeof(Tx_Packet));
	tx_packet.Command = CMD_SHOT;
	tx_packet.FilterIndex = filter_index;
	tx_packet.CurrentCycle = current_cycle;
	strcpy(tx_packet.StartTime, CStringToUTF8(start_time).c_str());

	Rx_Packet rx_packet = TCP_xfer(tx_packet);
	return rx_packet.Intensity;
}

int ServerProcess::Status()
{
	Tx_Packet tx_packet;
	memset(&tx_packet, 0, sizeof(Tx_Packet));
	tx_packet.Command = CMD_STATUS;
	Rx_Packet rx_packet = TCP_xfer(tx_packet);
	return rx_packet.Intensity;
}

void ServerProcess::StartProcess(long serial_number)
{
	int mode = SW_HIDE;
	FindUnusedPort();
	CString arguments;
	arguments.Format(L"-p %d %05ld", port, serial_number);
#ifdef EMULATOR
	mode = SW_SHOW;
	arguments.Format(L"-E -p %d %05ld", port, serial_number);
#endif
	ShellExecute(NULL, L"open", L"HelloPCR-Runner.exe", arguments, NULL, mode);
}

void ServerProcess::StopProcess()
{
	Tx_Packet tx_packet;
	memset(&tx_packet, 0, sizeof(Tx_Packet));
	tx_packet.Command = CMD_EXIT;
	Rx_Packet rx_packet = TCP_xfer(tx_packet);
}
