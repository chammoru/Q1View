// Viewer.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "Viewer.h"
#include "MainFrm.h"

#include "ViewerDoc.h"
#include "ViewerView.h"

// #include "vld.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CViewerApp

BEGIN_MESSAGE_MAP(CViewerApp, CWinApp)
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, &CWinApp::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, &CWinApp::OnFileOpen)
END_MESSAGE_MAP()


CViewerApp::CViewerApp()
: mExplicitFileOpen(false)
{
}


CViewerApp theApp;


// CViewerApp initialization

BOOL CViewerApp::InitInstance()
{
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	SetRegistryKey(_T("Chammoru"));
#ifdef Q1VIEW_GALLERY_TESTS
	// Test binaries use isolated preferences and never modify the user's settings.
	SetRegistryKey(_T("Q1ViewGalleryTests"));
	SetEnvironmentVariableW(L"Q1VIEW_TRACE_PLAYBACK", L"1");
#endif
	LoadStdProfileSettings(10);  // Load standard INI file options (including MRU)
	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views
	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CViewerDoc),
		RUNTIME_CLASS(CMainFrame),       // main SDI frame window
		RUNTIME_CLASS(CViewerView));
	if (!pDocTemplate)
		return FALSE;
	AddDocTemplate(pDocTemplate);


	// Enable DDE Execute open
	EnableShellOpen();
	// File associations are owned by the MSIX manifest and Inno installer so
	// their photo/video/raw icons stay consistent. Do not let MFC recreate the
	// legacy Viewer.Document ProgID with the executable's generic app icon.

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
	// Enable drag/drop open
	m_pMainWnd->DragAcceptFiles();
#ifdef Q1VIEW_GALLERY_TESTS
	extern int RunGalleryIntegrationTests();
	int testResult = RunGalleryIntegrationTests();
	m_pMainWnd->SendMessage(WM_CLOSE);
	PostQuitMessage(testResult);
#endif
	return TRUE;
}

CDocument* CViewerApp::OpenDocumentFile(LPCTSTR lpszFileName)
{
	// All MFC user-open paths (command line/DDE, File > Open, recent files,
	// one-file drag-and-drop, and thumbnail activation) route through here.
	// Direct document reload/navigation calls intentionally do not.
	class ExplicitOpenFlagScope
	{
	public:
		ExplicitOpenFlagScope(bool &flag)
			: mFlag(flag), mPrevious(flag) { mFlag = true; }
		~ExplicitOpenFlagScope() { mFlag = mPrevious; }
	private:
		bool &mFlag;
		bool mPrevious;
	} scope(mExplicitFileOpen);

	return CWinApp::OpenDocumentFile(lpszFileName);
}

bool CViewerApp::IsVideoAutoplayEnabled()
{
	return GetProfileInt(_T("Playback"), _T("AutoplayVideos"), 1) != 0;
}

void CViewerApp::SetVideoAutoplayEnabled(bool enabled)
{
	WriteProfileInt(_T("Playback"), _T("AutoplayVideos"), enabled ? 1 : 0);
}

// CViewerApp message handlers

