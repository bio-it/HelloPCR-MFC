// CMainDialog.cpp: 구현 파일
//

#include "stdafx.h"
#include "CMainDialog.h"
#include "afxdialogex.h"
#include "resource.h"

#include "PasswordDialog.h"
#include "SetupDialog.h"
#include "DeviceSetup.h"
#include "FileManager.h"
#include "ConfirmDialog.h"

#include <numeric>
#include <dbt.h> // 220325 KBH Device Change Handler

// CMainDialog 대화 상자

IMPLEMENT_DYNAMIC(CMainDialog, CDialogEx)

CMainDialog::CMainDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_MAIN, pParent)
	, lastSelectedProtocol(L"")
	, isProtocolLoaded(false)
	, isConnected(false)
	, isStarted(false)
	, magnetoProtocolIdx(0)
	, isFirstDraw(false)
	, isCompletePCR(false)
	, isTargetArrival(false)
	, m_prevTargetTemp(25)
	, m_currentTargetTemp(25)
	, m_kp(0.0)
	, m_ki(0.0)
	, m_kd(0.0)
	, m_cArrivalDelta(0.5)
	, maxCycles(40)
	, compensation(0)
	, integralMax(INTGRALMAX)
	, displayDelta(0.0f)
	, flRelativeMax(FL_RELATIVE_MAX)
	, m_timerCounter(0)
	, totalLeftSec(0)
	, leftSec(0)
	, m_currentActionNumber(-1)
	, targetTempFlag(false)
	, m_timeOut(0)
	, m_leftGotoCount(-1)
	, filterIndex(0)
	, filterRunning(false)
	, shotCounter(0)
	, ledControl_wg(1)
	, ledControl_r(1)
	, ledControl_g(1)
	, ledControl_b(1)
	, emergencyStop(false)
	, freeRunning(false)
	, freeRunningCounter(0)
	, currentCycle(0)
	, recordingCount(0)
	, logStopped(false)
	, m_strStylesPath(L"./")
	, isConnectionBroken(false)
{
	// load icon
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	// set skin
	XTPSkinManager()->SetApplyOptions(XTPSkinManager()->GetApplyOptions() | xtpSkinApplyMetrics);
	XTPSkinManager()->LoadSkin(m_strStylesPath + _T("QuicksilverR.cjstyles"));

}

CMainDialog::~CMainDialog()
{
	if (device != NULL)
		delete device;
	if (m_Timer != NULL)
		delete m_Timer;
	if (magneto != NULL)
		delete magneto;
}

void CMainDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_CUSTOM_RESULT_TABLE, resultTable);

	DDX_Control(pDX, IDC_COMBO_PROTOCOLS, protocolList);
	DDX_Control(pDX, IDC_COMBO_DEVICE_LIST, deviceList);
	// progressStatus
}


BEGIN_MESSAGE_MAP(CMainDialog, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_START, &CMainDialog::OnBnClickedButtonStart)
	ON_BN_CLICKED(IDC_BUTTON_SETUP, &CMainDialog::OnBnClickedButtonSetup)
	ON_LBN_SELCHANGE(IDC_COMBO_PROTOCOLS, &CMainDialog::OnLbnSelchangeComboProtocols)
	ON_BN_CLICKED(IDC_BUTTON_CONNECT, &CMainDialog::OnBnClickedButtonConnect)
	ON_WM_TIMER()
	ON_MESSAGE(WM_MMTIMER, OnmmTimer)

	ON_WM_DEVICECHANGE()
END_MESSAGE_MAP()


// CMainDialog 메시지 처리기


BOOL CMainDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	GetDlgItem(IDC_BUTTON_START)->EnableWindow(FALSE);
	SetDlgItemText(IDC_EDIT_CONNECTI_STATUS, L"Disconnected");
	SetDlgItemText(IDC_STATIC_PROGRESS_STATUS, L"Idle..");

	progressStatus.SetRange(0, 100);

	SetIcon(m_hIcon, TRUE);	 // set Large Icon
	SetIcon(m_hIcon, FALSE); // set Small Icon

	// Initialize the connection information
	initConnection();

	// initialize the device list
	initPCRDevices();

	// Initialize UI
	loadConstants();
	loadProtocolList();

	initResultTable();

#ifdef EMULATOR
	AfxMessageBox(L"이 프로그램은 에뮬레이터 장비용 입니다.");
#endif

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 예외: OCX 속성 페이지는 FALSE를 반환해야 합니다.
}

void CMainDialog::OnBnClickedButtonSetup()
{
	CPasswordDialog passwordDialog;

	int res = passwordDialog.DoModal();
	if (res == IDOK) {

		// biomedux setup menu
		if (passwordDialog.resultType == PASSWORD_RESULT_KWELLLABS) {
			SetupDialog dlg;
			dlg.DoModal();

			// Reload the protocol list
			loadProtocolList();
			loadConstants();
			initResultTable();
		}
		else if (passwordDialog.resultType == PASSWORD_RESULT_DEVICE_SETUP) {
			// Check connection state
			if (isConnected) {
				AfxMessageBox(L"Can't setup the device when the device is running.");
				return;
			}

			CDeviceSetup dlg;
			dlg.DoModal();
		}
	}
}

BOOL CMainDialog::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		if (pMsg->wParam == VK_RETURN)
			return TRUE;

		if (pMsg->wParam == VK_ESCAPE)
			return TRUE;
	}

	return CDialogEx::PreTranslateMessage(pMsg);
}
void CMainDialog::OnPaint()
{
	CPaintDC dc(this);

	if (IsIconic()) {
		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		dc.DrawIcon(x, y, m_hIcon);
	}
}

HCURSOR CMainDialog::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


static const int RESULT_TABLE_COLUMN_WIDTHS[2] = { 100, 130 };

void CMainDialog::initResultTable() {
	resultTable.SetListMode(true);

	resultTable.DeleteAllItems();

	resultTable.SetRowCount(5);
	resultTable.SetColumnCount(2);
	resultTable.SetFixedRowCount(1);
	resultTable.SetFixedColumnCount(1);
	resultTable.SetEditable(false);
	resultTable.SetRowResize(false);
	resultTable.SetColumnResize(false);

	// 초기 gridControl 의 table 값들을 설정해준다.
	DWORD dwTextStyle = DT_RIGHT | DT_VCENTER | DT_SINGLELINE;

	for (int col = 0; col < resultTable.GetColumnCount(); col++) {
		GV_ITEM item;
		item.mask = GVIF_TEXT | GVIF_FORMAT;
		item.row = 0;
		item.col = col;

		item.nFormat = DT_CENTER | DT_WORDBREAK;
		item.strText = RESULT_TABLE_COLUMNS[col];

		if (col == 0) {
			resultTable.SetRowHeight(col, 25);
		}

		// resultTable.SetItemFont(item.row, item.col, )
		resultTable.SetItem(&item);
		resultTable.SetColumnWidth(col, RESULT_TABLE_COLUMN_WIDTHS[col]);
	}

	vector<CString> labels;

	if (currentProtocol.useFam) {
		labels.push_back(currentProtocol.labelFam);
	}

	if (currentProtocol.useHex) {
		labels.push_back(currentProtocol.labelHex);
	}

	if (currentProtocol.useRox) {
		labels.push_back(currentProtocol.labelRox);
	}

	if (currentProtocol.useCY5) {
		labels.push_back(currentProtocol.labelCY5);
	}

	for (int i = 0; i < labels.size(); ++i) {
		GV_ITEM item;
		item.mask = GVIF_TEXT | GVIF_FORMAT;
		item.row = i + 1;
		item.col = 0;
		item.nFormat = DT_CENTER | DT_WORDBREAK;

		item.strText = labels[i];

		// resultTable.SetItemFont(item.row, item.col, )
		resultTable.SetItem(&item);
	}
}

void CMainDialog::loadProtocolList() {
	protocolList.ResetContent();

	FileManager::loadProtocols(protocols);

	int idx = 0;
	for (int i = 0; i < protocols.size(); ++i) {
		protocolList.AddString(protocols[i].protocolName);

		if (lastSelectedProtocol.Compare(protocols[i].protocolName) == 0) {
			idx = i;
		}
	}

	if (protocolList.GetCount() > 0) {
		protocolList.SetCurSel(idx);
		// Load the data from protocol
		currentProtocol = protocols[idx];
		calcTotalTime();
		isProtocolLoaded = true;

		// Getting the magneto data and check magneto data
		loadMagnetoProtocol();
	}
	else {
		AfxMessageBox(L"You need to make the protocol first.");
	}
}

void CMainDialog::loadMagnetoProtocol() {
	// Getting the magneto data and check magneto data
	CString magnetoProtocolRes = magneto->loadProtocolFromData(currentProtocol.magnetoData);

	if (magneto->isCompileSuccess(magnetoProtocolRes)) {
		// initialize the protocol
		vector<ActionBeans> treeList;
		magneto->generateActionList(treeList);

		int maxActions = magneto->getTotalActionNumber();
		progressStatus.SetRange(0, maxActions);
	}
}

void CMainDialog::loadConstants() {
	FileManager::loadConstants(maxCycles, compensation, integralMax, displayDelta, flRelativeMax, pids);

	// if there is no pids, make initial pid data
	if (pids.empty()) {
		PID pid1(25.0f, 95.0f, 0.0f, 0.0f, 0.0f);
		PID pid2(95.0f, 60.0f, 0.0f, 0.0f, 0.0f);
		PID pid3(60.0f, 72.0f, 0.0f, 0.0f, 0.0f);
		PID pid4(72.0f, 95.0f, 0.0f, 0.0f, 0.0f);
		PID pid5(95.0f, 50.0f, 0.0f, 0.0f, 0.0f);
		pids.push_back(pid1);
		pids.push_back(pid2);
		pids.push_back(pid3);
		pids.push_back(pid4);
		pids.push_back(pid5);
	}
}

void CMainDialog::OnLbnSelchangeComboProtocols() {
	int selectedIdx = protocolList.GetCurSel();
	protocolList.GetLBText(selectedIdx, lastSelectedProtocol);

	// Load the data from protocol
	currentProtocol = protocols[selectedIdx];
	calcTotalTime();
	initResultTable();

	isProtocolLoaded = true;

	// Getting the magneto data and check magneto data
	loadMagnetoProtocol();
}

void CMainDialog::calcTotalTime() {
	totalLeftSec = 0;

	vector<Action> tempActions;

	// Deep copy
	for (int i = 0; i < currentProtocol.actionList.size(); ++i) {
		tempActions.push_back(Action());
		tempActions[i].Label = currentProtocol.actionList[i].Label;
		tempActions[i].Temp = currentProtocol.actionList[i].Temp;
		tempActions[i].Time = currentProtocol.actionList[i].Time;
	}

	for (int i = 0; i < tempActions.size(); ++i) {
		Action action = tempActions[i];

		if (action.Label.Compare(L"GOTO") == 0) {
			CString gotoLabel;
			gotoLabel.Format(L"%d", (int)action.Temp);
			int remain = (int)action.Time - 1;
			tempActions[i].Time = (double)remain;

			if (remain != -1) {
				int gotoIndex = -1;
				for (int j = 0; j < tempActions.size(); ++j) {
					if (tempActions[j].Label.Compare(gotoLabel) == 0) {
						gotoIndex = j;
					}
				}

				i = gotoIndex - 1;
			}
		}
		else if (action.Label.Compare(L"SHOT") != 0) {
			totalLeftSec += (int)action.Time;
		}
	}

	int second = totalLeftSec % 60;
	int minute = totalLeftSec / 60;
	// int hour = minute / 60;
	// minute = minute - hour * 60;

	CString totalTime;

	if (minute == 0)
		totalTime.Format(L"%02ds", second);
	else
		totalTime.Format(L"%02dm %02ds", minute, second);

	// Display total remaining time
	SetDlgItemText(IDC_STATIC_PROGRESS_REMAINING_TIME, totalTime);
}

void CMainDialog::initConnection() {
	m_Timer = new CMMTimers(1, GetSafeHwnd());
	device = new CDeviceConnect(GetSafeHwnd());
	magneto = new CMagneto();
}

void CMainDialog::initPCRDevices() {
	deviceList.ResetContent();

	int deviceNums = device->GetDevices();
	for (int i = 0; i < deviceNums; ++i) {
		CString deviceSerial = device->GetDeviceSerial(i);

		// Remove the QuPCR string
		deviceList.AddString(deviceSerial.Mid(5));
	}

	if (deviceList.GetCount() != 0) {
		deviceList.SetCurSel(0);
	}
}

// 210120 KBH initialize state
void CMainDialog::initState()
{

	GetDlgItem(IDC_BUTTON_START)->EnableWindow(FALSE);

	if (!magneto->isIdle()) {
		magneto->stop();
	}

	cleanupTask();

	KillTimer(Magneto::TimerRuntaskID);
}

void CMainDialog::OnBnClickedButtonConnect()
{
	CString buttonState;
	GetDlgItemText(IDC_BUTTON_CONNECT, buttonState);

	// Disable the buttons
	GetDlgItem(IDC_BUTTON_SETUP)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(FALSE);

	if (buttonState.Compare(L"Connect") == 0) {
		// Getting the device index
		int selectedIdx = deviceList.GetCurSel();

		if (selectedIdx != -1) {
			CString deviceSerial;
			deviceList.GetLBText(selectedIdx, deviceSerial);
			int usbSerial = _ttoi(deviceSerial);

			// Getting the com port list
			vector<CString> portList;
			magneto->searchPortByReg(portList);

			bool result = false;
			// Check all magneto devices
			for (int i = 0; i < portList.size(); ++i) {
				// Getting the serial number from magneto.
				CString tempPortNumber = portList[i];
				tempPortNumber.Replace(L"COM", L"");
				int comPortNumber = _ttoi(tempPortNumber);

				DriverStatus::Enum res = magneto->connect(comPortNumber);

				if (res == DriverStatus::CONNECTED)
				{
					long serialNumber = magneto->getSerialNumber();

					if (serialNumber == usbSerial) {
						result = true;
						break;
					}
					else {
						magneto->disconnect();
					}
				}
			}

			if (result) {
				// Found the same serial number device.
				// Found the same serial number device.
				CStringA pcrSerial;
				pcrSerial.Format("QuPCR%06d", usbSerial);
				char serialBuffer[20];
				sprintf(serialBuffer, "%s", pcrSerial);

				BOOL res = device->OpenDevice(LS4550EK_VID, LS4550EK_PID, serialBuffer, TRUE);

				if (res) {
					// Connection processing
					isConnected = true;

					SetDlgItemText(IDC_EDIT_CONNECTI_STATUS, L"Connected");
					SetDlgItemText(IDC_BUTTON_CONNECT, L"Disconnect");
					GetDlgItem(IDC_COMBO_DEVICE_LIST)->EnableWindow(FALSE);

					CString prevTitle;
					GetWindowText(prevTitle);
					prevTitle.Format(L"%s - %s", prevTitle, deviceSerial);
					SetWindowText(prevTitle);

					if (isProtocolLoaded) {
						GetDlgItem(IDC_BUTTON_START)->EnableWindow();
					}
				}
				else {
					AfxMessageBox(L"Magneto is connected but PCR is failed to connect(Unknown error).");
				}
			}
			else {
				AfxMessageBox(L"PCR 과 일치하는 Magneto 장비를 찾을 수 없습니다.");
			}
		}
		else {
			AfxMessageBox(L"Please select the device first.");
		}
	}
	else {
		isConnected = false;
		if (magneto->isConnected()) {
			magneto->disconnect();
		}

		device->CloseDevice();

		CString prevTitle;
		GetWindowText(prevTitle);
		CString left = prevTitle.Left(prevTitle.Find(L")") + 1);
		SetWindowText(left);

		SetDlgItemText(IDC_EDIT_CONNECTI_STATUS, L"Disconnected");
		SetDlgItemText(IDC_BUTTON_CONNECT, L"Connect");
		GetDlgItem(IDC_COMBO_DEVICE_LIST)->EnableWindow();
		GetDlgItem(IDC_BUTTON_START)->EnableWindow(FALSE);
	}

	GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON_SETUP)->EnableWindow(TRUE);
}

void CMainDialog::OnBnClickedButtonStart()
{
	if (!magneto->isCompileEnded()) {
		AfxMessageBox(L"Magneto data is not exist. Please setting the magneto protocol.");
		return;
	}

	// Add Confirm Dialog
	CString message;
	message = !isStarted ? L"프로토콜을 시작하겠습니까?" : L"프로토콜을 중지하겠습니까?";

	// 211111 KBH change dialog
	if (AfxMessageBox(message, MB_YESNO | MB_ICONQUESTION) != IDYES) {
		return;
	}


// 211117 KBH chip connection check 
// 220111 KBH changed iteration count from 5 to 20 (waiting time is changed from 250ms to 1000ms)
// (20번째 read_buffer 에 온도 값이 10도 이하일 경우 PCR Chip Connection Error)
// change the position of sleep function
#ifndef EMULATOR
	if (!isStarted)
	{
		float currentTemp;

		for (int i = 0; i < 20; ++i)
		{
			RxBuffer rx;
			TxBuffer tx;
			currentTemp = 0.0f;

			memset(&rx, 0, sizeof(RxBuffer));
			memset(&tx, 0, sizeof(TxBuffer));

			tx.cmd = CMD_READY;

			BYTE senddata[65] = { 0, };
			BYTE readdata[65] = { 0, };
			memcpy(senddata, &tx, sizeof(TxBuffer));

			device->Write(senddata);
			Sleep(TIMER_DURATION);
			device->Read(&rx);

			memcpy(readdata, &rx, sizeof(RxBuffer));
			memcpy(&currentTemp, &(rx.chamber_temp_1), sizeof(float));
		}
		if (currentTemp < 10.0f)
		{
			message = L"Low temperature! Chip connection check!";
			AfxMessageBox(message);
			return;
		}
	}
#endif

	// Disable start button
	GetDlgItem(IDC_BUTTON_START)->EnableWindow(FALSE);

	if (isConnected && isProtocolLoaded) {
		isStarted = !isStarted;
		if (isStarted) {
			progressStatus.SetPos(0);
			GetDlgItem(IDC_COMBO_PROTOCOLS)->EnableWindow(FALSE);
			GetDlgItem(IDC_BUTTON_SETUP)->EnableWindow(FALSE);
			GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(FALSE);
			SetDlgItemText(IDC_BUTTON_START, L"Stop");
			SetDlgItemText(IDC_STATIC_PROGRESS_STATUS, L"Preparing the Magneto..");

			// init all values
			logStopped = false;
			initValues();
			calcTotalTime();
			initLog();

			// initialize the values
			currentCmd = CMD_PCR_RUN;

			m_prevTargetTemp = 25;

			m_currentTargetTemp = (BYTE)currentProtocol.actionList[0].Temp;

			findPID();

			magneto->start();
			SetTimer(Magneto::TimerRuntaskID, Magneto::TimerRuntaskDuration, NULL);

			// Enable stop button
			GetDlgItem(IDC_BUTTON_START)->EnableWindow(TRUE);
		}
		else {
			// 210120 KBH remove reduplication code 
			//GetDlgItem(IDC_COMBO_PROTOCOLS)->EnableWindow();
			//GetDlgItem(IDC_BUTTON_SETUP)->EnableWindow();
			//SetDlgItemText(IDC_BUTTON_START, L"Start");

			// 210120 KBH click magneto state is idle
			if (!magneto->isIdle()) {
				magneto->stop();
			}
			// PCREndTask();
			cleanupTask();
			

			KillTimer(Magneto::TimerRuntaskID);
		}
	}
	else {
		AfxMessageBox(L"Error occured!");
	}
}



void CMainDialog::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == Magneto::TimerRuntaskID)
	{
		if (!magneto->runTask()) {
			KillTimer(Magneto::TimerRuntaskID);
			// 210119 KBH remove unused code 
			//AfxMessageBox(L"Limit 스위치가 설정되어 task 가 종료되었습니다.\n기기를 확인하세요.");

			// 210119 KBH Motor Stucked
			AfxMessageBox(L"motor가 stuck 되었습니다.\n기기를 확인하세요.");

			exit(0); // 211117 KBH if motor is stuck, program exit
			//initState();	// 210120 KBH state initialize 
			
			return;
		}

		// Progress
		progressStatus.SetPos(magneto->getCurrentActionNumber());

		// Check magneto command
		if (magneto->getCurrentCmd() == ProtocolCmd::GO) {
			if (magneto->getCurrentfilter() == 1) {
				SetDlgItemText(IDC_STATIC_PROGRESS_STATUS, L"Washing 1...");
			}
			else if (magneto->getCurrentfilter() == 2) {
				SetDlgItemText(IDC_STATIC_PROGRESS_STATUS, L"Washing 2...");
			}
			else if (magneto->getCurrentfilter() == 5) {
				SetDlgItemText(IDC_STATIC_PROGRESS_STATUS, L"Elution...");
			}
			else if (magneto->getCurrentfilter() == 6) {
				SetDlgItemText(IDC_STATIC_PROGRESS_STATUS, L"Lysis...");
			}
//			210118 KBH remove progress status "Mixing"
//			else if (magneto->getCurrentfilter() == 10) {
//				SetDlgItemText(IDC_STATIC_PROGRESS_STATUS, L"Mixing...");
//			}
			else if (magneto->getCurrentfilter() == 12) {
				SetDlgItemText(IDC_STATIC_PROGRESS_STATUS, L"Moving to PCR...");
			}
		}

		if (magneto->isIdle()) {
			progressStatus.SetPos(magneto->getTotalActionNumber());
			KillTimer(Magneto::TimerRuntaskID);
			SetDlgItemText(IDC_STATIC_PROGRESS_STATUS, L"PCR in progress...");
			m_Timer->startTimer(TIMER_DURATION, FALSE);
			return;
		}
	}
	else if (nIDEvent == Magneto::TimerCleanupTaskID) {
		// 220215 KBH Remove home task in CleanupTask
		//if (!magneto->runTask()) {
		//	// 211119 KBH Timer kill CleanupTask
		//	//KillTimer(Magneto::TimerRuntaskID); 
		//	KillTimer(Magneto::TimerCleanupTaskID);

		//	// 210119 KBH remove unused code 
		//	//AfxMessageBox(L"Limit 스위치가 설정되어 task 가 종료되었습니다.\n기기를 확인하세요.");

		//	// 210119 KBH Motor Stucked
		//	AfxMessageBox(L"motor가 stuck 되었습니다.\n기기를 확인하세요.");

		//	exit(0); // 211117 KBH if motor is stuck, program exit
		//	//initState();	// 210120 KBH state initialize 
		//	
		//	return;
		//}

		if (magneto->isIdle()) {
			KillTimer(Magneto::TimerCleanupTaskID);
			PCREndTask();
			// Reload original magneto protocol
			loadMagnetoProtocol();
			return;
		}
	}

	CDialogEx::OnTimer(nIDEvent);
}

LRESULT CMainDialog::OnmmTimer(WPARAM wParam, LPARAM lParam) {
	if (isStarted)
	{
		int errorPrevent = 0;
		timeTask();
	}

	RxBuffer rx;
	TxBuffer tx;
	float currentTemp = 0.0;

	memset(&rx, 0, sizeof(RxBuffer));
	memset(&tx, 0, sizeof(TxBuffer));

	tx.cmd = currentCmd;
	tx.currentTargetTemp = (BYTE)m_currentTargetTemp;
	tx.ledControl = 1;
	tx.led_wg = ledControl_wg;
	tx.led_r = ledControl_r;
	tx.led_g = ledControl_g;
	tx.led_b = ledControl_b;
	tx.compensation = compensation;
	tx.currentCycle = currentCycle;

	// pid 값을 buffer 에 복사한다.
	memcpy(&(tx.pid_p1), &(m_kp), sizeof(float));
	memcpy(&(tx.pid_i1), &(m_ki), sizeof(float));
	memcpy(&(tx.pid_d1), &(m_kd), sizeof(float));

	// integral max 값을 buffer 에 복사한다.
	memcpy(&(tx.integralMax_1), &(integralMax), sizeof(float));

	BYTE senddata[65] = { 0, };
	BYTE readdata[65] = { 0, };
	memcpy(senddata, &tx, sizeof(TxBuffer));

	device->Write(senddata);

	if (device->Read(&rx) == 0)
		return FALSE;

	memcpy(readdata, &rx, sizeof(RxBuffer));

	// 기기로부터 받은 온도 값을 받아와서 저장함.
	// convert BYTE pointer to float type for reading temperature value.
	memcpy(&currentTemp, &(rx.chamber_temp_1), sizeof(float));

	// 기기로부터 받은 Photodiode 값을 받아와서 저장함.
	photodiode_h = rx.photodiode_h;
	photodiode_l = rx.photodiode_l;

	if (currentCmd == CMD_FAN_OFF) {
		currentCmd = CMD_READY;
	}
	else if (currentCmd == CMD_PCR_STOP) {
		currentCmd = CMD_READY;
	}

	if (currentTemp < 0.0)
		return FALSE;

	// 150918 YJ added, For falling stage routine
	if (targetTempFlag && !freeRunning)
	{
		// target temp 이하가 되는 순간부터 freeRunning 으로 
		if (currentTemp <= m_currentTargetTemp)
		{
			freeRunning = true;
			freeRunningCounter = 0;
		}
	}

	if (freeRunning)
	{
		freeRunningCounter++;
		// 기기와 다르게 3초 후부터 arrival 로 인식하도록 변경
		if (freeRunningCounter >= (3000 / TIMER_DURATION))
		{
			targetTempFlag = false;
			freeRunning = false;
			freeRunningCounter = 0;
			isTargetArrival = true;
		}
	}

	if (fabs(currentTemp - m_currentTargetTemp) < m_cArrivalDelta && !targetTempFlag)
		isTargetArrival = true;

	// Check the error from device
	static bool onceShow = true;
	if (rx.currentError == ERROR_ASSERT && onceShow) {
		onceShow = false;
		AfxMessageBox(L"Software error occured!\nPlease contact to developer");
	}
	else if (rx.currentError == ERROR_OVERHEAT && onceShow) {
		onceShow = false;
		emergencyStop = true;
		// PCREndTask();
		cleanupTask();
	}

	// logging
	if (!logStopped) {
		CString values;
		double currentTime = (double)(timeGetTime() - recStartTime);
		recordingCount++;
		values.Format(L"%6d	%8.0f	%3.1f\n", recordingCount, currentTime, currentTemp);
		m_recFile.WriteString(values);

		// for log message per 1 sec
		/*
		if (recordingCount % 20 == 0) {
			int elapsed_time = (int)((double)(timeGetTime() - recStartTime) / 1000.);
			int min = elapsed_time / 60;
			int sec = elapsed_time % 60;
			CString elapseTime, lineTime, totalTime;
			elapseTime.Format(L"%dm %ds", min, sec);

			min = leftSec / 60;
			sec = leftSec % 60;

			// current left protocol time
			if (min == 0)
				lineTime.Format(L"%ds", sec);
			else
				lineTime.Format(L"%dm %ds", min, sec);

			CString tempStr;
			tempStr.Format(L"%3.1f", currentTemp);

			double lights = (double)(photodiode_h & 0x0f)*256. + (double)(photodiode_l);

			CString log;
			log.Format(L"cmd: %d, targetTemp: %3.1f, temp: %s, elapsed time: %s, line Time: %s, protocol Time: %s, device TargetArr: %d, mfc TargetArr: %d, free Running: %d, free Running Counter: %d, ArrivalDelta: %3.1f, tempFlag: %d, photodiode: %3.1f\n",
				currentCmd, m_currentTargetTemp, tempStr, elapseTime, lineTime, totalTime, (int)rx.targetArrival, (int)isTargetArrival, (int)freeRunning, (int)freeRunningCounter, m_cArrivalDelta, (int)targetTempFlag, lights);
			FileManager::log(log);
		}
		*/
	}

	return FALSE;
}

void CMainDialog::findPID()
{
	if (fabs(m_prevTargetTemp - m_currentTargetTemp) < .5)
		return; // if target temp is not change then do nothing 1->.5 correct

	double dist = 10000;
	int paramIdx = 0;

	for (UINT i = 0; i < pids.size(); i++)
	{
		double tmp = fabs(m_prevTargetTemp - pids[i].startTemp) + fabs(m_currentTargetTemp - pids[i].targetTemp);

		if (tmp < dist)
		{
			dist = tmp;
			paramIdx = i;
		}
	}

	m_kp = pids[paramIdx].kp;
	m_ki = pids[paramIdx].ki;
	m_kd = pids[paramIdx].kd;
}

void CMainDialog::initValues() {
	if (isStarted) {
		currentCmd = CMD_PCR_STOP;
	}

	magnetoProtocolIdx = 0;
	m_currentActionNumber = -1;
	m_leftGotoCount = -1;
	leftSec = 0;
	totalLeftSec = 0;
	m_timerCounter = 0;
	m_timeOut = 0;
	isTargetArrival = false;
	filterIndex = 0;
	filterRunning = false;
	shotCounter = 0;

	ledControl_wg = 1;
	ledControl_r = 1;
	ledControl_g = 1;
	ledControl_b = 1;

	m_kp = 0;
	m_ki = 0;
	m_kd = 0;
	emergencyStop = false;
	freeRunning = false;
	freeRunningCounter = 0;
	currentCycle = 0;
	recordingCount = 0;

	m_prevTargetTemp = m_currentTargetTemp = 25;
}

void CMainDialog::timeTask() {
	m_timerCounter++;

	// 1s 마다 실행되도록 설정
	if (m_timerCounter == (1000 / TIMER_DURATION))
	{
		m_timerCounter = 0;

		CString debug;
		debug.Format(L"Current cmd : %d, current action : %d target temp : %.1f, current left sec : %d\n", currentCmd, m_currentActionNumber, m_currentTargetTemp, leftSec);
		::OutputDebugString(debug);

		if (leftSec == 0) {
			m_currentActionNumber++;

			if ((m_currentActionNumber) >= currentProtocol.actionList.size())
			{
				::OutputDebugString(L"complete!\n");
				isCompletePCR = true;
				// PCREndTask();
				cleanupTask();
				return;
			}

			debug.Format(L"label : %s filterIndex : %d\n", currentProtocol.actionList[m_currentActionNumber].Label, filterIndex);
			::OutputDebugString(debug);

			// If the current protocol is SHOT
			if (currentProtocol.actionList[m_currentActionNumber].Label.Compare(L"SHOT") == 0)
			{
				if (filterIndex == 0) {
					// Check the this filter is used.
					if (!currentProtocol.useFam) {
						filterIndex = 1;
					}
				}
				if (filterIndex == 1) {
					// Check the this filter is used.
					if (!currentProtocol.useHex) {
						filterIndex = 2;
					}
				}
				if (filterIndex == 2) {
					// Check the this filter is used.
					if (!currentProtocol.useRox) {
						filterIndex = 3;
					}
				}
				if (filterIndex == 3) {
					// Check the this filter is used.
					if (!currentProtocol.useCY5) {
						filterIndex = 4;
					}
				}

				// 4 is finished
				if (filterIndex == 4) {
					filterIndex = 0;
					currentCycle++;

					CString values;
					double currentTime = (double)(timeGetTime() - recStartTime);
					double sensorValue = 0.0, sensorValue2 = 0.0, sensorValue3 = 0.0, sensorValue4 = 0.0;

					if (currentProtocol.useFam) {
						sensorValue = sensorValuesFam[currentCycle - 1];
					}
					if (currentProtocol.useHex) {
						sensorValue2 = sensorValuesHex[currentCycle - 1];
					}
					if (currentProtocol.useRox) {
						sensorValue3 = sensorValuesRox[currentCycle - 1];
					}
					if (currentProtocol.useCY5) {
						sensorValue4 = sensorValuesCy5[currentCycle - 1];
					}

					values.Format(L"%6d	%8.0f	%3.1f	%3.1f	%3.1f	%3.1f\n", currentCycle, currentTime, sensorValue, sensorValue2, sensorValue3, sensorValue4);
					::OutputDebugString(values);
					m_recPDFile.WriteString(values);
				}
				else {
					// Run the filter
					if (filterRunning) {
						// Maybe finished..
						if (magneto->isFilterActionFinished() == false) {
							int* led;
							vector<double>* sensorValues;

							if (filterIndex == 0) {
								led = &ledControl_b;
								sensorValues = &sensorValuesFam;
							}
							else if (filterIndex == 1) {
								led = &ledControl_wg;
								sensorValues = &sensorValuesHex;
							}
							else if (filterIndex == 2) {
								led = &ledControl_g;
								sensorValues = &sensorValuesRox;
							}
							else if (filterIndex == 3) {
								led = &ledControl_r;
								sensorValues = &sensorValuesCy5;
							}

							// Turn on the led
							*led = 0;

							// Shot sequence
							shotCounter++;
							if (shotCounter >= 2) {
								// Getting the photodiode data
								double lights = (double)(photodiode_h & 0x0f) * 256. + (double)(photodiode_l);
								sensorValues->push_back(lights);

								debug.Format(L"filter value : %d, %d, %f\n", photodiode_h, photodiode_l, lights);
								::OutputDebugString(debug);

								// Turn off led
								*led = 1;
								shotCounter = 0;

								// Next filter
								filterIndex++;
								filterRunning = false;
							}
						}
					}
					else {
						filterRunning = true;
						magneto->runFilterAction(filterIndex);
					}
					m_currentActionNumber--;	// For checking the shot command 
				}
			}
			// If the current protocol is GOTO
			else if (currentProtocol.actionList[m_currentActionNumber].Label.Compare(L"GOTO") == 0)
			{
				if (m_leftGotoCount < 0)
					m_leftGotoCount = (int)currentProtocol.actionList[m_currentActionNumber].Time;

				if (m_leftGotoCount == 0)
					m_leftGotoCount = -1;
				else
				{
					m_leftGotoCount--;

					// GOTO 문의 target label 값을 넣어줌
					CString tmpStr;
					tmpStr.Format(L"%d", (int)currentProtocol.actionList[m_currentActionNumber].Temp);

					int pos = 0;
					for (pos = 0; pos < currentProtocol.actionList.size(); ++pos)
						if (tmpStr.Compare(currentProtocol.actionList[pos].Label) == 0)
							break;

					m_currentActionNumber = pos - 1;
				}
			}
			// Label
			else {
				m_prevTargetTemp = m_currentTargetTemp;
				m_currentTargetTemp = (int)currentProtocol.actionList[m_currentActionNumber].Temp;

				targetTempFlag = m_prevTargetTemp > m_currentTargetTemp;

				isTargetArrival = false;
				leftSec = (int)(currentProtocol.actionList[m_currentActionNumber].Time);
				m_timeOut = TIMEOUT_CONST * 10;

				// find the proper pid values.
				findPID();
			}
		}
		else { // The action is in progress.
			if (!isTargetArrival)
			{
				m_timeOut--;

				if (m_timeOut == 0)
				{
					AfxMessageBox(L"The target temperature cannot be reached!!");
					// PCREndTask();
					cleanupTask();
				}
			}
			else {
				leftSec--;
				totalLeftSec--;

				CString leftTime;
				int min = totalLeftSec / 60;
				int sec = totalLeftSec % 60;

				if (min == 0)
					leftTime.Format(L"%02ds", sec);
				else
					leftTime.Format(L"%02dm %02ds", min, sec);
				SetDlgItemText(IDC_STATIC_PROGRESS_REMAINING_TIME, leftTime);
			}
		}
	}
}

void CMainDialog::cleanupTask() {
	// Stop timer
	m_Timer->stopTimer();

	magneto->Alarmreset();	// 210120 KBH Magneto AlarmReset

	// 220111 KBH disk & syringe motors not homing after PCR protocol is over
	// Start cleanup timer
	// Setting the home command
	// CString magnetoProtocolRes = magneto->loadProtocolFromData(L"home");

	// if (magneto->isCompileSuccess(magnetoProtocolRes)) {
	// 	// initialize the protocol
	// 	vector<ActionBeans> treeList;
	// 	magneto->generateActionList(treeList);
	// }

	// magneto->start();
	SetTimer(Magneto::TimerCleanupTaskID, Magneto::TimerCleanupTaskDuration, NULL);
}

void CMainDialog::PCREndTask() {
	while (true)
	{
		RxBuffer rx;
		TxBuffer tx;

		memset(&rx, 0, sizeof(RxBuffer));
		memset(&tx, 0, sizeof(TxBuffer));

		tx.cmd = CMD_PCR_STOP;

		BYTE senddata[65] = { 0, };
		BYTE readdata[65] = { 0, };
		memcpy(senddata, &tx, sizeof(TxBuffer));

		device->Write(senddata);

		device->Read(&rx);

		memcpy(readdata, &rx, sizeof(RxBuffer));

		if (rx.state == STATE_READY) break;

		Sleep(TIMER_DURATION);
	}

	initValues();
	clearLog();

	isStarted = false;

	progressStatus.SetPos(0);
	GetDlgItem(IDC_COMBO_PROTOCOLS)->EnableWindow();
	GetDlgItem(IDC_BUTTON_SETUP)->EnableWindow();
	GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow();
	GetDlgItem(IDC_BUTTON_START)->EnableWindow();	// 210120 KBH enable start button

	SetDlgItemText(IDC_BUTTON_START, L"Start");
	SetDlgItemText(IDC_STATIC_PROGRESS_STATUS, L"Idle..");

	if (!emergencyStop)
	{
		if (isCompletePCR) {
			// Getting the current date time
			CTime cTime = CTime::GetCurrentTime();
			CString dateTime;
			dateTime.Format(L"%04d.%02d.%02d %02d:%02d:%02d", cTime.GetYear(), cTime.GetMonth(), cTime.GetDay(),
				cTime.GetHour(), cTime.GetMinute(), cTime.GetSecond());

			// result index
			int resultIndex = 0;

			if (currentProtocol.useFam) {
				setCTValue(dateTime, sensorValuesFam, resultIndex++, 0);
			}
			if (currentProtocol.useHex) {
				setCTValue(dateTime, sensorValuesHex, resultIndex++, 1);
			}
			if (currentProtocol.useRox) {
				setCTValue(dateTime, sensorValuesRox, resultIndex++, 2);
			}
			if (currentProtocol.useCY5) {
				setCTValue(dateTime, sensorValuesCy5, resultIndex++, 3);
			}

			InvalidateRect(&CRect(226, 149, 453, 262));

			AfxMessageBox(L"PCR ended!!");
		}
		else AfxMessageBox(L"PCR incomplete!!");
	}
	else
	{
		AfxMessageBox(L"Emergency stop!(overheating)");
	}

	// 210714 KBH : reset SersorValues 
	sensorValuesFam.clear();
	sensorValuesHex.clear();
	sensorValuesRox.clear();
	sensorValuesCy5.clear();

	emergencyStop = false;
	isCompletePCR = false;

	// reset the remaining time
	calcTotalTime();
}

static CString filterTable[4] = { L"FAM", L"HEX", L"ROX", L"CY5" };

void CMainDialog::setCTValue(CString dateTime, vector<double>& sensorValue, int resultIndex, int filterIndex) {
	// save the result and setting the result
	CString result = L"FAIL";
	CString ctText = L"";
	CString filterLabel[4] = { currentProtocol.labelFam, currentProtocol.labelHex, currentProtocol.labelRox, currentProtocol.labelCY5 };

	// 220222 KBH filter threshold values array
	float filterThreshold[4] = { currentProtocol.thresholdFam, currentProtocol.thresholdHex, currentProtocol.thresholdRox, currentProtocol.thresholdCY5 };

	// ignore the data when the data is over the 10
	int idx = sensorValue.size();

	// If the idx is under the 10, fail
	if (idx >= 10) {
		// BaseMean value
		float baseMean = 0.0;
		// 220215 KBH Change baseMean calculation range (0 ~ 10 -> 2 ~ 15)
		for (int i = 2; i < 15; ++i) {
			baseMean += sensorValue[i];
		}
		baseMean /= 13.;

		// 220222 KBH Change how thresholds are loaded.
		float logThreshold = filterThreshold[filterIndex];
		float ct;

		// 211119 KBH start index change (i = 0 -> i = 10)
		for (int i = 10; i < sensorValue.size(); ++i) {
			if (log(sensorValue[i] - baseMean) > logThreshold) {
				idx = i;
				break;
			}
		}

		if (idx >= sensorValue.size() || idx <= 0) {
			// 210714 KBH Change "Not detected" to "Negative"
			//result = L"Not detected";
			result = L"Negative";
		}
		else {
			double resultRange[4] = { currentProtocol.ctFam, currentProtocol.ctHex, currentProtocol.ctRox, currentProtocol.ctCY5 };

			float cpos = idx + 1;
			float cval = log(sensorValue[idx] - baseMean);
			float delta = cval - log(sensorValue[idx - 1] - baseMean);
			ct = cpos - (cval - logThreshold) / delta;
			ctText.Format(L"%.2f", ct);

			if (resultRange[filterIndex] <= ct) {
				result = L"Negative";
			}
			else {
				result = L"Positive";
			}
		}
	}

	vector<History> historyList;
	FileManager::loadHistory(historyList);

	History history(dateTime, filterLabel[filterIndex], filterTable[filterIndex], ctText, result);
	historyList.push_back(history);
	FileManager::saveHistory(historyList);

	GV_ITEM item;
	item.mask = GVIF_TEXT | GVIF_FORMAT;
	item.row = resultIndex + 1;
	item.col = 1;
	item.nFormat = DT_CENTER | DT_WORDBREAK;

	item.strText = result;

	// resultTable.SetItemFont(item.row, item.col, )
	resultTable.SetItem(&item);
}

// 200804 KBH change log file name 
void CMainDialog::initLog() {
	long serialNumber = magneto->getSerialNumber();
	CreateDirectory(L"./Record/", NULL);

	CString fileName, fileName2;
	CTime time = CTime::GetCurrentTime();
	CString currentTime = time.Format(L"%Y%m%d-%H-%M-%S"); // 220402 KBH 오타수정
	
	// change file name
	//fileName = time.Format(L"./Record/%Y%m%d-%H%M-%S.txt");
	//fileName2 = time.Format(L"./Record/pd%Y%m%d-%H%M-%S.txt");

	fileName.Format(L"./Record/log%06ld-%s.txt", serialNumber, currentTime);
	fileName2.Format(L"./Record/log%06ld-pd%s.txt", serialNumber, currentTime);

	m_recFile.Open(fileName, CStdioFile::modeCreate | CStdioFile::modeWrite);
	m_recFile.WriteString(L"Number	Time	Temperature\n");

	m_recPDFile.Open(fileName2, CStdioFile::modeCreate | CStdioFile::modeWrite);
	m_recPDFile.WriteString(L"Cycle	Time	FAM	HEX	ROX	CY5\n");

	recStartTime = timeGetTime();
}

void CMainDialog::clearLog() {
	logStopped = true;
	if (m_recFile != NULL) {
		m_recFile.Close();
	}
	if (m_recPDFile != NULL) {
		m_recPDFile.Close();
	}
}

// 220325 KBH Device Change Handler
BOOL CMainDialog::OnDeviceChange(UINT nEventType, DWORD dwData)
{
	if (isConnected)
	{
		// get current connected device serial number
		CString deviceSerial;
		int selectedIdx = deviceList.GetCurSel();
		deviceList.GetLBText(selectedIdx, deviceSerial);
		int serial_number = _ttoi(deviceSerial);

		// check device list
		int deviceNums = device->GetDevices();
		bool detected = false;

		// check Running State
		bool isPCR = false;
		CString progressState;
		GetDlgItemText(IDC_STATIC_PROGRESS_STATUS, progressState);
		isPCR = progressState.Compare(L"PCR in progress...") == 0;

		for (int i = 0; i < deviceNums; ++i)
		{
			CString serial = device->GetDeviceSerial(i).Mid(5);
			// check existed connected device
			if (deviceSerial.Compare(serial) == 0)
			{
				detected = true;
			}
		}

		if (isConnectionBroken && detected)
		{
			// Open Device & Start Timer
			char serialBuffer[20];
			sprintf(serialBuffer, "QuPCR%06d", serial_number);

			BOOL res = device->OpenDevice(LS4550EK_VID, LS4550EK_PID, serialBuffer, TRUE);
			isConnectionBroken = false;

			if (isStarted && isPCR)
			{
				m_Timer->startTimer(TIMER_DURATION, FALSE);
			}

			FileManager::log(L"USB Connected", serial_number);
		}
		else if (!isConnectionBroken && !detected)
		{
			// Stop Timer & Close Device
			if (isStarted && isPCR)
			{
				m_Timer->stopTimer();
			}
			device->CloseDevice();
			isConnectionBroken = true;
	
			FileManager::log(L"USB Disconnected", serial_number);
		}
	}
	return false;
}