# allocator policy
All object managers and allocators require a policy template type.

A policy type is a special template type that has all the static constexpr members that the buddy allocator requires.

A policy type is a template type that accepts a size_t as its template parameter.

During explicit instantiation, the size_t parameter will always be a power of 2, inclusively between minAllocSize and maxAllocSize.

The default policy looks as follows:
```cpp
	template<size_t>
	struct defaultPolicies {
	static constexpr size_t minAllocSize = 64;
	static constexpr size_t maxAllocSize = 4096 * 4;
	static constexpr bool eanbleSpliting = true;
	static constexpr bool enableCombining = true;
	static constexpr size_t allocBlockCount = 0;

};
```
The required members:

### minAllocSize
minAllocSize - determines the minimum size of a memory block that the allocator can allocate. If a number of bytes less than minAllocSize is requested, the allocator returns a block of bytes of the size of minAllocSize. minAllocSize must be greater than or equal to 64, and be a power of 2, or else a compilation will occur.

### maxAllocSize
maxAllocSize - determines the maximum size that a single allocation can be. If more bytes than maxAllocSize are requested in a single allocation, an error will occur. You can allocate maxAllocSize or fewer bytes any number of times, just not in one allocation. maxAllocSize must be greater than or equal to minAllocSize and be a power of 2, else a compilation will occur.

__Note:__ minAllocSize and maxAllocSize should not change with size, though no error will occur if they do, as only policy<0>:: minAllocSize and policy<0>:: maxAllocSize are used during compilation.

The following members are allowed to be different depending on the template parameter 'Size'.

### enableSplitting
enableSplitting - determines whether, during the allocation of a block of memory of 'Size' bytes, a block of 'Size'*2 bytes can be split into two blocks of 'Size' bytes and one of them returned as the allocated block. 

### enableCombining
enableCombining - determines if, during a deallocation, the newly freed block tries to combine with its buddy.

### allocBlockCount 
allocBlockCount - determines how many blocks of 'Size' are allocated at once. If allocBlockCount equals 0, then allocBlockCount will be the maximum number of 'Size' blocks that can fit on a memory page. If 'Size' is greater than a page, then allocBlockCount is 1.