#InFileAllocator

This repository aims to provide an easy way to manage objects constructed within a memory mapped file.

Objects that exist within a memory mapped file are not tied to the lifetime of the heap or stack. This allows them to persist beyond a programs lifetime, as with an unexpected crash, and keep themselves and their data intact for the next execution of the same or any other program. So long as all allocations are performed within the mapped area, and the file is mapped to the same address every time, all pointers and data will remain valid.

For this the repository provides a couple of useful tools:
- "persistentObjectManager", a manager that will map and unmap files, construct and destroy object within a file
- "inFileAllocator", an STL compatible allocator that will automatically allocated to a file.

A swell as mirror version of those types for allocating to an anonymous file instead of a real file.
These will not persist beyond a process lifetime, but can be used as an alternative to allocating on heap, or as a way to manage shared memory between processes.

They are:
- "anonymousObjectmanager"
- "anonymousFileAllocator"

For the management of allocated memory a buddy allocator is provided. Which has customizable policy settings to configure the allocator.
Any other type of allocator can be used if it can expand and map a file on demand.
  
For details see docs.

Example:
Say you have a map<size_t,dataType> that is used as a small internal database for a process, and you want the database to be automatically loaded and saved to a file.

We can use the persistentObjectManager to do this easily

'''Cpp
//setup
struct dataType; // arbitrary type that we want to map to.

Struct myPolicy{
...
}; // the policy for the allocator, you can use the default one instead.

template<typename T>
using AllocT = inFileAllocator<T,myPolicy>;
//the allocator that the map will use.
//if dataType allocates memory of its own, you would wrap inFileAllocator 
//in a std::scoped_allocator_adapter.

using mapAllocT = AllocT<std::pair<const size_t, dataType>>;

using dataBaseT = std::map<size_t,dataType,std::less<size_t>,mapAllocT>;
// type of the database, can be any container that takes an STL allocator

using managerT = persistentObjectManager<myPolicy,size_t,dataType>;
// the type of the manager;

int main(){

int fd = getFD(...); // can be any function or way that obtains a file descriptor to the underline file.

managerT manager(fd,adrs); //will map the fd to adrs. See docs for requirements for adrs.

dataBaseT& db = manager.aquireConstruct<dataBaseT>(0,manager);
//here 0 is the key to a specific dataBaseT object.
//here it calls the constructer of dataBaseT with the argument being 'manager'.
//'manager' will be converted to a mapAllocT that will be used in the constructor.
//notice that db is a reference, if it was not then db would copy the object in the file.



//db will contain any changes that happened in all previous runs of main.

...
changeDataBase(db); // any changes will automatically pass to the file
...
readDataBase(db); // any reads from dp will refer to the object in the file
...

close(fd);// fd needs to be valid for the manager, if any extension of the file is needed.
}


''' 

