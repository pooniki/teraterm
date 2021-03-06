/* Tera Term
 Copyright(C) 1994-1998 T. Teranishi
 All rights reserved. */

/* misc. routines  */
#include "teraterm.h"
#include <sys/stat.h>
#include <sys/utime.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include "tttypes.h"
#include <shlobj.h>
#include <ctype.h>

// for _ismbblead
#include <mbctype.h>

// for isInvalidFileNameChar / replaceInvalidFileNameChar
static char *invalidFileNameChars = "\\/:*?\"<>|";

// ファイルに使用することができない文字
// cf. Naming Files, Paths, and Namespaces
//     http://msdn.microsoft.com/en-us/library/aa365247.aspx
// (2013.3.9 yutaka)
static char *invalidFileNameStrings[] = {
	"AUX", "CLOCK$", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
	"CON", "CONFIG$", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
	"NUL", "PRN",
	".", "..", 
	NULL
};


// for b64encode/b64decode
static char *b64enc_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static char b64dec_table[] = {
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
   52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
   -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
   15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
   -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
   41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

void b64encode(PCHAR d, int dsize, PCHAR s, int len)
{
	unsigned int b = 0;
	int state = 0;
	unsigned char *src, *dst;

	src = (unsigned char *)s;
	dst = (unsigned char *)d;

	if (dsize == 0 || dst == NULL || src == NULL) {
		return;
	}
	if (dsize < 5) {
		*dst = 0;
		return;
	}

	while (len > 0) {
		b = (b << 8) | *src++;
		len--;
		state++;

		if (state == 3) {
			*dst++ = b64enc_table[(b>>18) & 0x3f];
			*dst++ = b64enc_table[(b>>12) & 0x3f];
			*dst++ = b64enc_table[(b>>6) & 0x3f];
			*dst++ = b64enc_table[b & 0x3f];
			dsize -= 4;
			state = 0;
			b = 0;
			if (dsize < 5)
				break;
		}
	}

	if (dsize >= 5) {
		if (state == 1) {
			*dst++ = b64enc_table[(b>>2) & 0x3f];
			*dst++ = b64enc_table[(b<<4) & 0x3f];
			*dst++ = '=';
			*dst++ = '=';
		}
		else if (state == 2) {
			*dst++ = b64enc_table[(b>>10) & 0x3f];
			*dst++ = b64enc_table[(b>>4) & 0x3f];
			*dst++ = b64enc_table[(b<<2) & 0x3f];
			*dst++ = '=';
		}
	}

	*dst = 0;
	return;
}

int b64decode(PCHAR dst, int dsize, PCHAR src)
{
	unsigned int b = 0;
	char c;
	int len = 0, state = 0;

	if (src == NULL || dst == NULL || dsize == 0)
		return 0;

	while (1) {
		if (isspace(*src)) {
			src++;
			continue;
		}

		if ((c = b64dec_table[*src++]) == -1)
			break;

		b = (b << 6) | c;
		state++;

		if (state == 4) {
			if (dsize > len)
				dst[len++] = (b >> 16) & 0xff;
			if (dsize > len)
				dst[len++] = (b >> 8) & 0xff;
			if (dsize > len)
				dst[len++] = b & 0xff;
			state = 0;
			if (dsize <= len)
				break;
		}
	}

	if (state == 2) {
		b <<= 4;
		if (dsize > len)
			dst[len++] = (b >> 8) & 0xff;
	}
	else if (state == 3) {
		b <<= 6;
		if (dsize > len)
			dst[len++] = (b >> 16) & 0xff;
		if (dsize > len)
			dst[len++] = (b >> 8) & 0xff;
	}
	return len;
}

BOOL GetFileNamePos(PCHAR PathName, int far *DirLen, int far *FNPos)
{
	BYTE b;
	LPTSTR Ptr, DirPtr, FNPtr, PtrOld;

	*DirLen = 0;
	*FNPos = 0;
	if (PathName==NULL)
		return FALSE;

	if ((strlen(PathName)>=2) && (PathName[1]==':'))
		Ptr = &PathName[2];
	else
		Ptr = PathName;
	if (Ptr[0]=='\\')
		Ptr = CharNext(Ptr);

	DirPtr = Ptr;
	FNPtr = Ptr;
	while (Ptr[0]!=0) {
		b = Ptr[0];
		PtrOld = Ptr;
		Ptr = CharNext(Ptr);
		switch (b) {
			case ':':
				return FALSE;
			case '\\':
				DirPtr = PtrOld;
				FNPtr = Ptr;
				break;
		}
	}
	*DirLen = DirPtr-PathName;
	*FNPos = FNPtr-PathName;
	return TRUE;
}

BOOL ExtractFileName(PCHAR PathName, PCHAR FileName, int destlen)
{
	int i, j;

	if (FileName==NULL)
		return FALSE;
	if (! GetFileNamePos(PathName,&i,&j))
		return FALSE;
	strncpy_s(FileName,destlen,&PathName[j],_TRUNCATE);
	return (strlen(FileName)>0);
}

BOOL ExtractDirName(PCHAR PathName, PCHAR DirName)
{
	int i, j;

	if (DirName==NULL)
		return FALSE;
	if (! GetFileNamePos(PathName,&i,&j))
		return FALSE;
	memmove(DirName,PathName,i); // do not use memcpy
	DirName[i] = 0;
	return TRUE;
}

/* fit a filename to the windows-filename format */
/* FileName must contain filename part only. */
void FitFileName(PCHAR FileName, int destlen, PCHAR DefExt)
{
	int i, j, NumOfDots;
	char Temp[MAX_PATH];
	BYTE b;

	NumOfDots = 0;
	i = 0;
	j = 0;
	/* filename started with a dot is illeagal */
	if (FileName[0]=='.') {
		Temp[0] = '_';  /* insert an underscore char */
		j++;
	}

	do {
		b = FileName[i];
		i++;
		if (b=='.')
			NumOfDots++;
		if ((b!=0) &&
		    (j < MAX_PATH-1)) {
			Temp[j] = b;
			j++;
		}
	} while (b!=0);
	Temp[j] = 0;

	if ((NumOfDots==0) &&
	    (DefExt!=NULL)) {
		/* add the default extension */
		strncat_s(Temp,sizeof(Temp),DefExt,_TRUNCATE);
	}

	strncpy_s(FileName,destlen,Temp,_TRUNCATE);
}

// Append a slash to the end of a path name
void AppendSlash(PCHAR Path, int destlen)
{
	if (strcmp(CharPrev((LPCTSTR)Path,
	           (LPCTSTR)(&Path[strlen(Path)])),
	           "\\") != 0) {
		strncat_s(Path,destlen,"\\",_TRUNCATE);
	}
}

// Delete slashes at the end of a path name
void DeleteSlash(PCHAR Path)
{
	size_t i;
	for (i=strlen(Path)-1; i>=0; i--) {
		if (i ==0 && Path[i] == '\\' ||
		    Path[i] == '\\' && !_ismbblead(Path[i-1])) {
			Path[i] = '\0';
		}
		else {
			break;
		}
	}
}

void Str2Hex(PCHAR Str, PCHAR Hex, int Len, int MaxHexLen, BOOL ConvSP)
{
	BYTE b, low;
	int i, j;

	if (ConvSP)
		low = 0x20;
	else
		low = 0x1F;

	j = 0;
	for (i=0; i<=Len-1; i++) {
		b = Str[i];
		if ((b!='$') && (b>low) && (b<0x7f)) {
			if (j < MaxHexLen) {
				Hex[j] = b;
				j++;
			}
		}
		else {
			if (j < MaxHexLen-2) {
				Hex[j] = '$';
				j++;
				if (b<=0x9f) {
					Hex[j] = (char)((b >> 4) + 0x30);
				}
				else {
					Hex[j] = (char)((b >> 4) + 0x37);
				}
				j++;
				if ((b & 0x0f) <= 0x9) {
					Hex[j] = (char)((b & 0x0f) + 0x30);
				}
				else {
					Hex[j] = (char)((b & 0x0f) + 0x37);
				}
				j++;
			}
		}
	}
	Hex[j] = 0;
}

BYTE ConvHexChar(BYTE b)
{
	if ((b>='0') && (b<='9')) {
		return (b - 0x30);
	}
	else if ((b>='A') && (b<='F')) {
		return (b - 0x37);
	}
	else if ((b>='a') && (b<='f')) {
		return (b - 0x57);
	}
	else {
		return 0;
	}
}

int Hex2Str(PCHAR Hex, PCHAR Str, int MaxLen)
{
	BYTE b, c;
	int i, imax, j;

	j = 0;
	imax = strlen(Hex);
	i = 0;
	while ((i < imax) && (j<MaxLen)) {
		b = Hex[i];
		if (b=='$') {
			i++;
			if (i < imax) {
				c = Hex[i];
			}
			else {
				c = 0x30;
			}
			b = ConvHexChar(c) << 4;
			i++;
			if (i < imax) {
				c = Hex[i];
			}
			else {
				c = 0x30;
			}
			b = b + ConvHexChar(c);
		};

		Str[j] = b;
		j++;
		i++;
	}
	if (j<MaxLen) {
		Str[j] = 0;
	}

	return j;
}

BOOL DoesFileExist(PCHAR FName)
{
	// check if a file exists or not
	// フォルダまたはファイルがあれば TRUE を返す
	struct _stat st;

	return (_stat(FName,&st)==0);
}

BOOL DoesFolderExist(PCHAR FName)
{
	// check if a folder exists or not
	// マクロ互換性のため
	// DoesFileExist は従来通りフォルダまたはファイルがあれば TRUE を返し
	// DoesFileExist はフォルダがある場合のみ TRUE を返す。
	struct _stat st;

	if (_stat(FName,&st)==0) {
		if ((st.st_mode & _S_IFDIR) > 0) {
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
}

long GetFSize(PCHAR FName)
{
	struct _stat st;

	if (_stat(FName,&st)==-1) {
		return 0;
	}
	return (long)st.st_size;
}

long GetFMtime(PCHAR FName)
{
	struct _stat st;

	if (_stat(FName,&st)==-1) {
		return 0;
	}
	return (long)st.st_mtime;
}

BOOL SetFMtime(PCHAR FName, DWORD mtime)
{
	struct _utimbuf filetime;

	filetime.actime = mtime;
	filetime.modtime = mtime;
	return _utime(FName, &filetime);
}

void uint2str(UINT i, PCHAR Str, int destlen, int len)
{
	char Temp[20];

	memset(Temp, 0, sizeof(Temp));
	_snprintf_s(Temp,sizeof(Temp),_TRUNCATE,"%u",i);
	Temp[len] = 0;
	strncpy_s(Str,destlen,Temp,_TRUNCATE);
}

void QuoteFName(PCHAR FName)
{
	int i;

	if (FName[0]==0) {
		return;
	}
	if (strchr(FName,' ')==NULL) {
		return;
	}
	i = strlen(FName);
	memmove(&(FName[1]),FName,i);
	FName[0] = '\"';
	FName[i+1] = '\"';
	FName[i+2] = 0;
}

// ファイル名に使用できない文字が含まれているか確かめる (2006.8.28 maya)
int isInvalidFileNameChar(PCHAR FName)
{
	int i, len;
	char **p, c;

	// チェック対象の文字を強化した。(2013.3.9 yutaka)
	p = invalidFileNameStrings;
	while (*p) {
		if (_strcmpi(FName, *p) == 0) {
			return 1;  // Invalid
		}
		p++;
	}

	len = strlen(FName);
	for (i=0; i<len; i++) {
		if (_ismbblead(FName[i])) {
			i++;
			continue;
		}
		if ((FName[i] >= 0 && FName[i] < ' ') || strchr(invalidFileNameChars, FName[i])) {
			return 1;
		}
	}

	// ファイル名の末尾にピリオドおよび空白はNG。
	c = FName[len - 1];
	if (c == '.' || c == ' ')
		return 1;

	return 0;
}

// ファイル名に使用できない文字を c に置き換える
// c に 0 を指定した場合は文字を削除する
void replaceInvalidFileNameChar(PCHAR FName, unsigned char c)
{
	int i, j=0, len;

	if ((c >= 0 && c < ' ') || strchr(invalidFileNameChars, c)) {
		c = 0;
	}

	len = strlen(FName);
	for (i=0; i<len; i++) {
		if (_ismbblead(FName[i])) {
			FName[j++] = FName[i];
			FName[j++] = FName[++i];
			continue;
		}
		if ((FName[i] >= 0 && FName[i] < ' ') || strchr(invalidFileNameChars, FName[i])) {
			if (c) {
				FName[j++] = c;
			}
		}
		else {
			FName[j++] = FName[i];
		}
	}
	FName[j] = 0;
}

// strftime に渡せない文字が含まれているか確かめる (2006.8.28 maya)
int isInvalidStrftimeChar(PCHAR FName)
{
	int i, len, p;

	len = strlen(FName);
	for (i=0; i<len; i++) {
		if (FName[i] == '%') {
			if (FName[i+1] != 0) {
				p = i+1;
				if (FName[i+2] != 0 && FName[i+1] == '#') {
					p = i+2;
				}
				switch (FName[p]) {
					case 'a':
					case 'A':
					case 'b':
					case 'B':
					case 'c':
					case 'd':
					case 'H':
					case 'I':
					case 'j':
					case 'm':
					case 'M':
					case 'p':
					case 'S':
					case 'U':
					case 'w':
					case 'W':
					case 'x':
					case 'X':
					case 'y':
					case 'Y':
					case 'z':
					case 'Z':
					case '%':
						i = p;
						break;
					default:
						return 1;
				}
			}
			else {
				// % で終わっている場合はエラーとする
				return 1;
			}
		}
	}

	return 0;
}

// strftime に渡せない文字を削除する (2006.8.28 maya)
void deleteInvalidStrftimeChar(PCHAR FName)
{
	int i, j=0, len, p;

	len = strlen(FName);
	for (i=0; i<len; i++) {
		if (FName[i] == '%') {
			if (FName[i+1] != 0) {
				p = i+1;
				if (FName[i+2] != 0 && FName[i+1] == '#') {
					p = i+2;
				}
				switch (FName[p]) {
					case 'a':
					case 'A':
					case 'b':
					case 'B':
					case 'c':
					case 'd':
					case 'H':
					case 'I':
					case 'j':
					case 'm':
					case 'M':
					case 'p':
					case 'S':
					case 'U':
					case 'w':
					case 'W':
					case 'x':
					case 'X':
					case 'y':
					case 'Y':
					case 'z':
					case 'Z':
					case '%':
						FName[j] = FName[i]; // %
						j++;
						i++;
						if (p-i == 2) {
							FName[j] = FName[i]; // #
							j++;
							i++;
						}
						FName[j] = FName[i];
						j++;
						break;
					default:
						i++; // %
						if (p-i == 2) {
							i++; // #
						}
				}
			}
			// % で終わっている場合はコピーしない
		}
		else {
			FName[j] = FName[i];
			j++;
		}
	}

	FName[j] = 0;
}

// フルパスから、ファイル名部分のみを strftime で変換する (2006.8.28 maya)
void ParseStrftimeFileName(PCHAR FName, int destlen)
{
	char filename[MAX_PATH];
	char dirname[MAX_PATH];
	char buf[MAX_PATH];
	char *c;
	time_t time_local;
	struct tm *tm_local;

	// ファイル名部分のみを flename に格納
	ExtractFileName(FName, filename ,sizeof(filename));

	// strftime に使用できない文字を削除
	deleteInvalidStrftimeChar(filename);

	// 現在時刻を取得
	time(&time_local);
	tm_local = localtime(&time_local);

	// 時刻文字列に変換
	if (strftime(buf, sizeof(buf), filename, tm_local) == 0) {
		strncpy_s(buf, sizeof(buf), filename, _TRUNCATE);
	}

	// ファイル名に使用できない文字を削除
	deleteInvalidFileNameChar(buf);

	c = strrchr(FName, '\\');
	if (c != NULL) {
		ExtractDirName(FName, dirname);
		strncpy_s(FName, destlen, dirname, _TRUNCATE);
		AppendSlash(FName,destlen);
		strncat_s(FName, destlen, buf, _TRUNCATE);
	}
	else { // "\"を含まない(フルパスでない)場合に対応 (2006.11.30 maya)
		strncpy_s(FName, destlen, buf, _TRUNCATE);
	}
}

void ConvFName(PCHAR HomeDir, PCHAR Temp, int templen, PCHAR DefExt, PCHAR FName, int destlen)
{
	// destlen = sizeof FName
	int DirLen, FNPos;

	FName[0] = 0;
	if ( ! GetFileNamePos(Temp,&DirLen,&FNPos) ) {
		return;
	}
	FitFileName(&Temp[FNPos],templen - FNPos,DefExt);
	if ( DirLen==0 ) {
		strncpy_s(FName,destlen,HomeDir,_TRUNCATE);
		AppendSlash(FName,destlen);
	}
	strncat_s(FName,destlen,Temp,_TRUNCATE);
}

// "\n" を改行に変換する (2006.7.29 maya)
// "\t" をタブに変換する (2006.11.6 maya)
void RestoreNewLine(PCHAR Text)
{
	int i, j=0, size=strlen(Text);
	char *buf = (char *)_alloca(size+1);

	memset(buf, 0, size+1);
	for (i=0; i<size; i++) {
		if (Text[i] == '\\' && i<size ) {
			switch (Text[i+1]) {
				case '\\':
					buf[j] = '\\';
					i++;
					break;
				case 'n':
					buf[j] = '\n';
					i++;
					break;
				case 't':
					buf[j] = '\t';
					i++;
					break;
				case '0':
					buf[j] = '\0';
					i++;
					break;
				default:
					buf[j] = '\\';
			}
			j++;
		}
		else {
			buf[j] = Text[i];
			j++;
		}
	}
	/* use memcpy to copy with '\0' */
	memcpy(Text, buf, size);
}

BOOL GetNthString(PCHAR Source, int Nth, int Size, PCHAR Dest)
{
	int i, j, k;

	i = 1;
	j = 0;
	k = 0;

	while (i<Nth && Source[j] != 0) {
		if (Source[j++] == ',') {
			i++;
		}
	}

	if (i == Nth) {
		while (Source[j] != 0 && Source[j] != ',' && k<Size-1) {
			Dest[k++] = Source[j++];
		}
	}

	Dest[k] = 0;
	return (i>=Nth);
}

void GetNthNum(PCHAR Source, int Nth, int far *Num)
{
	char T[15];

	GetNthString(Source,Nth,sizeof(T),T);
	if (sscanf(T, "%d", Num) != 1) {
		*Num = 0;
	}
}

void WINAPI GetDefaultFName(char *home, char *file, char *dest, int destlen)
{
	// My Documents に file がある場合、
	// それを読み込むようにした。(2007.2.18 maya)
	char MyDoc[MAX_PATH];
	char MyDocSetupFName[MAX_PATH];
	LPITEMIDLIST pidl;

	IMalloc *pmalloc;
	SHGetMalloc(&pmalloc);
	if (SHGetSpecialFolderLocation(NULL, CSIDL_PERSONAL, &pidl) == S_OK) {
		SHGetPathFromIDList(pidl, MyDoc);
		pmalloc->lpVtbl->Free(pmalloc, pidl);
		pmalloc->lpVtbl->Release(pmalloc);
	}
	else {
		pmalloc->lpVtbl->Release(pmalloc);
		goto homedir;
	}
	strncpy_s(MyDocSetupFName, sizeof(MyDocSetupFName), MyDoc, _TRUNCATE);
	AppendSlash(MyDocSetupFName,sizeof(MyDocSetupFName));
	strncat_s(MyDocSetupFName, sizeof(MyDocSetupFName), file, _TRUNCATE);
	if (GetFileAttributes(MyDocSetupFName) != -1) {
		strncpy_s(dest, destlen, MyDocSetupFName, _TRUNCATE);
		return;
	}

homedir:
	strncpy_s(dest, destlen, home, _TRUNCATE);
	AppendSlash(dest,destlen);
	strncat_s(dest, destlen, file, _TRUNCATE);
}

// デフォルトの TERATERM.INI のフルパスを ttpmacro からも
// 取得するために追加した。(2007.2.18 maya)
void GetDefaultSetupFName(char *home, char *dest, int destlen)
{
	GetDefaultFName(home, "TERATERM.INI", dest, destlen);
}

void GetUILanguageFile(char *buf, int buflen)
{
	char HomeDir[MAX_PATH];
	char Temp[MAX_PATH];
	char SetupFName[MAX_PATH];
	char CurDir[MAX_PATH];

	/* Get home directory */
	if (GetModuleFileName(NULL,Temp,sizeof(Temp)) == 0) {
		memset(buf, 0, buflen);
		return;
	}
	ExtractDirName(Temp, HomeDir);

	/* Get SetupFName */
	GetDefaultSetupFName(HomeDir, SetupFName, sizeof(SetupFName));
	
	/* Get LanguageFile name */
	GetPrivateProfileString("Tera Term", "UILanguageFile", "",
	                        Temp, sizeof(Temp), SetupFName);

	GetCurrentDirectory(sizeof(CurDir), CurDir);
	SetCurrentDirectory(HomeDir);
	_fullpath(buf, Temp, buflen);
	SetCurrentDirectory(CurDir);
}

// 指定したエントリを teraterm.ini から読み取る (2009.3.23 yutaka)
void GetOnOffEntryInifile(char *entry, char *buf, int buflen)
{
	char HomeDir[MAX_PATH];
	char Temp[MAX_PATH];
	char SetupFName[MAX_PATH];

	/* Get home directory */
	if (GetModuleFileName(NULL,Temp,sizeof(Temp)) == 0) {
		strncpy_s(buf, buflen, "off", _TRUNCATE);
		return;
	}
	ExtractDirName(Temp, HomeDir);

	/* Get SetupFName */
	GetDefaultSetupFName(HomeDir, SetupFName, sizeof(SetupFName));
	
	/* Get LanguageFile name */
	GetPrivateProfileString("Tera Term", entry, "off",
	                        Temp, sizeof(Temp), SetupFName);

	strncpy_s(buf, buflen, Temp, _TRUNCATE);
}

void get_lang_msg(PCHAR key, PCHAR buf, int buf_len, PCHAR def, PCHAR iniFile)
{
	GetI18nStr("Tera Term", key, buf, buf_len, def, iniFile);
}

int get_lang_font(PCHAR key, HWND dlg, PLOGFONT logfont, HFONT *font, PCHAR iniFile)
{
	if (GetI18nLogfont("Tera Term", key, logfont,
	                   GetDeviceCaps(GetDC(dlg),LOGPIXELSY),
	                   iniFile) == FALSE) {
		return FALSE;
	}

	if ((*font = CreateFontIndirect(logfont)) == NULL) {
		return FALSE;
	}

	return TRUE;
}

//
// cf. http://homepage2.nifty.com/DSS/VCPP/API/SHBrowseForFolder.htm
//
int CALLBACK setDefaultFolder(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	if(uMsg == BFFM_INITIALIZED) {
		SendMessage(hwnd, BFFM_SETSELECTION, (WPARAM)TRUE, lpData);
	}
	return 0;
}

BOOL doSelectFolder(HWND hWnd, char *path, int pathlen, char *def, char *msg)
{
	BROWSEINFO      bi;
	LPITEMIDLIST    pidlRoot;      // ブラウズのルートPIDL
	LPITEMIDLIST    pidlBrowse;    // ユーザーが選択したPIDL
	char buf[MAX_PATH];
	BOOL ret = FALSE;

	// ダイアログ表示時のルートフォルダのPIDLを取得
	// ※以下はデスクトップをルートとしている。デスクトップをルートとする
	//   場合は、単に bi.pidlRoot に０を設定するだけでもよい。その他の特
	//   殊フォルダをルートとする事もできる。詳細はSHGetSpecialFolderLoca
	//   tionのヘルプを参照の事。
	if (!SUCCEEDED(SHGetSpecialFolderLocation(hWnd, CSIDL_DESKTOP, &pidlRoot))) {
		return FALSE;
	}

	// BROWSEINFO構造体の初期値設定
	// ※BROWSEINFO構造体の各メンバの詳細説明もヘルプを参照
	bi.hwndOwner = hWnd;
	bi.pidlRoot = pidlRoot;
	bi.pszDisplayName = buf;
	bi.lpszTitle = msg;
	bi.ulFlags = 0;
	bi.lpfn = setDefaultFolder;
	bi.lParam = (LPARAM)def;
	// フォルダ選択ダイアログの表示 
	pidlBrowse = SHBrowseForFolder(&bi);
	if (pidlBrowse != NULL) {  
		// PIDL形式の戻り値のファイルシステムのパスに変換
		if (SHGetPathFromIDList(pidlBrowse, buf)) {
			// 取得成功
			strncpy_s(path, pathlen, buf, _TRUNCATE);
			ret = TRUE;
		}
		// SHBrowseForFolderの戻り値PIDLを解放
		CoTaskMemFree(pidlBrowse);
	}
	// クリーンアップ処理
	CoTaskMemFree(pidlRoot);

	return ret;
}

void OutputDebugPrintf(char *fmt, ...) {
	char tmp[1024];
	va_list arg;
	va_start(arg, fmt);
	_vsnprintf(tmp, sizeof(tmp), fmt, arg);
	OutputDebugString(tmp);
}

BOOL is_NT4()
{
	OSVERSIONINFO osvi;

	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);
	if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT &&
	    osvi.dwMajorVersion == 4) {
		return TRUE;
	}
	return FALSE;
}

int get_OPENFILENAME_SIZE()
{
	OSVERSIONINFO osvi;

	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);
	if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT &&
	    osvi.dwMajorVersion >= 5) {
		return sizeof(OPENFILENAME);
	}
	//return OPENFILENAME_SIZE_VERSION_400;
	return 76;
}

// convert table for KanjiCodeID and ListID
// cf. KanjiList,KanjiListSend
//     KoreanList,KoreanListSend
//     Utf8List,Utf8ListSend
//     IdSJIS, IdEUC, IdJIS, IdUTF8, IdUTF8m
//     IdEnglish, IdJapanese, IdRussian, IdKorean, IdUtf8
/* KanjiCode2List(Language,KanjiCodeID) returns ListID */
int KanjiCode2List(int lang, int kcode)
{
	int Table[5][5] = {
		{1, 2, 3, 4, 5}, /* English (dummy) */
		{1, 2, 3, 4, 5}, /* Japanese(dummy) */
		{1, 2, 3, 4, 5}, /* Russian (dummy) */
		{1, 1, 1, 2, 3}, /* Korean */
		{1, 1, 1, 1, 2}, /* Utf8 */
	};
	lang--;
	kcode--;
	return Table[lang][kcode];
}
/* List2KanjiCode(Language,ListID) returns KanjiCodeID */
int List2KanjiCode(int lang, int list)
{
	int Table[5][5] = {
		{1, 2, 3, 4, 5}, /* English (dummy) */
		{1, 2, 3, 4, 5}, /* Japanese(dummy) */
		{1, 2, 3, 4, 5}, /* Russian (dummy) */
		{1, 4, 5, 1, 1}, /* Korean */
		{4, 5, 4, 4, 4}, /* Utf8 */
	};
	lang--;
	list--;
	if (list < 0) {
		list = 0;
	}
	return Table[lang][list];
}
/* KanjiCodeTranslate(Language(dest), KanjiCodeID(source)) returns KanjiCodeID */
int KanjiCodeTranslate(int lang, int kcode)
{
	int Table[5][5] = {
		{1, 2, 3, 4, 5}, /* to English (dummy) */
		{1, 2, 3, 4, 5}, /* to Japanese(dummy) */
		{1, 2, 3, 4, 5}, /* to Russian (dummy) */
		{1, 1, 1, 4, 5}, /* to Korean */
		{4, 4, 4, 4, 5}, /* to Utf8 */
	};
	lang--;
	kcode--;
	return Table[lang][kcode];
}

char *mctimelocal()
{
	SYSTEMTIME LocalTime;
	static char strtime[29];
	char week[][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
	char month[][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	                   "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	GetLocalTime(&LocalTime);
	_snprintf_s(strtime, sizeof(strtime), _TRUNCATE,
	            "%s %s %02d %02d:%02d:%02d.%03d %04d",
	            week[LocalTime.wDayOfWeek],
	            month[LocalTime.wMonth-1],
	            LocalTime.wDay,
	            LocalTime.wHour,
	            LocalTime.wMinute,
	            LocalTime.wSecond,
	            LocalTime.wMilliseconds,
	            LocalTime.wYear);

	return strtime;
}

PCHAR FAR PASCAL GetParam(PCHAR buff, int size, PCHAR param)
{
	int i = 0;
	BOOL quoted = FALSE;

	while (*param == ' ' || *param == '\t') {
		param++;
	}

	if (*param == '\0' || *param == ';') {
		return NULL;
	}

#if 1
	while (*param != '\0' && (quoted || (*param != ';' && *param != ' ' && *param != '\t'))) {
		if (*param == '"') {
			if ((!quoted && *(param+1) != '"') || (quoted && *(param+1) != '"')) {
				quoted = !quoted;
			}
			else {
				if (i < size - 1) {
					buff[i++] = *param;
				}
				param++;
			}
		}
		if (i < size - 1) {
			buff[i++] = *param;
		}
		param++;
	}
	if (!quoted && (buff[i-1] == ';')) {
		i--;
	}
#else
	while (*param != '\0' && (quoted || (*param != ';' && *param != ' ' && *param != '\t'))) {
		if (*param == '"' && (*++param != '"' || !quoted)) {
			quoted = !quoted;
			continue;
		}
		else if (i < size - 1) {
			buff[i++] = *param;
		}
		param++;
	}
#endif

	buff[i] = '\0';
	return (param);
}

void FAR PASCAL DequoteParam(PCHAR dest, int dest_len, PCHAR src)
{
	int i, j;
	char q, c;

	dest[0] = 0;
	if (src[0] == 0)
		return;
	i = 0;
	/* quoting char */
	q = src[i];
	/* only '"' is used as quoting char */
	if (q == '"')
		i++;

	c = src[i];
	i++;
	j = 0;
	while ((c != 0 && j<dest_len)) {
		if (c != '"') {
			dest[j] = c;
			j++;
		}
		else {
			if (q == '"' && src[i] == '"') {
				dest[j] = c;
				j++;
				i++;
			}
			else if (q == '"' && src[i] == '\0') {
				break;
			}
			else {
				dest[j] = c;
				j++;
			}
		}
		c = src[i];
		i++;
	}

	dest[j] = 0;
}

void split_buffer(char *buffer, int delimiter, char **head, char **body)
{
	char *p1, *p2;

	*head = *body = NULL;

	if (!isalnum(*buffer) || (p1 = strchr(buffer, delimiter)) == NULL) {
		return;
	}

	*head = buffer;

	p2 = buffer;
	while (p2 < p1 && !isspace(*p2)) {
		p2++;
	}

	*p2 = '\0';

	p1++;
	while (*p1 && isspace(*p1)) {
		p1++;
	}

	*body = p1;
}
