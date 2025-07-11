#pragma once
#include <sys/mman.h>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <array>
#include <cstring>
#include <list>

namespace inFileAllocatorNS {
namespace detail {
using byteT = uint8_t;
constexpr size_t pageSize = 4096;

static_assert(sizeof(byteT) == 1,"byteT size must be eqaul to a byte");

constexpr byteT minPow2(size_t i) {
	return 64 - __builtin_clzll(i);
}

constexpr size_t pow2(int i) {
	return 1ull << i;
}

constexpr bool isPow2(size_t i) {
	if (i == 0)
		return false;
	return !(i & (i - 1));
}

constexpr size_t mylog2(size_t size) {
	if (isPow2(size))
		return minPow2(size) - 1;
	else
		return minPow2(size);
}

static_assert(mylog2(64)==6);
static_assert(mylog2(1024)==10);
static_assert(mylog2(1023)==10);

constexpr bool enableDebugLog = true;

struct debugAction {
	enum actionType {
		allocNewBlock, deleteBlock, splitBlock, combineBlocks, returnBlock
	};

	debugAction(actionType AT, size_t S, byteT *A, byteT *B) :
			action(AT), size(S), block(A), buddyBlock(B) {
	}

	actionType action;
	size_t size;
	byteT *block;
	byteT *buddyBlock;
};

struct debugActionLog {
	std::list<debugAction> actions;

	void add(debugAction::actionType TP, size_t size, void *A, void *B) {
		actions.push_back(debugAction { TP, size, reinterpret_cast<byteT*>(A),
				reinterpret_cast<byteT*>(B) });
	}

	debugAction::actionType nextType() {
		return actions.front().action;
	}

	debugAction next() {
		debugAction ret = actions.front();
		actions.pop_front();
		return ret;
	}

	void clear() {
		actions.clear();
	}
};

static debugActionLog debugLog;

template<size_t size>
struct memBlock;

template<>
struct memBlock<0> {

};

constexpr size_t memBlockInfoSpecialNumber = 103460011944;

template<size_t size>
struct memBlockInfo {
	byteT spanSize; // if =0, then block is used
	memBlock<size> *next;
	size_t specialNumber;
	memBlock<size> *prev;

	void clear() {
		spanSize = 0;
		specialNumber = 0;
		next = nullptr;
		prev = nullptr;
	}
	void reset() {
		spanSize = mylog2(size);
		specialNumber = memBlockInfoSpecialNumber;
	}

	~memBlockInfo() = delete;
};

template<size_t size>
struct memBlockData {
	byteT data[size];

	memBlockData() :
			data { } {
	}
	void clear() {
		memset(data, 0, size);
	}

	~memBlockData() = delete;
};

template<size_t size>
struct memBlock {
private:
	static_assert(sizeof(memBlockInfo<size>) <= sizeof(memBlockData<size>) );
	union {
		memBlockInfo<size> info;
		memBlockData<size> storage;
	};
public:
	memBlock() {
	}

	~memBlock() = delete;

	bool isUsed() {
		return info.specialNumber != memBlockInfoSpecialNumber;
	}

	void makeUnused() {
		storage.clear();
		info.reset();
	}

	void makeUsed() {
		info.clear();
	}

	memBlock<size>*& nextBlock() {
		return info.next;
	}

	memBlock<size>*& previousBlock() {
		return info.prev;
	}

	byteT& sizeOfSpan() {
		return info.spanSize;
	}

	byteT* data() {
		return storage.data;
	}

};

template<size_t size>
struct defaultPolicies {
	static constexpr size_t minAllocSize = 64;
	static constexpr size_t maxAllocSize = 4096 * 4;
	static constexpr bool eanbleSpliting = true;
	static constexpr bool enableCombining = true;
	static constexpr size_t allocBlockCount = 0;

};

byteT* buddyAdressInterm(size_t size, byteT *adrs) {
	return reinterpret_cast<byteT*>(reinterpret_cast<size_t>(adrs)
			^ size_t { 1 } << mylog2(size));
}

byteT* buddyAdress(size_t size, byteT *adrs) {
	return buddyAdressInterm(size, adrs);
//	size_t dif = pow2(minPow2(size));
//	if(buddyAdressInterm(size, adrs) < adrs){
//		return reinterpret_cast<byteT*>(reinterpret_cast<size_t>(adrs)+dif);
//	}else{
//		return reinterpret_cast<byteT*>(reinterpret_cast<size_t>(adrs)-dif);
//	}
}

template<size_t size, typename policy>
constexpr size_t freePageAllocCount = (
		policy::allocBlockCount != 0 ?
				policy::allocBlockCount :
				(size >= pageSize ? 1 : pageSize / size));

template<size_t size, typename policy>
constexpr size_t freePageAllocSize = (
		policy::allocBlockCount != 0 ?
				(size * policy::allocBlockCount) / pageSize
						+ ((size * policy::allocBlockCount) % pageSize == 0 ?
								0 : 1) :
				(size >= pageSize ? size : pageSize));

template<size_t size, auto allocNewBlockFunc, typename ANBFrefT,
		template<size_t> typename policies>
struct memSpan {
private:

	memBlock<size> *first;
	memBlock<size> *last;
	memBlock<size> *nextFreeBlock;
	size_t freePageBlockCount;

	using policy = policies<size>;

	memSpan<size * 2, allocNewBlockFunc, ANBFrefT, policies>& nextSpan() {
		return *reinterpret_cast<memSpan<size * 2, allocNewBlockFunc, ANBFrefT,
				policies>*>(this + 1);
	}

	bool anyFreeBlocks() {
		return first != nullptr;
	}

	void allocateNewFreeMemPage(ANBFrefT allocObj) {
		nextFreeBlock = reinterpret_cast<memBlock<size>*>((allocObj
				.*allocNewBlockFunc)(freePageAllocSize<size, policy>));
		freePageBlockCount = freePageAllocCount<size, policy>;
	}
	memBlock<size>* getBlockFromFreePage(ANBFrefT allocObj) {
		if (freePageBlockCount == 0) {
			allocateNewFreeMemPage(allocObj);
		}
		memBlock<size> *ret = nextFreeBlock;
		nextFreeBlock++;
		freePageBlockCount--;
		return ret;

	}

	memBlock<size>* getNewBlocks(ANBFrefT allocObj) {
		// allocate new mem ans turn it into blocks
		if constexpr (policy::eanbleSpliting && policy::maxAllocSize > size) {
			static_assert(size*2 <= policy::maxAllocSize);
			// get block from the next span and split it
			auto blocks = nextSpan().splitBlock(allocObj);
			putBlockInList(blocks.second);
			blocks.first->makeUsed();
			return blocks.first;
		}
		memBlock<size> *retPtr = getBlockFromFreePage(allocObj);
		if constexpr (enableDebugLog) {
			debugLog.add(debugAction::allocNewBlock, size,
					reinterpret_cast<byteT*>(retPtr), 0);
		}
		new (retPtr) memBlock<size> { };
		return retPtr;

	}

	memBlock<size>* getBlockFromList() {
		auto tmpPtr = first;
		if ((first == last)) {
			first = nullptr;
			last = nullptr;
			tmpPtr->makeUsed();
			return tmpPtr;
		} else {
			if (first->nextBlock() == nullptr) {
				std::cerr << "error here" << std::endl;
			}
			first = tmpPtr->nextBlock();

			first->previousBlock() = nullptr;
			tmpPtr->makeUsed();
			return tmpPtr;
		}
	}

	memBlock<size>* getFreeBlock(ANBFrefT allocObj) {
		if (!anyFreeBlocks()) {
			return getNewBlocks(allocObj);
		} else {
			return getBlockFromList();
		}
	}

public:
	void putBlockInList(memBlock<size> *ptr) {
		ptr->makeUnused();
		if constexpr (policy::enableCombining && size < policy::maxAllocSize) {
			memBlock<size> *buddy =
					reinterpret_cast<memBlock<size>*>(buddyAdress(size - 1,
							reinterpret_cast<byteT*>(ptr)));
			if (!buddy->isUsed() && buddy->sizeOfSpan() == ptr->sizeOfSpan()) {
				memBlock<size * 2> *basePtr =
						reinterpret_cast<memBlock<size * 2>*>(std::min(buddy,
								ptr));
				if constexpr (enableDebugLog) {
					debugLog.add(debugAction::combineBlocks, size, basePtr,
							std::max(ptr, buddy));
				}
				if (buddy->previousBlock() != nullptr) {
					buddy->previousBlock()->nextBlock() = buddy->nextBlock();
				}
				if (buddy->nextBlock() != 0) {
					buddy->nextBlock()->previousBlock() =
							buddy->previousBlock();
				}
				if (first == buddy) {
					first = buddy->nextBlock();
				}
				if (last == buddy) {
					last = buddy->previousBlock();
				}
				if (last == 0) {
					last = first;
				}
				basePtr->makeUnused();
				nextSpan().putBlockInList(basePtr);

				return;
			}

		}

		if (!anyFreeBlocks()) {
			first = ptr;
		} else {
			last->nextBlock() = ptr;
			ptr->previousBlock() = last;
		}
		last = ptr;

	}

	std::pair<memBlock<size / 2>*, memBlock<size / 2>*> splitBlock(
			ANBFrefT allocObj) {
		auto block = getFreeBlock(allocObj);
		memBlock<size / 2> *blockA =
				reinterpret_cast<memBlock<size / 2>*>(block);
		memBlock<size / 2> *blockB = blockA + 1;

		if constexpr (enableDebugLog) {
			debugLog.add(debugAction::splitBlock, size / 2, blockA, blockB);
		}

		return {blockA, blockB};
	}

	byteT* getBlock(ANBFrefT allocObj) {
		memBlock<size> *ptr = getFreeBlock(allocObj);
		if constexpr (enableDebugLog) {
			debugLog.add(debugAction::returnBlock, size, ptr, 0);
		}
		return ptr->data();
	}

	void putBlock(byteT *adrs, ANBFrefT allocObj) {
		putBlockInList(reinterpret_cast<memBlock<size>*>(adrs));
	}

	memSpan() :
			first(nullptr), last(nullptr), nextFreeBlock(nullptr), freePageBlockCount(
					0) {
	}

};

//constexpr const size_t spanCount = 30;
//constexpr const size_t spanOffset = 6;

template<template<size_t> typename policies>
constexpr size_t policyToSpanOffset = mylog2(policies<0>::minAllocSize);

template<template<size_t> typename policies>
constexpr size_t policyToSpanCount = 1 + mylog2(policies<0>::maxAllocSize)
		- policyToSpanOffset<policies>;

template<auto, template<size_t> typename, typename >
struct memorySpans;

template<auto T, template<size_t> typename policies, typename ANBFrefT>
using getFptrT =std::array<byteT*(*const)(memorySpans<T,policies,ANBFrefT>*,ANBFrefT),policyToSpanCount<policies>>;

template<auto T, template<size_t> typename policies, typename ANBFrefT>
using putFptrT =std::array<void(*const)(memorySpans<T,policies,ANBFrefT>*,byteT*,ANBFrefT),policyToSpanCount<policies>>;

template<template<size_t> typename, typename, auto, template<size_t> typename,
		typename >
struct getMemFuncArrayCreation;

template<template<size_t> typename func, size_t ... INTS,
		auto allocNewBlockFunc, template<size_t> typename policies,
		typename ANBFrefT>
struct getMemFuncArrayCreation<func, std::integer_sequence<size_t, INTS...>,
		allocNewBlockFunc, policies, ANBFrefT> {
	static constexpr getFptrT<allocNewBlockFunc, policies, ANBFrefT> initList =
			{ &func<INTS>::get... };

};

template<template<size_t> typename, typename, auto, template<size_t> typename,
		typename >
struct freeMemFuncArrayCreation;

template<template<size_t> typename func, size_t ... INTS,
		auto allocNewBlockFunc, template<size_t> typename policies,
		typename ANBFrefT>
struct freeMemFuncArrayCreation<func, std::integer_sequence<size_t, INTS...>,
		allocNewBlockFunc, policies, ANBFrefT> {
	static constexpr putFptrT<allocNewBlockFunc, policies, ANBFrefT> initList =
			{ &func<INTS>::free... };

};

template<size_t, size_t, typename >
struct createIntegerSeqFromToImpl;

template<size_t i, size_t end, size_t ... INTS>
struct createIntegerSeqFromToImpl<i, end, std::integer_sequence<size_t, INTS...>> {
	static constexpr auto f() {
		if constexpr (i >= end) {
			return std::integer_sequence<size_t, INTS...> { };
		} else {
			return typename createIntegerSeqFromToImpl<i + 1, end,
					std::integer_sequence<size_t, INTS..., i>>::type { };
		}
	}

	using type = decltype(f());
};

template<auto allocNewBlockFunc, template<size_t> typename policies,
		typename ANBFrefT>
struct memorySpans {
private:

	using arrayT = memSpan<64, allocNewBlockFunc, ANBFrefT, policies>[policyToSpanCount<policies>];
	byteT mem[sizeof(arrayT)];
	arrayT& spans() {
		return *reinterpret_cast<arrayT*>(mem);
	}

	using intSeq = typename createIntegerSeqFromToImpl<policyToSpanOffset<policies>,
	policyToSpanCount<policies> + policyToSpanOffset<policies>,
	std::integer_sequence<size_t>>::type;

#pragma GCC diagnostic push
	// ignore for now...
#pragma GCC diagnostic ignored "-Wstrict-aliasing"

	template<size_t i>
	struct getBlockfromSpanI {
		static byteT* get(memorySpans *self, ANBFrefT obj) {
			return reinterpret_cast<memSpan<pow2(i), allocNewBlockFunc,
					ANBFrefT, policies>*>(&self->spans()[i
					- policyToSpanOffset<policies> ])->getBlock(obj);
			//return reinterpret_cast<byteT*>(i);
		}

		static void free(memorySpans *self, byteT *adrs, ANBFrefT obj) {
			reinterpret_cast<memSpan<pow2(i), allocNewBlockFunc, ANBFrefT,
					policies>*>(&self->spans()[i - policyToSpanOffset<policies> ])->putBlock(
					adrs, obj);
		}
	};
#pragma GCC diagnostic pop

	static constexpr getFptrT<allocNewBlockFunc, policies, ANBFrefT> getMemFuncPtrs =
			getMemFuncArrayCreation<getBlockfromSpanI, intSeq,
					allocNewBlockFunc, policies, ANBFrefT>::initList;
	static constexpr putFptrT<allocNewBlockFunc, policies, ANBFrefT> freeMemFuncPtrs =
			freeMemFuncArrayCreation<getBlockfromSpanI, intSeq,
					allocNewBlockFunc, policies, ANBFrefT>::initList;

public:
	byteT* getMem(size_t i, ANBFrefT obj) {
		return getMemFuncPtrs[i](this, obj);
	}

	void freeMem(byteT *adrs, size_t i, ANBFrefT obj) {
		freeMemFuncPtrs[i](this, adrs, obj);
	}

	memorySpans() {
		new (&spans()) arrayT { };
	}

	~memorySpans() = delete;

};

template<typename >
struct getTypeFromMemberPtr;

template<typename T, typename U>
struct getTypeFromMemberPtr<T U::*> {
	using type = U;
};

template<auto allocNewBlockFunc,
		template<size_t> typename policies = defaultPolicies>
struct buddyAllocator {
	static_assert(policies<0>::minAllocSize >= sizeof(memBlockInfo<0>), "min allocation size must be bigger or equal to sizeof memBlock<0>");
	static_assert(isPow2(policies<0>::minAllocSize),"minAllocSize must be a power of 2");
	static_assert(isPow2(policies<0>::maxAllocSize),"maxAllocSize must be a power of 2");

	using ANBFrefT = typename getTypeFromMemberPtr<decltype(allocNewBlockFunc)>::type&;

	byteT mem[sizeof(memorySpans<allocNewBlockFunc, policies, ANBFrefT> )];
	memorySpans<allocNewBlockFunc, policies, ANBFrefT> &spans =
			*reinterpret_cast<memorySpans<allocNewBlockFunc, policies, ANBFrefT>*>(mem);

	void clear() {
		memset(mem, 0,
				sizeof(memorySpans<allocNewBlockFunc, policies, ANBFrefT> ));
	}
	byteT* allocate(size_t size, ANBFrefT obj) {
		if (size > policies<0>::maxAllocSize) {
			throw std::runtime_error(
					std::string(
							"buddyAllocator: tried allocate more than the policy allowed ")
							+ std::to_string(size));
			return reinterpret_cast<byteT*>(-1);
		} else {
			size_t i = mylog2(size);

			i = i <= policyToSpanOffset<policies> ?
					0 : i - policyToSpanOffset<policies>;
			return spans.getMem(i, obj);
		}
	}

	void deallocate(byteT *adrs, size_t size, ANBFrefT obj) {
		size_t i = mylog2(size);
		i = i <= policyToSpanOffset<policies> ?
				0 : i - policyToSpanOffset<policies>;
		spans.freeMem(adrs, i, obj);
	}

	buddyAllocator() {
		new (mem) memorySpans<allocNewBlockFunc, policies, ANBFrefT> { };
	}

	~buddyAllocator() = delete;
};

}
}
