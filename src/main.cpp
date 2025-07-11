#include "tests/buddyAllocatorTests.hpp"
#include "tests/inAnonymousFileAllocatorTests.hpp"
#include "tests/inFileAllocatorTests.hpp"
#include "tests/objManagerTests.hpp"


int main(int argc, char **argv) {
    ::testing::InitGoogleTest( &argc, argv );
    return RUN_ALL_TESTS();
}
