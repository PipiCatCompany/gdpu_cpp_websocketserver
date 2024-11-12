#include "AsioIOServicePool.h"
#include <iostream>

using namespace std;
//轮询获取一个io_context
boost::asio::io_context& AsioIOServicePool::GetIOService()
{
    auto& servic = _ioServices[_nextIOService++];
    if (_nextIOService == _ioServices.size())
    {
        _nextIOService = 0;
    }
    return servic;
}
//停止同时把线程给停掉
void AsioIOServicePool::Stop()
{
    for (auto& work : _works)
    {
        work.reset();
    }
    for (auto& t : _threads)
    {
        t.join();
    }
}
//拿到池子的单例
AsioIOServicePool& AsioIOServicePool::GetInstance()
{
    static AsioIOServicePool instance(std::thread::hardware_concurrency());
    return instance;
}
//初始化池子
AsioIOServicePool::AsioIOServicePool(std::size_t size) :_ioServices(size),
_works(size), _nextIOService(0)
{
    for (std::size_t i = 0; i < size; i++)
    {
        //创建工作,需要给一个右值,也就是代码运行时才知道的东西
        _works[i] = std::unique_ptr<Work>(new Work(_ioServices[i]));
    }

    for (std::size_t i = 0; i < _ioServices.size(); i++)
    {
        //这里不用push是为了方便和提高性能
        //push是拷贝构造,而emplace_back是直接构造
        //如果用push的话应该先创建一个临时的线程然后move进去,麻烦而且多了个移动操作
        _threads.emplace_back([this, i]() {
            _ioServices[i].run();
            });
    }
}