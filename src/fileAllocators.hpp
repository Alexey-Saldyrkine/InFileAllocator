#pragma once
#include "buddyAllocator.hpp"
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>

namespace inFileAllocatorNS::detail {

template<template<size_t> typename policy>
struct anonymousFileAllocatorManager {

	size_t mappedSize = 0;
	byteT *basePtr;
	void *objManagerPtr;

	byteT* mapPages(size_t size) {
		int pageCount = size / pageSize + (size % pageSize == 0 ? 0 : 1);
		size_t psize = pageCount * pageSize;
		byteT *adrs = basePtr + mappedSize;
		mappedSize += psize;
		byteT *ret = reinterpret_cast<byteT*>(mmap(adrs, psize + pageSize,
		PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0));
		if (ret == reinterpret_cast<byteT*>(-1)) {
			perror("mmap");
			exit(101);
		}
		return ret;
	}

	void unmapPages() {
		munmap(basePtr, mappedSize);
	}

	byteT* allocNewBlock(size_t size) {
		return mapPages(size);
	}

	using allocT = buddyAllocator<&anonymousFileAllocatorManager::allocNewBlock,policy>;
	byteT mem[sizeof(allocT)];
	allocT &alloc = *reinterpret_cast<allocT*>(mem);

	byteT* allocate(size_t size) {
		return alloc.allocate(size, *this);
	}

	void deallocate(byteT *adrs, size_t size) {
		alloc.deallocate(adrs, size, *this);
	}

	anonymousFileAllocatorManager(byteT *ptr) {
		new (&alloc) allocT { };
		basePtr = ptr;
		objManagerPtr = nullptr;
	}
	~anonymousFileAllocatorManager() = delete;
};

template<template<size_t> typename policy>
anonymousFileAllocatorManager<policy>* createAnonymousFileAllocatorManager(
		byteT *ptr) {
	void *ret = mmap(ptr, pageSize, PROT_READ | PROT_WRITE, MAP_SHARED |
	MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	if (ret == reinterpret_cast<void*>(-1)) {
		perror("mmap");
		exit(102);
	}
	new (ptr) anonymousFileAllocatorManager<policy>(ptr + pageSize);
	return reinterpret_cast<anonymousFileAllocatorManager<policy>*>(ptr);
}

template<template<size_t> typename policy>
void destroyAnonymousFileAllocatorManager(byteT *ptr) {
	reinterpret_cast<anonymousFileAllocatorManager<policy>*>(ptr)->unmapPages();
	munmap(ptr, pageSize);
}

template<typename T, template<size_t> typename policy>
struct AnonymousFileAllocator: public std::pointer_traits<T*> {
	anonymousFileAllocatorManager<policy> *allocPtr;

	using value_type = T;
	using size_type = size_t;
	using pointer = T*;
	using const_pointer = const T*;
	using difference_type = typename std::pointer_traits<pointer>::difference_type;
	template<typename U>
	struct rebind {
		using other = AnonymousFileAllocator<U,policy>;
	};
	using propagate_on_container_copy_assignment = std::true_type;
	using propagate_on_container_move_assignment = std::true_type;
	using propagate_on_container_swap = std::true_type;
	using is_always_equal = std::false_type;

	AnonymousFileAllocator(anonymousFileAllocatorManager<policy> *ptr) :
			allocPtr(ptr) {
	}

	template<typename U>
	constexpr AnonymousFileAllocator(
			const AnonymousFileAllocator<U, policy> &other) noexcept :
			allocPtr(other.allocPtr) {
	}

	T* allocate(size_t n) {
		return reinterpret_cast<T*>(allocPtr->allocate(n * sizeof(T)));
	}

	void deallocate(T *ptr, size_t n) noexcept {
		allocPtr->deallocate(reinterpret_cast<byteT*>(ptr), n * sizeof(T));
	}

	template<typename U, typename ... Args>
	void construct(U *ptr, Args &&... args) {
		new (ptr) U(std::forward<Args>(args)...);
	}

	template<typename U>
	void destroy(U *p) noexcept {
		p->~U();
	}

};

template<typename T, typename U, template<size_t> typename policy>
bool operator==(const AnonymousFileAllocator<T, policy> &a,
		const AnonymousFileAllocator<U, policy> &b) {
	return a.allocPtr == b.allocPtr;
}

template<typename T, typename U, template<size_t> typename policy>
bool operator!=(const AnonymousFileAllocator<T, policy> &a,
		const AnonymousFileAllocator<U, policy> &b) {
	return !(a == b);
}

constexpr size_t inFileAllocatorSpecialNumber = 7452372573;

template<template<size_t> typename policy>
struct inFileAllocatorManager {

	size_t specialNumber = inFileAllocatorSpecialNumber;
	size_t mappedSize = 0;
	byteT *basePtr;
	void *objManagerPtr;
	int fd;

	bool isConstructed() {
		return specialNumber == inFileAllocatorSpecialNumber;
	}

	void insureFileSize() {
		struct stat st;
		fstat(fd, &st);
		size_t fileSize = st.st_size;
		if (fileSize < mappedSize + 2 * pageSize) {
			ftruncate(fd, mappedSize + 2 * pageSize);
		}
	}

	byteT* mapPages(size_t size) {
		int pageCount = size / pageSize + (size % pageSize == 0 ? 0 : 1);
		size_t psize = pageCount * pageSize;
		byteT *adrs = basePtr + mappedSize;
		size_t preMappedSize = mappedSize;
		mappedSize += psize;
		insureFileSize();
		byteT *ret = reinterpret_cast<byteT*>(mmap(adrs, psize + pageSize,
		PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd,
				pageSize + preMappedSize));
		if (ret == reinterpret_cast<byteT*>(-1)) {
			perror("mmap");
			exit(103);
		}
		return ret;
	}

	void unmapPages() {
		munmap(basePtr, mappedSize);
	}

	byteT* allocNewBlock(size_t size) {
		return mapPages(size);
	}

	using allocT = buddyAllocator<&inFileAllocatorManager::allocNewBlock,policy>;
	byteT mem[sizeof(allocT)];
	allocT &alloc = *reinterpret_cast<allocT*>(mem);

	void clearFile() {
		ftruncate(fd, 2 * pageSize);
		mappedSize = 0;
		objManagerPtr = nullptr;
		alloc.clear();
	}

	byteT* allocate(size_t size) {
		return alloc.allocate(size, *this);
	}

	void deallocate(byteT *adrs, size_t size) {
		alloc.deallocate(adrs, size, *this);
	}

	inFileAllocatorManager(int _fd, byteT *ptr) :
			basePtr(ptr), objManagerPtr(nullptr), fd(_fd) {
		new (&alloc) allocT { };
	}
	~inFileAllocatorManager() = delete;
};

template<template<size_t> typename policy>
inFileAllocatorManager<policy>* createInFileAllocatorManager(int fd,
		byteT *ptr) {
	struct stat st;
	fstat(fd, &st);
	size_t fileSize = st.st_size;
	if (fileSize < pageSize) {
		ftruncate(fd, pageSize);
	}

	void *ret = mmap(ptr, pageSize, PROT_READ | PROT_WRITE,
	MAP_SHARED | MAP_FIXED, fd, 0);
	if (ret == reinterpret_cast<byteT*>(-1)) {
		perror("mmap");
		exit(104);
	}
	inFileAllocatorManager<policy> *manager =
			reinterpret_cast<inFileAllocatorManager<policy>*>(ptr);
	if (!manager->isConstructed()) {
		new (ptr) inFileAllocatorManager<policy>(fd, ptr + pageSize);
	} else {
		manager->fd = fd;
		if (manager->mappedSize != 0) {
			ret = mmap(ptr + pageSize, manager->mappedSize,
					PROT_READ | PROT_WRITE,
					MAP_SHARED | MAP_FIXED, fd, pageSize);
			if (ret == reinterpret_cast<byteT*>(-1)) {
				perror("mmap");
				exit(105);
			}
		}
	}
	return manager;
}

template<template<size_t> typename policy>
void destroyInFileAllocatorManager(byteT *ptr) {
	reinterpret_cast<inFileAllocatorManager<policy>*>(ptr)->unmapPages();
	munmap(ptr, pageSize);
}

template<typename T, template<size_t> typename policy>
struct inFileAllocator: public std::pointer_traits<T*> {
	inFileAllocatorManager<policy> *allocPtr;

	using value_type = T;
	using size_type = size_t;
	using pointer = T*;
	using const_pointer = const T*;
	using difference_type = typename std::pointer_traits<pointer>::difference_type;
	template<typename U>
	struct rebind {
		using other = inFileAllocator<U,policy>;
	};
	using propagate_on_container_copy_assignment = std::true_type;
	using propagate_on_container_move_assignment = std::true_type;
	using propagate_on_container_swap = std::true_type;
	using is_always_equal = std::false_type;

	inFileAllocator(inFileAllocatorManager<policy> *ptr) :
			allocPtr(ptr) {
	}

	template<typename U>
	constexpr inFileAllocator(const inFileAllocator<U, policy> &other) noexcept :
			allocPtr(other.allocPtr) {
	}

	T* allocate(size_t n) {
		return reinterpret_cast<T*>(allocPtr->allocate(n * sizeof(T)));
	}

	void deallocate(T *ptr, size_t n) noexcept {
		allocPtr->deallocate(reinterpret_cast<byteT*>(ptr), n * sizeof(T));
	}

	template<typename U, typename ... Args>
	void construct(U *ptr, Args &&... args) {
		new (ptr) U(std::forward<Args>(args)...);
	}

	template<typename U>
	void destroy(U *p) noexcept {
		p->~U();
	}

};

template<typename T, typename U, template<size_t> typename policy>
bool operator==(const inFileAllocator<T, policy> &a,
		const inFileAllocator<U, policy> &b) {
	return a.allocPtr == b.allocPtr;
}

template<typename T, typename U, template<size_t> typename policy>
bool operator!=(const inFileAllocator<T, policy> &a,
		const inFileAllocator<U, policy> &b) {
	return !(a == b);
}

}

namespace inFileAllocatorNS{
using inFileAllocatorNS::detail::inFileAllocator;
using inFileAllocatorNS::detail::AnonymousFileAllocator;
}

