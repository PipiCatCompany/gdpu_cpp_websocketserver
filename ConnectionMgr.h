#pragma once
#include "Connection.h"
#include "boost/unordered_map.hpp"
#include "Singleton.h"
class ConnectionMgr :public Singleton<ConnectionMgr>, std::enable_shared_from_this<ConnectionMgr>
{
public:
    //添加连接
    void AddConnection(std::shared_ptr<Connection>conptr);
    //删除连接
    void RmvConnection(std::string id);
private:
    //map记录连接
    boost::unordered_map<std::string, std::shared_ptr<Connection>> _map_cons;
};