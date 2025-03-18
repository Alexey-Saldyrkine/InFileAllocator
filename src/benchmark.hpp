/*
 * benchmark.hpp
 *
 *  Created on: Feb 26, 2025
 *      Author: alexey
 */

#ifndef BENCHMARK_HPP_
#define BENCHMARK_HPP_

#include "InFileAllocator.hpp"
#include <x86intrin.h>
#include <queue>

struct benchmarkDatum {
	size_t allocAvrg;
	size_t allocWorst;
	size_t deallocAvrg;
	size_t deallocWorst;

};

template<typename Allocator>
struct BenchmarkSingleAllocator {
	static constexpr size_t repeatCount = 1000;
	Allocator alloc;
	benchmarkDatum allocateAlotOnce() {
		size_t avrgSumAlloc = 0;
		size_t worstAlloc = 0;
		size_t avrgSumDealloc = 0;
		size_t worstDealloc = 0;

		//size_t allocSizes[] = {8,16,32,64,128,256,[]}

		for (size_t i = 0; i < repeatCount; ++i) {
			size_t start = __rdtsc();
			char *ptr = alloc.allocate(4096 * 50);
			size_t time = __rdtsc() - start;
			avrgSumAlloc += time;
			worstAlloc = (worstAlloc > time ? worstAlloc : time);

			start = __rdtsc();
			alloc.deallocate(ptr, 4096 * 50);
			time = __rdtsc() - start;
			avrgSumDealloc += time;
			worstDealloc = (worstDealloc > time ? worstDealloc : time);
		}

		return {avrgSumAlloc/repeatCount,worstAlloc,avrgSumDealloc/repeatCount,worstDealloc};
	}

	benchmarkDatum allocateDiffSizes() {
		size_t avrgSumAlloc = 0;
		size_t worstAlloc = 0;
		size_t avrgSumDealloc = 0;
		size_t worstDealloc = 0;

		for (size_t i = 0; i < repeatCount; ++i) {
			size_t start = __rdtsc();
			char *ptr1 = alloc.allocate(4096 * 1);
			char *ptr2 = alloc.allocate(4096 * 10);
			char *ptr3 = alloc.allocate(4096 * 20);
			size_t time = __rdtsc() - start;
			avrgSumAlloc += time;
			worstAlloc = (worstAlloc > time ? worstAlloc : time);

			start = __rdtsc();
			alloc.deallocate(ptr1, 4096);
			alloc.deallocate(ptr2, 4096 * 10);
			alloc.deallocate(ptr3, 4096 * 20);
			time = __rdtsc() - start;
			avrgSumDealloc += time;
			worstDealloc = (worstDealloc > time ? worstDealloc : time);

		}

		return {avrgSumAlloc/repeatCount,worstAlloc,avrgSumDealloc/repeatCount,worstDealloc};
	}

	benchmarkDatum allocateSmallSizesNoKeep() {
		benchmarkDatum ret;
		const size_t sizes[10] = { 7, 15, 31, 63, 127, 255, 511, 1023, 2047,
				4095 };

		for (size_t i = 0; i < repeatCount; ++i) {
			size_t start = __rdtsc();
			size_t size = __rdtsc() % 10;
			char *ptr = alloc.allocate(sizes[size]);
			size_t time = __rdtsc() - start;
			ret.allocAvrg += time;
			ret.allocWorst = (ret.allocWorst > time ? ret.allocWorst : time);

			start = __rdtsc();
			alloc.deallocate(ptr, sizes[size]);
			time = __rdtsc() - start;
			ret.deallocAvrg += time;
			ret.deallocWorst = (
					ret.deallocWorst > time ? ret.deallocWorst : time);
		}

		ret.allocAvrg /= repeatCount;
		ret.deallocAvrg /= repeatCount;
		return ret;
	}

	benchmarkDatum allocateSmallSizesYesKeep() {
		benchmarkDatum ret;
		const size_t sizes[10] = { 7, 15, 31, 63, 127, 255, 511, 1023, 2047,
				4095 };

		std::queue<std::pair<size_t, char*>> allocs;

		for (size_t i = 0; i < repeatCount; ++i) {
			size_t start = __rdtsc();
			size_t size = __rdtsc() % 10;
			char *ptr = alloc.allocate(sizes[size]);
			size_t time = __rdtsc() - start;
			allocs.emplace(size, ptr);
			ret.allocAvrg += time;
			ret.allocWorst = (ret.allocWorst > time ? ret.allocWorst : time);
		}

		while (allocs.size()) {
			auto pair = allocs.front();
			allocs.pop();
			size_t start = __rdtsc();
			alloc.deallocate(pair.second, sizes[pair.first]);
			size_t time = __rdtsc() - start;
			ret.deallocAvrg += time;
			ret.deallocWorst = (
					ret.deallocWorst > time ? ret.deallocWorst : time);
		}

		ret.allocAvrg /= repeatCount;
		ret.deallocAvrg /= repeatCount;
		return ret;
	}

};

struct RAIIFD {
	int fd;
	RAIIFD(const char *path) {
		fd = open(path, O_CREAT | O_RDWR, 0777);
		if (fd == -1) {
			fprintf(stderr, "mmap [mapHeader] failed: %s\n", strerror(errno));
		}
	}
	~RAIIFD() {
		close(fd);
	}
	operator int() const {
		return fd;
	}
};

size_t percent(size_t def, size_t file) {
	return ((double) file / def) * 100;
}

struct BenchmarkAllocators {

	static void printHeader() {
		std::cout << "Name\tavrgAlloc\tworstAlloc\tavrgDealloc\tworstDealloc\n";
	}
	static constexpr const char *seperator = "\t\t";
	static void printDatum(const char *name, benchmarkDatum data) {

		std::cout << name << "\t" << data.allocAvrg << seperator
				<< data.allocWorst << seperator << data.deallocAvrg << seperator
				<< data.deallocWorst << "\n";
	}
	static void printPercentCompare(benchmarkDatum def, benchmarkDatum file) {
		std::cout << "comp:\t" << percent(def.allocAvrg, file.allocAvrg)
				<< seperator << percent(def.allocWorst, file.allocWorst)
				<< seperator << percent(def.deallocAvrg, file.deallocAvrg)
				<< seperator << percent(def.deallocWorst, file.deallocWorst)
				<< "\n";
	}
	static void compareResults(benchmarkDatum def, benchmarkDatum file) {
		printHeader();
		printDatum("def:", def);
		printDatum("file:", file);
		printPercentCompare(def, file);
	}

	static void test() {
		//RAIIFD fd("benchmarkAlloc.txt");

		RAIIFD fd("benchmarkAlloc.txt");
		void *ptr = (void*) 0x500000000000;
		size_t memsz = 4096 * 5000;
		inFileAllocator::detail::FileMemoryManagerHandler handler(fd, ptr,
				memsz);
		handler.setDefCstr();

		BenchmarkSingleAllocator<inFileAllocator::detail::fileAllocator<char>> fileAlloc;
		BenchmarkSingleAllocator<std::allocator<char>> defAlloc;

		std::cout << "allocateSmallSizesNoKeep\n";
		compareResults(defAlloc.allocateSmallSizesNoKeep(),
				fileAlloc.allocateSmallSizesNoKeep());
		handler.getManager()->reset();


		std::cout << "allocateSmallSizesYesKeep\n";
				compareResults(defAlloc.allocateSmallSizesYesKeep(),
						fileAlloc.allocateSmallSizesYesKeep());
				handler.getManager()->reset();



		std::cout << "\n\nallocateAlotOnce\n";
		compareResults(defAlloc.allocateAlotOnce(),
				fileAlloc.allocateAlotOnce());
		handler.getManager()->reset();

		std::cout << "allocateDiffSizes\n";
		compareResults(defAlloc.allocateDiffSizes(),
				fileAlloc.allocateDiffSizes());
		handler.getManager()->reset();
	}

};

#endif /* BENCHMARK_HPP_ */
