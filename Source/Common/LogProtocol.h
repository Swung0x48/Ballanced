#ifndef COMMON_LOGPROTOCOL_H
#define COMMON_LOGPROTOCOL_H

#include <exception>

class CLogProtocolException : public std::exception { };

class CLogProtocol
{
public:
	static CLogProtocol &GetInstance();
	void Open(const char *filename, const char *path, bool overwrite);
	void Write(const char *str);

private:
	CLogProtocol();
	CLogProtocol(const CLogProtocol &);
	CLogProtocol& operator=(const CLogProtocol&);
	
	char m_FullPath[512];
	char m_FileName[512];
	char m_Path[512];
};

#define TT_LOG_OPEN(filename, path, overwrite) CLogProtocol::GetInstance().Open(filename, path, overwrite)
#define TT_LOG(str) CLogProtocol::GetInstance().Write(str)

#endif // COMMON_LOGPROTOCOL_H
