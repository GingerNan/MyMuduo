#include "InetAddress.h"
#include <arpa/inet.h>
#include <strings.h>

InetAddress::InetAddress(uint16_t port, std::string ip)
{
    bzero(&addr_, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
}

std::string InetAddress::toIp() const
{
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    return std::string(buf);
}

std::string InetAddress::toIpPort() const
{
    char buf[INET_ADDRSTRLEN + 6] = {0}; // 6 = 5 + 1
    snprintf(buf, sizeof(buf), "%s:%u", toIp().c_str(), ntohs(addr_.sin_port));
    return std::string(buf);
}

uint16_t InetAddress::toPort() const
{
    return ntohs(addr_.sin_port);
}

//#include <iostream>
//int main()
//{
//    InetAddress addr(8080);
//    std::cout << addr.toIpPort() << std::endl;
//
//    return 0;
//}