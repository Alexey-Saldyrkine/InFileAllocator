#pragma once
#include "testHelper.hpp"
#include<vector>
#include<scoped_allocator>
#include<unordered_map>

#include "../fileAllocators.hpp"
template<size_t size>
struct testPolicyA;

byteT* testingAdrs = reinterpret_cast<byteT*>(0x04fffffff000);
//byteT* testingAdrs = reinterpret_cast<byteT*>(0x050000000000);

template<typename F>
struct autoGaurd {
	F f;
	autoGaurd(F _f) :
			f(_f) {
	}
	~autoGaurd() {
		f();
	}
};

TEST(anonymousFileAllocator,basicUse) {
	for (int k = 0; k < kCount; k++) {
		auto *manager = createAnonymousFileAllocatorManager<testPolicyA>(testingAdrs);
		autoGaurd gaurd([]() {
			destroyAnonymousFileAllocatorManager<testPolicyA>(testingAdrs);
		});
		AnonymousFileAllocator<byteT, testPolicyA> alloc(manager);
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

template<size_t size>
struct fileAllocatorTestPolicy {
	static constexpr size_t minAllocSize = 64;
	static constexpr size_t maxAllocSize = 4096 * 8;
	static constexpr bool eanbleSpliting = true; //(size >= 9999 ? false : true);
	static constexpr bool enableCombining = true;
	static constexpr size_t allocBlockCount = 0;
};

TEST(anonymousFileAllocator,basicVectorUse) {
	auto *manager =
			createAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
	autoGaurd gaurd([]() {
		destroyAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
	});

	std::vector<int, AnonymousFileAllocator<int, fileAllocatorTestPolicy>> vec(
			manager);
	for (int i = 0; i < 8000; i++) {
		vec.push_back(i);
	}
	bool allGood = true;
	for (int i = 0; i < 8000; i++) {
		if (vec[i] != i) {
			allGood = false;
			break;
		}
	}
	ASSERT_TRUE(allGood);
}

struct selfCheckT {
	int a;
	int b;
	int c;

	selfCheckT() {
		a = rand() % 10000 + 10;
		b = rand() % 1000 + 10;
		c = a % b;
	}

	bool check() {
		return a % b == c;
	}

	friend bool operator!=(const selfCheckT &a, const selfCheckT &b) {
		return !(a.a == b.a && a.b == b.b && a.c == b.c);
	}
	friend bool operator==(const selfCheckT &a, const selfCheckT &b) {
		return (a.a == b.a && a.b == b.b && a.c == b.c);
	}

};

TEST(anonymousFileAllocator,vectorOfType) {
	using T = selfCheckT;
	for (int k = 0; k < kCount; k++) {
		auto *manager = createAnonymousFileAllocatorManager<
				fileAllocatorTestPolicy>(testingAdrs);
		autoGaurd gaurd([]() {
			destroyAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
		});
		srand(k * 8 + 16);
		std::vector<T, AnonymousFileAllocator<T, fileAllocatorTestPolicy>> vec(
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

TEST(anonymousFileAllocator,basicMap) {
	auto *manager =
			createAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
	autoGaurd gaurd([]() {
		destroyAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
	});

	std::map<size_t, size_t, std::less<size_t>,
			AnonymousFileAllocator<std::pair<const size_t, size_t>,
					fileAllocatorTestPolicy>> map(manager);

	for (unsigned int i = 0; i < 8000; i++) {
		map[i] = i;
	}
	bool allGood = true;
	for (unsigned int i = 0; i < 8000; i++) {
		if (map[i] != i) {
			allGood = false;
			break;
		}
	}
	ASSERT_TRUE(allGood);
}

TEST(anonymousFileAllocator,mapOfType) {
	for (int k = 0; k < kCount; k++) {
		auto *manager = createAnonymousFileAllocatorManager<
				fileAllocatorTestPolicy>(testingAdrs);
		autoGaurd gaurd([]() {
			destroyAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
		});

		std::map<size_t, selfCheckT, std::less<size_t>,
				AnonymousFileAllocator<std::pair<const size_t, selfCheckT>,
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

TEST(anonymousFileAllocator,unorderedMapOfType) {
	for (int k = 0; k < kCount; k++) {
		auto *manager = createAnonymousFileAllocatorManager<
				fileAllocatorTestPolicy>(testingAdrs);
		autoGaurd gaurd([]() {
			destroyAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
		});

		std::unordered_map<size_t, selfCheckT, std::hash<size_t>,
				std::equal_to<size_t>,
				AnonymousFileAllocator<std::pair<const size_t, selfCheckT>,
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
using allocT = AnonymousFileAllocator<T,fileAllocatorTestPolicy>;
template<typename T>
using scopedAllocT = std::scoped_allocator_adaptor<allocT<T>>;
template<typename T>
using vecT = std::vector<T, scopedAllocT<T>>;
template<typename T>
using mapT = std::map<size_t, T, std::less<size_t>,
scopedAllocT<std::pair<const size_t, T>>>;
template<typename T>
using listT = std::list<T,scopedAllocT<T>>;

TEST(anonymousFileAllocator,basicList) {
	auto *manager =
			createAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
	autoGaurd gaurd([]() {
		destroyAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
	});

	listT<int> list(manager);

	for (int i = 0; i < 4000; i++) {
		list.push_back(i);
	}
	bool allGood = true;
	auto it = list.begin();
	for (int i = 0; i < 4000; i++) {
		if (*it != i) {
			allGood = false;
			break;
		}
		it++;
	}

	ASSERT_TRUE(allGood);
}

TEST(anonymousFileAllocator,listOftype) {
	auto *manager =
			createAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
	autoGaurd gaurd([]() {
		destroyAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
	});

	listT<selfCheckT> list(manager);

	for (int i = 0; i < 4000; i++) {
		list.push_back(selfCheckT { });
	}
	bool allGood = true;
	auto it = list.begin();
	for (int i = 0; i < 4000; i++) {
		if (!it->check()) {
			allGood = false;
			break;
		}
		it++;
	}

	ASSERT_TRUE(allGood);
}

TEST(NestedAnonymousFileAllocator,vecOfvec) {
	for (int k = 0; k < kCount; k++) {
		auto *manager = createAnonymousFileAllocatorManager<
				fileAllocatorTestPolicy>(testingAdrs);
		autoGaurd gaurd([]() {
			destroyAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
		});

		vecT<vecT<selfCheckT>> vec(manager);
		std::vector<std::vector<selfCheckT>> stdVec;

		srand(k * 2 + 18);

		for (int i = 0; i < 100; i++) {
			vec.emplace_back();
			stdVec.emplace_back();
			for (int j = 0; j < 100; j++) {
				auto ck = selfCheckT { };
				vec.back().push_back(ck);
				stdVec.back().push_back(ck);
			}
		}
		auto allGood = [&]() {
			for (int i = 0; i < 100; i++) {
				for (int j = 0; j < 100; j++) {
					if (vec[i][j] != stdVec[i][j] || !vec[i][j].check()) {
						return false;
					}
				}
			}
			return true;
		};

		ASSERT_TRUE(allGood());

	}
}

TEST(NestedAnonymousFileAllocator,vecOfvecOfvec) {
	for (int k = 0; k < kCount; k++) {
		auto *manager = createAnonymousFileAllocatorManager<
				fileAllocatorTestPolicy>(testingAdrs);
		autoGaurd gaurd([]() {
			destroyAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
		});

		vecT<vecT<vecT<selfCheckT>>> vec(manager);
		std::vector<std::vector<std::vector<selfCheckT>>> stdVec;

		srand(k * 2 + 18);

		for (int i = 0; i < 10; i++) {
			vec.emplace_back();
			stdVec.emplace_back();
			for (int j = 0; j < 10; j++) {
				vec[i].emplace_back();
				stdVec[i].emplace_back();
				for (int l = 0; l < 100; l++) {
					auto ck = selfCheckT { };
					vec[i][j].push_back(ck);
					stdVec[i][j].push_back(ck);
				}
			}
		}
		auto allGood = [&]() {
			for (int i = 0; i < 10; i++) {
				for (int j = 0; j < 10; j++) {
					for (int l = 0; l < 100; l++) {
						if (vec[i][j][l] != stdVec[i][j][l]
								|| !vec[i][j][l].check()) {
							return false;
						}
					}
				}
			}
			return true;
		};

		ASSERT_TRUE(allGood());

	}
}

TEST(NestedAnonymousFileAllocator,mapOfVector) {
	for (int k = 0; k < kCount; k++) {
		auto *manager = createAnonymousFileAllocatorManager<
				fileAllocatorTestPolicy>(testingAdrs);
		autoGaurd gaurd([]() {
			destroyAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
		});

		mapT<vecT<selfCheckT>> map(manager);
		std::map<size_t, std::vector<selfCheckT>> stdMap;
		srand(k * 3 + 18);
		for (unsigned int i = 0; i < 2000; i++) {
			size_t n = rand() % 30;
			auto ck = selfCheckT { };
			map[n].push_back(ck);
			stdMap[n].push_back(ck);
		}

		size_t c = 0;
		auto allGood = [&]() {
			for (auto &it : map) {
				auto &other = stdMap[it.first];
				for (unsigned int i = 0; i < it.second.size(); i++) {
					c++;
					if (!it.second[i].check() || it.second[i] != other[i]) {
						return false;
					}
				}
			}
			return true;
		};

		ASSERT_TRUE(allGood());
		ASSERT_TRUE(c == 2000);
	}
}

TEST(NestedAnonymousFileAllocator,vectorOfMap) {
	for (int k = 0; k < kCount; k++) {
		auto *manager = createAnonymousFileAllocatorManager<
				fileAllocatorTestPolicy>(testingAdrs);
		autoGaurd gaurd([]() {
			destroyAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
		});

		vecT<mapT<selfCheckT>> vec(manager);
		std::vector<std::map<size_t, selfCheckT>> stdVec;
		srand(k * 3 + 19);
		for (unsigned int i = 0; i < 1000; i++) {
			if (vec.size() > 0 && rand() % 4 != 0) {
				size_t n = rand() % vec.size();
				auto ck = selfCheckT { };
				auto &mp = vec[n];
				mp[mp.size()] = ck;
				stdVec[n][stdVec[n].size()] = ck;
			} else {
				vec.emplace_back();
				stdVec.emplace_back();
				auto &mp = vec.back();
				auto ck = selfCheckT { };
				mp[mp.size()] = ck;
				stdVec.back()[stdVec.back().size()] = ck;
			}
		}

		size_t c = 0;
		auto allGood = [&]() {
			for (unsigned int i = 0; i < vec.size(); i++) {
				for (auto &it2 : vec[i]) {
					c++;
					if (!it2.second.check()
							|| it2.second != vec[i][it2.first]) {
						return false;
					}
				}
			}
			return true;
		};

		ASSERT_TRUE(allGood());
		ASSERT_EQ(c, 1000);
	}
}

TEST(NestedAnonymousFileAllocator,mapOfmap) {
	for (int k = 0; k < kCount; k++) {
		auto *manager = createAnonymousFileAllocatorManager<
				fileAllocatorTestPolicy>(testingAdrs);
		autoGaurd gaurd([]() {
			destroyAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
		});

		mapT<mapT<selfCheckT>> map(manager);
		std::map<size_t, std::map<size_t, selfCheckT>> stdMap;
		srand(k * 4 + 20);

		for (int i = 0; i < 2000; i++) {
			if (map.size() > 0 && rand() % 4 != 0) {
				int n = rand() % map.size();
				int n2 = map[n].size();
				auto ck = selfCheckT { };
				map[n][n2] = ck;
				stdMap[n][n2] = ck;
			} else {
				int n = map.size();
				int n2 = map[n].size();
				auto ck = selfCheckT { };
				map[n][n2] = ck;
				stdMap[n][n2] = ck;
			}
		}

		size_t c = 0;
		auto allGood = [&]() {
			for (auto &it1 : map) {
				auto &other = stdMap[it1.first];
				for (auto &it2 : it1.second) {
					c++;
					if (!it2.second.check() || it2.second != other[it2.first]) {
						return false;
					}
				}
			}
			return true;
		};

		ASSERT_TRUE(allGood());
		ASSERT_EQ(c, 2000);

	}
}

TEST(NestedAnonymousFileAllocator,mapOfvecOflistOfmap) {
	for (int k = 0; k < kCount; k++) {
		auto *manager = createAnonymousFileAllocatorManager<
				fileAllocatorTestPolicy>(testingAdrs);
		autoGaurd gaurd([]() {
			destroyAnonymousFileAllocatorManager<fileAllocatorTestPolicy>(testingAdrs);
		});
		srand(k * 5 + 21);
		mapT<vecT<listT<mapT<selfCheckT>>>> obj(manager);
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

