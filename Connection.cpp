#include "Connection.h"
#include "ConnectionMgr.h"
//���캯�����յ�ioc,��ȥ����һ��websocket�ܵ�,
//��_ws_ptrֻ������������ʹ��,���ݵĲ�����ioc,make_strand������֤�̰߳�ȫ
Connection::Connection(net::io_context& ioc) :
    _ioc(ioc), _ws_ptr(std::make_unique<websocket::stream<tcp_stream>>(make_strand(ioc))),
    _b_close(false)
{
    boost::uuids::random_generator generator;
    //ѩ���㷨����Ψһuuid
    this->_uuid = boost::uuids::to_string(generator());
}
Connection::Connection(net::io_context& ioc, boost::asio::ip::tcp::socket socket, http::request<http::dynamic_body> request) :
    _ioc(ioc), _ws_ptr(std::make_unique<websocket::stream<tcp_stream>>(std::move(socket))),
    _request(request), _b_close(false)
{
    boost::uuids::random_generator generator;
    //ѩ���㷨����Ψһuuid
    this->_uuid = boost::uuids::to_string(generator());
}

std::string Connection::GetUid()
{
    return this->_uuid;
}
//ǰ��tcp������Ҫwebsocket��ײ��tcpsocket,beast�з�װ��get_lowest_layer���ܻ�ȡ��
net::ip::tcp::socket& Connection::GetSocket()
{
    //������Ͳ��socket
    return boost::beast::get_lowest_layer(*_ws_ptr).socket();
}

void Connection::AsyncAccept()
{
    auto self = shared_from_this();
    //webserver���������,��α�հ�,��ֹ�ں�������֮��,ֱ�Ӿ�û��,���½��ղ����ص�
    _ws_ptr->async_accept([self](boost::system::error_code err) {
        try
        {
            if (!err)
            {
                //���û��������ô�Ͱ�ConnectionMgr�ӵ�Mgr����ȥ����
                //�����װ��һ��map��insert
                ConnectionMgr::GetInstance()->AddConnection(self);
                //Ȼ����ǿ�ʼ����,�ͻ��˷���������
                self->Start();
            }
            else
            {
                std::cout << err.what() << std::endl;
            }
        }
        catch (const std::exception& exp)
        {
            std::cout << "websocket err" << exp.what() << std::endl;
        }
        });
}

void Connection::HttpUpToWebsocketAsyncAccept()
{
    auto self = shared_from_this();
    _ws_ptr->async_accept(_request, [self](boost::system::error_code err) {
        try
        {
            if (!err)
            {
                //���û��������ô�Ͱ�ConnectionMgr�ӵ�Mgr����ȥ����
                //�����װ��һ��map��insert
                ConnectionMgr::GetInstance()->AddConnection(self);
                //if(self->_ws_ptr)
                //Ȼ����ǿ�ʼ����,�ͻ��˷���������
                self->Start();
            }
            else
            {
                std::cout << err.what() << std::endl;
            }
        }
        catch (const std::exception& exp)
        {
            std::cout << "websocket err" << exp.what() << std::endl;
        }
        });
}

void Connection::Start()
{
    //�����selfͬ��Ҳ��α�հ�
    auto self = shared_from_this();
    //�������û�д���buffer_bytes���ж��������ݶ೤,����err
    //Ȼ��Ҫ�����ǵ���Ϣ������

    //boost::asio::co_spawnЭ�̵�����,����Э��,��Ҫ���̵߳�io_context,
    //[=]()->boost::asio::awaitable<void> {}���lambda���ʽ����ĺ����ǿ��Եȴ���
    //����»���һ��boost::asio::detached,���������̺߳�Э�̷���
    boost::asio::co_spawn(_ioc, [=]()->boost::asio::awaitable<void> {
        try
        {
            //���û�йرվ�һֱѭ��
            for (; !_b_close;)
            {
                //�ȴ���,����˵��boost::asio::detached�����̺߳�Э�̷���,awaitable�ǿɵȴ�
                //����ʲô��˼�ؾ���,co_await���ǵȴ�һ������ִ�н���,���û�����Ϳ�����,����ͬ������˼
                //���ǿ�ס�����̵߳�Э�̶������߳�,�̻߳�����������
                //buffer_bytes�������ַ���
                            //boost::asio::use_awaitable���ǰ�ԭ���󶨵Ļص������������,����Ҫ���ص���,��ߵȵ�������
                std::size_t buffer_bytes = co_await _ws_ptr->async_read(_recv_buffer, boost::asio::use_awaitable);
                std::cout << "read from:" << GetUid() << std::endl;
                if (buffer_bytes == 0)
                {
                    std::cout << "empty data" << std::endl;
                    co_return;
                }
                //�����յ�������Ϊ�ı�
                _ws_ptr->text(_ws_ptr->got_text());
                //���յ�������
                std::string recv_data = boost::beast::buffers_to_string(_recv_buffer.data());
                //�ѻ��������������
                _recv_buffer.consume(_recv_buffer.size());
                std::cout << "websocket receive msg is" << recv_data << std::endl;

                //Ȼ���������Ȥ����3��д��,1����Э���������Э��ȥ������,2.��co_awaitͬ����,3.�����첽д�ص�ȥ��
                //boost::asio::co_spawn(_ioc, XcSend(recv_data), boost::asio::detached);
                //co_await XcSend(recv_data);
                AsyncSend(recv_data);

            }
        }
        catch (const std::exception& exp)//���ڴ�����ߵ�����
        {
            std::cout << "exception is" << exp.what() << std::endl;
            Close();
            ConnectionMgr::GetInstance()->RmvConnection(GetUid());
        }

        //����boost::asio::detached����
        }, boost::asio::detached);
}

void Connection::AsyncSend(std::string msg)
{
    {
        std::lock_guard<std::mutex> lck_guard(send_mtx);
        int que_len = this->_send_que.size();
        _send_que.push(msg);
        if (que_len > 0)return;
    }

    auto self = shared_from_this();
    //�ú����󶨵�д��
    _ws_ptr->async_write(boost::asio::buffer(msg.c_str(), msg.length()),
        std::bind(&Connection::SendHandle, self, std::placeholders::_1, std::placeholders::_2));
}

void Connection::SendHandle(error_code err, std::size_t buffer_bytes)
{
    try
    {
        if (err)
        {
            std::cout << "async_send err" << err.what() << std::endl;
            ConnectionMgr::GetInstance()->RmvConnection(GetUid());
            return;
        }

        std::string send_msg;
        {
            std::lock_guard<std::mutex> lck_gurad(send_mtx);
            _send_que.pop();
            if (_send_que.empty())
            {
                return;
            }
            send_msg = _send_que.front();
        }
        auto self = shared_from_this();
        //�ڰ󶨵ĺ��������ٵ���д,��֤����д��
        _ws_ptr->async_write(boost::asio::buffer(send_msg.c_str(), send_msg.length()),
            std::bind(&Connection::SendHandle, self, std::placeholders::_1, std::placeholders::_2));
    }
    catch (const std::exception& exp)
    {
        std::cout << "async_send err" << exp.what() << std::endl;
        ConnectionMgr::GetInstance()->RmvConnection(GetUid());
    }
}


void Connection::Close()
{
    _ws_ptr->close(websocket::close_code::normal);
    _b_close = true;
}

boost::asio::awaitable<void> Connection::XcSend(std::string msg)
{
    {
        std::lock_guard<std::mutex> lck_guard(send_mtx);
        int que_len = this->_send_que.size();
        _send_que.push(msg);
        if (que_len > 0)
        {
            std::cout << GetUid() << " " << msg << " wait quelen: " << que_len << std::endl;
            co_return;
        }
    }
    //�����Ϣ���в��ǿյ�ʱ��,��ȫ��������ȥ
    for (; !_send_que.empty();)
    {
        //֮ǰ��msg�߼�Ҫ��,�Լ����Ժð�
        std::string send_msg;
        {
            std::lock_guard<std::mutex> lck_gurad(send_mtx);
            std::cout << GetUid() << " send:" << send_msg << std::endl;
            if (_send_que.empty())
            {
                co_return;
            }
            send_msg = _send_que.front();
            _send_que.pop();
        }
        //�ȴ�һ�η��������ٷ�
        co_await _ws_ptr->async_write(boost::asio::buffer(msg.c_str(), msg.length()), boost::asio::use_awaitable);
    }
}


//void Connection::Start()
//{
//    //�����selfͬ��Ҳ��α�հ�
//    auto self = shared_from_this();
//    //�������û�д���buffer_bytes���ж��������ݶ೤,����err
//    //Ȼ��Ҫ�����ǵ���Ϣ������
//    _ws_ptr->async_read(_recv_buffer, [self](error_code err, std::size_t buffer_bytes) {
//        try
//        {
//            if (err)//�Ͽ����Ӻ���ߵ�����
//            {
//                std::cout << "websocket read err" << std::endl;
//                //���err���յ��Ļ�����Mgr�аѶ�Ӧ��Connection��ȥ����
//                ConnectionMgr::GetInstance()->RmvConnection(self->GetUid());
//                return;
//            }
//            //�����յ�������Ϊ�ı�
//            self->_ws_ptr->text(self->_ws_ptr->got_text());
//            //���յ�������
//            std::string recv_data = boost::beast::buffers_to_string(self->_recv_buffer.data());
//            //�ѻ��������������
//            self->_recv_buffer.consume(self->_recv_buffer.size());
//
//            std::cout << "websocket receive msg is" << recv_data << std::endl;
//            //���÷�������
//            self->AsyncSend(std::move(recv_data));
//            //�ٴο���������
//            self->Start();
//        }
//        catch (const std::exception& exp)
//        {
//            std::cout << "exception is" << exp.what() << std::endl;
//            ConnectionMgr::GetInstance()->RmvConnection(self->GetUid());
//        }
//        });
//}