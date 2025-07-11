#pragma once
#include "testHelper.hpp"

template<size_t size>
struct testPolicyA {
	static constexpr size_t minAllocSize = 64;
	static constexpr size_t maxAllocSize = 4096 * 4;
	static constexpr bool eanbleSpliting = true; //(size >= 9999 ? false : true);
	static constexpr bool enableCombining = true;
	static constexpr size_t allocBlockCount = 0;
	static constexpr bool enableDebug = true;
};

constexpr int kCount = 30;

TEST(buddyAllocator,splitBlockTest) {
	for (int k = 0; k < kCount; k++) {
		testerAllocator<testPolicyA> alloc;
		debugLog.clear();
		buddyAllocatorBlockLog ledger;
		memChecker memoryCheck;
		srand(k * 5 + 13);
		for (int i = 0; i < 1000; i++) {
			if (ledger.anyBlocks() && rand() % 4 == 0) {
				// deallocate
				auto block = ledger.randUsedBlock();
				memoryCheck.testPtr(block.first,block.second);
				alloc.deallocate(block.first, block.second);
				ledger.testDealloc(block.first, block.second);
			} else {
				// allocate
				size_t size = max(rand() % 9999, 64);
				byteT *ret = alloc.allocate(size);
				memoryCheck.registerPtr(ret,size);
				ledger.testAlloc(ret, size);
			}
		}

		while (ledger.anyBlocks()) {
			auto block = ledger.randUsedBlock();
			memoryCheck.testPtr(block.first,block.second);
			alloc.deallocate(block.first, block.second);
			ledger.testDealloc(block.first, block.second);
		}
		//ledger.printData();
	}

}

TEST(buddyAllocator,combineBlockTest) {
	for (int k = 0; k < kCount; k++) {
		testerAllocator<testPolicyA> alloc;
		debugLog.clear();
		buddyAllocatorBlockLog ledger;
		memChecker memoryCheck;
		srand(k * 6 + 14);
		for (int i = 0; i < 1000; i++) {

			// allocate
			size_t size = 64;
			byteT *ret = alloc.allocate(size);

			memoryCheck.registerPtr(ret,64);
			ledger.testAlloc(ret, size);

		}
		//ledger.printData();
		while (ledger.anyBlocks()) {
			auto block = ledger.randUsedBlock();
			memoryCheck.testPtr(block.first,block.second);
			alloc.deallocate(block.first, block.second);
			ledger.testDealloc(block.first, block.second);
		}

	}

}

template<size_t size>
struct testPolicyB {
	static constexpr size_t minAllocSize = 64;
	static constexpr size_t maxAllocSize = 4096 * 4;
	static constexpr bool eanbleSpliting = (
			size == 4096 * 4 || size == 4096 || size == 1024 ? false : true);
	static constexpr bool enableCombining = true;
	static constexpr size_t allocBlockCount = 0;
	static constexpr bool enableDebug = true;
};

TEST(buddyAllocator,disableSplittingAtLevels) {
	for (int k = 0; k < kCount; k++) {
		testerAllocator<testPolicyB> alloc;
		debugLog.clear();
		buddyAllocatorBlockLog ledger;
		memChecker memoryCheck;
		srand(k * 7 + 15);
		for (int i = 0; i < 1000; i++) {
			if (ledger.anyBlocks() && rand() % 4 == 0) {
				auto block = ledger.randUsedBlock();
				memoryCheck.testPtr(block.first,block.second);
				alloc.deallocate(block.first, block.second);
				ledger.testDealloc(block.first, block.second);
			} else {
				size_t size = max(rand() % 9999, 64);
				byteT *ptr = alloc.allocate(size);
				memoryCheck.registerPtr(ptr,size);
				ledger.testAlloc(ptr, size);
			}

		}

		while (ledger.anyBlocks()) {
			auto block = ledger.randUsedBlock();
			memoryCheck.testPtr(block.first,block.second);
			alloc.deallocate(block.first, block.second);
			ledger.testDealloc(block.first, block.second);

		}
		ASSERT_EQ(ledger.blockFreqMap[64],0);
		ASSERT_EQ(ledger.blockFreqMap[128],0);
		ASSERT_EQ(ledger.blockFreqMap[256],0);
		ASSERT_EQ(ledger.blockFreqMap[512],0);
		ASSERT_NE(ledger.blockFreqMap[1024],0);
		ASSERT_EQ(ledger.blockFreqMap[2048],0);
		ASSERT_NE(ledger.blockFreqMap[4096],0);
		ASSERT_EQ(ledger.blockFreqMap[4096*2],0);
		ASSERT_NE(ledger.blockFreqMap[4096*4],0);
		//ledger.printData();
	}
}


