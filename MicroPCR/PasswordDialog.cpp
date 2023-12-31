// PasswordDialog.cpp: 구현 파일
//

#include "stdafx.h"
#include "PasswordDialog.h"
#include "afxdialogex.h"
#include "resource.h"

// CPasswordDialog 대화 상자

IMPLEMENT_DYNAMIC(CPasswordDialog, CDialogEx)

CPasswordDialog::CPasswordDialog(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_PASSWORD, pParent)
	, resultType(PASSWORD_RESULT_KWELLLABS)
{

}

CPasswordDialog::~CPasswordDialog()
{
}

void CPasswordDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CPasswordDialog, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_PASSWORD_OK, &CPasswordDialog::OnBnClickedButtonPasswordOk)
	ON_BN_CLICKED(IDC_BUTTON_PASSWORD_CANCEL, &CPasswordDialog::OnBnClickedButtonPasswordCancel)
END_MESSAGE_MAP()


// CPasswordDialog 메시지 처리기

BOOL CPasswordDialog::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN)
	{
		if (pMsg->wParam == VK_RETURN) {
			OnBnClickedButtonPasswordOk();
			return TRUE;
		}
	}

	return CDialogEx::PreTranslateMessage(pMsg);
}


void CPasswordDialog::OnBnClickedButtonPasswordOk()
{
	// Check the password is correct.
	CString password;
	GetDlgItemText(IDC_EDIT_PASSWORD_INPUT, password);

	if (password.CompareNoCase(L"kwelllabs") == 0) {
		resultType = PASSWORD_RESULT_KWELLLABS;
		OnOK();
	}
	else if (password.CompareNoCase(L"bio-it") == 0) {
		resultType = PASSWORD_RESULT_DEVICE_SETUP;
		OnOK();
	}
	else {
		AfxMessageBox(L"Incorrect password!");
	}
}


void CPasswordDialog::OnBnClickedButtonPasswordCancel()
{
	OnCancel();
}
