#pragma once
#include <boost/asio.hpp>
#include <vector>
#include <iostream>
class AsioIOServicePool
{
public:
    using IOService = boost::asio::io_context;
    using Work = boost::asio::io_context::work;
    using WorkPtr = std::unique_ptr<Work>;
    AsioIOServicePool& operator = (const AsioIOServicePool&) = delete;
    AsioIOServicePool(const AsioIOServicePool&) = delete;
    ~AsioIOServicePool() { std::cout << "asioservicepool delete" << std::endl; };
    //获取一个子线程的io_context
    boost::asio::io_context& GetIOService();
    //线程池停止工作
    void Stop();
    //获取单例,因为含参的原因,没有用之前的单例模板,而是按c++11之后static为线程安全的情况来书写单例
    static AsioIOServicePool& GetInstance();
private:
    //size默认为系统cpu核数
    AsioIOServicePool(std::size_t size = std::thread::hardware_concurrency());
    //一个vector用来记录子线程的io_context
    std::vector<IOService> _ioServices;
    //记录io_context的work,怎么说呢你创建的io_context是需要有事干的
    //但是如果在没收到消息调用GetIOService的时候,io_context是宕机状态,会被销毁掉,所以要给它一个默认的工作
    std::vector<WorkPtr> _works;
    //记录线程
    std::vector<std::thread> _threads;
    //下一个io_context在_ioServices中的位置
    std::size_t _nextIOService;
};
