#pragma once
class TextureProp : public CBCGPProp
{
public:
	TextureProp(const CString &strName, const CString &strResName, LPCTSTR lpszDesc, DWORD_PTR dwData = 0UL);
	~TextureProp(void);
	
	void ShowFileDlg();

	//
	// CMFCPropertyGridProperty
	//
public:
	void OnClickButton(CPoint point);
};
