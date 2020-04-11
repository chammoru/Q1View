#pragma once


// CCustomSizeDlg dialog

class CCustomSizeDlg : public CDialog
{
	DECLARE_DYNAMIC(CCustomSizeDlg)

public:
	CCustomSizeDlg(int &w, int &h, CWnd* pParent = NULL);
	virtual ~CCustomSizeDlg();

// Dialog Data
	enum { IDD = IDD_CUSTOM_SIZE };
	int &mW, &mH;

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
};
