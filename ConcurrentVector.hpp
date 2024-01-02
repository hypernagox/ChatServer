#pragma once
#include <memory>
#include <atomic>

template <typename T>
class ConcurrentVector
{
public:
	ConcurrentVector(const unsigned int n = 1)
		:data_arr{ std::make_unique_for_overwrite<T[]>(n) }
		, capacity{n}
		, data_ptr{data_arr.get()}
	{
	}

	template<typename... Args>
	T& emplace_back(Args&&... args)noexcept
	{
		const unsigned int old_size = internal_size.fetch_add(1);
		T* const temp = new (data_ptr + old_size) T{std::forward<Args>(args)...};
		while (external_size.load() != old_size) {
		}
		external_size.store(old_size + 1);
		return *temp;
	}

	T& operator[](const unsigned int idx)noexcept { return data_ptr[idx]; }
	const T& operator[](const unsigned int idx)const noexcept { return data_ptr[idx]; }

	const T* begin()const noexcept { return data_ptr; }
	const T* end()const noexcept { return data_ptr + external_size.load(std::memory_order_relaxed); }

	T* begin() noexcept { return data_ptr; }
	T* end() noexcept { return data_ptr + external_size.load(std::memory_order_relaxed); }

	const T* data()const noexcept { return data_ptr; }
	T* data() noexcept { return data_ptr; }

	unsigned int size()const noexcept { return external_size.load(std::memory_order_relaxed); }

	void reserve(const unsigned int n)noexcept 
	{
		capacity.store(n);
		std::unique_ptr<T[]> temp = std::make_unique_for_overwrite<T[]>(n);
		const unsigned int num_of_ele = external_size.load();
		std::copy(data_arr.get(), data_arr.get() + num_of_ele, temp.get());
		data_arr.swap(temp);
		data_ptr = data_arr.get();
	}

private:
	std::unique_ptr<T[]> data_arr;
	std::atomic_uint internal_size = 0;
	std::atomic_uint external_size = 0;
	std::atomic_uint capacity = 0;
	T* data_ptr = nullptr;
};