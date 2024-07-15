#pragma once
#ifndef I18N_H
#define I18N_H

#include <string>
#include "standard.h"

// Define error codes only if not already defined
#ifndef ERROR_OUTOFMEMORY
#define ERROR_OUTOFMEMORY 2001
#endif

#ifndef ERROR_SOCKET_TIMEOUT
#define ERROR_SOCKET_TIMEOUT 2002
#endif

#ifndef ERROR_SOCKET_GENERALFAILURE
#define ERROR_SOCKET_GENERALFAILURE 2003
#endif

#ifndef ERROR_SOCKET_CANNOTRESOLVE
#define ERROR_SOCKET_CANNOTRESOLVE 2004
#endif

#ifndef ERROR_SOCKET_CLOSEDPORT
#define ERROR_SOCKET_CLOSEDPORT 2006
#endif

#define STRING_USAGE 1
#define STRING_CONNECT_INFO_FULL 1001
#define STRING_CONNECT_INFO_IP 1002
#define STRING_CONNECT_SUCCESS 1003
#define STRING_STATS 1004
#define STRING_CONNECTION_TIMEOUT 2005

class i18n_c
{
public:
	static std::string GetString(int string_id);
};

#endif // I18N_H
