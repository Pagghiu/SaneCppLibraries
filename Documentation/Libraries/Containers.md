@page library_containers Containers

@brief 🟨 Generic containers (SC::Vector, SC::SmallVector, SC::Array etc.)

Containers is a library holding some commonly used generic data structures.

[TOC]

# Features
| Class                             | Description                               |
|:----------------------------------|:------------------------------------------|
| SC::Vector                        | @copybrief SC::Vector                     |
| SC::Array                         | @copybrief SC::Array                      |
| SC::SmallVector                   | @copybrief SC::SmallVector                |
| SC::VectorMap                     | @copybrief SC::VectorMap                  |
| SC::VectorSet                     | @copybrief SC::VectorSet                  |
| SC::ArenaMap                      | @copybrief SC::ArenaMap                   |
| SC::IntrusiveDoubleLinkedList     | @copybrief SC::IntrusiveDoubleLinkedList  |

# Status
🟨 MVP  
SC::Vector, SC::Array and friends should be reasonably stable.  

# Description

Generic data structures are a fundamental building blocks for almost every application.  
These are some of commonly used ones for common tasks, and the library will grow adding what's needed.

SC::Vector is the king of all generic containers for this library, being in many case the main backend storage for other containers.

SC::Array mimics all methods of SC::Vector but it's guaranteed never to allocate on heap.  
All methods are designed to fail with a `[[nodiscard]]` return value when the container is full.

SC::SmallVector is the middle ground between SC::Array and SC::Vector.  
It's a vector with inline storage for `N` elements, deriving from SC::Vector and it's designed to be passed everywhere a reference to SC::Vector is needed. This allows the caller to get rid of temporary heap allocations if an estimate of the space required is already known or if it's possible providing a reasonable default.  
If this estimation is wrong, heap allocation will happen.

## Vector

@copydoc SC::Vector

Example:
```cpp
    Vector<int> myVector;
    SC_TRY(myVector.reserve(10));
    SC_TRY(myVector.push_back(1));
    console.print("[0]={}", myVector[0]);
    SC_TRY(myVector.push_back(2));
    SC_TRY(myVector.pop_back());
    SC_TRY(myVector.pop_front());
    console.print("Vector<int> is {}", myVector.isEmpty() ? "empty" : "not empty");
```

## Array

@copydoc SC::Array

Example:
```cpp
    Array<int, 3> myVector;
    SC_TRY(myVector.push_back(1));
    SC_TRY(myVector.push_back(2));
    SC_TRY(myVector.push_back(3));
    (void)myVector.push_back(4); // <-- This will fail
    SC_TRY(myVector.pop_back());
    SC_TRY(myVector.pop_front());
    SC_TRY(myVector.pop_front());
    (void)myVector.pop_front(); // <-- This will fail
    console.print("Array<int, 3> is {}", myVector.isEmpty() ? "empty" : "not empty");
```

## SmallVector

@copydoc SC::SmallVector

Example:
```cpp
[[nodiscard]] bool pushThreeIntegers(Vector<int>& myVector)
{
    SC_TRY(myVector.push_back(1));
    SC_TRY(myVector.push_back(2));
    SC_TRY(myVector.push_back(3));
}

//...

SmallVector<int, 3> mySmallVector;
SC_TRY(pushThreeIntegers(mySmallVector)); // <-- No heap allocation will happen

// ... later on

mySmallVector.push_back(4); // <-- Vector is now moved to heap

// ... later on

mySmallVector.pop_back(); // <-- Vector is moved back to SmallVector inline storage
```

## VectorMap

@copydoc SC::VectorMap

Example:
```cpp
VectorMap<String, int> map;
SC_TRY(map.insertIfNotExists({"A", 2})); // Allocates a String
SC_TRY(map.insertIfNotExists({"B", 3})); // Allocates a String
const int* value;
SC_TRY(map.contains("A", value) && *value == 2); // <-- "A" is a StringView, avoiding allocation
SC_TRY(map.contains("B", value) && *value == 3); // <-- "B" is a StringView, avoiding allocation
SC_TRY(not map.contains("C")); // <-- "C" is a StringView, avoiding allocation
```

## VectorSet

@copydoc SC::VectorSet

Example:
```cpp
VectorSet<String> setOfStrings;
SC_TRY(setOfStrings.insert("123"));
SC_TRY(setOfStrings.insert("123"));
SC_TRY(setOfStrings.contains("123"));
SC_TRY(setOfStrings.insert("456"));
SC_TRY(setOfStrings.contains("123"));
SC_TRY(setOfStrings.contains("456"));
SC_TRY(setOfStrings.size() == 2);
SC_TRY(setOfStrings.remove("123"));
SC_TRY(setOfStrings.size() == 1);
SC_TRY(setOfStrings.contains("456"));
SC_TRY(not setOfStrings.contains("123"));
```

## ArenaMap

@copydoc SC::ArenaMap

Example:
```cpp
ArenaMap<String> map;
SC_TRY(not map.insert("ASD").isValid());
SC_TRY(map.resize(3));
ArenaMap<String>::Key keys[3];
keys[0] = map.insert("ASD");
SC_TRY(map.size() == 1);
SC_TRY(not map.resize(4)); // cannot resize unless is empty
keys[1]   = map.insert("DSA");
keys[2]   = map.insert("BDA");
SC_TRY(map.size() == 3);
SC_TRY(not map.insert("123").isValid()); // Arena is full

SC_TRY(map.get(keys[0])->view() == "ASD"); // Get first element
SC_TRY(map.get(keys[1])->view() == "DSA"); // Get second element
SC_TRY(map.get(keys[2])->view() == "BDA"); // Get third element
```

## IntrusiveDoubleLinkedList

@copydoc SC::IntrusiveDoubleLinkedList
```cpp
struct Item
{
    Item* next = nullptr;   // <-- Required by IntrusiveDoubleLinkedList
    Item* prev = nullptr;   // <-- Required by IntrusiveDoubleLinkedList
    int   data = 0;
};

IntrusiveDoubleLinkedList<Item> queue;

Item items[2];
items[0].data = 0;
items[1].data = 1;
SC_TRY(queue.isEmpty());

queue.queueBack(items[0]);
queue.queueBack(items[1]);

SC_TRY(not queue.isEmpty());
Item* first = queue.dequeueFront();
SC_TRY(first->data == 0);
SC_TRY(not queue.isEmpty());

Item* second = queue.dequeueFront();
SC_TRY(second->data == 1);
SC_TRY(queue.isEmpty());
```

# Details
- `SC::SegmentItems` is the class representing a variable and contiguous slice of objects backing both SC::Vector and SC::Array.  
- Memory layout of a segment is a `SC::SegmentHeaderBase` holding size and capacity of the segment followed by the actual elements. 
- `SC::SegmentHeaderBase` is aligned to `uint64_t`.
- SC::Vector and SC::Array use `SC::SegmentHeader` = `SC::SegmentHeaderBase` so the `SC::SegmentHeader` size is 8 bytes.

# Roadmap

🟩 Usable Features:
- Add option to let user disable heap allocations in SC::SmallVector
- `HashMap<T>`
- `Map<K, V>`

🟦 Complete Features:
- Explicit control on Segment / Vector allocators

💡 Unplanned Features:
- None