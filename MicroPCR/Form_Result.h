#pragma once
#include "Chart.h"


// CForm_Result ��ȭ �����Դϴ�.

class CForm_Result : public CDialog
{
	DECLARE_DYNAMIC(CForm_Result)

public:
	CForm_Result(CWnd* pParent = NULL);   // ǥ�� �������Դϴ�.
	virtual ~CForm_Result();

	// ��ȭ ���� �������Դϴ�.
	enum { IDD = IDD_FORM_RESULT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV �����Դϴ�.

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL PreTranslateMessage(MSG* pMsg);

	afx_msg void OnPaint();

	CXYChart m_Chart;

};
