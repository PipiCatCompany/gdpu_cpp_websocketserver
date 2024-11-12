#include "AsioIOServicePool.h"
#include <iostream>

using namespace std;
//��ѯ��ȡһ��io_context
boost::asio::io_context& AsioIOServicePool::GetIOService()
{
    auto& servic = _ioServices[_nextIOService++];
    if (_nextIOService == _ioServices.size())
    {
        _nextIOService = 0;
    }
    return servic;
}
//ֹͣͬʱ���̸߳�ͣ��
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
//�õ����ӵĵ���
AsioIOServicePool& AsioIOServicePool::GetInstance()
{
    static AsioIOServicePool instance(std::thread::hardware_concurrency());
    return instance;
}
//��ʼ������
AsioIOServicePool::AsioIOServicePool(std::size_t size) :_ioServices(size),
_works(size), _nextIOService(0)
{
    for (std::size_t i = 0; i < size; i++)
    {
        //��������,��Ҫ��һ����ֵ,Ҳ���Ǵ�������ʱ��֪���Ķ���
        _works[i] = std::unique_ptr<Work>(new Work(_ioServices[i]));
    }

    for (std::size_t i = 0; i < _ioServices.size(); i++)
    {
        //���ﲻ��push��Ϊ�˷�����������
        //push�ǿ�������,��emplace_back��ֱ�ӹ���
        //�����push�Ļ�Ӧ���ȴ���һ����ʱ���߳�Ȼ��move��ȥ,�鷳���Ҷ��˸��ƶ�����
        _threads.emplace_back([this, i]() {
            _ioServices[i].run();
            });
    }
}