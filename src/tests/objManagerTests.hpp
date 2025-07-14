#pragma once
#include "../persistentObjectManager.hpp"

byteT *adrs = reinterpret_cast<byteT*>(0x04fffffff000);

struct selfCheckT;

template<size_t>
struct fileAllocatorTestPolicy;

template<typename ... Ts>
using anonObjManager = anonymousObjectManager<fileAllocatorTestPolicy,size_t,Ts...>;

TEST(anonymousObjManager,basicUse) {
	for(int k=0;k<kCount;k++) {
		anonObjManager<int,bool,double,selfCheckT> manager(adrs);
		srand(k*13+21);
		int iValue = rand()%1000;
		bool bValue = rand()%2;
		double dValue = (rand()%5000) *0.01;
		selfCheckT sValue = selfCheckT {};
		int& i1 = manager.aquireConstruct<int>(1,iValue);
		bool& b1 = manager.aquireConstruct<bool>(2,bValue);
		double& d1 = manager.aquireConstruct<double>(3,dValue);
		selfCheckT& s1 = manager.aquireConstruct<selfCheckT>(4,sValue);

		ASSERT_EQ(i1,iValue);
		ASSERT_EQ(b1,bValue);
		ASSERT_EQ(d1,dValue);
		ASSERT_EQ(s1,sValue);
		ASSERT_TRUE(s1.check());

		int& i2 = manager.aquire<int>(1);
		bool& b2 = manager.aquire<bool>(2);
		double& d2 = manager.aquire<double>(3);
		selfCheckT& s2 = manager.aquire<selfCheckT>(4);

		ASSERT_EQ(i1,i2);
		ASSERT_EQ(b1,b2);
		ASSERT_EQ(d1,d2);
		ASSERT_EQ(s1,s2);

		ASSERT_EQ(&i1,&i2);
		ASSERT_EQ(&b1,&b2);
		ASSERT_EQ(&d1,&d2);
		ASSERT_EQ(&s1,&s2);
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

TEST(anonymousObjManager,vectorOftype) {
	for(int k=0;k<kCount;k++) {
		srand(k*14+22);
		anonObjManager<vecT<selfCheckT>> manager(adrs);
		auto managerPtr = manager.getManager();
		auto& vec1 = manager.aquireConstruct<vecT<selfCheckT>>(0,managerPtr);
		auto& vec2 = manager.aquireConstruct<vecT<selfCheckT>>(1,managerPtr);

		for(int i=0;i<2000;i++) {
			selfCheckT ck;
			vec1.push_back(ck);
			vec2.push_back(ck);
		}
		auto allGood = [&]() {
			for(int i=0;i<2000;i++) {
				if(!vec1[i].check() || !vec2[i].check() || vec1[i]!=vec2[i]) {
					return false;
				}
			}
			return true;
		};
		ASSERT_TRUE(allGood());
	}
}

using vecMatrix = vecT<vecT<selfCheckT>>;
TEST(anonymousObjManager,nestedVevOfVec) {
	for(int k=0;k<kCount;k++) {
		srand(k*16+24);
		anonObjManager<vecMatrix> manager(adrs);
		vecMatrix& mat = manager.aquireConstruct<vecMatrix>(0,manager);
		std::vector<std::vector<selfCheckT>> stdMat;

		for(int i=0;i<100;i++) {
			mat.emplace_back();
			stdMat.emplace_back();
			for(int j=0;j<100;j++) {
				selfCheckT ck;
				mat.back().push_back(ck);
				stdMat.back().push_back(ck);
			}
		}
		size_t c =0;
		auto allGood =[&]() {
			for(int i=0;i<100;i++) {
				for(int j =0;j<100;j++) {
					c++;
					if(!mat[i][j].check() || stdMat[i][j] != mat[i][j]) {
						return false;
					}
				}
			}
			return true;
		};
		ASSERT_TRUE(allGood());
		ASSERT_EQ(c,10000);
	}
}

using mapOfThings = mapT<vecT<listT<mapT<selfCheckT>>>>;

TEST(anonymousObjManager,mapOfvecOflistOfmapOftype) {
	for (int k = 0; k < kCount; k++) {
		srand(k*15+23);
		anonObjManager<mapOfThings> manager(adrs);
		auto managerPtr = manager.getManager();

		mapOfThings& obj = manager.aquireConstruct<mapOfThings>(0,managerPtr);
		mapOfThings& obj2 = manager.aquireConstruct<mapOfThings>(1,managerPtr);

		for (int i = 0; i < 10; i++) {
			auto &vec = obj[i];
			auto &vec2 = obj2[i];
			for (int j = 0; j < 10; j++) {
				vec.emplace_back();
				vec2.emplace_back();
				auto &list = vec.back();
				auto &list2 = vec.back();
				for (int l = 0; l < 10; l++) {
					list.emplace_back();
					list2.emplace_back();
					auto &map = list.back();
					auto &map2 = list.back();
					for (int p = 0; p < 10; p++) {
						selfCheckT ck;
						map[p] = ck;
						map2[p] = ck;
					}
				}
			}
		}
		size_t c = 0;
		auto allGood = [&]() {
			auto bit1 = obj.begin();
			for (auto &it1 : obj) {
				auto bit2 = bit1->second.begin();
				for (auto &it2 : it1.second) {
					auto bit3 = bit2->begin();
					for (auto &it3 : it2) {
						auto bit4 = bit3->begin();
						for (auto &it4 : it3) {
							c++;
							if (!it4.second.check() || !bit4->second.check() || it4.second != bit4->second) {
								return false;
							}
							bit4++;
						}
						bit3++;
					}
					bit2++;
				}
				bit1++;
			}
			return true;
		};

		ASSERT_TRUE(allGood());
		ASSERT_EQ(c, 10000);
	}
}

struct fileManager;

template<typename ... Ts>
using persObjManager = persistentObjectManager<fileAllocatorTestPolicy,size_t,Ts...>;

TEST(persistentObjeManager,basicUseNonPersistent) {
	for(int k=0;k<kCount;k++) {
		fileManager fm;
		persObjManager<int,bool,double,selfCheckT> manager(fm.fd,adrs);

		srand(k*19+22);
		int iValue = rand()%1000;
		bool bValue = rand()%2;
		double dValue = (rand()%5000) *0.01;
		selfCheckT sValue = selfCheckT {};
		int& i1 = manager.acquireConstruct<int>(1,iValue);
		bool& b1 = manager.acquireConstruct<bool>(2,bValue);
		double& d1 = manager.acquireConstruct<double>(3,dValue);
		selfCheckT& s1 = manager.acquireConstruct<selfCheckT>(4,sValue);

		ASSERT_EQ(i1,iValue);
		ASSERT_EQ(b1,bValue);
		ASSERT_EQ(d1,dValue);
		ASSERT_EQ(s1,sValue);
		ASSERT_TRUE(s1.check());

		int& i2 = manager.acquire<int>(1);
		bool& b2 = manager.acquire<bool>(2);
		double& d2 = manager.acquire<double>(3);
		selfCheckT& s2 = manager.acquire<selfCheckT>(4);

		ASSERT_EQ(i1,i2);
		ASSERT_EQ(b1,b2);
		ASSERT_EQ(d1,d2);
		ASSERT_EQ(s1,s2);

		ASSERT_EQ(&i1,&i2);
		ASSERT_EQ(&b1,&b2);
		ASSERT_EQ(&d1,&d2);
		ASSERT_EQ(&s1,&s2);

		manager.clearManager();
	}
}
TEST(persistentObjeManager,basicUse) {
	{
		fileManager fm;
		persObjManager<int,bool,double,selfCheckT> manager(fm.fd,adrs);
		manager.clearManager();
	}
	for(int k=0;k<10000;k++) {
		fileManager fm;
		persObjManager<int,bool,double,selfCheckT> manager(fm.fd,adrs);

		int& i1 = manager.acquireConstruct<int>(1,0);

		ASSERT_EQ(i1,k);
		i1++;
	}
	{
		fileManager fm;
		persObjManager<int,bool,double,selfCheckT> manager(fm.fd,adrs);
		manager.clearManager();
	}
}

using mapOfThings2 = inmapT<invecT<inlistT<inmapT<selfCheckT>>>>;

TEST(persistentObjeManager,mapOfvecOflistOfmapOftype) {
	for (int k = 0; k < kCount; k++) {
		srand(k*16+25);
		fileManager fm;
		persObjManager<mapOfThings2> manager(fm.fd,adrs);

		mapOfThings2& obj = manager.acquireConstruct<mapOfThings2>(0,manager);
		std::map<size_t,std::vector<std::list<std::map<size_t,selfCheckT>>>> obj2;

		for (int i = 0; i < 10; i++) {
			auto &vec = obj[i];
			auto &vec2 = obj2[i];
			for (int j = 0; j < 10; j++) {
				vec.emplace_back();
				vec2.emplace_back();
				auto &list = vec.back();
				auto &list2 = vec.back();
				for (int l = 0; l < 10; l++) {
					list.emplace_back();
					list2.emplace_back();
					auto &map = list.back();
					auto &map2 = list.back();
					for (int p = 0; p < 10; p++) {
						selfCheckT ck;
						map[p] = ck;
						map2[p] = ck;
					}
				}
			}
		}
		size_t c = 0;
		auto allGood = [&]() {
			auto bit1 = obj.begin();
			for (auto &it1 : obj) {
				auto bit2 = bit1->second.begin();
				for (auto &it2 : it1.second) {
					auto bit3 = bit2->begin();
					for (auto &it3 : it2) {
						auto bit4 = bit3->begin();
						for (auto &it4 : it3) {
							c++;
							if (!it4.second.check() || !bit4->second.check() || it4.second != bit4->second) {
								return false;
							}
							bit4++;
						}
						bit3++;
					}
					bit2++;
				}
				bit1++;
			}
			return true;
		};

		ASSERT_TRUE(allGood());
		ASSERT_EQ(c, 10000);

		manager.clearManager();
	}

}

struct conDesCounter {
	static inline int conCount = 0;
	static inline int desCount = 0;
	static inline int curCount = 0;
	char b = 'f';
	static bool check() {
		return conCount - desCount == curCount;
	}
	conDesCounter() {
		conCount++;
		curCount++;
	}
	conDesCounter(const conDesCounter &other) {
		conCount++;
		curCount++;
	}
	~conDesCounter() {
		if (b == 'f') {
			desCount++;
			curCount--;
		}
	}
};

TEST(persistentObjeManager,constructionDestructionCheck) {
	fileManager fm;
	persObjManager<invecT<conDesCounter>> manager(fm.fd,adrs);

	invecT<conDesCounter>& vec1 = manager.acquireConstruct<invecT<conDesCounter>>(1,manager);
	invecT<conDesCounter>& vec2 = manager.acquireConstruct<invecT<conDesCounter>>(2,manager);

	constexpr int l1 = 1000;
	constexpr int l2 = 3000;

	for(int i=0;i<l1;i++) {
		vec1.emplace_back();
	}
	for(int i=0;i<l2;i++) {
		vec2.emplace_back();
	}

	ASSERT_EQ(conDesCounter::curCount,l1+l2);
	ASSERT_TRUE(conDesCounter::check());

	manager.destroy(1);
	ASSERT_EQ(conDesCounter::curCount,l2);
	ASSERT_TRUE(conDesCounter::check());

	manager.destroy(2);
	ASSERT_EQ(conDesCounter::curCount,0);
	ASSERT_TRUE(conDesCounter::check());

	manager.clearManager();
}

