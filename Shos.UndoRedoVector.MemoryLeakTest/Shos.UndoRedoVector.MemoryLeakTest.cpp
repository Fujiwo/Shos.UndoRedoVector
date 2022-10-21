#include "MemoryLeakTest.h"
#include "../undo_redo_vector.h"

using namespace shos;

class foo
{
    int value;

public:
    foo(int value) : value(value) {}
    operator int() { return value; }
};

class function_test
{
    std::function<void(foo*)> func;
    foo* foo1;

public:
    function_test(std::function<void(foo*)> func) : func(func), foo1(new foo(100))
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
}

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
}
