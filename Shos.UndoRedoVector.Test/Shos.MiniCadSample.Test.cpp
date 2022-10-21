#include "pch.h"
#include "CppUnitTest.h"
#include <vector>
#include <list>
#include <iterator>
#include "../Shos.MiniCadSample/undo_redo_vector.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ShosMiniCadSampleTest
{
    using namespace shos;

    TEST_CLASS(undo_redo_vector_test)
    {
    public:
        TEST_METHOD(check_vector)
        {
            std::vector<int> array = { 0, 1, 1, 2, 3, 5, 8, 13 };
            std::vector<int>::iterator iterator = array.begin();

            for (; iterator != array.begin(); iterator++)
                Assert::AreEqual(*iterator, array[iterator - array.begin()]);

            iterator = std::find(array.begin(), array.end(), 8);
            Assert::AreEqual<std::size_t>(6, std::distance(array.begin(), iterator));
            Assert::AreEqual<std::size_t>(6, iterator - array.begin());
        }
        
        TEST_METHOD(check_list)
        {
            std::list<int> array = { 0, 1, 1, 2, 3, 5, 8, 13 };
            std::list<int>::iterator iterator = array.begin();

            iterator = std::find(array.begin(), array.end(), 8);
            Assert::AreEqual<std::size_t>(6, std::distance(array.begin(), iterator));
        }

        TEST_METHOD(construct)
        {
            undo_redo_vector<int> array;
            Assert::AreEqual<size_t>(array.size(), 0UL);
        }

        TEST_METHOD(push_back)
        {
            undo_redo_vector<int> array;
            array.push_back(100);

            Assert::AreEqual<size_t>(array.size(), 1UL);
            Assert::AreEqual<int>(array[0], 100);

            array.push_back(300);

            Assert::AreEqual<size_t>(array.size(), 2UL);
            Assert::AreEqual<int>(array[0], 100);
            Assert::AreEqual<int>(array[1], 300);
        }

        TEST_METHOD(erace)
        {
            undo_redo_vector<int> array;
            array.push_back(100);

            array.erase(array.begin());
            Assert::AreEqual<size_t>(array.size(), 0UL);

            array.push_back(400);
            array.push_back(500);
            array.push_back(600);
            array.push_back(700);

            array.erase(std::next(array.begin(), 1));
            Assert::AreEqual<size_t>(array.size(), 3UL);
            Assert::AreEqual<int>(array[0], 400);
            Assert::AreEqual<int>(array[1], 600);
            Assert::AreEqual<int>(array[2], 700);

            array.erase(std::next(array.begin(), 2));
            Assert::AreEqual<size_t>(array.size(), 2UL);
            Assert::AreEqual<int>(array[0], 400);
            Assert::AreEqual<int>(array[1], 600);

            array.erase(array.begin());
            Assert::AreEqual<size_t>(array.size(), 1UL);
            Assert::AreEqual<int>(array[0], 600);

            array.erase(array.begin());
            Assert::AreEqual<size_t>(array.size(), 0UL);
        }

        TEST_METHOD(update)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);
            array.push_back(400);

            array.update(std::next(array.begin(), 1), 1200);
            Assert::AreEqual<size_t>(array.size(), 3UL);
            Assert::AreEqual<int>(array[0], 100);
            Assert::AreEqual<int>(array[1], 1200);
            Assert::AreEqual<int>(array[2], 400);
        }

        TEST_METHOD(undo_push_back)
        {
            undo_redo_vector<int> array;
            Assert::IsFalse(array.can_undo());
            Assert::IsFalse(array.undo());

            array.push_back(400);
            array.push_back(500);
            array.push_back(700);
            
            Assert::IsTrue(array.can_undo());
            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(array.size(), 2UL);
            Assert::AreEqual<int>(array[0], 400);
            Assert::AreEqual<int>(array[1], 500);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(array.size(), 1UL);
            Assert::AreEqual<int>(array[0], 400);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(array.size(), 0UL);

            Assert::IsFalse(array.can_undo());
            Assert::IsFalse(array.undo());

            array.push_back(1000);
            array.push_back(1100);
            Assert::AreEqual<size_t>(array.size(), 2UL);
            Assert::AreEqual<int>(array[0], 1000);
            Assert::AreEqual<int>(array[1], 1100);
        }

        TEST_METHOD(undo_erase)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);
            array.push_back(400);

            array.erase(std::next(array.begin(), 1));
            Assert::IsTrue(array.undo());

            Assert::AreEqual<size_t>(array.size(), 3UL);
            Assert::AreEqual<int>(array[0], 100);
            Assert::AreEqual<int>(array[1], 200);
            Assert::AreEqual<int>(array[2], 400);

            array.erase(std::next(array.begin(), 2));
            array.erase(std::next(array.begin(), 1));
            array.erase(array.begin());

            Assert::IsTrue(array.can_undo());
            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(array.size(), 1UL);
            Assert::AreEqual<int>(array[0], 100);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(array.size(), 2UL);
            Assert::AreEqual<int>(array[0], 100);
            Assert::AreEqual<int>(array[1], 200);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(array.size(), 3UL);
            Assert::AreEqual<int>(array[0], 100);
            Assert::AreEqual<int>(array[1], 200);
            Assert::AreEqual<int>(array[2], 400);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(array.size(), 2UL);
            Assert::AreEqual<int>(array[0], 100);
            Assert::AreEqual<int>(array[1], 200);
        }

        TEST_METHOD(undo_update)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);
            array.push_back(400);

            array.update(std::next(array.begin(), 1), 1200);
            Assert::AreEqual<size_t>(array.size(), 3UL);
            Assert::AreEqual<int>(array[0], 100);
            Assert::AreEqual<int>(array[1], 1200);
            Assert::AreEqual<int>(array[2], 400);

            Assert::IsTrue(array.can_undo());
            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(array.size(), 3UL);
            Assert::AreEqual<int>(array[0], 100);
            Assert::AreEqual<int>(array[1], 200);
            Assert::AreEqual<int>(array[2], 400);
        }

        TEST_METHOD(redo)
        {
            undo_redo_vector<int> array;
            Assert::IsFalse(array.can_redo());
            
            array.push_back(300);
            Assert::IsFalse(array.can_redo());

            array.push_back(600);
            array.push_back(800);

            Assert::AreEqual<size_t>(array.size(), 3UL);
            Assert::AreEqual<int>(array[0], 300);
            Assert::AreEqual<int>(array[1], 600);
            Assert::AreEqual<int>(array[2], 800);

            array.erase(std::next(array.begin(), 1));
            array.update(array.begin(), 1200);
            array.erase(array.begin());
            array.push_back(1400);
            array.undo();
            array.push_back(1400);
            Assert::IsFalse(array.can_redo());

            Assert::AreEqual<size_t>(array.size(), 2UL);
            Assert::AreEqual<int>(array[0], 800);
            Assert::AreEqual<int>(array[1], 1400);

            array.undo();
            Assert::AreEqual<size_t>(array.size(), 1UL);
            Assert::AreEqual<int>(array[0], 800);

            array.undo();
            Assert::AreEqual<size_t>(array.size(), 2UL);
            Assert::AreEqual<int>(array[0], 1200);
            Assert::AreEqual<int>(array[1], 800);

            array.undo();
            Assert::AreEqual<size_t>(array.size(), 2UL);
            Assert::AreEqual<int>(array[0], 300);
            Assert::AreEqual<int>(array[1], 800);

            Assert::IsTrue(array.can_redo());
            Assert::IsTrue(array.redo());
            Assert::AreEqual<size_t>(array.size(), 2UL);
            Assert::AreEqual<int>(array[0], 1200);
            Assert::AreEqual<int>(array[1], 800);

            Assert::IsTrue(array.redo());
            Assert::AreEqual<size_t>(array.size(), 1UL);
            Assert::AreEqual<int>(array[0], 800);

            Assert::IsTrue(array.redo());
            Assert::AreEqual<size_t>(array.size(), 2UL);
            Assert::AreEqual<int>(array[0], 800);
            Assert::AreEqual<int>(array[1], 1400);

            Assert::IsFalse(array.can_redo());
            Assert::IsFalse(array.redo());
        }

        TEST_METHOD(undo_group)
        {
            undo_redo_vector<int> array;

            array.push_back(100);
            {
                undo_redo_vector<int>::transaction transaction(array);
                array.push_back(300);
                array.push_back(400);
                array.erase(array.begin());
            }
            array.erase(array.begin());

            Assert::AreEqual<size_t>(array.size(), 1UL);
            Assert::AreEqual<int>(array[0], 400);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(array.size(), 2UL);
            Assert::AreEqual<int>(array[0], 300);
            Assert::AreEqual<int>(array[1], 400);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(array.size(), 1UL);
            Assert::AreEqual<int>(array[0], 100);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(array.size(), 0UL);
            Assert::IsFalse(array.can_undo());

            Assert::IsTrue(array.redo());
            Assert::AreEqual<size_t>(array.size(), 1UL);
            Assert::AreEqual<int>(array[0], 100);

            Assert::IsTrue(array.redo());
            Assert::AreEqual<size_t>(array.size(), 2UL);
            Assert::AreEqual<int>(array[0], 300);
            Assert::AreEqual<int>(array[1], 400);

            Assert::IsTrue(array.redo());
            Assert::AreEqual<size_t>(array.size(), 1UL);
            Assert::AreEqual<int>(array[0], 400);

            Assert::IsFalse(array.can_redo());

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(array.size(), 2UL);
            Assert::AreEqual<int>(array[0], 300);
            Assert::AreEqual<int>(array[1], 400);
        }

        TEST_METHOD(clear)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);

            array.clear();
            Assert::AreEqual<size_t>(array.size(), 0UL);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(array.size(), 2UL);
            Assert::AreEqual<int>(array[0], 100);
            Assert::AreEqual<int>(array[1], 200);

            Assert::IsTrue(array.redo());
            Assert::AreEqual<size_t>(array.size(), 0UL);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(array.size(), 2UL);
            Assert::AreEqual<int>(array[0], 100);
            Assert::AreEqual<int>(array[1], 200);
        }

        class foo
        {
            int value;
            
        public:
            foo(int value) : value(value) {}
            operator int() { return value; }
        };
        
        TEST_METHOD(clean_up)
        {
            undo_redo_vector<foo*> array;

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

            std::for_each(array.begin(), array.end(), [](foo* p) { delete p; });
        }


        TEST_METHOD(pointer_vector)
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
    };
}
