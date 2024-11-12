#pragma once
#include "Connection.h"
#include "boost/unordered_map.hpp"
#include "Singleton.h"
class ConnectionMgr :public Singleton<ConnectionMgr>, std::enable_shared_from_this<ConnectionMgr>
{
public:
    //�������
    void AddConnection(std::shared_ptr<Connection>conptr);
    //ɾ������
    void RmvConnection(std::string id);
private:
    //map��¼����
    boost::unordered_map<std::string, std::shared_ptr<Connection>> _map_cons;
};