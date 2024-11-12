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
    //看到awaitable了吗,这个就是说这个函数是可以等待的,类似nodejs的async
    boost::asio::awaitable<void> XcSend(std::string msg);

private:
    //websocket,socket
    std::unique_ptr<stream<tcp_stream>>_ws_ptr;
    //http升级到websocket使用的http::dynamic_body
    http::request<http::dynamic_body> _request;
    //雪花uuid管理
    std::string _uuid;
    //上下文通信管理
    net::io_context& _ioc;
    //消息缓存区,会自动去扩容
    flat_buffer _recv_buffer;
    //发送队列
    std::queue<std::string>_send_que;
    //原子锁
    std::mutex send_mtx;
    //是否停止
    bool _b_close;
};