#include <iostream>
#include "WebSocketServer.h"
#include "HttpServer.h"
#include <csignal>
#include <thread>
#include <mutex>
#include "AsioIOServicePool.h"
int main()
{
    try
    {       //这里有个线程池,因为是多线程+协程并发编程嘛,这里按cpu核数分配线程跑一个子线程的io_context
        auto& pool = AsioIOServicePool::GetInstance();
        //这个是主线程的io_context
        boost::asio::io_context io_context;
        //结束信号
        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        //收到信号之后会把线程池里面的io_context给停掉,以及主线程的io_conxtext也停掉
        signals.async_wait([&io_context, &pool](auto, auto) {
            io_context.stop();
            pool.Stop();
            });
        //这里开启创建服务连接,也就是准备tcp握手
        //WebSocketServer server(io_context, 10086);
        std::make_shared<HttpServer>(io_context, 10087)->Start();
        //开始服务,也就是可以接收握手请求了
        //server.StartAccept();
        io_context.run();
    }
    catch (const std::exception& exp)
    {
        std::cout << "err " << exp.what() << std::endl;
    }
    //net::io_context ioc;
    //WebSocketServer server(ioc, 10086);
    //server.StartAccept();
    //ioc.run();
}