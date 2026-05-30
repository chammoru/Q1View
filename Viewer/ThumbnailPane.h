// ThumbnailPane.h : left-side thumbnail explorer drawer for the Viewer.
//
// A CListCtrl (icon view) that lists the supported image/raw files in the
// folder of the current document. Encoded images get an asynchronously decoded
// thumbnail; raw formats (whose pixel layout can't be guessed) get a labeled
// placeholder. Clicking / pressing Enter on an item opens it in the main view.

#pragma once

#include <afxcmn.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <list>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

// Worker -> UI notification carrying a decoded thumbnail (see Result).
#define WM_THUMB_READY (WM_APP + 200)
// Self-post to run a load/navigate after the list's click notification returns
// (mutating the list inside its own notification handler is unsafe).
#define WM_DRAWER_ACTIVATE (WM_APP + 201)

class CThumbnailPane : public CListCtrl
{
public:
	CThumbnailPane();
	virtual ~CThumbnailPane();

	// Create the list control as a child of pParent with control id nID.
	BOOL CreatePane(CWnd *pParent, UINT nID);

	// Show the folder containing lpszPath (repopulating only when the folder
	// changes) and select the matching item.
	void SetCurrentFile(LPCTSTR lpszPath);

	// Stop the worker thread and release cached bitmaps. Safe to call twice.
	void Shutdown();

	// Owner-drawn rows: thumbnail / extension badge / plain folder text.
	virtual void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct);

protected:
	DECLARE_MESSAGE_MAP()
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnItemActivate(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnGetInfoTip(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg LRESULT OnThumbReady(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnActivatePosted(WPARAM wParam, LPARAM lParam);

private:
	struct Task { unsigned gen; int index; CString path; };
	struct Result { unsigned gen; int index; HBITMAP hbmp; };

	void Populate(const CString &folder, const CString &current);
	void NavigateTo(const CString &folder);
	void ActivateIndex(int index, bool allowNavigate);
	void SelectByPath(const CString &path);
	void ResetImageList();
	int  AddImageCopy(HBITMAP hbmp);          // copy into the image list, return index
	void QueueThumb(int index, const CString &path);
	void WorkerLoop();

	// Cache (LRU) of decoded source bitmaps keyed by full path.
	HBITMAP CacheFind(const CString &path);
	void    CacheStore(const CString &path, HBITMAP hbmp);
	void    CacheClear();

	HBITMAP MakePlaceholder(const CString &ext);            // UI thread
	static HBITMAP DecodeThumbnail(const CString &path, int size, COLORREF bg); // worker

	static bool IsDecodableExt(const CString &ext);
	static bool IsRawExt(const CString &ext);
	static bool IsSupportedExt(const CString &ext);

	CImageList mImages;
	CFont      mLabelFont;
	CFont      mExtFont;
	int        mThumb;                 // icon edge size in px

	enum EntryKind { ENTRY_PARENT, ENTRY_DIR, ENTRY_FILE };
	struct Entry {
		EntryKind kind;
		CString   path;   // target folder for parent/dir; file path for file
		int       img;    // image-list index for image thumbnails, else -1
	};
	std::vector<Entry> mEntries;       // item index -> entry
	Entry      mPending;               // deferred load/navigate target
	CString    mFolder;                // folder currently listed (trailing '\\')
	int        mLoadingImg;            // image index shown while a thumb decodes

	std::map<CString, HBITMAP> mCache;
	std::list<CString>         mCacheOrder;
	size_t                     mCacheCap;

	std::thread              mWorker;
	std::mutex               mMutex;
	std::condition_variable  mCv;
	std::deque<Task>         mTasks;
	bool                     mStop;
	std::atomic<unsigned>    mGen;
	bool                     mWorkerStarted;
};
