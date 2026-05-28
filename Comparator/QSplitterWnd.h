#pragma once

#define QSPLITTER_W           5

// CQSplitterWnd

class CQSplitterWnd : public CSplitterWnd
{
	DECLARE_DYNAMIC(CQSplitterWnd)

public:
	CQSplitterWnd();
	virtual ~CQSplitterWnd();

protected:
	DECLARE_MESSAGE_MAP()
public:
	virtual void OnDrawSplitter(CDC* pDC, ESplitType nType, const CRect& rect);
};


