#pragma once
#include "ConnectionMgr.h"
class WebSocketServer
{
public:
    WebSocketServer(net::io_context& ioc, unsigned short port);
    WebSocketServer& operater(const WebSocketServer& ws) = delete;//赋值构造禁用
    WebSocketServer(const WebSocketServer&) = delete;//拷贝构造禁用
    void StartAccept();
private:
    net::ip::tcp::acceptor _acceptor;
    net::io_context& _ioc;
};