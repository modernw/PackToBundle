#pragma once
#include <vector>
#include <algorithm>
#include <functional>

template <class T> bool compare_default (const T &l, const T &r) { return l == r; }
template <class T> bool find_vec (std::vector <T> &vec, const T &value, const std::function <void (size_t)> &callback, std::function<bool (const T &, const T &)> compare = compare_default <T>, bool sorted = false)
{
	const size_t n = vec.size ();
	if (!compare) compare = compare_default<T>;
	if (sorted)
	{
		size_t left = 0, right = n;
		while (left < right)
		{
			size_t mid = left + (right - left) / 2;
			if (compare (vec [mid], value)) 
			{
				callback (mid);
				return true;
			}
			if (vec [mid] < value) left = mid + 1;
			else right = mid;
		}
		return false;
	}
	if (n < 64)
	{
		for (size_t i = 0; i < n; i++)
		{
			if (compare (vec [i], value)) 
			{
				callback (i);
				return true;
			}
		}
		return false;
	}
	const size_t blockSize = 8;
	size_t i = 0;
	for (; i + blockSize <= n; i += blockSize)
	{
		for (size_t j = 0; j < blockSize; j ++)
		{
			if (compare (vec [i + j], value))
			{
				callback (i + j);
				return true;
			}
		}
	}
	for (; i < n; i ++) 
	{
		if (compare (vec [i], value)) 
		{
			callback (i);
			return true;
		}
	}
	return false;
}
template <class T> void push_unique (std::vector <T> &vec, const T &value, std::function <bool (const T &, const T &)> compare = compare_default <T>)
{
	bool found = find_vec <T> (
		vec, value,
		[] (size_t) {},
		compare);
	if (!found) vec.push_back (value);
}
template <class T> void push_normal (std::vector <T> &vec, const T &value)
{
	vec.push_back (value);
}
template <class T> void push_normal (std::vector <T> &target, const std::vector <T> &another)
{
	target.insert (target.end (), another.begin (), another.end ());
}