#ifndef INFILEALLOCATOR_HPP_
#define INFILEALLOCATOR_HPP_

#include <iostream>
#include <sys/mman.h>
#include <sys/stat.h>
#include<fcntl.h>
#include<string>
#include<string.h>
#include <vector>
#include <unistd.h>
#include <stdio.h>
#include<functional>
#include <list>
#include <errno.h>
#include <map>
#include <memory>
#include<ctime>
#include <type_traits>
#include <limits>

namespace inFileAllocator {

namespace detail {

//force size to 8bit
using Forceduint8_t = uint8_t;
static_assert(sizeof(Forceduint8_t)==1);

//system dependent
const size_t pageSize = 4096;

// compiler dependent
template<size_t size>
constexpr unsigned int sizeToPowIndex = 64 - __builtin_clzll(size);

template<size_t x>
constexpr size_t pow2 = 1ul << x;

template<size_t x>
constexpr bool isPowerOf2 = (!(x & (x - 1)));

template<>
constexpr bool isPowerOf2<0> = false;

template<size_t size>
union MemBlock;

template<size_t size>
struct UnusedMemBlock {
	static inline constexpr size_t marker1Hash = 5747124830538865000;
	size_t unusedMarker1 = marker1Hash;
	MemBlock<size> *next = nullptr;
	MemBlock<size> *prev = nullptr;
	size_t spanPower = 0;

	bool isNotUsed() {
		return unusedMarker1 == marker1Hash; // && unusedMarker2 == marker2Hash;
	}

	void setUnused(size_t spanPow) {
		next = nullptr;
		prev = nullptr;
		unusedMarker1 = marker1Hash;
		spanPower = spanPow;
	}

	void setUsed() {
		unusedMarker1 = 0;
		next = nullptr;
		prev = nullptr;
		spanPower = 0;
	}

	static MemBlock<size>* interBuddyAdress(UnusedMemBlock<size> *ptr) {
		return reinterpret_cast<MemBlock<size>*>(reinterpret_cast<size_t>(ptr)
				^ static_cast<size_t>(1) << sizeToPowIndex<size - 1> );
	}

	MemBlock<size>* buddyAdress() {
		return interBuddyAdress(this);
	}

};

template<size_t size>
union MemBlock {
	Forceduint8_t asData[size];
	UnusedMemBlock<size> asUnused;
	static_assert(size >=sizeof(UnusedMemBlock<size>));
	static_assert(isPowerOf2<size>);

	std::pair<MemBlock<size / 2>*, MemBlock<size / 2>*> split() {
		return {reinterpret_cast<MemBlock<size/2>*>(&asData[0]), reinterpret_cast<MemBlock<size/2>*>(&asData[size/2])};
	}
};

template<size_t elementSize>
struct MemBlockStoragePage {
	static inline constexpr size_t pageCount = elementSize / pageSize
			+ !!(elementSize % pageSize);
	static inline constexpr size_t blockCount = (pageCount * pageSize)
			/ elementSize;

	MemBlock<elementSize> blocks[blockCount];
	MemBlock<elementSize>& operator[](size_t i) {
		return blocks[i];
	}
};

void ensureFileSize(const int &_fd, const size_t &_size) {
	struct stat st;
	fstat(_fd, &st);
	size_t fileSize = st.st_size;
	if (fileSize < _size) {
		ftruncate(_fd, _size);
	}
}

struct MemoryFileHandler {
	int fd;
	size_t mappedMemSize;
	size_t size = pageSize;
	Forceduint8_t *dataAdress = 0;

	MemoryFileHandler(int _fd, Forceduint8_t *_adr, size_t _mappedMemSize) :
			fd(_fd), mappedMemSize(_mappedMemSize), dataAdress(_adr) {
	}

	void reset() {
		size = pageSize;
		ftruncate(fd, pageSize);
	}

	void* getFreePages(const size_t &pageCount) {
		if (mappedMemSize + pageSize - size < pageCount * pageSize) {
			std::string str = "out of mem, remaning mem: ";
			str += std::to_string(mappedMemSize + pageSize - size)
					+ ", requested mem: " + std::to_string(pageCount * pageSize);
			throw std::runtime_error(str);
		}
		void *retPtr = static_cast<void*>(dataAdress + size);
		size += pageSize * pageCount;
		ensureFileSize(fd, size);
		return retPtr;
	}

};

template<size_t powerIndex>
struct SpanOfSize {
	static constexpr size_t blockSize = pow2<powerIndex>;
	MemBlock<blockSize> *first = nullptr;
	MemBlock<blockSize> *last = nullptr;

	void reset() {
		first = nullptr;
		last = nullptr;
	}

	SpanOfSize<powerIndex + 1>& nextSpan() {
		return *reinterpret_cast<SpanOfSize<powerIndex + 1>*>(this + 1);
	}

	MemBlock<blockSize>* getFreeBlock(MemoryFileHandler &fileHandler) {
		if constexpr (powerIndex >= 16) {
			return static_cast<MemBlock<blockSize>*>(fileHandler.getFreePages(
					MemBlockStoragePage<blockSize>::pageCount));
		} else {
			MemBlock<blockSize * 2> *dualBLock = reinterpret_cast<MemBlock<
					blockSize * 2>*>(nextSpan().getBlock(fileHandler));
			auto blockPair = dualBLock->split();
			putBlock(blockPair.second);
			blockPair.first->asUnused.setUsed();
			return blockPair.first;
		}
	}

	Forceduint8_t* getBlock(MemoryFileHandler &fileHandler) {

		if (first == nullptr) {
			MemBlock<blockSize> &retBlock = *getFreeBlock(fileHandler);
			retBlock.asUnused.setUsed();
			return retBlock.asData;
		}
		if (first == last) {
				MemBlock<blockSize> *retBlock = first;
				first = nullptr;
				last = nullptr;
				retBlock->asUnused.setUsed();
				return retBlock->asData;

		}

			MemBlock<blockSize> *retBlock = first;
			first = retBlock->asUnused.next;
			first->asUnused.prev = nullptr;
			retBlock->asUnused.setUsed();
			return retBlock->asData;

	}

	void putBlock(MemBlock<blockSize> *block) {
		if (!block->asUnused.isNotUsed()) {
			block->asUnused.setUnused(powerIndex);
		} else {
			block->asUnused.spanPower = powerIndex;
		}

		if constexpr (powerIndex < 16) {
			if (auto *buddyPtr = block->asUnused.buddyAdress(); (buddyPtr->asUnused.spanPower
					== powerIndex) && buddyPtr->asUnused.isNotUsed()) {
				auto &buddyBlock = buddyPtr->asUnused;
				if (buddyBlock.prev != nullptr)
					buddyBlock.prev->asUnused.next = buddyBlock.next;
				if (buddyBlock.next != nullptr)
					buddyBlock.next->asUnused.prev = buddyBlock.prev;

				auto *leftBlock = (block < buddyPtr ? block : buddyPtr);
				auto *rightBlock = (block > buddyPtr ? block : buddyPtr);
				rightBlock->asUnused.setUsed();
				nextSpan().putBlock(
						reinterpret_cast<MemBlock<blockSize * 2>*>(leftBlock));
				return;
			}
		}

		if (first != nullptr) {
			last->asUnused.next = block;
			block->asUnused.prev = last;
			block->asUnused.next = nullptr;
			last = block;
		} else {
			block->asUnused.next = nullptr;
			block->asUnused.prev = nullptr;
			first = block;
			last = block;
		}

	}
};

constexpr size_t IndexOffset = 5;

template<size_t Index>
Forceduint8_t* allocateI(void *spanPtr, MemoryFileHandler &fileHandler) {
	return static_cast<SpanOfSize<Index + IndexOffset>*>(spanPtr)->getBlock(
			fileHandler);
}

template<size_t Index>
void deallocateI(void *spanPtr, void *ptr) {
	static_cast<SpanOfSize<Index + IndexOffset>*>(spanPtr)->putBlock(
			static_cast<MemBlock<pow2<Index + IndexOffset>>*>(ptr));
}

template<typename T>
struct SpanListHelper {

};

template<size_t ... Is>
struct SpanListHelper<std::integer_sequence<size_t, Is...>> {
	inline static constexpr Forceduint8_t* (*allocByIndx[])(void*,
			MemoryFileHandler&) = {allocateI<Is>...};
	inline static constexpr void (*deallocByIndx[])(void*,
			void*) = {deallocateI<Is>...};
};

// compiler dependent
unsigned int sizeToIndex(const size_t size) {
	if (size >= pow2<IndexOffset>) {
		return 64 - __builtin_clzll(size) - IndexOffset;
	} else {
		return 0;
	}
}

class SpanList: public SpanListHelper<
		std::make_integer_sequence<size_t, 63 - IndexOffset>> {

	struct Dummy {
		static_assert(sizeof(SpanOfSize<1>)==sizeof(SpanOfSize<20>));
		char data[sizeof(SpanOfSize<1> )] = { };
	};
	Dummy spans[60];

public:
	Forceduint8_t* allocate(size_t size, MemoryFileHandler &fileHandler) {
		unsigned int index = sizeToIndex(size);
		return allocByIndx[index](&spans[index], fileHandler);
	}

	void deallocate(void *ptr, size_t size) {
		unsigned int index = sizeToIndex(size);
		deallocByIndx[index](&spans[index], ptr);
	}

	void resetAll() {
		for (size_t i = 0; i < 63 - IndexOffset; ++i) {
			reinterpret_cast<SpanOfSize<1>*>(&spans[i])->reset();
		}
	}

};

const size_t confirmationNumber = 1217160;

class alignas(pageSize) FileMemoryManager {
	static constexpr size_t minSize = 8;
	static constexpr size_t maxSize = pow2<63>;
	static constexpr size_t minI = 3;
	static constexpr size_t maxI = 62;

	void *objPtr = 0;
	size_t confNum = confirmationNumber;
	MemoryFileHandler fileHandler;
	alignas(8) SpanList listOfSpans;

public:
	FileMemoryManager(int _fd, void *adrs, size_t mappedMemSize) :
			fileHandler(_fd, static_cast<Forceduint8_t*>(adrs), mappedMemSize) {
	}

	void reset() {
		objPtr = 0;
		fileHandler.reset();
		listOfSpans.resetAll();
	}

	bool isConstructed() {
		return confNum == confirmationNumber;
	}
	bool testmemSize(size_t memSize) {
		return memSize == fileHandler.mappedMemSize;
	}
	void setFd(int fd) {
		fileHandler.fd = fd;
	}

	MemoryFileHandler& getFilehandler() {
		return fileHandler;
	}

	size_t getMemSize() {
		return fileHandler.mappedMemSize;
	}

	Forceduint8_t* allocate(size_t _size) {
		return listOfSpans.allocate(_size, fileHandler);
	}

	void deallocate(void *ptr, size_t _size) {
		if (ptr >= (fileHandler.dataAdress + pageSize)
				&& ptr
						<= (fileHandler.dataAdress + fileHandler.mappedMemSize
								+ pageSize)) {
			listOfSpans.deallocate(ptr, _size);
		}
	}

	template<typename U, typename ... Args>
	U* getObj(Args &&... args) {
		if (objPtr == NULL) {
			Forceduint8_t *tmp = allocate(sizeof(U));
			new (tmp) U(args...);
			objPtr = tmp;
		}
		return reinterpret_cast<U*>(objPtr);
	}
};

struct FileMemoryManagerSharedPtrDeleter {
	void operator()(FileMemoryManager *ptr) {
		munmap(ptr, ptr->getMemSize());
	}
};

class FileMemoryManagerHandler {
	static inline FileMemoryManager *DefCstrWorkAroundPtr;
	static_assert(sizeof(FileMemoryManager) <= pageSize);
	std::shared_ptr<FileMemoryManager> manager;
	void mapHeader(int fd, void *adrs, size_t mappedMemSize) {
		ensureFileSize(fd, pageSize);

		FileMemoryManager *adr = static_cast<FileMemoryManager*>(mmap(adrs,
				mappedMemSize,
				PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_NORESERVE, fd, 0));

		if (adr == MAP_FAILED) {
			fprintf(stderr, "mmap [mapHeader] failed: %s\n", strerror(errno));
			throw std::runtime_error("failed to map header");
		}

		if (adr != adrs) {
			//std::cerr << "adrs = " << (void*) adr << std::endl;
			//throwError("[mapHeader]did not map to dedicated adrs");
			throw std::runtime_error("failed to map to given adrs");
		}
	}

public:
	FileMemoryManagerHandler(int fd, void *adrs, size_t mappedMemSize) :
			manager(static_cast<FileMemoryManager*>(adrs),
					FileMemoryManagerSharedPtrDeleter()) {
		mapHeader(fd, adrs, mappedMemSize);
		if (!manager->isConstructed()) {
			new (manager.get()) FileMemoryManager(fd, adrs, mappedMemSize);
		} else {
			if (!manager->testmemSize(mappedMemSize)) {
				throw std::runtime_error("different size of memory given");
			}
			manager->setFd(fd);
		}
	}

	FileMemoryManager* getManager() {
		return manager.get();
	}

	void setDefCstr() {
		DefCstrWorkAroundPtr = manager.get();
	}

	static FileMemoryManager* getDefPtr() {
		return DefCstrWorkAroundPtr;
	}

	~FileMemoryManagerHandler() {

	}

};

template<typename T>
class fileAllocator: public std::pointer_traits<T*> {
private:
	FileMemoryManager *manager;

public:
	using value_type = T;
	using size_type = std::size_t;
	using pointer = T*;
	using const_pointer = const T*;
	using difference_type = typename std::pointer_traits<pointer>::difference_type;
	template<typename U>
	struct rebind {
		using other = fileAllocator<U>;
	};

	fileAllocator() :
			manager(FileMemoryManagerHandler::getDefPtr()) {
	}

	fileAllocator(FileMemoryManager *_manager) :
			manager(_manager) {

	}

	template<typename U>
	fileAllocator(const fileAllocator<U> &other) noexcept :
			manager(other.getManagerPtr()) {
	}

	T* allocate(size_t count, const void* = 0) {
		return reinterpret_cast<T*>(manager->allocate(count * sizeof(T)));
	}

	void deallocate(T *ptr, size_t size) noexcept {
		manager->deallocate(ptr, size);
	}

	template<typename U, typename ... Args>
	void construct(U *ptr, Args &&... args) {
		new (ptr) U(args...);
	}

	template<typename U>
	void destroy(U *p) noexcept {
		p->~U();
	}

	FileMemoryManager* getManagerPtr() const {
		return manager;
	}

};

template<typename T, typename U>
constexpr bool operator==(const fileAllocator<T> &a,
		const fileAllocator<U> &b) noexcept {
	return false;
}

template<typename T>
constexpr bool operator==(const fileAllocator<T> &a,
		const fileAllocator<T> &b) noexcept {
	return true;
}

template<typename T, typename U>
constexpr bool operator!=(const fileAllocator<T> &a,
		const fileAllocator<U> &b) noexcept {
	return !(a == b);
}

}
}

#endif /* INFILEALLOCATOR_HPP_ */
