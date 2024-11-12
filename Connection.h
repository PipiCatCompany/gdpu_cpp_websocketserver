#pragma once
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <memory.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <queue>
#include <mutex>
#include <boost/uuid/random_generator.hpp>
#include <iostream>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

namespace net = boost::asio;
namespace beast = boost::beast;
using namespace boost::beast;
using namespace boost::beast::websocket;

class Connection :public std::enable_shared_from_this<Connection>
{
public:
    Connection(net::io_context& ioc);
    Connection(net::io_context& ioc, boost::asio::ip::tcp::socket socket,
        http::request<http::dynamic_body> request);
    std::string GetUid();
    net::ip::tcp::socket& GetSocket();
    void AsyncAccept();
    void Start();
    void Close();
    void AsyncSend(std::string msg);
    void SendHandle(error_code err, std::size_t buffer_bytes);
    void HttpUpToWebsocketAsyncAccept();
    //����awaitable����,�������˵��������ǿ��Եȴ���,����nodejs��async
    boost::asio::awaitable<void> XcSend(std::string msg);

private:
    //websocket,socket
    std::unique_ptr<stream<tcp_stream>>_ws_ptr;
    //http������websocketʹ�õ�http::dynamic_body
    http::request<http::dynamic_body> _request;
    //ѩ��uuid����
    std::string _uuid;
    //������ͨ�Ź���
    net::io_context& _ioc;
    //��Ϣ������,���Զ�ȥ����
    flat_buffer _recv_buffer;
    //���Ͷ���
    std::queue<std::string>_send_que;
    //ԭ����
    std::mutex send_mtx;
    //�Ƿ�ֹͣ
    bool _b_close;
};