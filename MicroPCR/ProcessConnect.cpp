#pragma once
#include "stdafx.h"
#include "ProcessConnect.h"

namespace ProcessConnect 
{

	string CStringToUTF8(CString cstr)
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
	//const char* CStringToUTF8(CString cstring)
	//{
	//	CStringA cstr_a(cstring);
	//	const char* str = cstr_a.GetString();
	//	string utf8_str(str);
	//	//cstr_a.ReleaseBuffer();
	//	return utf8_str.c_str();
	//}

	Rx_Packet TCP_xfer(Tx_Packet tx_packet)
	{
		int bytes = 0;
		Rx_Packet rx_packet;
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

		if (!socket.Connect((const wchar_t*)HOST, PORT))
		{
			AfxMessageBox(L"Socket Connect failed...");
			return rx_packet;
		}
		
		
		bytes = socket.Send((Tx_Packet*)&tx_packet, sizeof(Tx_Packet));
		bytes = socket.Receive((Rx_Packet*)&rx_packet, sizeof(Rx_Packet));

		socket.Close();
		return rx_packet;
	}

	int Shot(int filter_index, int current_cycle, CString start_time)
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

	int Status()
	{
		Tx_Packet tx_packet;
		memset(&tx_packet, 0, sizeof(Tx_Packet));
		tx_packet.Command = CMD_STATUS;
		Rx_Packet rx_packet = TCP_xfer(tx_packet);
		return rx_packet.Intensity;
	}
}
