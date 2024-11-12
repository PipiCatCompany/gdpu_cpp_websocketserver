#include "Connection.h"
#include "ConnectionMgr.h"
//构造函数接收到ioc,就去创建一个websocket管道,
//让_ws_ptr只能在这里这里使用,传递的参数有ioc,make_strand创建保证线程安全
Connection::Connection(net::io_context& ioc) :
    _ioc(ioc), _ws_ptr(std::make_unique<websocket::stream<tcp_stream>>(make_strand(ioc))),
    _b_close(false)
{
    boost::uuids::random_generator generator;
    //雪花算法配置唯一uuid
    this->_uuid = boost::uuids::to_string(generator());
}
Connection::Connection(net::io_context& ioc, boost::asio::ip::tcp::socket socket, http::request<http::dynamic_body> request) :
    _ioc(ioc), _ws_ptr(std::make_unique<websocket::stream<tcp_stream>>(std::move(socket))),
    _request(request), _b_close(false)
{
    boost::uuids::random_generator generator;
    //雪花算法配置唯一uuid
    this->_uuid = boost::uuids::to_string(generator());
}

std::string Connection::GetUid()
{
    return this->_uuid;
}
//前面tcp握手需要websocket最底层的tcpsocket,beast有封装用get_lowest_layer就能获取到
net::ip::tcp::socket& Connection::GetSocket()
{
    //返回最低层的socket
    return boost::beast::get_lowest_layer(*_ws_ptr).socket();
}

void Connection::AsyncAccept()
{
    auto self = shared_from_this();
    //webserver层面的握手,并伪闭包,防止在函数走完之后,直接就没了,导致接收不到回调
    _ws_ptr->async_accept([self](boost::system::error_code err) {
        try
        {
            if (!err)
            {
                //如果没有问题那么就把ConnectionMgr加到Mgr里面去管理
                //这里封装了一个map的insert
                ConnectionMgr::GetInstance()->AddConnection(self);
                //然后就是开始监听,客户端发送是数据
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
                //如果没有问题那么就把ConnectionMgr加到Mgr里面去管理
                //这里封装了一个map的insert
                ConnectionMgr::GetInstance()->AddConnection(self);
                //if(self->_ws_ptr)
                //然后就是开始监听,客户端发送是数据
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
    //这里的self同理也是伪闭包
    auto self = shared_from_this();
    //如果读到没有错误buffer_bytes会有读到的数据多长,否则err
    //然后要绑定我们的消息缓存区

    //boost::asio::co_spawn协程的区域,开启协程,需要子线程的io_context,
    //[=]()->boost::asio::awaitable<void> {}这个lambda表达式里面的函数是可以等待的
    //最底下还有一个boost::asio::detached,就是设置线程和协程分离
    boost::asio::co_spawn(_ioc, [=]()->boost::asio::awaitable<void> {
        try
        {
            //如果没有关闭就一直循环
            for (; !_b_close;)
            {
                //等待读,上面说了boost::asio::detached会让线程和协程分离,awaitable是可等待
                //具体什么意思呢就是,co_await就是等待一个函数执行结束,如果没结束就卡在这,就是同步的意思
                //但是卡住的是线程的协程而不是线程,线程还是正常工作
                //buffer_bytes读到是字符数
                            //boost::asio::use_awaitable就是把原来绑定的回调函数给替代掉,不需要他回调了,这边等到他结束
                std::size_t buffer_bytes = co_await _ws_ptr->async_read(_recv_buffer, boost::asio::use_awaitable);
                std::cout << "read from:" << GetUid() << std::endl;
                if (buffer_bytes == 0)
                {
                    std::cout << "empty data" << std::endl;
                    co_return;
                }
                //设置收到的数据为文本
                _ws_ptr->text(_ws_ptr->got_text());
                //接收到的数据
                std::string recv_data = boost::beast::buffers_to_string(_recv_buffer.data());
                //把缓冲区数据清除掉
                _recv_buffer.consume(_recv_buffer.size());
                std::cout << "websocket receive msg is" << recv_data << std::endl;

                //然后这里就有趣了有3种写法,1是在协程里面调用协程去发数据,2.是co_await同步发,3.是用异步写回调去发
                //boost::asio::co_spawn(_ioc, XcSend(recv_data), boost::asio::detached);
                //co_await XcSend(recv_data);
                AsyncSend(recv_data);

            }
        }
        catch (const std::exception& exp)//现在错误会走到这里
        {
            std::cout << "exception is" << exp.what() << std::endl;
            Close();
            ConnectionMgr::GetInstance()->RmvConnection(GetUid());
        }

        //看到boost::asio::detached了吗
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
    //用函数绑定的写法
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
        //在绑定的函数里面再调用写,保证队列写完
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
    //如果消息队列不是空的时候,把全部都发出去
    for (; !_send_que.empty();)
    {
        //之前的msg逻辑要改,自己动脑好吧
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
        //等待一次发结束后再发
        co_await _ws_ptr->async_write(boost::asio::buffer(msg.c_str(), msg.length()), boost::asio::use_awaitable);
    }
}


//void Connection::Start()
//{
//    //这里的self同理也是伪闭包
//    auto self = shared_from_this();
//    //如果读到没有错误buffer_bytes会有读到的数据多长,否则err
//    //然后要绑定我们的消息缓存区
//    _ws_ptr->async_read(_recv_buffer, [self](error_code err, std::size_t buffer_bytes) {
//        try
//        {
//            if (err)//断开连接后会走到这里
//            {
//                std::cout << "websocket read err" << std::endl;
//                //如果err被收到的话就在Mgr中把对应的Connection他去除掉
//                ConnectionMgr::GetInstance()->RmvConnection(self->GetUid());
//                return;
//            }
//            //设置收到的数据为文本
//            self->_ws_ptr->text(self->_ws_ptr->got_text());
//            //接收到的数据
//            std::string recv_data = boost::beast::buffers_to_string(self->_recv_buffer.data());
//            //把缓冲区数据清除掉
//            self->_recv_buffer.consume(self->_recv_buffer.size());
//
//            std::cout << "websocket receive msg is" << recv_data << std::endl;
//            //调用发送数据
//            self->AsyncSend(std::move(recv_data));
//            //再次开启读操作
//            self->Start();
//        }
//        catch (const std::exception& exp)
//        {
//            std::cout << "exception is" << exp.what() << std::endl;
//            ConnectionMgr::GetInstance()->RmvConnection(self->GetUid());
//        }
//        });
//}