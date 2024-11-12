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
    //��ȡһ�����̵߳�io_context
    boost::asio::io_context& GetIOService();
    //�̳߳�ֹͣ����
    void Stop();
    //��ȡ����,��Ϊ���ε�ԭ��,û����֮ǰ�ĵ���ģ��,���ǰ�c++11֮��staticΪ�̰߳�ȫ���������д����
    static AsioIOServicePool& GetInstance();
private:
    //sizeĬ��Ϊϵͳcpu����
    AsioIOServicePool(std::size_t size = std::thread::hardware_concurrency());
    //һ��vector������¼���̵߳�io_context
    std::vector<IOService> _ioServices;
    //��¼io_context��work,��ô˵���㴴����io_context����Ҫ���¸ɵ�
    //���������û�յ���Ϣ����GetIOService��ʱ��,io_context��崻�״̬,�ᱻ���ٵ�,����Ҫ����һ��Ĭ�ϵĹ���
    std::vector<WorkPtr> _works;
    //��¼�߳�
    std::vector<std::thread> _threads;
    //��һ��io_context��_ioServices�е�λ��
    std::size_t _nextIOService;
};
