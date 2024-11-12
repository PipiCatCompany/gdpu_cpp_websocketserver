#include "WebSocketServer.h"
//_acceptor�Ĺ�������ioc����ǰ�����ѯ����������,�Լ���idЭ��Ͷ˿ں�
WebSocketServer::WebSocketServer(net::io_context& ioc, unsigned short port) :
    _ioc(ioc), _acceptor(ioc, net::ip::tcp::endpoint(net::ip::tcp::v4(), port))
{
    std::cout << "Server start on port:" << port << std::endl;
}
//��ʼ��������
void WebSocketServer::StartAccept()
{
    //���������������ὲ
    //������ʱ��ᴴ��һ��ȫ˫���Ĺܵ�websocket
    auto con_ptr = std::make_shared<Connection>(_ioc);
    //tcp����,����������֮��,�ж���û�д���,û�о�������ȥwebsocket����,���µ���StartAccept()������һ������
    //Ȼ��������Ҫ�õ�websocket��ײ��tcpsocketȥ������,GetSocket�Ǻ����װ�Ļ�ȡwebsocket�ײ�tcpsocket�ķ���
    _acceptor.async_accept(con_ptr->GetSocket(), [this, con_ptr](error_code err) {
        try
        {
            if (!err)
            {
                //websocket����
                con_ptr->AsyncAccept();
            }
            else
            {
                std::cout << "acceptor async_acceptor err" << err.what() << std::endl;
            }
            StartAccept();
        }
        catch (const std::exception& exp)
        {
            std::cout << "async_accept error is" << exp.what() << std::endl;
        }
        });
}