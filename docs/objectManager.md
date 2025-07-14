# object manager

There are two types of object managers: anonymousObjectManager and persistentObjectManager.

anonymousObjectManager constructs objects within an anonymous memory mapping.

persistentObjectManager constructs objects within the memory mapping of a file.


Both of these types have the same template parameters:
```cpp
template<template<size_t> typename policy, typename keyT, typename ... Ts>
```
Where:
- policy is a templated type that meets the requirements of being a policy for the buddy allocator. See documentation for details.

- keyT is the type that will be used as the key type.

- Ts... is a pack of types that the manager can acquire. 

These manager types are effectively a wrapper for an std::unordered_map, where the key type is KeyT and the value type is a std::variant<Ts...>. They also manage the memory mapping.

## Construction
The constructors of the managers look like this:
```cpp
anonymousObjectManager(void* ptr);

persistentObjectManager(int fd, void* ptr);
```
Where:
- ptr is the address that the mapping will begin at. Ptr should be an address one page less than an address 'adrs' that fulfills the following condition: adrs % maxAllocSize == 0.

- fd is the file descriptor for the file that will be used in the memory-mapping.

## managing objects
In the context of object managers, the term acquire refers to getting a reference to an object that is constructed in the memory-mapping.

There are two member functions to acquire objects: acquire and acquireConstruct.

- acquire<T>(keyT key) - returns a reference to the object, of type T, that key refers to. If there is no object referred to by the key or the object is a different type than T, then an error occurs. T must be one of the types in the Ts... pack.

-acquireConstruct<T>(keyT key, args...) - same as acquire, but if the key does not refer to any object, then an object of type T is constructed with the arguments args... forwarded.

To remove an object from the manager there is the member function destroy.
- destroy(keyT key) - will remove the object from the manager. The proper destructor for the object referred to by key will be called.

To clear and reset a manager there is the member function clearManager.
-clearManager() - destroys all of its managed objects and clears the mappings and file.
  