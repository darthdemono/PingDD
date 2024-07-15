#include "i18n.h"
#include <iostream>
#include <new>

std::string i18n_c::GetString(int string_id)
{
	switch (string_id)
	{
	case ERROR_OUTOFMEMORY:
		return "Out of memory";

	case ERROR_SOCKET_TIMEOUT:
		return "Connection timed out";

	case ERROR_SOCKET_GENERALFAILURE:
		return "General failure";

	case ERROR_SOCKET_CANNOTRESOLVE:
		return "Cannot resolve host";

	case ERROR_SOCKET_CLOSEDPORT:
		return "Port is Closed";

	case STRING_CONNECT_SUCCESS:
		return "Successfully connected to %s in %.2f ms using %s on port %d";

	case STRING_CONNECT_INFO_IP:
		return "Connecting to %s using %s on port %d";

	case STRING_CONNECT_INFO_FULL:
		return "Connecting to %s [%s] using %s on port %d";

	case STRING_CONNECTION_TIMEOUT:
		return "Connection timed out";

	default:
		return "Unknown string ID";
	}
}
