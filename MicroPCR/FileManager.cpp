
#include "stdafx.h"
#include "FileManager.h"
#include <locale.h>
#include "resource.h"

namespace FileManager
{
	int getTempValue() {
		CString path = L"diskOffset.txt";

		CStdioFile file;
		BOOL res = file.Open(path, CFile::modeRead);

		if (!res) {
			return -1;
		}

		CString line;
		int result = 0;
		file.ReadString(line);
		file.Close();

		result = _ttoi(line);
		return result;
	}

	void loadRecentPath(RECENT_TYPE recentType, CString &returnPath)
	{
		CString recentPath = (recentType == PID_PATH) ? RECENT_PID_PATH : RECENT_PROTOCOL_PATH;

		CStdioFile file;
		if( file.Open(recentPath, CFile::modeRead) )
		{
			// 경로에 한글이 포함될 수 있기 때문에 추가함.
			setlocale(LC_ALL, "korean");
			file.ReadString(returnPath);
			file.Close();
		}
	}

	void saveRecentPath(RECENT_TYPE recentType, CString path)
	{
		CString recentPath = (recentType == PID_PATH) ? RECENT_PID_PATH : RECENT_PROTOCOL_PATH;

		CStdioFile file;
		file.Open(recentPath, CStdioFile::modeCreate|CStdioFile::modeWrite);
		file.WriteString(path);
		file.Close();
	}

	bool findFile(CString path, CString fileName)
	{
		CFileFind finder;
		BOOL hasFile = finder.FindFile( path + "*.*" );

		while( hasFile )
		{
			hasFile = finder.FindNextFileW();

			if( finder.IsDots() )
				continue;

			if( finder.GetFileName().Compare(fileName) == 0 )
				return true;
		}

		return false;
	}

	void enumFiles(CString path, vector<CString> &list)
	{
		CFileFind finder;
		BOOL hasFile = finder.FindFile( path + "*.*" );

		list.clear();

		while( hasFile )
		{
			hasFile = finder.FindNextFileW();

			if( finder.IsDots() )
				continue;

			list.push_back(finder.GetFileName());
		}
	}

	bool loadPID(CString label, vector< PID > &pid)
	{
		CFile file;
		bool res = false;

		pid.clear();

		if( file.Open(L"./PID/" + label, CFile::modeRead) )
		{
			CArchive ar(&file, CArchive::load);
			int size = 0;
			float startTemp = 0, targetTemp = 0, kp = 0, ki = 0, kd = 0;
			
			try{
				ar >> size;
	
				for(int i=0; i<size; ++i){
					ar >> startTemp >> targetTemp >> kp >> ki >> kd;

					pid.push_back( PID(startTemp, targetTemp, kp, ki, kd) );
				}
			}catch(CFileException *e1){
				pid.clear();
				ar.Close();
				file.Close();
			}catch(CArchiveException *e2){
				pid.clear();
				ar.Close();
				file.Close();
			}

			res = true;
		}

		return res;
	}

	bool savePID(CString label, vector< PID > &pid)
	{
		CFile file;
		bool res = false;

		file.Open(L"./PID/" + label, CFile::modeCreate|CFile::modeWrite);
		CArchive ar(&file, CArchive::store);
		
		int size = pid.size();

		try{
			ar << size;
	
			for(int i=0; i<size; ++i){
				ar << pid[i].startTemp << pid[i].targetTemp
					<< pid[i].kp << pid[i].ki << pid[i].kd;
			}

			res = true;
		}catch(CFileException *e1){
			ar.Close();
			file.Close();
		}catch(CArchiveException *e2){
			ar.Close();
			file.Close();
		}

		return res;
	}

	// 230709 KBH change filename and path 
	void log(CString msg, long serialNumber)
	{
		CString path;
		CStdioFile file;
		CFileFind finder;
		CTime time = CTime::GetCurrentTime();
		CString path_serial; 
		path_serial.Format(L"/HelloPCR%05ld", serialNumber);
		
		path = L"./Record";
		CreateDirectory(path, NULL);
		path = path + path_serial;
		CreateDirectory(path, NULL);
		path = path + L"/Log";
		CreateDirectory(path, NULL);
		path =  path +  L"/HelloPCR-" + time.Format("%Y%m%d") + L".log";
		
		if( finder.FindFile(path) )
			file.Open(path, CStdioFile::modeWrite);
		else
			file.Open(path, CStdioFile::modeCreate|CStdioFile::modeWrite);

		file.SeekToEnd();
		file.WriteString(time.Format(L"[%Y:%m:%d-%H:%M:%S]\t") + msg + L"\n");
		file.Close();
	}

	//200803 KBH added serialNumber parameter
	void errorLog(CString msg, long serialNumber, int count) {
		CTime time = CTime::GetCurrentTime();
		CString path;
		path.Format(L"errorLog%06ld.txt", serialNumber);
		CStdioFile file;
		CFileFind finder;
		if (finder.FindFile(path))
			file.Open(path, CStdioFile::modeWrite);
		else
			file.Open(path, CStdioFile::modeCreate | CStdioFile::modeWrite);

		file.SeekToEnd();
		if (count < 0) {
			file.WriteString(time.Format(L"[%Y:%m:%d-%H:%M:%S] ") + msg + L"\n");
		}
		else {
			CString message;
			message.Format(L"%s%s - count(%d)\n", time.Format(L"[%Y:%m:%d-%H:%M:%S] "), msg, count);
			file.WriteString(message);
		}
		file.Close();
	}

	bool loadConstants(int &maxCycle, int &compensation, float &integralMax, float &displayDelta, float &flRelativeMax, vector<PID> &pids)
	{
		CFile file;
		bool res = false;

		pids.clear();

		if (file.Open(L"./HelloPCR.data", CFile::modeRead))
		{
			CArchive ar(&file, CArchive::load);
			float startTemp = 0, targetTemp = 0, kp = 0, ki = 0, kd = 0;
			int size = 0;
			CString version;

			try {
				ar >> version;

				if (!version.IsEmpty() && version.GetAt(0) == 'V') {
					ar >> maxCycle >> compensation >> integralMax >> displayDelta >> flRelativeMax;
					ar >> size;

					for (int i = 0; i < size; ++i) {
						ar >> startTemp >> targetTemp >> kp >> kd >> ki;

						pids.push_back(PID(startTemp, targetTemp, kp, ki, kd));
					}

					res = true;
				}
			}
			catch (CFileException *e1) {
				ar.Close();
				file.Close();
			}
			catch (CArchiveException *e2) {
				ar.Close();
				file.Close();
			}
		}

		return res;
	}

	bool saveConstants(int &maxCycle, int &compensation, float &integralMax, float &displayDelta, float &flRelativeMax, vector<PID> &pids)
	{
		CFile file;
		bool res = false;

		file.Open(L"./HelloPCR.data", CFile::modeCreate | CFile::modeWrite);
		CArchive ar(&file, CArchive::store);
		CString version = VERSION_CONSTANTS;

		try {
			ar << version;
			ar << maxCycle << compensation << integralMax << displayDelta << flRelativeMax;

			int size = pids.size();

			ar << size;

			for (int i = 0; i < size; ++i) {
				ar << pids[i].startTemp << pids[i].targetTemp
					<< pids[i].kp << pids[i].kd << pids[i].ki;
			}

			res = true;
		}
		catch (CFileException *e1) {
			ar.Close();
			file.Close();
		}
		catch (CArchiveException *e2) {
			ar.Close();
			file.Close();
		}

		return res;
	}

	bool loadProtocols(vector< Protocol > &protocols)
	{
		CFile file;
		bool res = false;

		protocols.clear();

		if (file.Open(L"./Protocol.data", CFile::modeRead))
		{
			CArchive ar(&file, CArchive::load);
			int size = 0, size2 = 0;
			CString protocolName;
			bool useFam, useHex, useRox, useCY5;
			CString labelFam, labelHex, labelRox, labelCY5;
			float ctFam, ctHex, ctRox, ctCY5;
			CString label;
			double temp, time;
			CString version;

			try {
				ar >> version;

				if (!version.IsEmpty() && version.GetAt(0) == 'V') {
					ar >> size;

					for (int i = 0; i < size; ++i) {
						Protocol protocol;

						ar >> protocol.protocolName;
						ar >> protocol.useFam;
						ar >> protocol.useHex;
						ar >> protocol.useRox;
						ar >> protocol.useCY5;

						ar >> protocol.labelFam;
						ar >> protocol.labelHex;
						ar >> protocol.labelRox;
						ar >> protocol.labelCY5;

						ar >> protocol.ctFam;
						ar >> protocol.ctHex;
						ar >> protocol.ctRox;
						ar >> protocol.ctCY5;

						// 220222 KBH Save the thresholds
						ar >> protocol.thresholdFam;
						ar >> protocol.thresholdHex;
						ar >> protocol.thresholdRox;
						ar >> protocol.thresholdCY5;

						ar >> protocol.magnetoData;

						ar >> size2;

						for (int j = 0; j < size2; ++j) {
							ar >> label >> temp >> time;

							Action action;
							action.Label = label;
							action.Temp = temp;
							action.Time = time;

							protocol.actionList.push_back(action);
						}

						protocols.push_back(protocol);
						res = true;
					}
				}
			}
			catch (CFileException *e1) {
				protocols.clear();
				ar.Close();
				file.Close();
			}
			catch (CArchiveException *e2) {
				protocols.clear();
				ar.Close();
				file.Close();
			}
		}

		return res;
	}

	bool saveProtocols(vector< Protocol > &protocols)
	{
		CFile file;
		bool res = false;

		file.Open(L"./Protocol.data", CFile::modeCreate | CFile::modeWrite);
		CArchive ar(&file, CArchive::store);

		int size = protocols.size();

		CString version = VERSION_PROTOCOL;

		try {
			ar << version;
			ar << size;

			for (int i = 0; i < size; ++i) {
				ar << protocols[i].protocolName;
				ar << protocols[i].useFam;
				ar << protocols[i].useHex;
				ar << protocols[i].useRox;
				ar << protocols[i].useCY5;

				// save the filter labels
				ar << protocols[i].labelFam;
				ar << protocols[i].labelHex;
				ar << protocols[i].labelRox;
				ar << protocols[i].labelCY5;

				ar << protocols[i].ctFam;
				ar << protocols[i].ctHex;
				ar << protocols[i].ctRox;
				ar << protocols[i].ctCY5;

				// 220222 KBH Save the thresholds
				ar << protocols[i].thresholdFam;
				ar << protocols[i].thresholdHex;
				ar << protocols[i].thresholdRox;
				ar << protocols[i].thresholdCY5;

				ar << protocols[i].magnetoData;

				ar << protocols[i].actionList.size();

				for (int j = 0; j < protocols[i].actionList.size(); ++j) {
					ar << protocols[i].actionList[j].Label << protocols[i].actionList[j].Temp << protocols[i].actionList[j].Time;
				}
			}

			res = true;
		}
		catch (CFileException *e1) {
			ar.Close();
			file.Close();
		}
		catch (CArchiveException *e2) {
			ar.Close();
			file.Close();
		}

		return res;
	}

	bool loadMagnetoProtocols(vector<MagnetoProtocol>& magnetoProtocols) {
		CFile file;
		bool res = false;

		magnetoProtocols.clear();

		if (file.Open(L"./Magneto.data", CFile::modeRead))
		{
			CArchive ar(&file, CArchive::load);
			CString version;
			int size = 0;

			try {
				ar >> version;

				if (!version.IsEmpty() && version.GetAt(0) == 'V') {
					ar >> size;

					for (int i = 0; i < size; ++i) {
						MagnetoProtocol protocol;

						ar >> protocol.protocolName;
						ar >> protocol.protocolData;

						magnetoProtocols.push_back(protocol);

						res = true;
					}
				}
			}
			catch (CFileException * e1) {
				magnetoProtocols.clear();
				ar.Close();
				file.Close();
			}
			catch (CArchiveException * e2) {
				magnetoProtocols.clear();
				ar.Close();
				file.Close();
			}
		}

		return res;
	}

	MagnetoProtocol loadMagnetoProtocol(CString& protocolName) {
		CFile file;
		bool res = false;
		MagnetoProtocol protocol;

		if (file.Open(L"./Magneto.data", CFile::modeRead))
		{
			CArchive ar(&file, CArchive::load);
			CString version;
			int size = 0;

			try {
				ar >> version;
				
				if (!version.IsEmpty() && version.GetAt(0) == 'V') {
					ar >> size;

					for (int i = 0; i < size; ++i) {
						ar >> protocol.protocolName;
						ar >> protocol.protocolData;

						// Found the same protocol.
						if (protocol.protocolName.Compare(protocolName) == 0) {
							res = true;
							break;
						}
					}
				}
			}
			catch (CFileException * e1) {
				ar.Close();
				file.Close();
			}
			catch (CArchiveException * e2) {
				ar.Close();
				file.Close();
			}
		}

		if (!res) {
			protocol.protocolName = L"";
			protocol.protocolData = L"";
		}

		// if can't find the protocolName, just return empty protocol
		return protocol;
	}

	bool saveMagnetoProtocols(vector <MagnetoProtocol>& magnetoProtocols) {
		CFile file;
		bool res = false;

		file.Open(L"./Magneto.data", CFile::modeCreate | CFile::modeWrite);
		CArchive ar(&file, CArchive::store);

		int size = magnetoProtocols.size();
		CString version = VERSION_MAGNETO;

		try {
			ar << version;
			ar << size;

			for (int i = 0; i < size; ++i) {
				ar << magnetoProtocols[i].protocolName;
				ar << magnetoProtocols[i].protocolData;
			}

			res = true;
		}
		catch (CFileException * e1) {
			ar.Close();
			file.Close();
		}
		catch (CArchiveException * e2) {
			ar.Close();
			file.Close();
		}

		return res;
	}

	bool loadHistory(vector<History>& historyList) {
		CFile file;
		bool res = false;

		historyList.clear();

		if (file.Open(L"./History.data", CFile::modeRead))
		{
			CArchive ar(&file, CArchive::load);
			CString version;
			int size = 0;

			try {
				ar >> version;

				if (!version.IsEmpty() && version.GetAt(0) == 'V') {
					ar >> size;

					for (int i = 0; i < size; ++i) {
						// version 에 따른 다른 구현 필요 추후 업데이트 시.

						History history;

						ar >> history.date;
						ar >> history.target;
						ar >> history.filter;
						ar >> history.ctValue;
						ar >> history.result;

						historyList.push_back(history);
					}
				}
			}
			catch (CFileException * e1) {
				historyList.clear();
				ar.Close();
				file.Close();
			}
			catch (CArchiveException * e2) {
				historyList.clear();
				ar.Close();
				file.Close();
			}

			res = true;
		}

		return res;
	}

	bool saveHistory(vector<History>& historyList) {
		CFile file;
		bool res = false;

		file.Open(L"./History.data", CFile::modeCreate | CFile::modeWrite);
		CArchive ar(&file, CArchive::store);

		int size = historyList.size();
		CString version = VERSION_HISTORY;

		try {
			ar << version;
			ar << size;

			for (int i = 0; i < size; ++i) {
				ar << historyList[i].date;
				ar << historyList[i].target;
				ar << historyList[i].filter;
				ar << historyList[i].ctValue;
				ar << historyList[i].result;
			}

			res = true;
		}
		catch (CFileException * e1) {
			ar.Close();
			file.Close();
		}
		catch (CArchiveException * e2) {
			ar.Close();
			file.Close();
		}

		return res;
	}

	void loadFirmwareFile(CString& firmwareData) {
		// Load from resource firmware data
		// very good example https://stackoverflow.com/questions/2933295/embed-text-file-in-a-resource-in-a-native-windows-application
		char *result;
		HMODULE handle = ::GetModuleHandle(NULL);
		HRSRC rc = ::FindResource(handle, MAKEINTRESOURCE(IDR_FIRMWARE_FILE), MAKEINTRESOURCE(FIRMWARE_FILE));
		HGLOBAL data = ::LoadResource(handle, rc);
		DWORD size = ::SizeofResource(handle, rc);
		result = static_cast<char *>(::LockResource(data));

		// Convert to char * to CString (Ansi) -> (Unicode)
		CStringA tmpResult(result);
		firmwareData = tmpResult;
	}

	void saveFirmwareFile(CString path, vector <CString>& firmwareData) {
		CStdioFile file;
		file.Open(path, CStdioFile::modeCreate | CStdioFile::modeWrite);

		file.SeekToBegin();

		for (int i = 0; i < firmwareData.size(); ++i) {
			// Do not write the \n when the last line
			if (i != firmwareData.size() - 1) {
				file.WriteString(firmwareData[i] + "\n");
			}
			else {
				file.WriteString(firmwareData[i]);
			}
		}

		file.Close();

		CString message;
		message.Format(L"%s 경로에 저장되었습니다.", path);
		AfxMessageBox(message);
	}

	// Filter type 
	// 0: FAM
	// 1: HEX
	// 2: ROX
	// 3: CY5
	float getFilterValue(int filterType) {
		CString filters[4] = {L"FAM.txt", L"HEX.txt", L"ROX.txt", L"CY5.txt"};

		CString path = filters[filterType];

		CStdioFile file;
		BOOL res = file.Open(path, CFile::modeRead);

		if (!res) {
			return -1.0;
		}

		CString line;
		float result = 0;
		file.ReadString(line);
		file.Close();

		result = _ttof(line);
		return result;
	}
};