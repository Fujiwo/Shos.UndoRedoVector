# [C++] Shos.UndoRedoVector

A header-only vector template class with undo/redo.

## Files

- `undo_redo_vector.h`: the library (just include this file)
    - `shos::undo_redo_vector<T>`: vector with undo/redo
    - `shos::undo_redo_pointer_vector<T>`: undo/redo vector of `T*` that owns and automatically deletes the pointed objects
    - `shos::undo_redo_collection<T, TCollection>`: the general form (`TCollection` defaults to `std::vector<T>`; `std::list<T>` can also be used)
- `Shos.UndoRedoVector.Test`: unit tests (Microsoft C++ Unit Test Framework)
- `Shos.UndoRedoVector.MemoryLeakTest`: memory leak test (MSVC CRT debug heap)

## Usage

### Basic operations

```cpp
#include "undo_redo_vector.h"

shos::undo_redo_vector<int> values;

values.push_back(100);                                // [100]
values.push_back(300);                                // [100, 300]
values.insert(std::next(values.begin(), 1), 200);     // [100, 200, 300]
values.update(values.begin(), 1000);                  // [1000, 200, 300]
values.erase(std::next(values.begin(), 2));           // [1000, 200]

values.undo();                                        // [1000, 200, 300]
values.undo();                                        // [100, 200, 300]
values.redo();                                        // [1000, 200, 300]

bool undoable = values.can_undo();
bool redoable = values.can_redo();
```

Any modification after `undo()` discards the redo history.

### Grouping modifications into one undo step

```cpp
{
    shos::undo_redo_vector<int>::transaction transaction(values);
    values.push_back(400);
    values.push_back(500);
}   // committed as one undo step here

values.undo();   // undoes both push_back
```

Transactions can be nested; only the outermost one creates the undo step.
`clear()` is also recorded as one undo step.

### Owning pointers

```cpp
shos::undo_redo_pointer_vector<Shape> shapes;   // deletes the Shape objects when no longer needed
shapes.push_back(new Circle());
shapes.erase(shapes.begin());   // the Circle is kept for undo, and deleted when the undo history is discarded
```

Any clean-up function can also be given:

```cpp
shos::undo_redo_vector<Handle> handles([](Handle& handle) { close_handle(handle); });
```

The clean-up function is called exactly once for each element when it is no longer needed:
when the undo/redo step holding it is discarded, or when the collection is `reset()` or destroyed.

## Notes

- Elements can be modified only through `push_back` / `insert` / `erase` / `update` / `clear`, so that every modification is recorded. `begin()` / `end()` return `const_iterator`.
- `undo()` / `redo()` do nothing (return `false`) during a transaction.
- `reset()` removes all the elements and the undo history, and cannot be undone. It throws `std::logic_error` during a transaction.
- Not copyable. Movable, but must not be moved during a transaction.

## Development Environment

- Language: C++20 or later
- Visual Studio 2026 (Platform Toolset v145)
