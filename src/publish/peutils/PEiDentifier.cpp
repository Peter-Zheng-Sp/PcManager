// dummyz@126.com

#include "stdafx.h"
#include "PEParser.h"
#include "PEiDentifier.h"

using namespace std;

/*
[Name of the Packer v1.0]
signature = 50 E8 ?? ?? ?? ?? 58 25 ?? F0 FF FF 8B C8 83 C1 60 51 83 C0 40 83 EA 06 52 FF 20 9D C3
ep_only = true
*/
//////////////////////////////////////////////////////////////////////////
static LPCTSTR SkipWhite(LPCTSTR lpStr)
{
	TCHAR c;
	while ( (c = *lpStr) != 0 )
	{
		if ( c != _T('\t') && c != _T(' ') )
		{
			break;
		}

		lpStr++;
	}

	return lpStr;
}

LPCTSTR SkipWord(LPCTSTR lpStr)
{
	TCHAR c;
	while ( (c = *lpStr) != 0 )
	{
		if ( c == _T(' ') || c == _T('\t') || c == _T('=') )
		{
			break;
		}

		lpStr++;
	}

	return lpStr;
}

WORD _ToHex(LPCTSTR lpStr, BOOL& bError)
{
	bError = FALSE;

	WORD w = -1;
	TCHAR c = lpStr[0];
	if ( c != _T('?') )
	{
		if ( c >= 'a' && c <= 'f' )
		{
			w = (c - 'a' + 10) << 4;
		}
		else if ( c >= 'A' && c <= 'F' )
		{
			w = (c - 'A' + 10) << 4;
		}
		else if ( c >= '0' && c <= '9' )
		{
			w = (c - '0') << 4;
		}
		else
		{
			bError = TRUE;
		}

		c = lpStr[1];
		if ( c >= 'a' && c <= 'f' )
		{
			w |= (c - 'a' + 10);
		}
		else if ( c >= 'A' && c <= 'F' )
		{
			w |= (c - 'A' + 10);
		}
		else if ( c >= '0' && c <= '9' )
		{
			w |= (c - '0');
		}
		else
		{
			bError = TRUE;
		}
	}
	else if ( lpStr[1] == _T('?') )
	{
		w = -1;
	}
	else
	{
		bError = TRUE;
	}

	return w;
}

inline BOOL _MatchSignature(PBYTE lpBuff, PWORD lpSignature, DWORD dwLen)
{
	for ( DWORD i = 0; i < dwLen; i++ )
	{
		if ( *lpSignature != (WORD)-1 && (BYTE)*lpSignature != *lpBuff )
		{
			return FALSE;
		}

		lpSignature++;
		lpBuff++;
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
CPEiDentifier::CPEiDentifier()
{
	m_dwNoOepMaxLen = 0;
	m_dwOepMaxLen = 0;
}

CPEiDentifier::~CPEiDentifier()
{

}

BOOL CPEiDentifier::Load(LPCTSTR lpFilePath)
{
	DWORD bResult = FALSE;

	FILE* lpFile = _tfopen(lpFilePath, _T("r"));
	if ( lpFile != NULL )
	{
		DWORD	dwLineBuffLen = 0x1024;
		TCHAR*	lpszLineBuff = new TCHAR[dwLineBuffLen + 1];
		SIGNATURE sig;
		BOOL	bOep = FALSE;
		DWORD	flag = 0;

		while ( _fgetts(lpszLineBuff, dwLineBuffLen, lpFile) != NULL )
		{
			LPTSTR lpLineEnd = _tcschr(lpszLineBuff, _T('\n'));
			if ( lpLineEnd != NULL ) 
			{
				*lpLineEnd = 0;
			}

			LPCTSTR lpName = SkipWhite(lpszLineBuff);
			if ( lpName[0] == _T('/') && lpName[1] == _T('/') )
			{
				continue;
			}

			if ( *lpName == _T('[') )
			{
				flag = 0;
				bOep = FALSE;
				sig.data.clear();
				sig.strName.clear();
				sig.lpwPointer = NULL;
				
				lpName++;
				LPCTSTR lpName_End = _tcschr(lpName, ']');
				if ( lpName_End != NULL )
				{
					if ( _tcsstr(lpName_End, _T("Hint")) == NULL ) // 忽略 hint 节,不支持
					{
						sig.strName = wstring(lpName, lpName_End - lpName);
						flag = 1;
					}
				}
			}
			else if ( flag != 0 )
			{
				LPCTSTR lpValue = SkipWord(lpName);
				DWORD dwNameLen = (DWORD)(lpValue - lpName);
				
				lpValue = SkipWhite(lpValue);
				if ( lpValue[0] == _T('=') )
				{
					lpValue = SkipWhite(lpValue + 1);
					if ( _tcsnicmp(lpName, _T("signature"), dwNameLen) == 0 )
					{
						flag |= 0x02;

						BOOL bError;
						TCHAR c;

						while ( (c = *lpValue) != 0 && lpValue < lpLineEnd )
						{
							if ( c != ' ' )
							{
								WORD w = _ToHex(lpValue, bError);
								if ( bError )
								{
									flag = 0;
									break;
								}
								else
								{
									sig.data.push_back(w);
									lpValue += 2;
								}
							}
							else
							{
								lpValue++;
							}
						}
					}
					else if ( _tcsnicmp(lpName, _T("ep_only"), dwNameLen) == 0 )
					{
						flag |= 0x04;
						bOep = (_tcsicmp(lpValue, _T("true")) == 0);
					}
				}
			}

			if ( flag == (1 | 0x02 | 0x04) )
			{
				if ( bOep )
				{
					if ( sig.data.size() > m_dwNoOepMaxLen )
					{
						m_dwNoOepMaxLen = (DWORD)sig.data.size();
					}

					m_NoOepSignaList.push_back(sig);
				}
				else
				{
					if ( sig.data.size() > m_dwOepMaxLen )
					{
						m_dwOepMaxLen = (DWORD)sig.data.size();
					}

					m_OepSignaList.push_back(sig);
				}
				flag = 0;
			}
		}

		delete lpszLineBuff;
		lpszLineBuff = NULL;
		fclose(lpFile);

		bResult = TRUE;
	}

	return bResult;
}

void CPEiDentifier::Unload()
{
	m_dwOepMaxLen = 0;
	m_dwNoOepMaxLen = 0;

	m_NoOepSignaList.clear();
	m_OepSignaList.clear();
}

LPCTSTR CPEiDentifier::Scan(LPCTSTR lpFilePath)
{
	CPEMapFile file;
	LPCTSTR lpResult = NULL;

	if ( file.Open(lpFilePath, TRUE, TRUE, 20 * 1024 * 1024) )
	{
		CPEParser parser;

		if ( parser.Attach(&file) )
		{
			lpResult = Scan(&parser);
			parser.Detach();
		}
	}

	return lpResult;
}

LPCTSTR	CPEiDentifier::Scan(CPEParser* lpParser)
{
	LPCTSTR lpResult;

	lpResult = ScanOep(lpParser);
	if ( lpResult == NULL )
	{
		lpResult = ScanNoOep(lpParser);
	}

	return lpResult;
}

LPCTSTR CPEiDentifier::ScanOep(CPEParser* lpParser)
{
	LPCTSTR lpResult = NULL;
	DWORD dwOep = lpParser->GetEntryPoint();
	DWORD lFilePointer = lpParser->ToFilePointer(dwOep);
	IPEFile* lpFile = lpParser->GetFileObject();

	if ( lFilePointer != EOF && lFilePointer == lpFile->Seek(lFilePointer, FILE_BEGIN) )
	{
		PBYTE lpBuff = new BYTE[m_dwOepMaxLen];
		DWORD dwReadBytes = lpFile->Read(lpBuff, m_dwOepMaxLen);

		if ( dwReadBytes != 0 )
		{
			list<SIGNATURE>::iterator i = m_OepSignaList.begin();
			list<SIGNATURE>::iterator i_end = m_OepSignaList.end();
			while ( i != i_end )
			{
				SIGNATURE& s = *i++;

				if ( s.data.size() <= dwReadBytes )
				{
					if ( _MatchSignature(lpBuff, &s.data.front(), (DWORD)s.data.size()) )
					{
						lpResult = s.strName.c_str();
						break;
					}
				}
			}
		}

		delete lpBuff;
		lpBuff = NULL;
	}

	return lpResult;
}

LPCTSTR CPEiDentifier::ScanNoOep(CPEParser* lpParser)
{
	// 速度很慢...狄大人救我
	LPCTSTR lpResult = NULL;
	PBYTE lpBuff = new BYTE[m_dwNoOepMaxLen];
	IPEFile* lpFile = lpParser->GetFileObject();
	PIMAGE_SECTION_HEADER lpFirstSectHead = lpParser->GetSectionHead(0);
	list<SIGNATURE>::iterator j_begin = m_NoOepSignaList.begin();
	list<SIGNATURE>::iterator j_end = m_NoOepSignaList.end();

	list<SIGNATURE>::iterator j = j_begin;
	while ( j != j_end )
	{
		SIGNATURE& s = *j++;
		s.lpwPointer = &s.data.front();
	}

	DWORD dwSectionCount = lpParser->GetSectionCount();
	for ( DWORD i = 0; i < dwSectionCount; i++ )
	{
		PIMAGE_SECTION_HEADER lpSectHead = &lpFirstSectHead[i];
		if ( lpSectHead->PointerToRawData == 0 ||
			lpSectHead->SizeOfRawData == 0 
			)
		{
			continue;
		}
		
		ULONG lFilePointer = lpParser->ToFilePointer(lpSectHead->VirtualAddress);
		if ( lpFile->Seek(lFilePointer, FILE_BEGIN) == lFilePointer )
		{
			DWORD dwTotalReadBytes = 0;
			while ( dwTotalReadBytes < lpSectHead->SizeOfRawData )
			{
				DWORD dwReadBytes;
				
				if ( dwTotalReadBytes + m_dwNoOepMaxLen <= lpSectHead->SizeOfRawData )
				{
					dwReadBytes = m_dwNoOepMaxLen;
				}
				else
				{
					dwReadBytes = lpSectHead->SizeOfRawData - dwTotalReadBytes;
				}

				dwReadBytes = lpFile->Read(lpBuff, dwReadBytes);
				if ( dwReadBytes == 0 )
				{
					break;
				}
				dwTotalReadBytes += dwReadBytes;

				for ( DWORD i = 0; i < dwReadBytes; i++ )
				{
					BYTE c = lpBuff[i];

					list<SIGNATURE>::iterator j = j_begin;
					while ( j != j_end )
					{
						SIGNATURE& s = *j++;

						if ( *s.lpwPointer == (WORD)-1 || (BYTE)*s.lpwPointer == c )
						{
							if ( s.lpwPointer == &s.data.back() )
							{
								lpResult = s.strName.c_str();
								goto _Exit1;
							}

							s.lpwPointer++;
						}
						else
						{
							s.lpwPointer = &s.data.front();
						}
					}
				}
			}
		}
	}

_Exit1:
	delete lpBuff;
	lpBuff = NULL;

	return lpResult;
}


BOOL CPEiDentifier::IsVBLink(CPEParser* lpParser)
{
	// FindImpFunction("msvbvm60.dll", "ThunkMain");
	// 有时候会使用序号，所以不能直接使用 FindImpFunction

	class CImpEnumer
	{
	public:
		static BOOL WINAPI Callback(LPCSTR lpDll, LPCSTR lpName, ULONG lOffset, LPARAM lParam)
		{
			CImpEnumer* _this = (CImpEnumer*)LongToPtr(lParam);
			return _this->_Callback(lpDll, lpName, lOffset);
		}

	private:
		BOOL _Callback(LPCSTR lpDll, LPCSTR lpName, ULONG lOffset)
		{
			if ( _strnicmp(lpDll, "msvbvm60", sizeof ("msvbvm60") - 1) == 0 )
			{
				m_bResult = TRUE;
				return FALSE;
			}

			return TRUE;
		}

	public:
		BOOL	m_bResult;
	} enumer;

	enumer.m_bResult = FALSE;
	lpParser->EnumImpFunction(CImpEnumer::Callback, PtrToLong(&enumer));

	return enumer.m_bResult;
}

BOOL CPEiDentifier::IsDelphiLink(CPEParser* lpParser)
{
	ULONG lOffset, lFilePointer;
	BYTE cBuff[10];
	IPEFile* lpFile = lpParser->GetFileObject();
	PIMAGE_SECTION_HEADER lpSectHeads = lpParser->GetSectionHead(0);

	if ( memcmp(lpSectHeads->Name, "CODE\0\0\0\0", 8) == 0 )
	{
		return TRUE;
	}

	lOffset = lpParser->GetEntryPoint();
	lFilePointer = lpParser->ToFilePointer(lOffset);
	if ( lFilePointer != EOF && 
		lpFile->Seek(lFilePointer, FILE_BEGIN) == lFilePointer &&
		lpFile->Read(cBuff, 10) == 10
		)
	{
		if ( memcmp(cBuff, "\x55\x8b\xec\x83\xc4", 5) == 0 ||
			memcmp(cBuff, "\x55\x8b\xd8\x33\xc0", 5) == 0
			)
		{
			return TRUE;
		}
	}

	lOffset = lpParser->FindExpFunction("___CPPdebugHook");
	if ( lOffset != EOF )
	{
		return TRUE;
	}

	return FALSE;
}

BOOL CPEiDentifier::IsEyuyanLink(CPEParser* lpParser)
{
	return FALSE;
}

BOOL CPEiDentifier::IsDotNetLink(CPEParser* lpParser)
{
	BOOL bResult = FALSE;

	PIMAGE_DATA_DIRECTORY lpDir = lpParser->GetDataDirectory(IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR);
	if ( lpDir->VirtualAddress != 0 )
	{
		ULONG lOffset = lpParser->FindImpFunction("mscoree.dll", "_CorExeMain");
		if ( lOffset != EOF )
		{
			bResult = TRUE;
		}
	}

	return bResult;
}

BOOL CPEiDentifier::IsResourcePacked(CPEParser* lpParser)
{
	IPEFile* lpFile = lpParser->GetFileObject();

	PIMAGE_DATA_DIRECTORY lpDir = lpParser->GetDataDirectory(IMAGE_DIRECTORY_ENTRY_RESOURCE);
	if ( lpDir->VirtualAddress == 0 )
	{
		return FALSE;
	}

	PIMAGE_SECTION_HEADER lpSect = lpParser->GetSectionHeadByOffset(lpDir->VirtualAddress);
	if ( lpSect == NULL )
	{
		return FALSE;
	}

	class CResDataChecker
	{
	private:
		BOOL WINAPI _EnumTypeFunc(LPCWSTR lpType, ULONG lOffset)
		{
			m_lpPEParser->EnumResourceNames(lOffset, EnumNamesFunc, PtrToLong(this));
			return !m_bPacked;
		}

		BOOL WINAPI _EnumNameFunc(LPCTSTR lpName, ULONG lOffset, DWORD dwSize, DWORD dwCodePage)
		{
			if ( lOffset < m_lpSectHead->VirtualAddress || 
				lOffset + dwSize > m_lpSectHead->VirtualAddress + m_lpSectHead->SizeOfRawData
				)
			{
				m_bPacked = TRUE;
				return FALSE;
			}

			return TRUE;
		}

	public:
		static BOOL WINAPI EnumTypeFunc(PIMAGE_RESOURCE_DIRECTORY lpDir, LPCWSTR lpName, ULONG lOffset, LPARAM lParam)
		{
			CResDataChecker* _this = (CResDataChecker*)LongToPtr(lParam);
			return _this->_EnumTypeFunc(lpName, lOffset);
		}

		static BOOL WINAPI EnumNamesFunc(PIMAGE_RESOURCE_DIRECTORY lpDir, LPCTSTR lpName, ULONG lOffset, DWORD dwSize, DWORD dwCodePage, LPARAM lParam)
		{
			CResDataChecker* _this = (CResDataChecker*)LongToPtr(lParam);
			return _this->_EnumNameFunc(lpName, lOffset, dwSize, dwCodePage);
		}

	public:
		CPEParser*	m_lpPEParser;
		PIMAGE_SECTION_HEADER m_lpSectHead;
		BOOL		m_bPacked;
	};

	CResDataChecker checker;

	checker.m_lpSectHead = lpSect;
	checker.m_bPacked = FALSE;
	checker.m_lpPEParser = lpParser;

	lpParser->EnumResourceTypes(CResDataChecker::EnumTypeFunc, PtrToLong(&checker));

	return checker.m_bPacked;
}

BOOL CPEiDentifier::IsImportPacked(CPEParser* lpParser)
{
	if ( lpParser->IsPEPlus() )
	{
		return FALSE;
	}
	IPEFile* lpFile = lpParser->GetFileObject();

	PIMAGE_DATA_DIRECTORY lpDir = lpParser->GetDataDirectory(IMAGE_DIRECTORY_ENTRY_IMPORT);
	if ( lpDir->VirtualAddress == 0 )
	{
		return FALSE;
	}

	ULONG lImpDescPointer = lpParser->ToFilePointer(lpDir->VirtualAddress);
	if ( lImpDescPointer == EOF )
	{
		return FALSE;
	}

	BOOL bResult = TRUE;
	IMAGE_IMPORT_DESCRIPTOR ImpDesc;

	while ( lpFile->Seek(lImpDescPointer, FILE_BEGIN) == lImpDescPointer &&
		lpFile->ReadT(ImpDesc)
		)
	{
		if ( ImpDesc.Name == 0 )
		{
			break;
		}

		CHAR szDllName[MAX_PATH];
		ULONG lDllNamePointer = lpParser->ToFilePointer(ImpDesc.Name);
		if ( lpFile->Seek(lDllNamePointer, FILE_BEGIN) != lDllNamePointer ||
			lpFile->Read(szDllName, MAX_PATH) == 0
			)
		{
			break;
		}
		szDllName[MAX_PATH - 1] = 0;

		IMAGE_THUNK_DATA ThunkData;
		ULONG nThunkDataCount = 0;
		ULONG lThunkDataPointer = ImpDesc.OriginalFirstThunk != 0 ? ImpDesc.OriginalFirstThunk : ImpDesc.FirstThunk;

		lThunkDataPointer = lpParser->ToFilePointer(lThunkDataPointer);
		if ( lpFile->Seek(lThunkDataPointer, FILE_BEGIN) == lThunkDataPointer )
		{
			while ( lpFile->ReadT(ThunkData) )
			{
				if ( ThunkData.u1.AddressOfData == 0 )
				{
					break;
				}

				nThunkDataCount++;
				lThunkDataPointer += sizeof (ThunkData);

				if ( nThunkDataCount >= 2 )
				{
					break;
				}
			}
		}

		if ( _stricmp(szDllName, "kernel32.dll") == 0 ||
			_stricmp(szDllName, "user32.dll") == 0
			)
		{

		}
		else if ( nThunkDataCount > 1 )
		{
			bResult = FALSE;
			break;
		}
		lImpDescPointer += sizeof (ImpDesc);
	}

	return bResult;
}

BOOL CPEiDentifier::IsSectionPacked(CPEParser* lpParser)
{
	BOOL bResult = FALSE;
	DWORD dwCount = lpParser->GetSectionCount();
	PIMAGE_NT_HEADERS lpNtHead = lpParser->GetNtHead();
	PIMAGE_SECTION_HEADER lpSectHead = lpParser->GetSectionHead(0);
	PIMAGE_SECTION_HEADER lpLastSectHead = lpSectHead + dwCount - 1;

	while ( lpSectHead != lpLastSectHead ) // 忽略最后一个节
	{
		DWORD Characteristics = (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE);
		if ( (lpSectHead->Characteristics & Characteristics) == Characteristics )
		{
			if ( lpSectHead->Misc.VirtualSize > lpSectHead->SizeOfRawData &&
				lpSectHead->Misc.VirtualSize - lpSectHead->SizeOfRawData >= lpNtHead->OptionalHeader.SectionAlignment 
				)
			{
				bResult = TRUE;
				break;
			}
		}

		lpSectHead++;
	}

	return bResult;
}

BOOL CPEiDentifier::IsResourceBin(CPEParser* lpParser)
{
	if ( lpParser->IsDll() ) 
	{
		return (lpParser->GetEntryPoint() == 0);
	}
	else if ( lpParser->IsExe() )
	{
		if ( lpParser->GetSectionCount() == 2 &&
			lpParser->GetDataDirectory(IMAGE_DIRECTORY_ENTRY_IMPORT)->VirtualAddress == 0
			)
		{
			ULONG lOffset = lpParser->GetEntryPoint();
			if ( lOffset == lpParser->GetSectionHead(0)->VirtualAddress )
			{
				IPEFile* lpFile = lpParser->GetFileObject();
				BYTE cBuff[2];

				ULONG lFilePointer = lpParser->ToFilePointer(lOffset);
				if ( lpFile->Seek(lFilePointer, FILE_BEGIN) == lFilePointer &&
					lpFile->Read(&cBuff, 0x2) == 0x2
					)
				{
					if ( cBuff[0] == 0xC3 && cBuff[1] == 0x90 )
					{
						return TRUE;
					}
				}
			}
		}
	}

	return FALSE;
}

BOOL CPEiDentifier::GetLinkerInfo(CPEParser* lpParser, BOOL& bNotPacker, DWORD& dwLinker)
{
	BOOL bResult = FALSE;
	IPEFile* lpFile = lpParser->GetFileObject();

	DWORD dwSize;
	ULONG lOffset = lpParser->FindResource(MAKEINTRESOURCE(1), RT_VERSION, 0, &dwSize);
	if ( lOffset == EOF )
	{
		return FALSE;
	}

	if ( dwSize == 0 || dwSize > 0x1024 )
	{
		return FALSE;
	}

	ULONG lFilePointer = lpParser->ToFilePointer(lOffset);
	if ( lFilePointer == EOF ||
		lpFile->Seek(lFilePointer, FILE_BEGIN) != lFilePointer )
	{
		return FALSE;
	}

	PVOID lpBuff = new BYTE[dwSize];
	if ( lpBuff == NULL )
	{
		return FALSE;
	}

	DWORD dwReadBytes = lpFile->Read(lpBuff, dwSize);
	if ( dwReadBytes != 0 )
	{
		PBYTE lpBegin = (PBYTE)lpBuff;
		PBYTE lpEnd = lpBegin + dwReadBytes;
		while ( lpBegin < lpEnd )
		{
			DWORD dwLen = (DWORD)(lpEnd - lpBegin);

#define X_MATCH(name, link)	\
	if ( (dwLen >= sizeof (name) - sizeof (WCHAR)) && (memcmp(lpBegin, name, sizeof (name) - sizeof (WCHAR)) == 0) ) \
			{ \
			dwLinker = link; \
			bNotPacker = TRUE; \
			bResult = TRUE; \
			break; \
			}

			X_MATCH(L"Microsoft Corporation", VC_Linker | VB_Linker);
			X_MATCH(L"Microsoft® Windows®", VC_Linker | VB_Linker);
			X_MATCH(L" Tencent.", VC_Linker);
			X_MATCH(L"Xunlei Networking", VC_Linker | Delphi_Linker);
			X_MATCH(L"深圳市迅雷", VC_Linker | Delphi_Linker);

#undef X_MATCH
			lpBegin++;
		}
	}

	delete lpBuff;
	lpBuff = NULL;

	return bResult;
}

BOOL	CPEiDentifier::IsAntiVirusCleaned(CPEParser* lpParser)
{
	BOOL bResult = FALSE;
	IMAGE_SECTION_HEADER SectHeadBuff;
	IPEFile* lpFile = lpParser->GetFileObject();
	PIMAGE_DOS_HEADER lpDosHead = lpParser->GetDosHead();
	PIMAGE_NT_HEADERS lpNtHead = lpParser->GetNtHead();
	PIMAGE_SECTION_HEADER lpLastSectHead = lpParser->GetSectionHead(lpNtHead->FileHeader.NumberOfSections - 1);

	ULONG lOffset = lpDosHead->e_lfanew + FIELD_OFFSET(IMAGE_NT_HEADERS32, OptionalHeader) + \
		lpNtHead->FileHeader.SizeOfOptionalHeader;
	lOffset += lpNtHead->FileHeader.NumberOfSections * sizeof (IMAGE_SECTION_HEADER);

	ULONG lFilePointer = lpParser->ToFilePointer(lOffset);
	if ( lFilePointer != EOF && 
		lpFile->Seek(lFilePointer, FILE_BEGIN) == lFilePointer &&
		lpFile->ReadT(SectHeadBuff) 
		)
	{
		if (
			SectHeadBuff.PointerToRawData != 0 &&
			SectHeadBuff.VirtualAddress != 0 &&
			SectHeadBuff.SizeOfRawData != 0 &&
			SectHeadBuff.Misc.VirtualSize != 0
			)
		{
			ULONG PointerToRawData = lpLastSectHead->PointerToRawData + lpLastSectHead->SizeOfRawData;
			ULONG VirutalAddress = lpLastSectHead->VirtualAddress + lpLastSectHead->Misc.VirtualSize;

			//	PointerToRawData = CPEParser::AligUP(PointerToRawData, lpNtHead->OptionalHeader.FileAlignment);
			//	VirutalAddress = CPEParser::AligUP(VirutalAddress, lpNtHead->OptionalHeader.SectionAlignment);

			if ( SectHeadBuff.PointerToRawData >= PointerToRawData &&
				SectHeadBuff.VirtualAddress >= VirutalAddress &&
				SectHeadBuff.PointerToRawData % lpNtHead->OptionalHeader.FileAlignment == 0 &&
				SectHeadBuff.VirtualAddress % lpNtHead->OptionalHeader.SectionAlignment == 0
				)
			{
				bResult = TRUE;
			}
		}
	}

	return bResult;
}

BOOL CPEiDentifier::IsRiskPacker(CPEParser* lpParser)
{
	DWORD dwSectCount = lpParser->GetSectionCount();
	PIMAGE_DOS_HEADER lpDosHead = lpParser->GetDosHead();
	PIMAGE_SECTION_HEADER lpFirstSectHead = lpParser->GetSectionHead(0);
	IPEFile* lpFile = lpParser->GetFileObject();
	ULONG lEntryPoint = lpParser->GetEntryPoint();
	ULONG lFilePointer = lpParser->ToFilePointer(lEntryPoint);
	BYTE cBuff[0x10];

	if ( lFilePointer == EOF ||
		lpFile->Seek(lFilePointer, FILE_BEGIN) != lFilePointer ||
		lpParser->GetFileObject()->Read(cBuff, 0x10) != 0x10
		)
	{
		return FALSE;
	}

	if ( memcmp(lpFirstSectHead->Name, ".nsp0\0\0\0", 8) == 0 ||
		memcmp(lpFirstSectHead->Name, "nsp0\0\0\0\0", 8) == 0
		)
	{
		// 北斗
		return TRUE;
	}

	if ( memcmp(lpDosHead, "MZKERNEL32.DLL\0\0", 0x10) == 0 )
	{
		// upack
		return TRUE;
	}

	if ( memcmp(lpDosHead, "MZ[C]Anskya!PE\0\0", 0x10) == 0 )
	{
		// fsg modify by anaskya!
		return TRUE;
	}

	if ( memcmp((PBYTE)lpDosHead + 0x14, "FSG!", 4) == 0 )
	{
		// FSG
		return TRUE;
	}

	// 是否是免杀型 upx
	if ( dwSectCount >= 3 )
	{
		if ( lpFirstSectHead->SizeOfRawData == 0 &&
			 memcmp(lpFirstSectHead->Name, "UPX0\0\0\0\0", 8) == 0 
			 )
		{
			if ( lpParser->IsDll() )
			{
				if ( memcmp(cBuff, "\x80\x7c\x24\x08\x01\x0F\x85", 7) == 0 )
				{
					return FALSE;
				}
			}
			else 
			{
				if ( cBuff[0] == 0x60 && (cBuff[1] == 0xBE || cBuff[1] == 0xE8))
				{
					return FALSE;
				}
			}

			return TRUE;
		}
	}

	return FALSE;
}

BOOL CPEiDentifier::IsNormalPacker(CPEParser* lpParser)
{
	/*
	判断的条件都是快速验证，没有深度判定
	*/
	DWORD dwSectCount = lpParser->GetSectionCount();
	ULONG lEntryPoint = lpParser->GetEntryPoint();
	PIMAGE_SECTION_HEADER lpFirstSectHead = lpParser->GetSectionHead(0);
	PIMAGE_SECTION_HEADER lpLastSectHead = lpFirstSectHead + dwSectCount - 1;
	
	// upx
	if ( dwSectCount >= 3 )
	{
		if ( lpFirstSectHead->SizeOfRawData == 0 &&
			memcmp(lpFirstSectHead->Name, "UPX0\0\0\0\0", 8) == 0
			)
		{
			return TRUE;
		}
	}

	// aspack
	if ( dwSectCount >= 3 )
	{
		if ( lpLastSectHead->SizeOfRawData == 0 )
		{
			PIMAGE_SECTION_HEADER lpSectHead = lpLastSectHead - 1;
			if ( lEntryPoint == lpFirstSectHead->VirtualAddress ||
				lEntryPoint == lpSectHead->VirtualAddress ||
				lEntryPoint == lpSectHead->VirtualAddress + 1
				)
			{
				return TRUE;
			}
		}
	}

	// themida
	if ( dwSectCount == 2 )
	{
		if ( lEntryPoint == lpFirstSectHead->VirtualAddress &&
			lpFirstSectHead->SizeOfRawData == 0x200 &&
			lpFirstSectHead->PointerToRawData == 0x200 &&
			lpLastSectHead->PointerToRawData == 0x400 &&
			lpFirstSectHead->Characteristics == 0x60000020 &&
			lpLastSectHead->Characteristics == 0xE0000020
			)
		{
			return TRUE;
		}
	}

	return FALSE;
}