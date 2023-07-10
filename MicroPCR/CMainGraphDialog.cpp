// CMainDialog.cpp: 구현 파일
//

#include "stdafx.h"

#include "CMainGraphDialog.h"
#include "afxdialogex.h"
#include "resource.h"

#include "PasswordDialog.h"
#include "SetupDialog.h"
#include "DeviceSetup.h"
#include "FileManager.h"
#include "ProgressThread.h"
#include "ConfirmDialog.h"

#include <sstream>
#include <numeric>
#include <Dbt.h> // 220325 KBH Device Change Handler

// CMainDialog 대화 상자

IMPLEMENT_DYNAMIC(CMainGraphDialog, CDialogEx)

CMainGraphDialog::CMainGraphDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_MAIN_GRAPH, pParent)
	, lastSelectedProtocol(L"")
	, isProtocolLoaded(false)
	, isConnected(false)
	, isStarted(false)
	, isFirstDraw(false)
	, isCompletePCR(false)
	, isTargetArrival(false)
	, m_prevTargetTemp(25)
	, m_currentTargetTemp(25)
	, m_kp(0.0)
	, m_ki(0.0)
	, m_kd(0.0)
	, m_cArrivalDelta(0.5)
	, maxCycles(45)
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
	, useFam(false)
	, useHex(false)
	, useRox(false)
	, useCy5(false)
	, m_strStylesPath(L"./")
	, timerBlocked(false)
	, serverProcess()
	, usbSerial(0)
	, externalPower(true)
	, externalPowerCount(0)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	XTPSkinManager()->SetApplyOptions(XTPSkinManager()->GetApplyOptions() | xtpSkinApplyMetrics);
	XTPSkinManager()->LoadSkin(m_strStylesPath + _T("frcj.cjstyles"));
}

CMainGraphDialog::~CMainGraphDialog()
{
	if (device != NULL)
		delete device;
	if (m_Timer != NULL)
		delete m_Timer;

	// Process Stop HelloPCR-Runner.exe 
	serverProcess.StopProcess();
}

void CMainGraphDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);

	DDX_Control(pDX, IDC_CUSTOM_RESULT_TABLE, resultTable);

	DDX_Control(pDX, IDC_COMBO_PROTOCOLS, protocolList);
	DDX_Control(pDX, IDC_COMBO_DEVICE_LIST, deviceList);
	// progressStatus
}

BEGIN_MESSAGE_MAP(CMainGraphDialog, CDialogEx)
	ON_WM_PAINT()
	ON_BN_CLICKED(IDC_BUTTON_START, &CMainGraphDialog::OnBnClickedButtonStart)
	ON_BN_CLICKED(IDC_BUTTON_SETUP, &CMainGraphDialog::OnBnClickedButtonSetup)
	ON_LBN_SELCHANGE(IDC_COMBO_PROTOCOLS, &CMainGraphDialog::OnLbnSelchangeComboProtocols)
	ON_BN_CLICKED(IDC_BUTTON_CONNECT, &CMainGraphDialog::OnBnClickedButtonConnect)
	ON_WM_TIMER()
	ON_MESSAGE(WM_MMTIMER, OnmmTimer)

	ON_BN_CLICKED(IDC_BUTTON_FILTER_FAM, &CMainGraphDialog::OnBnClickedButtonFilterFam)
	ON_BN_CLICKED(IDC_BUTTON_FILTER_HEX, &CMainGraphDialog::OnBnClickedButtonFilterHex)
	ON_BN_CLICKED(IDC_BUTTON_FILTER_ROX, &CMainGraphDialog::OnBnClickedButtonFilterRox)
	ON_BN_CLICKED(IDC_BUTTON_FILTER_CY5, &CMainGraphDialog::OnBnClickedButtonFilterCy5)

	ON_WM_DEVICECHANGE() // 220325 KBH Append Device Change Handler
END_MESSAGE_MAP()

// CMainDialog 메시지 처리기
BOOL CMainGraphDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	GetDlgItem(IDC_BUTTON_START)->EnableWindow(FALSE);
	SetDlgItemText(IDC_EDIT_CONNECTI_STATUS, L"Disconnected");
	SetDlgItemText(IDC_STATIC_PROGRESS_STATUS, L"Idle..");

	// KJD230617 progressStatus.SetRange(0, 100);

	SetIcon(m_hIcon, TRUE);	 // set Large Icon
	SetIcon(m_hIcon, FALSE); // set Small Icon

	// Initialize the chart
	initChart();

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

void CMainGraphDialog::OnBnClickedButtonSetup()
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

BOOL CMainGraphDialog::PreTranslateMessage(MSG* pMsg)
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

void CMainGraphDialog::initChart() {
	CAxis* axis;
	axis = m_Chart.AddAxis(kLocationBottom);
	axis->m_TitleFont.lfWidth = 20;
	axis->m_TitleFont.lfHeight = 20;
	axis->SetTitle(L"PCR Cycles");
	axis->SetRange(0, maxCycles);
	axis->SetTickCount(8); // KBH230622 Setting X-Axis Major Ticks 

	axis = m_Chart.AddAxis(kLocationLeft);
	axis->m_TitleFont.lfWidth = 20;
	axis->m_TitleFont.lfHeight = 20;

	//210830 KJD Setting Axis and m_Chart
	axis->SetRange(-200, 4096);
	axis->SetTickCount(5);
	axis->m_ytickPos[0] = 0;
	axis->m_ytickPos[1] = 1000;
	axis->m_ytickPos[2] = 2000;
	axis->m_ytickPos[3] = 3000;
	axis->m_ytickPos[4] = 4000;

	m_Chart.m_UseMajorVerticalGrids = TRUE;
	m_Chart.m_UseMajorHorizontalGrids = TRUE;
	m_Chart.m_MajorGridLineStyle = PS_DOT;
	m_Chart.m_BackgroundColor = GetSysColor(COLOR_3DFACE);

	// Load bitmap
	offImg.LoadBitmapW(IDB_BITMAP_OFF);
	famImg.LoadBitmapW(IDB_BITMAP_FAM);
	hexImg.LoadBitmapW(IDB_BITMAP_HEX);
	roxImg.LoadBitmapW(IDB_BITMAP_ROX);
	cy5Img.LoadBitmapW(IDB_BITMAP_CY5);

	SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_FAM, offImg);
	SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_HEX, offImg);
	SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_ROX, offImg);
	SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_CY5, offImg);

	// 210910 KBH calc graph rect (IDC_GROUP_CONNECTION(right, top), IDC_GROUP_CT(right, top) ->  Graph(left, top, right, bottom)
	// static position -> dynamic position 

	GetDlgItem(IDC_STATIC_PLOT)->GetWindowRect(&m_graphRect);
	ScreenToClient(&m_graphRect);
}

void CMainGraphDialog::OnPaint() {
	CPaintDC dc(this); // device context for painting
					   // TODO: 여기에 메시지 처리기 코드를 추가합니다.
					   // 그리기 메시지에 대해서는 CDialogEx::OnPaint()을(를) 호출하지 마십시오.

	if (IsIconic())
	{
		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		dc.DrawIcon(x, y, m_hIcon);
	}
	else {
		CRect graphRect(&m_graphRect); // copy CRect

		int oldMode = dc.SetMapMode(MM_LOMETRIC);
		//graphRect.SetRect(15, 130, 470, 500); 211117 KBH static position -> dynamic position 

		dc.DPtoLP((LPPOINT)&graphRect, 2);
		CDC* dc2 = CDC::FromHandle(dc.m_hDC);
		m_Chart.OnDraw(dc2, graphRect, graphRect);
		dc.SetMapMode(oldMode);
	}

}

HCURSOR CMainGraphDialog::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

static const int RESULT_TABLE_COLUMN_WIDTHS[2] = { 100, 130 };

void CMainGraphDialog::initResultTable() {
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

void CMainGraphDialog::initProtocol() {
	useFam = currentProtocol.useFam;
	useHex = currentProtocol.useHex;
	useRox = currentProtocol.useRox;
	useCy5 = currentProtocol.useCY5;

	if (currentProtocol.useFam) {
		SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_FAM, famImg);
		GetDlgItem(IDC_BUTTON_FILTER_FAM)->EnableWindow();
	}
	else {
		SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_FAM, offImg);
		GetDlgItem(IDC_BUTTON_FILTER_FAM)->EnableWindow(FALSE);
	}

	if (currentProtocol.useHex) {
		SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_HEX, hexImg);
		GetDlgItem(IDC_BUTTON_FILTER_HEX)->EnableWindow();
	}
	else {
		SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_HEX, offImg);
		GetDlgItem(IDC_BUTTON_FILTER_HEX)->EnableWindow(FALSE);
	}

	if (currentProtocol.useRox) {
		SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_ROX, roxImg);
		GetDlgItem(IDC_BUTTON_FILTER_ROX)->EnableWindow();
	}
	else {
		SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_ROX, offImg);
		GetDlgItem(IDC_BUTTON_FILTER_ROX)->EnableWindow(FALSE);
	}

	if (currentProtocol.useCY5) {
		SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_CY5, cy5Img);
		GetDlgItem(IDC_BUTTON_FILTER_CY5)->EnableWindow();
	}
	else {
		SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_CY5, offImg);
		GetDlgItem(IDC_BUTTON_FILTER_CY5)->EnableWindow(FALSE);
	}

	calcTotalTime();
}

void CMainGraphDialog::loadProtocolList() {
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
		initProtocol();
		isProtocolLoaded = true;

		// Enable the window if the device is connected
		if (isConnected) {
			GetDlgItem(IDC_BUTTON_START)->EnableWindow();
		}
	}
	else {
		AfxMessageBox(L"You need to make the protocol first.");
	}
}

void CMainGraphDialog::loadConstants() {
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

	CAxis* axis = m_Chart.GetAxisByLocation(kLocationBottom);
	axis->SetRange(0, maxCycles);
	//InvalidateRect(&CRect(15, 130, 470, 500));
	InvalidateRect(&m_graphRect, FALSE); // 211117 KBH static position -> dynamic position 
}

void CMainGraphDialog::OnLbnSelchangeComboProtocols() {
	int selectedIdx = protocolList.GetCurSel();
	protocolList.GetLBText(selectedIdx, lastSelectedProtocol);

	// Load the data from protocol
	currentProtocol = protocols[selectedIdx];
	initProtocol();
	initResultTable();
}

void CMainGraphDialog::calcTotalTime() {
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

void CMainGraphDialog::initConnection() {
	m_Timer = new CMMTimers(1, GetSafeHwnd());
	device = new CDeviceConnect(GetSafeHwnd());
}

void CMainGraphDialog::initPCRDevices() {
	deviceList.ResetContent();
	
	int deviceNums = device->GetDevices();
	
	for (int i = 0; i < deviceNums; ++i) {
		CString deviceSerial = device->GetDeviceSerial(i);

		// Remove the HelloPCR string
		deviceList.AddString(deviceSerial.Mid(8));
	}

	if (deviceList.GetCount() != 0) {
		deviceList.SetCurSel(0);
	}
}

void CMainGraphDialog::OnBnClickedButtonConnect()
{
	CString buttonState;
	GetDlgItemText(IDC_BUTTON_CONNECT, buttonState);

	// Disable the buttons
	GetDlgItem(IDC_BUTTON_SETUP)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(FALSE);

	// KJD230617 magneto bypass,
	if (buttonState.Compare(L"Connect") == 0) {
		connectDevice();
	}
	else {
		disconnectDevice();
	}

	GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON_SETUP)->EnableWindow(TRUE);
}

void CMainGraphDialog::OnBnClickedButtonStart()
{
	// 211117 KBH change dialog (using AfxMessageBox)
	CString message;
	message = !isStarted ? L"Want to start the protocol?" : L"Want to stop the protocol?";
	
	if (AfxMessageBox(message, MB_YESNO | MB_ICONQUESTION) != IDYES) {
		return;
	}


// 211117 KBH chip connection check 
// 220111 KBH changed iteration count from 5 to 20 (waiting time is changed from 250ms to 1000ms)
// (20번째 read_buffer 에 온도 값이 10도 이하일 경우 PCR Chip Connection Error)
// change the position of sleep function
	// Disable start button
	GetDlgItem(IDC_BUTTON_START)->EnableWindow(FALSE);

	if (isConnected && isProtocolLoaded) {
		isStarted = !isStarted;
		if (isStarted) {
			// KJD230617 progressStatus.SetPos(0);
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

			clearChartValue();
			
			// KBH230621 Clear ResultTable Text
			int ID_LIST[4] = { IDC_EDIT_CT_FAM, IDC_EDIT_CT_HEX, IDC_EDIT_CT_ROX, IDC_EDIT_CT_CY5 };
			for (int i = 0; i < 4; i++)
				SetDlgItemText(ID_LIST[i], L"");
			initResultTable();
			RedrawWindow();

			// KJD230617 magneto start is not required
			// magneto->start();
			// SetTimer(Magneto::TimerRuntaskID, Magneto::TimerRuntaskDuration, NULL);
			// m_Timer->startTimer(TIMER_DURATION, FALSE);//KJD230617 PCR Start

			// Enable stop button
			GetDlgItem(IDC_BUTTON_START)->EnableWindow(TRUE);
			serverProcess.SetIndicatorLED(CMD_LED_RUN); // KBH230629 Device Indicator LED Running

		}
		else {
			PCREndTask();
			serverProcess.SetIndicatorLED(CMD_LED_READY); // KBH230629 Device Indicator LED Ready
		}
	}
	else {
		AfxMessageBox(L"Error occured!");
	}
}


LRESULT CMainGraphDialog::OnmmTimer(WPARAM wParam, LPARAM lParam) {
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

	CString tempString;
	tempString.Format(L"%.1f ℃", currentTemp);
	SetDlgItemText(IDC_EDIT_CURRENT_TEMP, tempString);

	if (fabs(currentTemp - m_currentTargetTemp) < m_cArrivalDelta && !targetTempFlag)
		isTargetArrival = true;

	// KBH230704 Check external power is supplied
	// existing photodiode is not using, so replace check external power connection 
 //#ifndef EMULATOR
	/*int voltage = (int)(photodiode_h & 0x0f) * 256 + (int)(photodiode_l);
	if (voltage < 1700)
		externalPower = ++externalPowerCount < 10;
	else 
		externalPowerCount = 0;*/
 //#endif
	// Check the error from device
	static bool onceShow = true;
	if (rx.currentError == ERROR_ASSERT && onceShow) {
		onceShow = false;
		AfxMessageBox(L"Software error occured!\nPlease contact to developer");
	}
	else if (rx.currentError == ERROR_OVERHEAT && onceShow) {
		onceShow = false;
		emergencyStop = true;
		PCREndTask();// KJD 
		serverProcess.SetIndicatorLED(CMD_LED_ERROR); // KBH230629 Device Indicator LED Error
	} 
	//else if (!externalPower && onceShow) { 
	//	onceShow = false;
	//	serverProcess.SetIndicatorLED(CMD_LED_OFF); // if external power is not supplied, turn off indicator LED
	//	if (isStarted) PCREndTask();
	//	disconnectDevice();
	//	AfxMessageBox(L"External power cable is not connected");
	//	externalPower = true;
	//	externalPowerCount = 0;
	//	onceShow = true;
	//}

	// logging
	if (!logStopped && isStarted) {
		CString values;
		double currentTime = (double)(timeGetTime() - recStartTime);
		recordingCount++;
		values.Format(L"%6d	%8.0f	%3.1f\n", recordingCount, currentTime, currentTemp);
		m_recFile.WriteString(values);
	}

	return FALSE;
}

void CMainGraphDialog::findPID()
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

void CMainGraphDialog::initValues() {
	if (isStarted) {
		currentCmd = CMD_PCR_STOP;
	}

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

void CMainGraphDialog::timeTask() {
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
				PCREndTask(); // KJD230622 call PCREndTask function
				serverProcess.SetIndicatorLED(CMD_LED_READY); // KBH230629 Device Indicator LED Ready
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
						sensorValue = sensorValuesFam[currentCycle];
					}
					if (currentProtocol.useHex) {
						sensorValue2 = sensorValuesHex[currentCycle];
					}
					if (currentProtocol.useRox) {
						sensorValue3 = sensorValuesRox[currentCycle];
					}
					if (currentProtocol.useCY5) {
						sensorValue4 = sensorValuesCy5[currentCycle];
					}

					values.Format(L"%6d	%8.0f	%3.1f	%3.1f	%3.1f	%3.1f\n", currentCycle, currentTime, sensorValue, sensorValue2, sensorValue3, sensorValue4);
					m_recPDFile.WriteString(values);
				}
				else {
					// int* led; KBH230621 Delete LED control
					vector<double>* sensorValues;

					if (filterIndex == 0) {
						// led = &ledControl_b;
						sensorValues = &sensorValuesFam;
					}
					else if (filterIndex == 1) {
						// led = &ledControl_wg;
						sensorValues = &sensorValuesHex;
					}
					else if (filterIndex == 2) {
						// led = &ledControl_g;
						sensorValues = &sensorValuesRox;
					}
					else if (filterIndex == 3) {
						// led = &ledControl_r;
						sensorValues = &sensorValuesCy5;
					}

					// Turn on the led
					// *led = 0;
					shotCounter++;
					if (shotCounter == 1)
					{
						serverProcess.Shot(filterIndex, currentCycle, experimentDate.Format(L"%Y%m%d-%H%M%S"));
					} 
					// Shot sequence
					else if (shotCounter >= 2) {
						// KBH230620 photo diode is not used
						//// Getting the photodiode data
						// double lights = (double)(photodiode_h & 0x0f) * 256. + (double)(photodiode_l);

						double lights = serverProcess.Status();
						if (lights != -1)
						{
							sensorValues->push_back(lights);

							setChartValue();

							// debug.Format(L"filter value : %d, %d, %f\n", photodiode_h, photodiode_l, lights);
							// ::OutputDebugString(debug);

							// Turn off led
							// 	*led = 1;
							shotCounter = 0;

							// Next filter
							filterIndex++;
							// filterRunning = false;
						}
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
					PCREndTask();
					serverProcess.SetIndicatorLED(CMD_LED_ERROR); // KBH230629 Device Indicator LED Error
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

void CMainGraphDialog::PCREndTask() {
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

	// KJD230617 progressStatus.SetPos(0);
	GetDlgItem(IDC_COMBO_PROTOCOLS)->EnableWindow();
	GetDlgItem(IDC_BUTTON_SETUP)->EnableWindow();
	GetDlgItem(IDC_BUTTON_CONNECT)->EnableWindow();
	GetDlgItem(IDC_BUTTON_START)->EnableWindow();
	SetDlgItemText(IDC_BUTTON_START, L"Start");
	SetDlgItemText(IDC_STATIC_PROGRESS_STATUS, L"Idle..");
	SetDlgItemText(IDC_EDIT_CT_FAM, L"");
	SetDlgItemText(IDC_EDIT_CT_HEX, L"");
	SetDlgItemText(IDC_EDIT_CT_ROX, L"");
	SetDlgItemText(IDC_EDIT_CT_CY5, L"");
	SetDlgItemText(IDC_EDIT_CURRENT_TEMP, L"");

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
			// KBH230621 Update ResultTable
			// InvalidateRect(&CRect(149, 309, 156, 72));
			RedrawWindow(); 

			AfxMessageBox(L"PCR ended!!");
		}
		else AfxMessageBox(L"PCR incomplete!!");
	}
	else
	{
		AfxMessageBox(L"Emergency stop!(overheating)");
	}

	// 211117 KBH reset SersorValues 
	sensorValuesFam.clear();
	sensorValuesHex.clear();
	sensorValuesRox.clear();
	sensorValuesCy5.clear();

	emergencyStop = false;
	isCompletePCR = false;

	// reset the remaining time
	calcTotalTime();
}

void CMainGraphDialog::setChartValue() {
	if (isStarted) {
		m_Chart.DeleteAllData();

		double* dataFam = new double[sensorValuesFam.size() * 2];
		double* dataHex = new double[sensorValuesHex.size() * 2];
		double* dataRox = new double[sensorValuesRox.size() * 2];
		double* dataCy5 = new double[sensorValuesCy5.size() * 2];
		vector<double> copySensorValuesFam, copySensorValuesHex, copySensorValuesRox, copySensorValuesCy5;

		bool* useColors = new bool[4]{ useFam, useHex, useRox, useCy5 };
		double** datas = new double* [4]{ dataFam, dataHex, dataRox, dataCy5 }; // 
		vector<double>* sensorValues = new vector<double>[4]{ sensorValuesFam, sensorValuesHex, sensorValuesRox, sensorValuesCy5 };
		vector<double>* copySensorValues = new vector<double>[4]{ copySensorValuesFam, copySensorValuesHex, copySensorValuesRox, copySensorValuesCy5 };
		COLORREF* colors = new COLORREF[4]{ RGB(0, 0, 255),  RGB(0, 255, 0), RGB(0, 128, 0), RGB(255, 0, 0) };

		int maxY = 0;

		for (int idx = 0; idx < 4; idx++) {
			if (!useColors[idx]) continue;
			double* targetData = datas[idx];
			COLORREF color = colors[idx];
			vector<double> values = sensorValues[idx];
			vector<double> copyValues = copySensorValues[idx];
			double meanValues = 0.0;
			if (values.size() < 16) {
				double sumValues = std::accumulate(values.begin(), values.end(), 0.0);
				meanValues = sumValues / values.size();

				for (int x = 0; x < values.size(); x++) {
					copyValues.push_back(values[x] - meanValues);
				}
			}
			else {
				float bs = 4, be = 16;// line fitting at cycle 4~15 data
				double xm = 0, ym = 0, sxx = 0, syy = 0, sxy = 0, a = 0, b = 0;
				for (int x = bs; x < be; x++) {
					double y = values[x];
					xm += x;
					ym += y;
					sxx += x * x;
					sxy += x * y;
				}
				double nos = be - bs;
				xm = xm / nos;
				ym = ym / nos;
				sxx = sxx / nos - xm * xm;
				sxy = sxy / nos - xm * ym;
				a = sxy / sxx;
				b = ym - a * xm;

				for (int x = 0; x < values.size(); x++) { //sensorValue size = idx
					copyValues.push_back(values[x] - (a * x + b));
				}
			}
			int nDims = 2, dims[2] = { 2, copyValues.size() };
			for (int x = 0; x < copyValues.size(); x++) {
				targetData[x] = x;
				targetData[x + copyValues.size()] = copyValues[x];
			}
			m_Chart.SetDataColor(m_Chart.AddData(targetData, nDims, dims), color);

			int tempMax = *max_element(copyValues.begin(), copyValues.end());

			if (maxY < tempMax) {
				maxY = tempMax;
			}
		}

		maxY += displayDelta;

		// Not use the range when the value is changed.
		CAxis* axis = m_Chart.GetAxisByLocation(kLocationLeft);
		// Not used maxY now
		//axis->SetRange(-displayDelta, maxY);	// 210203 KBH Fixed Y range 

		//InvalidateRect(&CRect(15, 130, 470, 500));
		InvalidateRect(&m_graphRect, FALSE); // 211117 KBH static position -> dynamic position
		Invalidate(FALSE);
	}
}

void CMainGraphDialog::clearChartValue() {
	sensorValuesFam.clear();
	sensorValuesHex.clear();
	sensorValuesRox.clear();
	sensorValuesCy5.clear();

	sensorValuesFam.push_back(1.0);
	sensorValuesHex.push_back(1.0);
	sensorValuesRox.push_back(1.0);
	sensorValuesCy5.push_back(1.0);

	m_Chart.DeleteAllData();

	CAxis* axis = m_Chart.GetAxisByLocation(kLocationLeft);

	// 210910 KBH remove update y axis ticks
	//axis->SetTickCount(8); // 210203 KBH tick count : 8
	//axis->SetRange(-512, 4096);  // 210118 KBH Y-range lower : 0 -> -512

	//InvalidateRect(&CRect(15, 130, 470, 500));
	InvalidateRect(&m_graphRect, FALSE); // 211117 KBH static position -> dynamic position 
	
}

static CString filterTable[4] = { L"FAM", L"HEX", L"ROX", L"CY5" };

void CMainGraphDialog::setCTValue(CString dateTime, vector<double>& sensorValue, int resultIndex, int filterIndex) {
	// save the result and setting the result
	CString result = L"FAIL";
	CString ctText = L"";
	CString filterLabel[4] = { currentProtocol.labelFam, currentProtocol.labelHex, currentProtocol.labelRox, currentProtocol.labelCY5 };
	
	// 220222 KBH filter threshold values array
	float filterThreshold[4] = { currentProtocol.thresholdFam, currentProtocol.thresholdHex, currentProtocol.thresholdRox, currentProtocol.thresholdCY5 };

	// ignore the data when the data is under the 15
	int idx = sensorValue.size();

	// If the idx is under the 14, fail
	if (idx >= 15) {
		float bs = 4, be = 16;//cycle 4~15 date에 line fitting
		double xm = 0, ym = 0, sxx = 0, syy = 0, sxy = 0, a = 0, b = 0;
		for (int x = bs; x < be; x++) {
			double y = sensorValue[x];
			xm += x;
			ym += y;
			sxx += x * x;
			sxy += x * y;
		}
		double nos = be - bs;
		xm = xm / nos;
		ym = ym / nos;
		sxx = sxx / nos - xm * xm;
		sxy = sxy / nos - xm * ym;
		a = sxy / sxx;
		b = ym - a * xm;
		float baseline[60];
		for (int x = 0; x < idx; x++) {
			baseline[x] = a * x + b;
		}
		float threshold = 0.697 * flRelativeMax / 10.;
		float logThreshold = log(threshold);
		float ct;

		// Getting the log threshold from file
		float tempLogThreshold = FileManager::getFilterValue(filterIndex);

		// Success to load
		if (tempLogThreshold > 0.0) {
			logThreshold = tempLogThreshold;
		}

		// 230706 KBH start index change (i = 11 -> i = 15)
		for (int i = 15; i < sensorValue.size(); ++i) {
			if (log(sensorValue[i] - baseline[i]) > logThreshold) {
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
			float cpos = idx; // 220216 KBH The starting index of sensorValue is 1
			float cval = log(sensorValue[idx] - baseline[idx]);
			float delta = cval - log(sensorValue[idx - 1] - baseline[idx - 1]);
			ct = cpos - (cval - logThreshold) / delta;
			ctText.Format(L"%.2f", ct);

			if (16 <= ct && ct <= maxCycles) {
				result = resultRange[filterIndex] <= ct ? L"Negative" : L"Positive";
			}
			else {
				result = L"Error";
			}

			// Setting the CT text
			int ID_LIST[4] = { IDC_EDIT_CT_FAM, IDC_EDIT_CT_HEX, IDC_EDIT_CT_ROX, IDC_EDIT_CT_CY5 };
			SetDlgItemText(ID_LIST[filterIndex], ctText);
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
void CMainGraphDialog::initLog() {
	CreateDirectory(L"./Record/", NULL);

	// KBH230629 change log file path 
	CString recordDirectoryPath;
	recordDirectoryPath.Format(L"./Record/HelloPCR%05ld", (long)usbSerial);
	CreateDirectory(recordDirectoryPath, NULL);

	CString fileName, fileName2;
	experimentDate = CTime::GetCurrentTime();
	CString currentTime = experimentDate.Format(L"%Y%m%d-%H-%M-%S"); // KBH 220402 오타 수정

	// change file name
	//fileName = time.Format(L"./Record/%Y%m%d-%H%M-%S.txt");
	//fileName2 = time.Format(L"./Record/pd%Y%m%d-%H%M-%S.txt");

	fileName.Format(L"%s/Temperature-%s.txt", recordDirectoryPath, currentTime);
	fileName2.Format(L"%s/RFU-%s.txt", recordDirectoryPath, currentTime);
	

	m_recFile.Open(fileName, CStdioFile::modeCreate | CStdioFile::modeWrite);
	m_recFile.WriteString(L"Number	Time	Temperature\n");

	m_recPDFile.Open(fileName2, CStdioFile::modeCreate | CStdioFile::modeWrite);
	m_recPDFile.WriteString(L"Cycle	Time	FAM	HEX	ROX	CY5\n");

	recStartTime = timeGetTime();
}

void CMainGraphDialog::clearLog() {
	logStopped = true;

	if (m_recFile != NULL) {
		m_recFile.Close();
	}
	if (m_recPDFile != NULL) {
		m_recPDFile.Close();
	}
}

void CMainGraphDialog::OnBnClickedButtonFilterFam()
{
	useFam = !useFam;
	if (useFam) {
		SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_FAM, famImg);
	}
	else {
		SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_FAM, offImg);
	}

	setChartValue();
	//InvalidateRect(&CRect(15, 130, 470, 500));
	InvalidateRect(&m_graphRect, FALSE); // 211117 KBH static position -> dynamic position
}


void CMainGraphDialog::OnBnClickedButtonFilterHex()
{
	useHex = !useHex;
	if (useHex) {
		SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_HEX, hexImg);
	}
	else {
		SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_HEX, offImg);
	}

	setChartValue();
	//InvalidateRect(&CRect(15, 130, 470, 500));
	InvalidateRect(&m_graphRect, FALSE); // 211117 KBH static position -> dynamic position
}


void CMainGraphDialog::OnBnClickedButtonFilterRox()
{
	useRox = !useRox;
	if (useRox) {
		SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_ROX, roxImg);
	}
	else {
		SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_ROX, offImg);
	}

	setChartValue();
	//InvalidateRect(&CRect(15, 130, 470, 500));
	InvalidateRect(&m_graphRect, FALSE); // 211117 KBH static position -> dynamic position 
}


void CMainGraphDialog::OnBnClickedButtonFilterCy5()
{
	useCy5 = !useCy5;
	if (useCy5) {
		SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_CY5, cy5Img);
	}
	else {
		SET_BUTTON_IMAGE(IDC_BUTTON_FILTER_CY5, offImg);
	}

	setChartValue();
	//InvalidateRect(&CRect(15, 130, 470, 500));
	InvalidateRect(&m_graphRect, FALSE); // 211117 KBH static position -> dynamic position
}

// 220325 KBH Device Change Handler
BOOL CMainGraphDialog::OnDeviceChange(UINT nEventType, DWORD dwData)
{	
	if (isConnected) {
		// serial number convert int to string 
		char serial_buffer[20];
		sprintf(serial_buffer, "HelloPCR%05d", usbSerial);
		
		// open device 
		BOOL status = device->OpenDevice(LS4550EK_VID, LS4550EK_PID, serial_buffer, TRUE);
		
		if (status) {
			if (timerBlocked) {
				timerBlocked = false;
				m_Timer->startTimer(TIMER_DURATION, FALSE);
				FileManager::log(L"USB Connected", usbSerial);
			}
		}
		else if (!timerBlocked) {
			timerBlocked = true;
			m_Timer->stopTimer();
			FileManager::log(L"USB Disconnected", usbSerial);
		}
	}
	else {
		initPCRDevices();
	}
	return TRUE;
}

void CMainGraphDialog::connectDevice()
{
	// Getting the device index
	int selectedIdx = deviceList.GetCurSel();

	if (selectedIdx != -1) {
		CString device_serial;
		deviceList.GetLBText(selectedIdx, device_serial);
		usbSerial = _ttoi(device_serial); // KBH230629 connected PCR device serial number 

		CString prevTitle;
		GetWindowText(prevTitle);

		// KBH230710 Check Device Serial is already used 
		if (isDeviceSerialUsed((string)CT2A(prevTitle), (string)CT2A(device_serial)))
		{
			AfxMessageBox(L"device is already running");
			return;
		}

		// Found the same serial number device.
		CStringA pcrSerial;
		char serial_buffer[20];
		pcrSerial.Format("HelloPCR%05d", usbSerial);
		sprintf(serial_buffer, "%s", pcrSerial);

		if (!device->OpenDevice(LS4550EK_VID, LS4550EK_PID, serial_buffer, TRUE)) {
			AfxMessageBox(L"PCR is failed to connect(Unknown error).");
			return;
		}

		prevTitle.Format(L"%s - %s", prevTitle, device_serial);
		SetWindowText(prevTitle);

		// Connection processing
		isConnected = true;

		SetDlgItemText(IDC_EDIT_CONNECTI_STATUS, L"Connected");
		SetDlgItemText(IDC_BUTTON_CONNECT, L"Disconnect");
		GetDlgItem(IDC_COMBO_DEVICE_LIST)->EnableWindow(FALSE);

		if (isProtocolLoaded) {
			GetDlgItem(IDC_BUTTON_START)->EnableWindow();
		}

		// Process Start HelloPCR-Runner.exe 
		serverProcess.StartProcess(usbSerial);
		serverProcess.SetIndicatorLED(CMD_LED_READY);

		// KBH230623 Start Timer when device connected
		timerBlocked = false;
		m_Timer->startTimer(TIMER_DURATION, FALSE);
	}
	else {
		AfxMessageBox(L"Please select the device first.");
	}
}
void CMainGraphDialog::disconnectDevice()
{
	isConnected = false;

	device->CloseDevice();

	CString prevTitle;
	GetWindowText(prevTitle);
	CString left = prevTitle.Left(prevTitle.Find(L")") + 1);
	SetWindowText(left);

	SetDlgItemText(IDC_EDIT_CONNECTI_STATUS, L"Disconnected");
	SetDlgItemText(IDC_BUTTON_CONNECT, L"Connect");
	GetDlgItem(IDC_COMBO_DEVICE_LIST)->EnableWindow();
	GetDlgItem(IDC_BUTTON_START)->EnableWindow(FALSE);

	// KBH230623 Stop Timer when device disconnected
	timerBlocked = false;
	m_Timer->stopTimer();

	// Process Stop HelloPCR-Runner.exe 
	serverProcess.StopProcess();
}

// trim from right 
inline std::string& rtrim(std::string& s, const char* t = " \t\n\r\f\v")
{
	s.erase(s.find_last_not_of(t) + 1);
	return s;
}

bool CMainGraphDialog::isDeviceSerialUsed(string title, string device_serial)
{
	// command setting
	char* cmd = "tasklist /v /fi \"imagename eq HelloPCR.exe\"";
	wchar_t command[256];
	mbstowcs(command, cmd, strlen(cmd) + 1);

	HANDLE read_pipe, write_pipe;
	
	// secure option setting
	SECURITY_ATTRIBUTES saAttr = { sizeof(SECURITY_ATTRIBUTES) };
	saAttr.bInheritHandle = TRUE; 
	saAttr.lpSecurityDescriptor = NULL;

	if (!CreatePipe(&read_pipe, &write_pipe, &saAttr, 0))
		return true;

	// start infomation setting
	STARTUPINFO start_info = { sizeof(STARTUPINFO) };
	start_info.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
	start_info.hStdOutput = write_pipe;
	start_info.hStdError = write_pipe;
	start_info.wShowWindow = SW_HIDE;

	// process infomation setting
	PROCESS_INFORMATION process_info = { 0 };
	// execute command
	if (!CreateProcess(NULL, command, NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &start_info, &process_info))
		return true;

	WaitForSingleObject(process_info.hProcess, 200); // wait command executing is done
	TerminateProcess(process_info.hProcess, 0);		 // kill process
	if (!CloseHandle(write_pipe)) return true;		 // close write pipe handler

	// read standard output stream
	int idx;
	DWORD size;
	string output; char buffer[4096];
	memset(buffer, 0, sizeof(buffer));

	while (ReadFile(read_pipe, buffer, sizeof(buffer), &size, NULL) && size != 0) {
		buffer[size] = 0;
		output += string(buffer);
		memset(buffer, 0, sizeof(buffer));
	}
	// close handlers 
	CloseHandle(read_pipe);
	CloseHandle(process_info.hProcess);
	CloseHandle(process_info.hThread);

	// parsing output string 
	string line, serial_number;
	stringstream lines(output);

	while (getline(lines, line, '\n')) {
		line = rtrim(line).c_str();							// remove right white space 
		if ((idx = line.find(title)) == -1) continue;		// filtering unused lines 
		serial_number = line.substr(idx + title.length());	// extract serial number from MainWindowTitle
		if (serial_number.length() == 0) continue;			// filtering empty serial number 
		serial_number = serial_number.substr(3);			// get serial number ( 5 charaters )
		if (serial_number.compare(device_serial) == 0)		// compare selected device serial number 
			return true;
	}
	return false; // not used selected device serial number 
}