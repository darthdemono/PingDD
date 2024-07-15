#pragma once
#ifndef SOCKET_H
#define SOCKET_H

#include "standard.h"
#include "host.h"
#include <string>

class socket_c
{
public:
	static std::string GetFriendlyTypeName(int type);
	static int Connect(host_c host, int timeout, double &rtt);
	static int Resolve(const std::string &destination, host_c &host);
	static void SetPortAndType(int port, int type, host_c &host);
	static int GetSocketType(int type);
};

#endif // SOCKET_H
