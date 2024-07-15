#pragma once
#ifndef HOST_H
#define HOST_H

#include <string>
#include "standard.h"

class host_c
{
public:
	std::string Hostname;
	std::string IPAddress;
	bool HostIsIP;
	int Port;
	int Type;

	const char *IPAddressString();
	int GetConnectInfoString(char *str);
	int GetSuccessfulConnectionString(char *str, double time);

	static pcc_t GetIPAddressAsString(in_addr ipAddress);
	static pcc_t GetFriendlyTypeName(int type);

private:
	unsigned long ipAddress; // Assuming ipAddress is an unsigned long type
};

#endif // HOST_H
