#include "host.h"
#include "i18n.h"
#include "socket.h"
#include <iostream>
#include <cstdio>	  // For snprintf, sprintf
#include <winsock2.h> // For inet_ntoa
#include <new>

const char *host_c::IPAddressString()
{
	return inet_ntoa(*(struct in_addr *)&ipAddress); // Cast ipAddress to in_addr
}

int host_c::GetSuccessfulConnectionString(char *str, double time)
{
	const char *format = i18n_c::GetString(STRING_CONNECT_SUCCESS).c_str();
	int length = snprintf(NULL, 0, format, this->IPAddressString(), time, socket_c::GetFriendlyTypeName(this->Type).c_str(), this->Port);

	if (str != NULL)
	{
		sprintf(str, format, this->IPAddressString(), time, socket_c::GetFriendlyTypeName(this->Type).c_str(), this->Port);
	}

	return length;
}
int host_c::GetConnectInfoString(char *str)
{
	const char *format;
	int length;

	if (this->IPAddress.empty())
	{
		format = i18n_c::GetString(STRING_CONNECT_INFO_IP).c_str();
		length = snprintf(NULL, 0, format, this->Hostname.c_str(), socket_c::GetFriendlyTypeName(this->Type).c_str(), this->Port);
		if (str != NULL)
		{
			sprintf(str, format, this->Hostname.c_str(), socket_c::GetFriendlyTypeName(this->Type).c_str(), this->Port);
		}
	}
	else
	{
		format = i18n_c::GetString(STRING_CONNECT_INFO_FULL).c_str();
		length = snprintf(NULL, 0, format, this->Hostname.c_str(), this->IPAddressString(), socket_c::GetFriendlyTypeName(this->Type).c_str(), this->Port);
		if (str != NULL)
		{
			sprintf(str, format, this->Hostname.c_str(), this->IPAddressString(), socket_c::GetFriendlyTypeName(this->Type).c_str(), this->Port);
		}
	}

	return length;
}
