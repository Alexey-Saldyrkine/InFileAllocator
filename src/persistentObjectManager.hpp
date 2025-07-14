#pragma once
#include "fileAllocators.hpp"
#include <type_traits>
#include <variant>
#include <utility>
#include <scoped_allocator>
#include <unordered_map>

namespace inFileAllocatorNS::detail {

template<template<size_t> typename policy, typename keyT, typename ... Ts>
struct anonymousObjectManager {
	using variantT = std::variant<Ts...>;
	using allocT = std::scoped_allocator_adaptor<AnonymousFileAllocator<std::pair<const keyT,variantT>,policy>>;
	using mapT = std::unordered_map<keyT,variantT,std::hash<size_t>,std::equal_to<size_t>,
	allocT>;
	using managerT = anonymousFileAllocatorManager<policy>;
private:
	managerT *manager;

	mapT*& objPtr() {
		return *reinterpret_cast<mapT**>(&manager->objManagerPtr);
	}
public:
	operator managerT*() {
		return manager;
	}
	anonymousObjectManager(void *ptr) {
		manager = createAnonymousFileAllocatorManager<policy>(reinterpret_cast<byteT*>(ptr));
		if (objPtr() == nullptr) {
			byteT *tmp = manager->allocate(sizeof(mapT));
			new (tmp) mapT(manager);
			objPtr() = reinterpret_cast<mapT*>(tmp);
		}
	}
	~anonymousObjectManager() {
		manager->unmapPages();
		destroyAnonymousFileAllocatorManager<policy>(
				reinterpret_cast<byteT*>(manager));
	}

	managerT* getManager() {
		return manager;
	}

	template<typename T>
	AnonymousFileAllocator<T, policy> getAllocator() {
		return AnonymousFileAllocator<T, policy>(manager);
	}

	template<typename T>
	std::scoped_allocator_adaptor<AnonymousFileAllocator<T, policy>> getScopedAllocator() {
		return {getAllocator<T>()};
	}

	template<typename T, typename ... Args>
	T& aquireConstruct(keyT key, Args &&... args) {
		auto pr = objPtr()->emplace(std::piecewise_construct,
				std::forward_as_tuple(key),
				std::forward_as_tuple(std::in_place_type_t<T> { },
						std::forward<Args>(args)...));
		return std::get<T>(pr.first->second);
	}
	template<typename T>
	T& aquire(keyT key) {
		return std::get<T>(objPtr()->find(key)->second);
	}

	void destroy(keyT key) {
		objPtr()->erase(key);
	}

};

template<template<size_t> typename policy, typename keyT, typename ... Ts>
struct persistentObjectManager {
	using variantT = std::variant<Ts...>;
	using allocT = std::scoped_allocator_adaptor<inFileAllocator<std::pair<const keyT,variantT>,policy>>;
	using mapT = std::unordered_map<keyT,variantT,std::hash<size_t>,std::equal_to<size_t>,
	allocT>;
	using managerT = inFileAllocatorManager<policy>;
private:
	managerT *manager;

	mapT*& objPtr() {
		return *reinterpret_cast<mapT**>(&manager->objManagerPtr);
	}
public:
	operator managerT*() {
		return manager;
	}

	void constructMapObj() {
		byteT *tmp = manager->allocate(sizeof(mapT));
		new (tmp) mapT(manager);
		objPtr() = reinterpret_cast<mapT*>(tmp);
	}

	persistentObjectManager(int fd, void *ptr) {
		manager = createInFileAllocatorManager<policy>(fd, reinterpret_cast<byteT*>(ptr));
		if (objPtr() == nullptr) {
			constructMapObj();
		}
	}
	~persistentObjectManager() {
		manager->unmapPages();
		destroyInFileAllocatorManager<policy>(
				reinterpret_cast<byteT*>(manager));
	}

	managerT* getManager() {
		return manager;
	}
	void clearManager() {
		objPtr()->~mapT();
		manager->clearFile();
		constructMapObj();
	}

	template<typename T>
	inFileAllocator<T, policy> getAllocator() {
		return inFileAllocator<T, policy>(manager);
	}

	template<typename T>
	std::scoped_allocator_adaptor<inFileAllocator<T, policy>> getScopedAllocator() {
		return {getAllocator<T>()};
	}

	template<typename T, typename ... Args>
	T& acquireConstruct(keyT key, Args &&... args) {
		auto pr = objPtr()->emplace(std::piecewise_construct,
				std::forward_as_tuple(key),
				std::forward_as_tuple(std::in_place_type_t<T> { },
						std::forward<Args>(args)...));
		return std::get<T>(pr.first->second);
	}
	template<typename T>
	T& acquire(keyT key) {
		return std::get<T>(objPtr()->find(key)->second);
	}

	void destroy(keyT key) {
		objPtr()->erase(key);
	}

};

}

namespace inFileAllocatorNS {
using inFileAllocatorNS::detail::persistentObjectManager;
using inFileAllocatorNS::detail::anonymousObjectManager;
}
