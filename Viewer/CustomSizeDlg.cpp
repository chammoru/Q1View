// CustomSizeDlg.cpp : implementation file
//

#include "stdafx.h"
#include "Viewer.h"
#include "CustomSizeDlg.h"

#include <QImageViewerCmn.h>

// CCustomSizeDlg dialog

IMPLEMENT_DYNAMIC(CCustomSizeDlg, CDialog)

CCustomSizeDlg::CCustomSizeDlg(int &w, int &h, CWnd* pParent /*=NULL*/)
	: mW(w), mH(h), CDialog(CCustomSizeDlg::IDD, pParent)
{

}

CCustomSizeDlg::~CCustomSizeDlg()
{
}

void CCustomSizeDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);

	DDX_Text(pDX, IDC_EDIT_CUSTOM_WIDTH, mW);
	DDV_MinMaxUInt(pDX, mW, 1, QIMG_MAX_LENGTH);

	DDX_Text(pDX, IDC_EDIT_CUSTOM_HEIGHT, mH);
	DDV_MinMaxUInt(pDX, mH, 1, QIMG_MAX_LENGTH);
}


BEGIN_MESSAGE_MAP(CCustomSizeDlg, CDialog)
	ON_WM_SHOWWINDOW()
END_MESSAGE_MAP()


// CCustomSizeDlg message handlers

void CCustomSizeDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialog::OnShowWindow(bShow, nStatus);

	// TODO: Add your message handler code here
	GetDlgItem(IDC_EDIT_CUSTOM_WIDTH)->SetFocus();
}
