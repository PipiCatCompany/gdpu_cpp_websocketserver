#include "HttpConnection.h"
#include "Connection.h"
HttpConnection::HttpConnection(boost::asio::io_context& ioc) :
	_socket(ioc),_ioc(ioc)
{

}

void HttpConnection::Start()
{
	auto self = shared_from_this();
	http::async_read(_socket, _buffer, _request, [self](boost::beast::error_code ec, std::size_t bytes_transferred) {
		try {
			if (ec)
			{
				std::cout << "http read err is" << ec.what() << std::endl;
				return;
			}
			boost::ignore_unused(bytes_transferred);
			self->HandleReq();
			self->CheckDeadline();
		}
		catch (std::exception& exp) {
			std::cout << "exception is " << exp.what() << std::endl;
		}
		});
}

unsigned char ToHex(unsigned char x)
{
	return x > 9 ? x + 55 : x + 48;
}

unsigned char FromHex(unsigned char x)
{
	unsigned char y;
	if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
	else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
	else if (x >= '0' && x <= '9') y = x - '0';
	else assert(0);
	return y;
}
std::string UrlEncode(const std::string& str)
{
	std::string strTemp = "";
	size_t length = str.length();
	for (size_t i = 0; i < length; i++)
	{
		//判断是否仅有数字和字母构成
		if (isalnum((unsigned char)str[i]) ||
			(str[i] == '-') ||
			(str[i] == '_') ||
			(str[i] == '.') ||
			(str[i] == '~'))
			strTemp += str[i];
		else if (str[i] == ' ') //为空字符
			strTemp += "+";
		else
		{
			//其他字符需要提前加%并且高四位和低四位分别转为16进制
			strTemp += '%';
			strTemp += ToHex((unsigned char)str[i] >> 4);
			strTemp += ToHex((unsigned char)str[i] & 0x0F);
		}
	}
	return strTemp;
}

std::string UrlDecode(const std::string& str)
{
	std::string strTemp = "";
	size_t length = str.length();
	for (size_t i = 0; i < length; i++)
	{
		//还原+为空
		if (str[i] == '+') strTemp += ' ';
		//遇到%将后面的两个字符从16进制转为char再拼接
		else if (str[i] == '%')
		{
			assert(i + 2 < length);
			unsigned char high = FromHex((unsigned char)str[++i]);
			unsigned char low = FromHex((unsigned char)str[++i]);
			strTemp += high * 16 + low;
		}
		else strTemp += str[i];
	}
	return strTemp;
}

void HttpConnection::PreParseGetParam() {
	// 提取 URI  
	auto uri = _request.target();
	// 查找查询字符串的开始位置（即 '?' 的位置）  
	auto query_pos = uri.find('?');
	if (query_pos == std::string::npos) {
		_get_url = uri;
		return;
	}
	_get_url = uri.substr(0, query_pos);
	std::string query_string = uri.substr(query_pos + 1);
	std::string key;
	std::string value;
	size_t pos = 0;
	while ((pos = query_string.find('&')) != std::string::npos) {
		auto pair = query_string.substr(0, pos);
		size_t eq_pos = pair.find('=');
		if (eq_pos != std::string::npos) {
			key = UrlDecode(pair.substr(0, eq_pos)); // 假设有 url_decode 函数来处理URL解码  
			value = UrlDecode(pair.substr(eq_pos + 1));
			_get_params[key] = value;
		}
		query_string.erase(0, pos + 1);
	}
	// 处理最后一个参数对（如果没有 & 分隔符）  
	if (!query_string.empty()) {
		size_t eq_pos = query_string.find('=');
		if (eq_pos != std::string::npos) {
			key = UrlDecode(query_string.substr(0, eq_pos));
			value = UrlDecode(query_string.substr(eq_pos + 1));
			_get_params[key] = value;
		}
	}
}

void HttpConnection::CheckDeadline()
{
	auto self = shared_from_this();
	deadline_.async_wait([self](beast::error_code ec) {
		if (!ec)
		{
			self->_socket.close(ec);
		}
		});
}

void HttpConnection::WriteResponse()
{
	auto self = shared_from_this();
	_response.content_length(_response.body().size());
	http::async_write(_socket, _response, [self](boost::beast::error_code ec, std::size_t bytes_transferred) {
		self->_socket.shutdown(tcp::socket::shutdown_send, ec);
		self->deadline_.cancel();
		});
}

void HttpConnection::HandleReq()
{
	_response.version(_request.version());
	_response.keep_alive(false);
	PreParseGetParam();
	if (_request.method() == boost::beast::http::verb::get)
	{
		//另一边需要一个智能指针做管理,所以shared_from_this
		//bool success = LogicSystem::GetInstance()->HandleGet(_get_url, shared_from_this());
			//看到这个if没有就是在这里判断url后缀匹配,以及是否升级
		if (_get_url == "/web" && boost::beast::websocket::is_upgrade(_request))
		{
			_response.keep_alive(true);//把连接是否保留置为true
			//这是webssocket那个管理类的构造
			auto con_ptr = std::make_shared<Connection>(this->_ioc, std::move(this->_socket), this->_request);
			//这是http升级到websocket握手
			con_ptr->HttpUpToWebsocketAsyncAccept();
			return;
		}
		bool success = true;
		if (!success)
		{
			_response.result(boost::beast::http::status::not_found);
			_response.set(boost::beast::http::field::content_type, "text/plain");
			boost::beast::ostream(_response.body()) << "url not found\r\n";
			WriteResponse();
			return;
		}
		_response.result(boost::beast::http::status::ok);
		_response.set(boost::beast::http::field::server, "GateServer");
		WriteResponse();
		return;
	}

	if (_request.method() == boost::beast::http::verb::post)
	{
		//bool success = LogicSystem::GetInstance()->HandlePost(_request.target(), shared_from_this());
		bool success = false;
		if (_get_url == "/message")
		{
			auto body_str = boost::beast::buffers_to_string(this->_request.body().data());
			std::cout << "receive body is" << body_str << std::endl;
			this->_response.set(http::field::content_type, "text/json");
			beast::ostream(this->_response.body()) << "success";
			success = true;
		}

		if (!success)
		{
			_response.result(boost::beast::http::status::not_found);
			_response.set(boost::beast::http::field::content_type, "text/plain");
			boost::beast::ostream(_response.body()) << "url not found\r\n";
			WriteResponse();
			return;
		}
		_response.result(boost::beast::http::status::ok);
		_response.set(boost::beast::http::field::server, "GateServer");
		WriteResponse();
		return;
	}
}