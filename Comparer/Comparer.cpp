
// Comparer.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "afxwinappex.h"
#include "Comparer.h"
#include "MainFrm.h"

#include "ComparerDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CComparerApp

BEGIN_MESSAGE_MAP(CComparerApp, CWinApp)
	ON_COMMAND(ID_APP_ABOUT, &CComparerApp::OnAppAbout)
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, &CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, &CComparerApp::OnFileOpen)
END_MESSAGE_MAP()


// CComparerApp construction

CComparerApp::CComparerApp()
{

	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

// The one and only CComparerApp object

CComparerApp theApp;


// CComparerApp initialization

BOOL CComparerApp::InitInstance()
{
	CWinApp::InitInstance();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("Chammoru"));
	LoadStdProfileSettings(4);  // Load standard INI file options (including MRU)

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views
	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CComparerDoc),
		RUNTIME_CLASS(CMainFrame),       // main SDI frame window
		NULL);
	if (!pDocTemplate)
		return FALSE;
	AddDocTemplate(pDocTemplate);



	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);


	// Dispatch commands specified on the command line.  Will return FALSE if
	// app was launched with /RegServer, /Register, /Unregserver or /Unregister.
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	// The one and only window has been initialized, so show and update it
	m_pMainWnd->ShowWindow(SW_SHOW);
	m_pMainWnd->UpdateWindow();
	// call DragAcceptFiles only if there's a suffix
	//  In an SDI app, this should occur after ProcessShellCommand
	return TRUE;
}



// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

// App command to run the dialog
void CComparerApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

// CComparerApp message handlers

void CComparerApp::OnFileOpen()
{
	std::vector<CString> filenames;

	// Prompt the user (with all document templates)
	CFileDialog dialog(TRUE, _T("*.*"), NULL,
		OFN_ALLOWMULTISELECT | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST, _T("All Files (*.*)|*.*||"));
	if (dialog.DoModal() == IDOK) {
		// Get the list of selected file names.
		POSITION pos = dialog.GetStartPosition();
		while (pos != NULL) {
			filenames.push_back(dialog.GetNextPathName(pos));
		}
	}

	if (filenames.size() == 0) {
		return;
	}

	CString filenamesCsv = filenames[0];
	int limit = std::min((int)CComparerDoc::IMG_VIEW_MAX, (int)filenames.size());
	for (int i = 1; i < limit; i++) {
		filenamesCsv += _T(",") + filenames[i];
	}

	AfxGetApp()->OpenDocumentFile(filenamesCsv, FALSE);
}
