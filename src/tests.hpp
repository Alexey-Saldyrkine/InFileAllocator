#include <gtest/gtest.h>
#include "inFileObjectManager.hpp"

#pragma once

namespace {
using namespace inFileAllocator::detail;

TEST(HelperFuncs,pow2) {
	ASSERT_EQ(pow2<0>, 1ul);
	ASSERT_EQ(pow2<1>, 2ul);
	ASSERT_EQ(pow2<2>, 4ul);
	ASSERT_EQ(pow2<10>, 1024ul);
	ASSERT_EQ(pow2<63>, 9223372036854775808ul);
	ASSERT_EQ(pow2 < 63 > +(pow2 < 63 > -1), 18446744073709551615ul);
}

TEST(HelperFuncs,isPowerOf2) {
	ASSERT_EQ(isPowerOf2<0>, 0);
	ASSERT_EQ(isPowerOf2<1>, 1);
	ASSERT_EQ(isPowerOf2<2>, 1);
	ASSERT_EQ(isPowerOf2<3>, 0);
	ASSERT_EQ(isPowerOf2<4>, 1);
	ASSERT_EQ(isPowerOf2<5>, 0);
	ASSERT_EQ(isPowerOf2<9223372036854775808ul>, 1);
	ASSERT_EQ(isPowerOf2<9223372036854775807ul>, 0);
	ASSERT_EQ(isPowerOf2<18446744073709551615ul>, 0);
}

TEST(HelperFuncs,sizeToPowIndex) {
	ASSERT_EQ(sizeToPowIndex<0>, 0u);
	ASSERT_EQ(sizeToPowIndex<1>, 1u);
	ASSERT_EQ(sizeToPowIndex<2>, 2u);
	ASSERT_EQ(sizeToPowIndex<3>, 2u);
	ASSERT_EQ(sizeToPowIndex<4>, 3u);
	ASSERT_EQ(sizeToPowIndex<5>, 3u);
	ASSERT_EQ(sizeToPowIndex<8>, 4u);
	ASSERT_EQ(sizeToPowIndex<9>, 4u);
	ASSERT_EQ(sizeToPowIndex<16>, 5u);
	ASSERT_EQ(sizeToPowIndex<1023>, 10u);
	ASSERT_EQ(sizeToPowIndex<1024>, 11u);
	ASSERT_EQ(sizeToPowIndex<9223372036854775808ul>, 64u);
	ASSERT_EQ(sizeToPowIndex<9223372036854775807ul>, 63u);
	ASSERT_EQ(sizeToPowIndex<18446744073709551615ul>, 64u);
}

TEST(MemBlock,size) {
	ASSERT_EQ(sizeof(MemBlock<32> ), 32u);
	ASSERT_EQ(sizeof(MemBlock<64> ), 64u);
	ASSERT_EQ(sizeof(MemBlock<4096> ), 4096u);
}

TEST(MemBlockStoragePage,elements) {
	ASSERT_EQ(MemBlockStoragePage<4096>::pageCount, 1u);
	ASSERT_EQ(MemBlockStoragePage<2 * 4096>::pageCount, 2u);
	ASSERT_EQ(MemBlockStoragePage<32>::pageCount, 1u);
	ASSERT_EQ(MemBlockStoragePage<32>::blockCount, pageSize / 32u);
}

TEST(BuddyBlock,Adress) {
	ASSERT_EQ(
			UnusedMemBlock<32768>::interBuddyAdress(
					reinterpret_cast<UnusedMemBlock<32768>*>(0x500000001000)),
			reinterpret_cast<MemBlock<32768>*>(0x500000009000));
}

void testSizeToIndexRTHelper(size_t size, unsigned int index) {
	ASSERT_EQ(sizeToIndex(size), index);
	ASSERT_TRUE(size <= 2ul << (index + IndexOffset));
}

TEST(SizeToIndex,runTime) {
	testSizeToIndexRTHelper(1, 0);
	testSizeToIndexRTHelper(8, 0);
	testSizeToIndexRTHelper(31, 0);
	testSizeToIndexRTHelper(32, 1);
	testSizeToIndexRTHelper(63, 1);
	testSizeToIndexRTHelper(64, 2);
	testSizeToIndexRTHelper(127, 2);
	testSizeToIndexRTHelper(128, 3);

}

TEST(objectManager,simpleTypes) {
	int fd = open("testFile.txt", O_CREAT | O_RDWR, 0777);
	if (fd == -1) {
		fprintf(stderr, "mmap [mapHeader] failed: %s\n", strerror(errno));
	}
	ASSERT_NE(fd, -1);
	void *ptr = (void*) 0x500000000000;
	size_t memsz = 4096 * 32;
	objectManager manager(fd, ptr, memsz);
	manager.resetFile();

	bool &bool1 = manager.aquire<bool>(0, false);
	bool1 = !bool1;
	int &int1 = manager.aquire<int>(1, 6);
	if (bool1) {
		EXPECT_EQ(int1, 6);
		int1 = 9;
	} else {
		EXPECT_EQ(int1, 9);
		int1 = 6;
	}
	float &float1 = manager.aquire<float>(2, 1.7);
	if (bool1) {
		EXPECT_TRUE(float1 < 2);
		float1 = 2.7;
	} else {
		EXPECT_TRUE(float1 > 2);
		float1 = 1.7;
	}
	double &double1 = manager.aquire<double>(3, 4.2);
	if (bool1) {
		EXPECT_EQ(double1, 4.2);
		double1 = 2.4;
	} else {
		EXPECT_EQ(double1, 2.4);
		double1 = 4.2;
	}
	close(fd);
}

struct TesterType {
	static inline size_t count = 0;
	static inline size_t snapShotCount;
	static inline char *minptr;
	static inline char *maxptr;

	static void setup(void *ptr, size_t size) {
		minptr = reinterpret_cast<char*>(ptr) + pageSize;
		maxptr = minptr + size;
	}

	size_t n;
	TesterType() {
		n = ++count;
	}
	TesterType(const TesterType&) {
		n = ++count;
	}
	TesterType(const TesterType&&) {
		n = ++count;
	}
	~TesterType() {
		count--;
	}

	void testSelfAddress() {
		ASSERT_TRUE(this >= reinterpret_cast<TesterType*>(minptr));
		ASSERT_TRUE(this <= reinterpret_cast<TesterType*>(maxptr));
	}

	static void snapShotCounter() {
		snapShotCount = count;
	}

	static size_t snapShotDiff() {
		return count - snapShotCount;
	}
};

bool testAdrs(char *adrs) {
	return ((char*) 0x500000000000 + pageSize <= adrs)
			&& ((char*) 0x500000000000 + pageSize * 11);
}

struct autoFd {
	int fd;
	autoFd(const char *path) {
		fd = open(path, O_CREAT | O_RDWR, 0777);
		if (fd == -1) {
			fprintf(stderr, "mmap [mapHeader] failed: %s\n", strerror(errno));
		}
	}
	~autoFd() {
		close(fd);
	}
	operator int() const {
		return fd;
	}
};

TEST(allocator,basicAlloc) {
	autoFd fd("testFile.txt");
	ASSERT_NE(fd, -1);
	void *ptr = (void*) 0x500000000000;
	size_t memsz = 4096 * 32;
	TesterType::setup(ptr, memsz);

	FileMemoryManagerHandler handler(fd, ptr, memsz);
	handler.getManager()->reset();
	fileAllocator<char> alloc(handler.getManager());

	char *alc1 = alloc.allocate(1);
	testAdrs(alc1);
	alloc.deallocate(alc1, 1);

	char *alc2 = alloc.allocate(15);
	testAdrs(alc1);
	alloc.deallocate(alc2, 15);

	EXPECT_THROW(
			{ try{ alloc.allocate(100*100*100); }catch(const std::runtime_error e){ EXPECT_STRCASEEQ("out of mem, remaning mem: 65536, requested mem: 1048576",e.what()); throw; } },
			std::runtime_error);

}

TEST(objectManager,simpleVec) {
	autoFd fd("testFile.txt");
	ASSERT_NE(fd, -1);
	void *ptr = (void*) 0x500000000000;
	size_t memsz = 4096 * 32;
	TesterType::setup(ptr, memsz);
	objectManager manager(fd, ptr, memsz);
	manager.resetFile();

	using vecT = std::vector<TesterType,fileAllocator<int>>;

	vecT &vec1 = manager.aquire<vecT>(0);
	TesterType::count = vec1.size();

	TesterType::snapShotCounter();
	for (int i = 0; i < 5; ++i) {
		vec1.emplace_back();
	}
	ASSERT_EQ(5ul, TesterType::snapShotDiff());

	TesterType::snapShotCounter();
	for (int i = 0; i < 5; ++i) {
		vec1.emplace_back();
	}
	ASSERT_EQ(5ul, TesterType::snapShotDiff());

	for (auto &v : vec1) {
		v.testSelfAddress();
	}

}

TEST(objectManager,simpleVecStressTest) {
	for (int i = 0; i < 10000; ++i) {
		autoFd fd("testFile.txt");
		ASSERT_NE(fd, -1);
		void *ptr = (void*) 0x500000000000;
		size_t memsz = 4096 * 32;
		TesterType::setup(ptr, memsz);
		objectManager manager(fd, ptr, memsz);
		manager.resetFile();

		using vecT = std::vector<TesterType,fileAllocator<int>>;

		vecT &vec1 = manager.aquire<vecT>(0);
		TesterType::count = vec1.size();

		TesterType::snapShotCounter();
		for (int i = 0; i < 100; ++i) {
			vec1.emplace_back();
		}
		ASSERT_EQ(100ul, TesterType::snapShotDiff());

		TesterType::snapShotCounter();
		for (int i = 0; i < 100; ++i) {
			vec1.emplace_back();
		}
		ASSERT_EQ(100ul, TesterType::snapShotDiff());

		for (auto &v : vec1) {
			v.testSelfAddress();
		}

		if (vec1.size() > 1000) {
			vec1.clear();
			ASSERT_EQ(TesterType::count, 0ul);
		}
	}

}

TEST(objectManager,compoundVec) {
	using vecT = std::vector<TesterType,fileAllocator<int>>;
	using vec2T = std::vector<vecT,fileAllocator<vecT>>;

	autoFd fd("testFile.txt");
	ASSERT_NE(fd, -1);
	void *ptr = (void*) 0x500000000000;
	size_t memsz = 4096 * 32;
	TesterType::setup(ptr, memsz);
	objectManager manager(fd, ptr, memsz);
	manager.resetFile();

	vec2T &vec1 = manager.aquire<vec2T>(0);

	TesterType::snapShotCounter();
	for (int j = 0; j < 5; j++) {
		auto &ref = vec1.emplace_back();
		for (int i = 0; i < 5; ++i) {
			ref.push_back(TesterType());
		}
	}
	ASSERT_EQ(25ul, TesterType::snapShotDiff());

}


}
