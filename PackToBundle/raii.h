#pragma once
#include <functional>
struct raii
{
	std::function <void ()> endtask = nullptr;
	raii (std::function <void ()> pFunc = nullptr): endtask (pFunc) {}
	~raii () { if (endtask) endtask (); }
};