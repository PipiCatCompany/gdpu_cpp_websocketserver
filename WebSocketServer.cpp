#include "WebSocketServer.h"
//_acceptor的构建传入ioc就是前面的轮询管理上下文,以及绑定id协议和端口号
WebSocketServer::WebSocketServer(net::io_context& ioc, unsigned short port) :
    _ioc(ioc), _acceptor(ioc, net::ip::tcp::endpoint(net::ip::tcp::v4(), port))
{
    std::cout << "Server start on port:" << port << std::endl;
}
//开始监听握手
void WebSocketServer::StartAccept()
{
    //创建的连接类后面会讲
    //创建的时候会创建一个全双工的管道websocket
    auto con_ptr = std::make_shared<Connection>(_ioc);
    //tcp握手,监听到握手之后,判断有没有错误,没有就升级到去websocket握手,重新调用StartAccept()监听下一个连接
    //然后我们是要拿到websocket最底层的tcpsocket去做握手,GetSocket是后面封装的获取websocket底层tcpsocket的方法
    _acceptor.async_accept(con_ptr->GetSocket(), [this, con_ptr](error_code err) {
        try
        {
            if (!err)
            {
                //websocket握手
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