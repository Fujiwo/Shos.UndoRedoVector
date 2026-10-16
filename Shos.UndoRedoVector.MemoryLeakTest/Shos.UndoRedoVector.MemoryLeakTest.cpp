// Checks that undo_redo_vector does not leak the objects pointed to by its elements.
// Run it in a debug build: leaks are reported to the debugger output and make the exit code 1.

#include "../undo_redo_vector.h"
// Include after the standard headers, since it redefines new.
#include "MemoryLeakTest.h"

using namespace shos;

// A heap-allocated object whose leak would be detected.
class foo
{
    int value;

public:
    foo(int initial_value) : value(initial_value) {}
    operator int() const { return value; }
};

// Checks that a lambda stored in std::function can delete an object (the mechanism used by clean_up).
class function_test
{
    std::function<void(foo*)> func;
    foo* foo1;

public:
    function_test(std::function<void(foo*)> clean_up) : func(clean_up), foo1(new foo(100))
    {}

    virtual ~function_test()
    {
        if (func != nullptr)
            func(foo1);
    }
};

void function_clean_up_test()
{
    auto clean_up = [](foo* foo) { delete foo; };
    function_test function_test1(clean_up);
}

// Uses a clean_up function to delete the objects.
// The objects are moved in and out of the collection by undo / redo,
// and some undo history is discarded, so every path that releases an element is exercised.
void undo_redo_vector_memory_leak_test()
{
    undo_redo_vector<foo*> array([](foo* foo) { delete foo; });

    array.push_back(new foo(300));
    array.push_back(new foo(600));
    array.push_back(new foo(800));

    array.undo();

    array.erase(std::next(array.begin(), 1));

    array.undo();
    array.redo();

    array.update(array.begin(), new foo(1200));

    array.erase(array.begin());
    array.push_back(new foo(1400));

    array.undo();
    array.redo();
    array.redo();
} // The destructor deletes the remaining objects, both in the collection and in the undo history.

// The same scenario with undo_redo_pointer_vector, which deletes the objects by itself.
void undo_redo_pointer_vector_memory_leak_test()
{
    undo_redo_pointer_vector<foo> array;

    array.push_back(new foo(300));
    array.push_back(new foo(600));
    array.push_back(new foo(800));

    array.undo();

    array.erase(std::next(array.begin(), 1));

    array.undo();
    array.redo();

    array.update(array.begin(), new foo(1200));

    array.erase(array.begin());
    array.push_back(new foo(1400));

    array.undo();
    array.redo();
    array.redo();
}

int main()
{
    CheckMemoryLeaksStart();

    function_clean_up_test();
    undo_redo_vector_memory_leak_test();
    undo_redo_pointer_vector_memory_leak_test();

#ifdef _DEBUG
    // All the collections above have been destroyed here, so any remaining block is a leak.
    // Returns non-zero if any leak is detected, so that the result can be checked automatically.
    return _CrtDumpMemoryLeaks() ? 1 : 0;
#else // _DEBUG
    return 0;
#endif // _DEBUG
}
