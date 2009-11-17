/*
	CryptVolumeMount

	by opa
*/

#include <string>
#include <algorithm>

#include <windows.h>

#define PGM					"CryptVolumeMount"
#define PGM_DEBUG			PGM ": "
#define PGM_INFO			PGM ": "
#define PGM_WARN			PGM " warning: "
#define PGM_ERR				PGM " error: "
#define VERSTR				"1.01"

#define CREDIT2009			"Copyright (c) 2009 by opa"

typedef signed char schar;
typedef unsigned char uchar;
typedef signed int sint;
typedef unsigned int uint;
typedef signed long slong;
typedef unsigned long ulong;

using namespace std;

char
	credit[] = PGM " version " VERSTR " " CREDIT2009;
bool
	error = false;
sint
	rcode = 0;

////////////////////////////////////////////////////////////////////////

template <class BidirectionalIterator, class Predicate>
BidirectionalIterator rfind_if(BidirectionalIterator first, BidirectionalIterator last, Predicate pred)
{
	while(first != last){
		--last;
		if(pred(*last))
			break;
	}

	return last;
}

inline bool isbackslash(wchar_t c)
{
	return c == L'\\' || c == L'/';
}

////////////////////////////////////////////////////////////////////////

class String : public wstring {
	typedef String Self;
	typedef wstring Super;

public:
	String();
	String(const Super &s);
	String(const wchar_t *s);
	String(const_iterator b, const_iterator e);
	String(const char *s);
	~String();

	string to_ansi() const;
	Self to_upper() const;
	Self trim() const;
	bool isdoublequote() const;
	Self doublequote() const;
	Self doublequote_del() const;

	Self &operator=(const Self &s);
	Self &operator=(const wchar_t *s);
	Self &operator=(const char *s);

	Self &operator+=(const Self &s)				{ append(s); return *this; }
	Self &operator+=(const wchar_t *s)			{ append(s); return *this; }
	Self &operator+=(const char *s);

	Self &assign_from_ansi(const char *s);
	Self &assign_from_ansi(const string &s)		{ return assign_from_ansi(s.c_str()); }
	Self &assign_from_utf8(const char *s);
	Self &assign_from_utf8(const string &s)		{ return assign_from_utf8(s.c_str()); }
	Self &assign_from_env(const Self &name);
	Self &printf(Self format, ...);

	// filename operator
	bool have_path() const;
	bool have_ext() const;
	bool isbackslash() const;
	Self backslash() const;
	Self subext(const Self &ext) const;
	Self drivename() const;
	Self dirname() const;
	Self basename() const;
};

String::String()									{}
String::String(const String::Super &s)				: Super(s) {}
String::String(const wchar_t *s)					: Super(s) {}
String::String(const_iterator b, const_iterator e)	: Super(b, e) {}
String::String(const char *s)						{ assign_from_ansi(s); }
String::~String()									{}
String &String::operator=(const String &s)			{ assign(s); return *this; }
String &String::operator=(const wchar_t *s)			{ assign(s); return *this; }
String &String::operator=(const char *s)			{ assign_from_ansi(s); return *this; }
String &String::operator+=(const char *s)			{ append(String(s)); return *this; }
String operator+(const String &s1, const char *s2)	{ return s1 + String(s2); }
String operator+(const char *s1, const String &s2)	{ return String(s1) + s2; }
bool operator==(const String &s1, const char *s2)	{ return s1 == String(s2); }
bool operator==(const char *s1, const String &s2)	{ return String(s1) == s2; }

bool String::isdoublequote() const
{
	// BUG:
	//  単に先頭と最後の文字が " かどうかを判定しているだけなので、文字列の途中に " があった場合などを考慮していない。

	if(size() >= 2 && *begin() == L'"' && *(end()-1) == L'"')
		return true;

	return false;
}

String String::doublequote_del() const
{
	if(isdoublequote())
		return String(begin()+1, end()-1);
	else
		return String(*this);
}

String &String::assign_from_ansi(const char *s)
{
	sint
		size = MultiByteToWideChar(CP_ACP, 0, s, -1, NULL, 0);
	wchar_t
		*buf = new wchar_t[size+1];

	fill(buf, buf + size+1, 0);

	MultiByteToWideChar(CP_ACP, 0, s, -1, buf, size);

	assign(buf);

	delete [] buf;

	return *this;
}

bool String::isbackslash() const
{
	return size() > 0 && ::isbackslash(*(end()-1));
}

String String::dirname() const
{
	const_iterator
		e = ::rfind_if(begin(), end(), ::isbackslash);

	if(e != end() && ::isbackslash(*e))
		++e;

	return Self(begin(), e);
}


////////////////////////////////////////////////////////////////////////

class WindowsAPI {
public:
	static bool CreateProcess(const String &cmd, DWORD CreationFlags, const String &wd, LPSTARTUPINFO si, LPPROCESS_INFORMATION pi);
	static String GetClipboardText();
	static String GetCommandLine()		{ return ::GetCommandLine(); }
	static String GetComputerName();
	static String GetModuleFileName(HMODULE Module = 0);
	static String GetTempPath();
	static String GetUserName();
	static String SHGetSpecialFolder(sint nFolder);
};

String WindowsAPI::GetModuleFileName(HMODULE Module)
{
	wchar_t
		buf[MAX_PATH+1];
	DWORD
		size = sizeof buf / sizeof(wchar_t);

	size = ::GetModuleFileName(Module, buf, size);

	if(size == 0)
		return String();

	return String(buf, buf + size);
}

////////////////////////////////////////////////////////////////////////

String get_command_name(const String &cmd)
{
	bool
		in_quote = false;

	for(String::const_iterator i = cmd.begin() ; i != cmd.end() ; ++i){
		wchar_t
			c = *i;

		if(!in_quote)
			if(iswspace(c))
				return String(cmd.begin(), i).doublequote_del();

		if(c == L'"')
			in_quote = in_quote ? false : true;
	}

	return cmd.doublequote_del();
}

bool CreateDaemonProcess(const String &cmd)
{
	STARTUPINFO
		si;
	PROCESS_INFORMATION
		pi;
	DWORD
		CreationFlags = 0;
	wchar_t
		*cmd_c_str;
	String
		wd(get_command_name(cmd).dirname());
	bool
		r;

	memset(&si, 0, sizeof si);
	si.cb = sizeof si;
	memset(&pi, 0, sizeof pi);

	cmd_c_str = new wchar_t[cmd.size()+1];
	copy(cmd.begin(), cmd.end(), cmd_c_str);
	cmd_c_str[cmd.size()] = 0;

//	CreationFlags |= CREATE_BREAKAWAY_FROM_JOB;
	CreationFlags |= CREATE_DEFAULT_ERROR_MODE;
	CreationFlags |= CREATE_NEW_PROCESS_GROUP;
	CreationFlags |= CREATE_NO_WINDOW;

	si.wShowWindow = SW_SHOWMINIMIZED;
	si.dwFlags |= STARTF_USESHOWWINDOW;
	si.dwFlags |= STARTF_FORCEOFFFEEDBACK;

	r = ::CreateProcess(NULL, cmd_c_str, NULL, NULL, FALSE, CreationFlags, NULL, (wd.size()>0 ? wd.c_str() : NULL), &si, &pi);

	delete [] cmd_c_str;

	return r;
}

void execute_command(const String &cmdline)
{
//	MessageBox(0, cmdline.c_str(), String(PGM).c_str(), 0);

	if(CreateDaemonProcess(cmdline) == 0){
		MessageBox(0, String("Command execute error").c_str(), String(PGM).c_str(), 0);
		error = true;
		rcode = 1;
	}
}

void CryptVolume_main()
{
	String
		exedir(WindowsAPI::GetModuleFileName().dirname()),
		cmdline;

#ifdef CRYPTVOLUME_MOUNT

//	CryptVolumeMount.bat:
//	start "TrueCrypt" "%~d0\bin\TrueCrypt.exe" /volume "%~d0\#CryptVolume@opa.tcdata" /auto /cache yes /quit background

	cmdline = "\"" + exedir + "bin\\TrueCrypt.exe\" /volume \"" + exedir + "#CryptVolume@opa.tcdata\" /auto /cache yes /quit background";
	execute_command(cmdline);

#endif

#ifdef CRYPTVOLUME_UNMOUNT

//	CryptVolumeUnmount.bat:
//	start "TrueCrypt" "%~d0\bin\TrueCrypt.exe" /dismount /quit
//	start "UnplugDrive" "%~d0\bin\UnplugDrive.exe"

	cmdline = "\"" + exedir + "bin\\TrueCrypt.exe\" /dismount /quit";
	execute_command(cmdline);

	cmdline = "\"" + exedir + "bin\\UnplugDrive.exe\"";
	execute_command(cmdline);

#endif
}

// ダミーのメッセージを送受信することで、プログラムが動き出したことをOSに伝える
void dummy_message()
{
	MSG msg;
	PostMessage(NULL, WM_APP, 0, 0);
	GetMessage(&msg, NULL, WM_APP, WM_APP);
}

extern "C"
sint WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
	dummy_message();

	CryptVolume_main();

	return rcode;
}

