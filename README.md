# [C++] Shos.UndoRedoVector

[English](README.md) | [日本語](README.jp.md)

A header-only C++ library that provides a vector with **undo / redo**.

Every modification (`push_back`, `insert`, `erase`, `update`, `clear`) is recorded automatically,
so you can step backward and forward through the history with `undo()` and `redo()`.
It is useful for editors, drawing tools, and any application that needs an undo history for a list of items.

## Features

- **Header-only**: just include `undo_redo_vector.h`. No build or dependencies.
- **Automatic history**: every modification becomes an undo step.
- **Transactions**: group several modifications into a single undo step (nesting supported).
- **Ownership support**: `undo_redo_pointer_vector<T>` owns `T*` elements and deletes them at the right time, and any custom clean-up function can be given instead.
- **Choice of container**: `std::vector` by default; `std::list` can also be used.
- **Safe**: if a modification throws (e.g. out of memory), the collection and its history are left unchanged.

## Requirements

- C++17 or later (the library uses `std::optional`)
- Only the standard library is needed. It works with any standard-conforming compiler (MSVC, GCC, Clang).

## Installation

Copy [`undo_redo_vector.h`](undo_redo_vector.h) into your project and include it.

```cpp
#include "undo_redo_vector.h"
```

Everything is in the `shos` namespace.

## Quick Start

```cpp
#include <iostream>
#include "undo_redo_vector.h"

int main()
{
    shos::undo_redo_vector<int> values;

    values.push_back(100);      // [100]
    values.push_back(200);      // [100, 200]

    values.undo();              // [100]
    values.redo();              // [100, 200]

    for (int value : values)
        std::cout << value << ' ';   // 100 200
}
```

## Usage

### Modifying elements

Elements can be modified only through the following member functions, so that every change is recorded.
Positions are given as iterators, like `std::vector`.

```cpp
shos::undo_redo_vector<int> values;

values.push_back(100);                   // [100]
values.push_back(300);                   // [100, 300]
values.insert(values.begin() + 1, 200);  // [100, 200, 300]   insert before the position
values.update(values.begin(), 1000);     // [1000, 200, 300]  replace the element at the position
values.erase(values.begin() + 2);        // [1000, 200]       remove the element at the position
values.clear();                          // []                remove all (one undo step)
```

> `update()` is used instead of assignment such as `values[0] = 1000;`,
> because `operator[]`, `begin()`, `end()` etc. return only `const` references / iterators.

### Reading elements

The usual read-only functions are available.

```cpp
int first  = values[0];
int second = values.at(1);       // throws std::out_of_range if out of range
int head   = values.front();
int tail   = values.back();
size_t n   = values.size();
bool none  = values.empty();

for (int value : values)          // range-based for works
    std::cout << value << '\n';
```

### Undo and redo

```cpp
shos::undo_redo_vector<int> values;
values.push_back(100);                   // [100]
values.push_back(300);                   // [100, 300]
values.insert(values.begin() + 1, 200);  // [100, 200, 300]
values.update(values.begin(), 1000);     // [1000, 200, 300]
values.erase(values.begin() + 2);        // [1000, 200]

values.undo();                           // [1000, 200, 300]  the erase is undone
values.undo();                           // [100, 200, 300]   the update is undone
values.redo();                           // [1000, 200, 300]  the update is redone

if (values.can_undo()) { /* ... */ }     // true if there is something to undo
if (values.can_redo()) { /* ... */ }     // true if there is something to redo
```

`undo()` / `redo()` return `true` on success, and `false` (doing nothing) when there is nothing to undo / redo.

Like most applications, **any modification after `undo()` discards the redo history**:

```cpp
values.undo();          // [100, 200, 300]
values.push_back(400);  // [100, 200, 300, 400]
values.can_redo();      // false
```

### Grouping modifications into one undo step (transaction)

Create a `transaction` object. All the modifications made while it is alive are combined into one undo step,
which is committed when the object goes out of scope.

```cpp
{
    shos::undo_redo_vector<int>::transaction transaction(values);
    values.push_back(500);
    values.push_back(600);
}   // committed here as one undo step

values.undo();   // removes both 500 and 600
```

- Transactions can be nested. Only the outermost one creates an undo step.
- A transaction with no modifications creates no undo step.
- During a transaction, `undo()` / `redo()` do nothing and return `false`, and `can_undo()` / `can_redo()` return `false`. Use `in_transaction()` to check.
- `clear()` internally uses a transaction, so it can be undone in one step.

### Clearing the history (`reset`)

`reset()` removes all the elements **and** the undo / redo history. It cannot be undone.

```cpp
values.reset();   // size() == 0, can_undo() == false, can_redo() == false
```

`reset()` throws `std::logic_error` if called during a transaction.

### Owning pointers (`undo_redo_pointer_vector`)

When elements are pointers to objects allocated with `new`, use `undo_redo_pointer_vector<T>`.
It owns the objects and `delete`s each of them exactly once, **when it is no longer reachable by undo / redo**.

```cpp
struct Shape  { virtual ~Shape() = default; };
struct Circle : Shape {};

shos::undo_redo_pointer_vector<Shape> shapes;

shapes.push_back(new Circle());
shapes.erase(shapes.begin());   // the Circle is NOT deleted yet: it is kept for undo
shapes.undo();                  // the same Circle is back in the vector
// When the history holding it is discarded, or shapes is reset / destroyed, the Circle is deleted.
```

Do not `delete` the pointers yourself.

### Custom clean-up function

For other kinds of resources (handles, files, etc.), pass a clean-up function to the constructor.
It is called exactly once for each element when the element is no longer needed, that is:

- when the undo / redo step holding it is discarded, or
- when the collection is `reset()` or destroyed.

```cpp
shos::undo_redo_vector<Handle> handles([](Handle& handle) { close_handle(handle); });
```

Note that an element replaced by `update()` is also kept for undo, and cleaned up later in the same way.

### Using another container (`undo_redo_collection`)

`undo_redo_vector<T>` is an alias of `undo_redo_collection<T, std::vector<T>>`.
You can specify another sequence container such as `std::list`:

```cpp
#include <list>

shos::undo_redo_collection<int, std::list<int>> values;
values.push_back(1);
values.push_back(2);
values.erase(std::next(values.begin()));   // list iterators are not random access
values.undo();                             // [1, 2]
```

Likewise, `undo_redo_pointer_collection<T, TCollection>` is the general form of `undo_redo_pointer_vector<T>`.

## API Reference

### Types

| Type | Description |
| --- | --- |
| `undo_redo_vector<T>` | Vector with undo / redo. Alias of `undo_redo_collection<T>` |
| `undo_redo_pointer_vector<T>` | Vector of `T*` that owns and deletes the pointed objects. Alias of `undo_redo_pointer_collection<T>` |
| `undo_redo_collection<T, TCollection = std::vector<T>>` | General form, with the underlying container specified |
| `undo_redo_pointer_collection<T, TCollection = std::vector<T*>>` | General form of the pointer version |
| `undo_redo_collection<...>::transaction` | RAII object that groups modifications into one undo step |

### Member functions

| Function | Description |
| --- | --- |
| `undo_redo_collection()` | Creates an empty collection |
| `undo_redo_collection(clean_up_function)` | Creates an empty collection with a clean-up function `void(T&)` |
| `push_back(element)` | Adds an element at the end |
| `insert(position, element)` | Inserts an element before `position` |
| `erase(position)` | Removes the element at `position` |
| `update(position, element)` | Replaces the element at `position` |
| `clear()` | Removes all the elements (one undo step) |
| `reset()` | Removes all the elements and the history (cannot be undone) |
| `undo()` / `redo()` | Undoes / redoes one step. Returns `false` if not possible |
| `can_undo()` / `can_redo()` | Whether `undo()` / `redo()` is possible |
| `in_transaction()` | Whether a transaction is in progress |
| `operator[](index)` / `at(index)` | Element access (`const`). `at` checks the range |
| `front()` / `back()` | First / last element (`const`) |
| `size()` / `empty()` | Number of elements / whether empty |
| `begin()` / `end()` / `cbegin()` / `cend()` | `const_iterator`s |

## Notes

- **Read-only access**: `begin()` / `end()` return `const_iterator`, and `operator[]` returns a `const` reference. Use `update()` to change an element.
- **Not copyable**. Movable, but must not be moved during a transaction.
- **Exception safety**: if `push_back` / `insert` / `erase` / `update` throws, the collection and its history are left unchanged.
- **Not thread-safe**: synchronize externally when accessed from multiple threads.

## Repository Structure

| Path | Description |
| --- | --- |
| [`undo_redo_vector.h`](undo_redo_vector.h) | The library (this single file is all you need) |
| `Shos.UndoRedoVector.Test/` | Unit tests (Microsoft C++ Unit Test Framework) |
| `Shos.UndoRedoVector.MemoryLeakTest/` | Memory leak test (MSVC CRT debug heap) |
| `Shos.UndoRedoVector.sln` | Visual Studio solution for the tests |

## Running the Tests

1. Open `Shos.UndoRedoVector.sln` in Visual Studio (2022 or later, with the "Desktop development with C++" workload).
2. Build the solution.
3. Run the unit tests from **Test Explorer**.
4. To check for memory leaks, run `Shos.UndoRedoVector.MemoryLeakTest` in the Debug configuration; leaks, if any, are reported in the Output window.

## Author info

Fujio Kojima: a software developer in Japan

* Microsoft MVP for Development Tools - Visual C# (Jul. 2005 - Dec. 2014)
* Microsoft MVP for .NET (Jan. 2015 - Oct. 2015)
* Microsoft MVP for Visual Studio and Development Technologies (Nov. 2015 - Jun. 2018)
* Microsoft MVP for Developer Technologies (Nov. 2018 - Jun. 2025)
* [MVP Profile](https://mvp.microsoft.com/en-US/mvp/profile/4185d172-3c9a-e411-93f2-9cb65495d3c4)
* [Blog (Japanese)](http://wp.shos.info)
* [Web Site (Japanese)](http://www.shos.info)
* [Twitter](https://twitter.com/Fujiwo)
* [Instagram](https://www.instagram.com/fujiwo/)

## License

[MIT License](LICENSE.txt)
