#pragma once
#include "testHelper.hpp"
#include <fcntl.h>
#include "../fileAllocators.hpp"
#include <unordered_map>

template<typename F>
struct autoGaurd;

template<size_t>
struct fileAllocatorTestPolicy;

struct selfCheckT;

struct fileManager {
	int fd;

	fileManager() {
		fd = open("testFile.bin", O_RDWR | O_CREAT, 00777);
		if (fd < 0) {
			perror("error opening file");
			exit(1);
		}
	}
	~fileManager() {
		close(fd);
	}

};
template<size_t size>
struct fileAllocatorTestPolicyDebug:fileAllocatorTestPolicy<size>{
	static constexpr bool enableDebug = true;
};

TEST(inFileAllocator,basicUse) {
	for (int k = 0; k < 1; k++) {
		fileManager fm;
		auto *manager = createInFileAllocatorManager<fileAllocatorTestPolicyDebug>(
				fm.fd,testingAdrs);
		manager->clearFile();
		autoGaurd gaurd([]() {
			destroyInFileAllocatorManager<fileAllocatorTestPolicyDebug>(testingAdrs);
		});
		inFileAllocator<byteT, fileAllocatorTestPolicyDebug> alloc(manager);
		debugLog.clear();
		buddyAllocatorBlockLog ledger;
		memChecker memoryCheck;
		srand(k * 8 + 16);
		for (int i = 0; i < 1000; i++) {
			if (ledger.anyBlocks() && rand() % 4 == 3) {
				// deallocate
				auto block = ledger.randUsedBlock();
				memoryCheck.testPtr(block.first, block.second);
				alloc.deallocate(block.first, block.second);
				ledger.testDealloc(block.first, block.second);
			} else {
				// allocate
				size_t size = max(rand() % 9999, 64);
				byteT *ret = alloc.allocate(size);
				memoryCheck.registerPtr(ret, size);
				ledger.testAlloc(ret, size);
			}
		}
		//ledger.printData();
		while (ledger.anyBlocks()) {
			auto block = ledger.randUsedBlock();
			memoryCheck.testPtr(block.first, block.second);
			alloc.deallocate(block.first, block.second);
			ledger.testDealloc(block.first, block.second);
		}
		//ledger.printData();

	}
}

TEST(inFileAllocator,vectorOfType) {
	using T = selfCheckT;
	for (int k = 0; k < kCount; k++) {
		fileManager fm;
		auto *manager = createInFileAllocatorManager<fileAllocatorTestPolicy>(
				fm.fd,testingAdrs);
		manager->clearFile();
		autoGaurd gaurd([]() {
			destroyInFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
		});
		srand(k * 8 + 16);
		std::vector<T, inFileAllocator<T, fileAllocatorTestPolicy>> vec(
				manager);

		for (int i = 0; i < 999; i++) {
			vec.emplace_back();
		}
		bool allGood = true;
		for (auto &obj : vec) {
			if (!obj.check()) {
				allGood = false;
				break;
			}
		}
		ASSERT_TRUE(allGood);

	}

}

TEST(inFileAllocator,mapOfType) {
	for (int k = 0; k < kCount; k++) {
		fileManager fm;
		auto *manager = createInFileAllocatorManager<fileAllocatorTestPolicy>(
				fm.fd,testingAdrs);
		manager->clearFile();
		autoGaurd gaurd([]() {
			destroyInFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
		});

		std::map<size_t, selfCheckT, std::less<size_t>,
				inFileAllocator<std::pair<const size_t, selfCheckT>,
						fileAllocatorTestPolicy>> map(manager);
		srand(k * 9 + 17);
		for (unsigned int i = 0; i < 2000; i++) {
			map[i] = selfCheckT();
		}
		bool allGood = true;
		for (unsigned int i = 0; i < 2000; i++) {
			if (!map[i].check()) {
				allGood = false;
				break;
			}
		}
		ASSERT_TRUE(allGood);
	}
}

TEST(inFileAllocator,unorderedMapOfType) {
	for (int k = 0; k < kCount; k++) {
		fileManager fm;
		auto *manager = createInFileAllocatorManager<fileAllocatorTestPolicy>(
				fm.fd,testingAdrs);
		manager->clearFile();
		autoGaurd gaurd([]() {
			destroyInFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
		});

		std::unordered_map<size_t, selfCheckT, std::hash<size_t>,
				std::equal_to<size_t>,
				inFileAllocator<std::pair<const size_t, selfCheckT>,
						fileAllocatorTestPolicy>> map(manager);
		srand(k * 9 + 17);
		for (unsigned int i = 0; i < 2000; i++) {
			map[i] = selfCheckT();
		}
		bool allGood = true;
		for (unsigned int i = 0; i < 2000; i++) {
			if (!map[i].check()) {
				allGood = false;
				break;
			}
		}
		ASSERT_TRUE(allGood);
	}
}

template<typename T>
using inallocT = inFileAllocator<T,fileAllocatorTestPolicy>;
template<typename T>
using inscopedAllocT = std::scoped_allocator_adaptor<inallocT<T>>;
template<typename T>
using invecT = std::vector<T, inscopedAllocT<T>>;
template<typename T>
using inmapT = std::map<size_t, T, std::less<size_t>,
inscopedAllocT<std::pair<const size_t, T>>>;
template<typename T>
using inlistT = std::list<T,inscopedAllocT<T>>;

TEST(inFileAllocator,mapOfvecOflistOfmap) {
	for (int k = 0; k < kCount; k++) {
		fileManager fm;
		auto *manager = createInFileAllocatorManager<fileAllocatorTestPolicy>(
				fm.fd,testingAdrs);
		manager->clearFile();
		autoGaurd gaurd([]() {
			destroyInFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
		});
		srand(k * 5 + 21);
		inmapT < invecT < inlistT<inmapT<selfCheckT>> >> obj(manager);
		for (int i = 0; i < 10; i++) {
			auto &vec = obj[i];
			for (int j = 0; j < 10; j++) {
				vec.emplace_back();
				auto &list = vec.back();
				for (int l = 0; l < 10; l++) {
					list.emplace_back();
					auto &map = list.back();
					for (int p = 0; p < 10; p++) {
						map[p] = selfCheckT { };
					}
				}
			}
		}

		size_t c = 0;
		auto allGood = [&]() {
			for (auto &it1 : obj) {
				for (auto &it2 : it1.second) {
					for (auto &it3 : it2) {
						for (auto &it4 : it3) {
							c++;
							if (!it4.second.check()) {
								return false;
							}
						}
					}
				}
			}
			return true;
		};

		ASSERT_TRUE(allGood());
		ASSERT_EQ(c, 10000);
	}
}

