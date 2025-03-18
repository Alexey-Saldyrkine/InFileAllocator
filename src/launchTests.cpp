#include <gtest/gtest.h>

#include "tests.hpp"
#include "benchmark.hpp"


int main(int argc, char** argv){


	::testing::InitGoogleTest(&argc,argv);
	return RUN_ALL_TESTS();
}
