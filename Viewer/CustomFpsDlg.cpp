// CustomFps.cpp : implementation file
//

#include "stdafx.h"
#include "Viewer.h"
#include "CustomFpsDlg.h"
#include "afxdialogex.h"


// CCustomFpsDlg dialog

IMPLEMENT_DYNAMIC(CCustomFpsDlg, CDialog)

CCustomFpsDlg::CCustomFpsDlg(double &fps, CWnd* pParent /*=NULL*/)
	: mFps(fps), CDialog(CCustomFpsDlg::IDD, pParent)
{

}

CCustomFpsDlg::~CCustomFpsDlg()
{
}

void CCustomFpsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Text(pDX, IDC_EDIT_CUSTOM_FPS, mFps);
	DDV_MinMaxDouble(pDX, mFps, 1, 300);
}


BEGIN_MESSAGE_MAP(CCustomFpsDlg, CDialog)
	ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()


void CCustomFpsDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);

	GetDlgItem(IDC_EDIT_CUSTOM_FPS)->SetFocus();
}
