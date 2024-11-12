#pragma once
#include <iostream>
#include <mutex>
#include <memory>
template <typename T>

class Singleton
{
protected:
	Singleton() = default;
	Singleton(const Singleton<T>&) = delete;
	Singleton& operator=(const Singleton<T>& st) = delete;
	static std::shared_ptr<T> _instance;//声明static类的存在
public:
	static std::shared_ptr<T> GetInstance() {
		static std::once_flag s_flag;
		std::call_once(s_flag, [&]() {
			_instance = std::shared_ptr<T>(new T);//因为调用的T的构造函数的私有的所以继承单例类的时候要把友元写上
			});
		return _instance;
	}

	void PrintAddress()
	{
		std::cout << _instance.get() << std::endl;
	}

	~Singleton()
	{
		std::cout << "this is singleton delete" << std::endl;
	}
};

template <typename T>
std::shared_ptr<T> Singleton<T>::_instance = nullptr;
//保证模板类的static变量类型一致,同时保证static只有一个,所以在,h中初始化,
