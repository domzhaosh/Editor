#pragma once
#include "afxwin.h"


// DDSConverter �Ի���

class DDSConverter : public CDialogEx
{
	DECLARE_DYNAMIC(DDSConverter)

public:
	DDSConverter(CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~DDSConverter();

// �Ի�������
	enum { IDD = IDD_DDSCONVERTER };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()
public:
	CString inputPath;
	CString outputPath;
	afx_msg void OnBnClickedLoadInput();
	afx_msg void OnBnClickedSaveOutput();
	afx_msg void OnBnClickedConvert();
};
