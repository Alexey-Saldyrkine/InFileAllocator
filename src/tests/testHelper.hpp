#pragma once
#include <gtest/gtest.h>
#include <cstdlib>
#include <list>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <sys/mman.h>
#include "../buddyAllocator.hpp"

using namespace inFileAllocatorNS::detail;
using namespace std;

template<template<size_t> typename T>
struct buddyAllocatorTester {
	byteT *nextPtr = 0;
	size_t prevAlloc = 0;
	byteT *basePtr;

	byteT* mapPages(size_t size) {
		int pageCount = size / pageSize + (size % pageSize == 0 ? 0 : 1);
		size_t psize = pageCount * pageSize;
		byteT *adrs = basePtr + reinterpret_cast<size_t>(nextPtr);
		nextPtr += psize;
		prevAlloc = psize;
		byteT *ret = reinterpret_cast<byteT*>(mmap(adrs, psize + pageSize,
		PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0));
		if (ret == reinterpret_cast<byteT*>(-1)) {
			perror("mmap");
			exit(1);
		}
		return ret;
	}

	void unmapPages() {
		munmap(basePtr, reinterpret_cast<size_t>(nextPtr));
	}

	byteT* allocNewBlock(size_t size) {
		return mapPages(size);
	}

	using allocT = buddyAllocator<&buddyAllocatorTester::allocNewBlock,T>;
	byteT mem[sizeof(allocT)];
	allocT &alloc = *reinterpret_cast<allocT*>(mem);

	byteT* allocate(size_t size) {
		return alloc.allocate(size, *this);
	}

	void deallocate(byteT *adrs, size_t size) {
		alloc.deallocate(adrs, size, *this);
	}

	buddyAllocatorTester() {
		new (&alloc) allocT { };
		basePtr = reinterpret_cast<byteT*>(0x050000000000);
	}

	~buddyAllocatorTester() = delete;

};



struct buddyAllocatorBlockLog {

	std::unordered_map<size_t, std::list<byteT*>> allocatorBlocks;
	std::unordered_map<size_t, std::unordered_set<byteT*>> userBlocks;
	size_t usedBlockCount = 0;
	size_t blocksAllocated = 0;
	size_t bytesAllocated = 0;
	size_t splitCount = 0;
	size_t combineCount = 0;
	std::map<size_t, size_t> blockFreqMap;
	std::map<size_t, size_t> allocfreqMap;
	std::map<size_t, size_t> deallocfreqMap;
	std::map<size_t, size_t> splitfreqMap;
	std::map<size_t, size_t> combinefreqMap;


	void printData() {
		std::cerr << "Data:" << endl << "Bytes allocated: " << bytesAllocated
				<< " (" << bytesAllocated / pageSize << " pages)" << endl
				<< "Blocks allocated: " << blocksAllocated << endl
				<< "Split count: " << splitCount << endl << "Combine count: "
				<< combineCount << endl;
		std::cerr << "Allocation size frequency" << endl;
		std::cerr << "size \talloc\tdealloc\tblockAlloc\tsplit\tcombine"
				<< endl;
		size_t totalCount = 0;
		size_t deallocCount = 0;
		size_t blockCount = 0;
		size_t splitCount = 0;
		size_t combineCount = 0;
		for (int i = 6; i < 15; i++) {
			size_t size = pow2(i);
			std::cerr << size;
			const auto &it = allocfreqMap.find(size);
			if (it != allocfreqMap.end()) {
				std::cerr << "\t" << it->second;
				totalCount += it->second;
			} else {
				std::cerr << "\t-";
			}

			auto it1 = deallocfreqMap.find(size);
			if (it1 != deallocfreqMap.end()) {
				std::cerr << "\t" << it1->second;
				deallocCount += it1->second;
			} else {
				std::cerr << "\t-";
			}

			auto it2 = blockFreqMap.find(size);
			if (it2 != blockFreqMap.end()) {
				std::cerr << "\t" << it2->second;
				blockCount += it2->second;
			} else {
				std::cerr << "\t-";
			}
			auto it3 = splitfreqMap.find(size/2);
			if (it3 != splitfreqMap.end()) {
				std::cerr << "\t\t" << it3->second;
				splitCount += it3->second;
			} else {
				std::cerr << "\t\t-";
			}
			auto it4 = combinefreqMap.find(size);
			if (it4 != combinefreqMap.end()) {
				std::cerr << "\t" << it4->second;
				combineCount += it4->second;
			} else {
				std::cerr << "\t-";
			}

			std::cerr << endl;

		}
		std::cerr << "Tc: \t" << totalCount << "\t" << deallocCount << "\t"
				<< blockCount << "\t\t" << splitCount << "\t" << combineCount
				<< endl;
		std::cerr << endl;
	}

	void doLog() {
		while (debugLog.actions.size() > 0
				&& debugLog.nextType() != debugAction::returnBlock) { //  &&
			auto action = debugLog.next();
			if (action.action == debugAction::allocNewBlock) {
				blocksAllocated++;
				bytesAllocated += action.size;
				blockFreqMap[action.size]++;
				allocatorBlocks[action.size].push_back(action.block);
				auto &mp = allocatorBlocks[action.size];
				ASSERT_TRUE(mp.size() > 0);
			} else if (action.action == debugAction::splitBlock) {
				splitCount++;
				splitfreqMap[action.size]++;

				auto &listB = allocatorBlocks[action.size * 2];
				auto it = find(listB.begin(), listB.end(), action.block);
				ASSERT_NE(it,listB.end())<< "block that was split was not in the ledger";
				listB.erase(it);
				auto &list = allocatorBlocks[action.size];
				auto it2 = find(list.begin(), list.end(), action.block);
				ASSERT_EQ(it2,list.end())<<"found block, that would be added from split, already there";
				auto it3 = find(list.begin(), list.end(), action.buddyBlock);
				ASSERT_EQ(it3,list.end())<<"found block, that would be added from split, already there";

				size_t adressDif = size_t(
						max(action.block, action.buddyBlock)
								- min(action.block, action.buddyBlock));
				ASSERT_EQ(adressDif, action.size);
//				cout << (void*) action.block << " - "
//						<< (void*) action.buddyBlock << endl;
//				cout << (void*) action.block << " -> "
//						<< (void*) buddyAdress(action.size - 1, action.block)
//						<< endl;
//				cout << (void*) action.buddyBlock << " -> "
//						<< (void*) buddyAdress(action.size - 1,
//								action.buddyBlock) << endl;

				//ASSERT_EQ(action.block,buddyAdress(action.size,action.buddyBlock))<<"Address of buddy block not matching";
				//ASSERT_EQ(action.buddyBlock,buddyAdress(action.size,action.block))<<"Address of buddy block not matching";

				list.push_back(action.block);
				list.push_back(action.buddyBlock);
			} else if (action.action == debugAction::combineBlocks) {
				combineCount++;
				combinefreqMap[action.size]++;

				auto &list2 = allocatorBlocks[action.size * 2];
				byteT *combinedBlock = min(action.block, action.buddyBlock);
				if (list2.size() > 0) {
					auto it = find(list2.begin(), list2.end(), combinedBlock);
					ASSERT_EQ(it,list2.end())<<"block that will be combined already there";
				}
				auto &list = allocatorBlocks[action.size];
				auto it2 = find(list.begin(), list.end(), action.block);
				ASSERT_NE(it2,list.end())<<"block that is to be combined is not in the allocator";
				list.erase(it2);
				auto it3 = find(list.begin(), list.end(), action.buddyBlock);
				ASSERT_NE(it3,list.end())<<"buddy block that is to be combined is not in the allocator";
				list.erase(it3);

				list2.push_back(combinedBlock);
			} else {
				// return block
				allocatorBlocks[action.size].pop_front();
			}
		}
	}

	byteT* getBlock(size_t size) {

		byteT *ret = allocatorBlocks[size].front();
		allocatorBlocks[size].pop_front();
		return ret;
	}
public:

	bool anyBlocks() {
		return usedBlockCount > 0;
	}

	std::pair<byteT*, size_t> randUsedBlock() {
		size_t randI = rand() % (usedBlockCount);
		for (auto &it : userBlocks) {
			if (randI >= it.second.size()) {
				randI -= it.second.size();
			} else {
				decltype(it.second)::iterator it2 = it.second.begin();
				while (randI > 0) {
					it2++;
					randI--;
				}

				return {*it2,it.first};
			}
		}
		return {0,0};
	}

	void testAlloc(byteT *adrs, size_t size) {
		doLog();
		size_t allocSize = max(size_t { 64 }, pow2(mylog2(size)));
		allocfreqMap[allocSize]++;
		ASSERT_EQ(debugLog.nextType(), debugAction::returnBlock);
		ASSERT_EQ(debugLog.actions.size(), 1);
		auto action = debugLog.next();
		ASSERT_TRUE(allocatorBlocks[action.size].size() > 0);
		byteT *expectedAdrs = getBlock(allocSize);
		ASSERT_EQ(adrs, expectedAdrs);
		ASSERT_EQ(adrs, action.block);
		ASSERT_EQ(allocSize, action.size);

		bool blockAlreadyInUse = false;
		for (auto &set : userBlocks) {
			if (set.second.find(adrs) != set.second.end()) {
				blockAlreadyInUse = true;
				break;
			}
		}
		ASSERT_FALSE(blockAlreadyInUse)<<"block that was allocated already in use";


		userBlocks[allocSize].insert(adrs);
		usedBlockCount++;
	}

	void testDealloc(byteT *adrs, size_t size) {
		deallocfreqMap[size]++;
		bool foundBlock = false;
		std::unordered_set<byteT*>::iterator it;
		for (auto &set : userBlocks) {
			it = set.second.find(adrs);
			if (it != set.second.end()) {
				ASSERT_EQ(set.first,size)<<"found block at a size different from the alloc";
				foundBlock = true;
				break;
			}
		}
		ASSERT_EQ(foundBlock, true); //<<"did not find block that will be deallocated in use";
		userBlocks[size].erase(it);
		usedBlockCount--;

		allocatorBlocks[size].push_back(adrs);
		doLog();
	}
};

template<template<size_t> typename policy>
struct testerAllocator {
	using allocT = buddyAllocatorTester<policy>;
	byteT mem[sizeof(allocT)];
	allocT &alloc = *reinterpret_cast<allocT*>(mem);

	testerAllocator() {
		new (mem) allocT;
	}
	~testerAllocator() {
		alloc.unmapPages();
	}

	byteT* allocate(size_t size) {
		return alloc.allocate(size);
	}

	void deallocate(byteT *ptr, size_t size) {
		alloc.deallocate(ptr, size);
	}
};

struct memChecker{
	std::unordered_map<byteT*,std::map<size_t,byteT>> mem;
	void registerPtr(byteT* ptr,size_t presize){
		size_t size = pow2(mylog2(presize));
		byteT checkValue = rand()%250+3;
		mem[ptr][size] = checkValue;
		memset(ptr,checkValue,size);
	}
	void testPtr(byteT* ptr,size_t size){
		byteT checkValue = mem[ptr][size];
		ASSERT_NE(checkValue,0);
		bool allGood = true;
		for(unsigned int i=0;i<size;i++){
			if(*(ptr+i) != checkValue){
				allGood = false;
				break;
			}
		}
		ASSERT_TRUE(allGood);
	}
};
