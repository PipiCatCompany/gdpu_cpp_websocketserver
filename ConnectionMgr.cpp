#include "ConnectionMgr.h"
void ConnectionMgr::AddConnection(std::shared_ptr<Connection> conptr)
{
    this->_map_cons[conptr->GetUid()] = conptr;
}
void ConnectionMgr::RmvConnection(std::string id)
{
    this->_map_cons.erase(id);
}
