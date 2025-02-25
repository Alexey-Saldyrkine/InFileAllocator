#ifndef INFILEOBJECTMANAGER_HPP_
#define INFILEOBJECTMANAGER_HPP_

#include "InFileAllocator.hpp"
#include <unordered_map>
namespace inFileAllocator {
namespace detail {
using keyT = size_t;
class objectManager {

	using ptrType = std::pair<size_t,void*>; // {hash of type, ptrToObject}
	using mapT = std::unordered_map<keyT,ptrType,std::hash<keyT>,std::equal_to<keyT>,fileAllocator<ptrType>>;
	FileMemoryManagerHandler handler;
	mapT *obj;

	template<typename T, typename ... Args>
	ptrType createObject(Args &&... args) {
		fileAllocator<T> alloc(handler.getManager());
		T *ptr = alloc.allocate(1);
		alloc.construct(ptr, std::forward<Args>(args)...);
		return {typeid(T).hash_code(),reinterpret_cast<void*>(ptr)};
	}


public:

	objectManager(int fd, void *adrs, size_t mappedMemSize):handler(fd,adrs,mappedMemSize){
		handler.setDefCstr();
		obj = handler.getManager()->getObj<mapT>();
	}

	template<typename U, typename ... Args>
	U& aquire(keyT key, Args &&... args) {
		auto iter = obj->find(key);
		if (iter == obj->end()) {
			auto oiter = obj->emplace(key,
					createObject<U>(std::forward<Args>(args)...)).first;
			ptrType &v = (*oiter).second;
			return *reinterpret_cast<U*>(v.second);
		}
		if (iter->second.first == typeid(U).hash_code()) {
			return *reinterpret_cast<U*>(iter->second.second);
		} else {
			throw std::runtime_error(
					"objectManager::aquire(), typeid.hash_code of aquired object is mismatched with what it is");
		}
	}

	template<typename T>
	fileAllocator<T> getAllocator(){
		return fileAllocator<T>(handler.getManager());
	}

	void resetFile(){
		handler.getManager()->reset();
	}

	FileMemoryManagerHandler& getHandler(){
		return handler;
	}

};

}

}

#endif /* INFILEOBJECTMANAGER_HPP_ */
