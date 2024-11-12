#pragma once
#include "ConnectionMgr.h"
class WebSocketServer
{
public:
    WebSocketServer(net::io_context& ioc, unsigned short port);
    WebSocketServer& operater(const WebSocketServer& ws) = delete;//��ֵ�������
    WebSocketServer(const WebSocketServer&) = delete;//�����������
    void StartAccept();
private:
    net::ip::tcp::acceptor _acceptor;
    net::io_context& _ioc;
};