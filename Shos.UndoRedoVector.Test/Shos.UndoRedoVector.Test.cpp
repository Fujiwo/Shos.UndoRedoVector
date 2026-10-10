#include "pch.h"
#include "CppUnitTest.h"
#include <atomic>
#include <list>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include "../undo_redo_vector.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ShosUndoRedoVectorTest
{
    using namespace shos;

    TEST_CLASS(undo_redo_vector_test)
    {
        // Counts the live instances to detect leaks and double deletes.
        class foo
        {
            int value;

        public:
            inline static int instance_count = 0;

            foo(int initial_value) : value(initial_value) { instance_count++; }
            ~foo() { instance_count--; }
            operator int() const { return value; }
        };

    public:
        TEST_METHOD_INITIALIZE(initialize)
        {
            foo::instance_count = 0;
        }

        TEST_METHOD(construct)
        {
            undo_redo_vector<int> array;
            Assert::AreEqual<size_t>(0, array.size());
            Assert::IsTrue(array.empty());
            Assert::IsFalse(array.can_undo());
            Assert::IsFalse(array.can_redo());
        }

        TEST_METHOD(not_copyable)
        {
            Assert::IsFalse(std::is_copy_constructible_v<undo_redo_vector<int>>);
            Assert::IsFalse(std::is_copy_assignable_v<undo_redo_vector<int>>);
        }

        TEST_METHOD(push_back)
        {
            undo_redo_vector<int> array;
            array.push_back(100);

            Assert::AreEqual<size_t>(1, array.size());
            Assert::AreEqual<int>(100, array[0]);

            array.push_back(300);

            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(100, array[0]);
            Assert::AreEqual<int>(300, array[1]);
            Assert::AreEqual<int>(100, array.front());
            Assert::AreEqual<int>(300, array.back());
        }

        TEST_METHOD(at)
        {
            undo_redo_vector<int> array;
            array.push_back(100);

            Assert::AreEqual<int>(100, array.at(0));
            Assert::ExpectException<std::out_of_range>([&] { static_cast<void>(array.at(1)); });
        }

        TEST_METHOD(insert)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(300);

            array.insert(std::next(array.begin(), 1), 200);
            Assert::AreEqual<size_t>(3, array.size());
            Assert::AreEqual<int>(100, array[0]);
            Assert::AreEqual<int>(200, array[1]);
            Assert::AreEqual<int>(300, array[2]);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(100, array[0]);
            Assert::AreEqual<int>(300, array[1]);

            Assert::IsTrue(array.redo());
            Assert::AreEqual<size_t>(3, array.size());
            Assert::AreEqual<int>(200, array[1]);
        }

        TEST_METHOD(erase)
        {
            undo_redo_vector<int> array;
            array.push_back(100);

            array.erase(array.begin());
            Assert::AreEqual<size_t>(0, array.size());

            array.push_back(400);
            array.push_back(500);
            array.push_back(600);
            array.push_back(700);

            array.erase(std::next(array.begin(), 1));
            Assert::AreEqual<size_t>(3, array.size());
            Assert::AreEqual<int>(400, array[0]);
            Assert::AreEqual<int>(600, array[1]);
            Assert::AreEqual<int>(700, array[2]);

            array.erase(std::next(array.begin(), 2));
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(400, array[0]);
            Assert::AreEqual<int>(600, array[1]);

            array.erase(array.begin());
            Assert::AreEqual<size_t>(1, array.size());
            Assert::AreEqual<int>(600, array[0]);

            array.erase(array.begin());
            Assert::AreEqual<size_t>(0, array.size());
        }

        TEST_METHOD(update)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);
            array.push_back(400);

            array.update(std::next(array.begin(), 1), 1200);
            Assert::AreEqual<size_t>(3, array.size());
            Assert::AreEqual<int>(100, array[0]);
            Assert::AreEqual<int>(1200, array[1]);
            Assert::AreEqual<int>(400, array[2]);
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
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(400, array[0]);
            Assert::AreEqual<int>(500, array[1]);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(1, array.size());
            Assert::AreEqual<int>(400, array[0]);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(0, array.size());

            Assert::IsFalse(array.can_undo());
            Assert::IsFalse(array.undo());

            array.push_back(1000);
            array.push_back(1100);
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(1000, array[0]);
            Assert::AreEqual<int>(1100, array[1]);
        }

        TEST_METHOD(undo_erase)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);
            array.push_back(400);

            array.erase(std::next(array.begin(), 1));
            Assert::IsTrue(array.undo());

            Assert::AreEqual<size_t>(3, array.size());
            Assert::AreEqual<int>(100, array[0]);
            Assert::AreEqual<int>(200, array[1]);
            Assert::AreEqual<int>(400, array[2]);

            array.erase(std::next(array.begin(), 2));
            array.erase(std::next(array.begin(), 1));
            array.erase(array.begin());

            Assert::IsTrue(array.can_undo());
            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(1, array.size());
            Assert::AreEqual<int>(100, array[0]);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(100, array[0]);
            Assert::AreEqual<int>(200, array[1]);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(3, array.size());
            Assert::AreEqual<int>(100, array[0]);
            Assert::AreEqual<int>(200, array[1]);
            Assert::AreEqual<int>(400, array[2]);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(100, array[0]);
            Assert::AreEqual<int>(200, array[1]);
        }

        TEST_METHOD(undo_redo_update)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);
            array.push_back(400);

            array.update(std::next(array.begin(), 1), 1200);
            Assert::AreEqual<size_t>(3, array.size());
            Assert::AreEqual<int>(100, array[0]);
            Assert::AreEqual<int>(1200, array[1]);
            Assert::AreEqual<int>(400, array[2]);

            Assert::IsTrue(array.can_undo());
            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(3, array.size());
            Assert::AreEqual<int>(100, array[0]);
            Assert::AreEqual<int>(200, array[1]);
            Assert::AreEqual<int>(400, array[2]);

            Assert::IsTrue(array.redo());
            Assert::AreEqual<int>(1200, array[1]);
        }

        TEST_METHOD(redo)
        {
            undo_redo_vector<int> array;
            Assert::IsFalse(array.can_redo());

            array.push_back(300);
            Assert::IsFalse(array.can_redo());

            array.push_back(600);
            array.push_back(800);

            Assert::AreEqual<size_t>(3, array.size());
            Assert::AreEqual<int>(300, array[0]);
            Assert::AreEqual<int>(600, array[1]);
            Assert::AreEqual<int>(800, array[2]);

            array.erase(std::next(array.begin(), 1));
            array.update(array.begin(), 1200);
            array.erase(array.begin());
            array.push_back(1400);
            array.undo();
            array.push_back(1400);
            Assert::IsFalse(array.can_redo());

            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(800, array[0]);
            Assert::AreEqual<int>(1400, array[1]);

            array.undo();
            Assert::AreEqual<size_t>(1, array.size());
            Assert::AreEqual<int>(800, array[0]);

            array.undo();
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(1200, array[0]);
            Assert::AreEqual<int>(800, array[1]);

            array.undo();
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(300, array[0]);
            Assert::AreEqual<int>(800, array[1]);

            Assert::IsTrue(array.can_redo());
            Assert::IsTrue(array.redo());
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(1200, array[0]);
            Assert::AreEqual<int>(800, array[1]);

            Assert::IsTrue(array.redo());
            Assert::AreEqual<size_t>(1, array.size());
            Assert::AreEqual<int>(800, array[0]);

            Assert::IsTrue(array.redo());
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(800, array[0]);
            Assert::AreEqual<int>(1400, array[1]);

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

            Assert::AreEqual<size_t>(1, array.size());
            Assert::AreEqual<int>(400, array[0]);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(300, array[0]);
            Assert::AreEqual<int>(400, array[1]);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(1, array.size());
            Assert::AreEqual<int>(100, array[0]);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(0, array.size());
            Assert::IsFalse(array.can_undo());

            Assert::IsTrue(array.redo());
            Assert::AreEqual<size_t>(1, array.size());
            Assert::AreEqual<int>(100, array[0]);

            Assert::IsTrue(array.redo());
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(300, array[0]);
            Assert::AreEqual<int>(400, array[1]);

            Assert::IsTrue(array.redo());
            Assert::AreEqual<size_t>(1, array.size());
            Assert::AreEqual<int>(400, array[0]);

            Assert::IsFalse(array.can_redo());

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(300, array[0]);
            Assert::AreEqual<int>(400, array[1]);
        }

        TEST_METHOD(empty_transaction)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            {
                undo_redo_vector<int>::transaction transaction(array);
            }

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(0, array.size());
            Assert::IsFalse(array.can_undo());
        }

        TEST_METHOD(nested_transaction)
        {
            undo_redo_vector<int> array;
            {
                undo_redo_vector<int>::transaction outer(array);
                array.push_back(100);
                {
                    undo_redo_vector<int>::transaction inner(array);
                    array.push_back(200);
                }
                Assert::IsTrue(array.in_transaction());
                array.push_back(300);
            }
            Assert::IsFalse(array.in_transaction());
            Assert::AreEqual<size_t>(3, array.size());

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(0, array.size());
            Assert::IsFalse(array.can_undo());

            Assert::IsTrue(array.redo());
            Assert::AreEqual<size_t>(3, array.size());
        }

        TEST_METHOD(undo_redo_during_transaction)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);
            array.undo();
            {
                undo_redo_vector<int>::transaction transaction(array);
                array.push_back(300);
                Assert::IsFalse(array.can_undo());
                Assert::IsFalse(array.can_redo());
                Assert::IsFalse(array.undo());
                Assert::IsFalse(array.redo());
            }
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(100, array[0]);
            Assert::AreEqual<int>(300, array[1]);
            Assert::IsFalse(array.can_redo());

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(1, array.size());
            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(0, array.size());
        }

        TEST_METHOD(clear)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);

            array.clear();
            Assert::AreEqual<size_t>(0, array.size());

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(100, array[0]);
            Assert::AreEqual<int>(200, array[1]);

            Assert::IsTrue(array.redo());
            Assert::AreEqual<size_t>(0, array.size());

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(100, array[0]);
            Assert::AreEqual<int>(200, array[1]);
        }

        TEST_METHOD(clear_during_transaction)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            {
                undo_redo_vector<int>::transaction transaction(array);
                array.push_back(200);
                array.clear();
                array.push_back(300);
            }
            Assert::AreEqual<size_t>(1, array.size());
            Assert::AreEqual<int>(300, array[0]);

            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(1, array.size());
            Assert::AreEqual<int>(100, array[0]);
        }

        TEST_METHOD(reset)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);
            array.undo();

            array.reset();
            Assert::AreEqual<size_t>(0, array.size());
            Assert::IsFalse(array.can_undo());
            Assert::IsFalse(array.can_redo());

            array.push_back(300);
            Assert::AreEqual<int>(300, array[0]);
        }

        TEST_METHOD(reset_during_transaction)
        {
            undo_redo_vector<int> array;
            array.push_back(100);
            {
                undo_redo_vector<int>::transaction transaction(array);
                array.push_back(200);
                Assert::ExpectException<std::logic_error>([&] { array.reset(); });
            }
            Assert::AreEqual<size_t>(2, array.size());
            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(1, array.size());
        }

        TEST_METHOD(move)
        {
            undo_redo_vector<int> array1;
            array1.push_back(100);
            array1.push_back(200);

            undo_redo_vector<int> array2(std::move(array1));
            Assert::AreEqual<size_t>(0, array1.size());
            Assert::IsFalse(array1.can_undo());
            Assert::AreEqual<size_t>(2, array2.size());

            Assert::IsTrue(array2.undo());
            Assert::AreEqual<size_t>(1, array2.size());

            undo_redo_vector<int> array3;
            array3.push_back(300);
            array3 = std::move(array2);
            Assert::AreEqual<size_t>(1, array3.size());
            Assert::AreEqual<int>(100, array3[0]);
            Assert::IsTrue(array3.redo());
            Assert::AreEqual<int>(200, array3[1]);
        }

        TEST_METHOD(const_iteration)
        {
            undo_redo_vector<int> array;
            array.push_back(1);
            array.push_back(2);
            array.push_back(3);

            const auto& const_array = array;
            int sum = 0;
            for (auto value : const_array)
                sum += value;
            Assert::AreEqual<int>(6, sum);

            Assert::IsTrue((std::is_same_v<undo_redo_vector<int>::iterator, undo_redo_vector<int>::const_iterator>));
        }

        TEST_METHOD(list_collection)
        {
            undo_redo_collection<int, std::list<int>> array;
            array.push_back(100);
            array.push_back(200);
            array.push_back(300);

            array.erase(std::next(array.begin(), 1));
            array.update(array.begin(), 1000);
            Assert::AreEqual<size_t>(2, array.size());
            Assert::AreEqual<int>(1000, array[0]);
            Assert::AreEqual<int>(300, array[1]);

            Assert::IsTrue(array.undo());
            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(3, array.size());
            Assert::AreEqual<int>(100, array[0]);
            Assert::AreEqual<int>(200, array[1]);
            Assert::AreEqual<int>(300, array[2]);
        }

        TEST_METHOD(move_only_element)
        {
            undo_redo_vector<std::unique_ptr<std::string>> array;
            array.push_back(std::make_unique<std::string>("a"));
            array.push_back(std::make_unique<std::string>("b"));
            array.erase(array.begin());
            array.update(array.begin(), std::make_unique<std::string>("c"));
            Assert::IsTrue(*array[0] == "c");

            Assert::IsTrue(array.undo());
            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(2, array.size());
            Assert::IsTrue(*array[0] == "a");
            Assert::IsTrue(*array[1] == "b");
        }

        TEST_METHOD(clean_up_function)
        {
            int clean_up_count = 0;
            {
                undo_redo_vector<foo*> array([&](foo* pointer) { clean_up_count++; delete pointer; });

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
            Assert::AreEqual<int>(5, clean_up_count);
            Assert::AreEqual<int>(0, foo::instance_count);
        }

        TEST_METHOD(clean_up_discarded_redo_steps)
        {
            int clean_up_count = 0;
            undo_redo_vector<foo*> array([&](foo* pointer) { clean_up_count++; delete pointer; });

            array.push_back(new foo(100));
            array.push_back(new foo(200));
            array.undo();
            Assert::AreEqual<int>(0, clean_up_count);

            // Discards the redo step holding foo(200).
            array.push_back(new foo(300));
            Assert::AreEqual<int>(1, clean_up_count);
            Assert::AreEqual<int>(2, foo::instance_count);

            array.update(array.begin(), new foo(400));
            array.undo();
            // Discards the redo step holding foo(400).
            array.erase(array.begin());
            Assert::AreEqual<int>(2, clean_up_count);
            Assert::AreEqual<int>(2, foo::instance_count);

            array.reset();
            Assert::AreEqual<int>(4, clean_up_count);
            Assert::AreEqual<int>(0, foo::instance_count);
        }

        TEST_METHOD(pointer_vector)
        {
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

                {
                    undo_redo_pointer_vector<foo>::transaction transaction(array);
                    array.push_back(new foo(1600));
                    array.clear();
                }
                array.undo();
                // foo(800) has been deleted when its redo step was discarded.
                Assert::AreEqual<int>(5, foo::instance_count);
            }
            Assert::AreEqual<int>(0, foo::instance_count);
        }
    };

    TEST_CLASS(synchronized_undo_redo_vector_test)
    {
    public:
        TEST_METHOD(not_copyable_nor_movable)
        {
            Assert::IsFalse(std::is_copy_constructible_v<synchronized_undo_redo_vector<int>>);
            Assert::IsFalse(std::is_move_constructible_v<synchronized_undo_redo_vector<int>>);
        }

        TEST_METHOD(modify_by_index)
        {
            synchronized_undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(300);

            Assert::IsTrue(array.insert(1, 200));
            Assert::IsFalse(array.insert(4, 400));
            Assert::IsTrue(array.update(0, 150));
            Assert::IsFalse(array.update(3, 400));
            Assert::IsTrue(array.erase(2));
            Assert::IsFalse(array.erase(2));
            Assert::IsTrue(array.snapshot() == std::vector<int>{ 150, 200 });

            Assert::IsTrue(array.undo());
            Assert::IsTrue(array.undo());
            Assert::IsTrue(array.snapshot() == std::vector<int>{ 100, 200, 300 });
            Assert::IsTrue(array.redo());
            Assert::IsTrue(array.snapshot() == std::vector<int>{ 150, 200, 300 });
        }

        TEST_METHOD(read_by_value)
        {
            synchronized_undo_redo_vector<int> array;
            Assert::IsFalse(array.get(0).has_value());
            Assert::IsFalse(array.front().has_value());
            Assert::IsFalse(array.back().has_value());

            array.push_back(100);
            array.push_back(200);
            Assert::AreEqual<int>(200, *array.get(1));
            Assert::IsFalse(array.get(2).has_value());
            Assert::AreEqual<int>(100, *array.front());
            Assert::AreEqual<int>(200, *array.back());
            Assert::AreEqual<int>(300, array.read([](const auto& collection) { return collection[0] + collection[1]; }));
        }

        TEST_METHOD(modify_is_one_undo_step)
        {
            synchronized_undo_redo_vector<int> array;
            array.push_back(100);

            auto size = array.modify([](auto& collection) {
                collection.push_back(200);
                collection.erase(collection.begin());
                return collection.size();
            });
            Assert::AreEqual<size_t>(1, size);
            Assert::AreEqual<int>(200, *array.get(0));

            Assert::IsTrue(array.undo());
            Assert::IsTrue(array.snapshot() == std::vector<int>{ 100 });

            // An empty modification does not create an undo step.
            array.modify([](auto&) {});
            Assert::IsTrue(array.undo());
            Assert::IsTrue(array.empty());
        }

        TEST_METHOD(clear_and_reset)
        {
            synchronized_undo_redo_vector<int> array;
            array.push_back(100);
            array.push_back(200);

            array.clear();
            Assert::IsTrue(array.empty());
            Assert::IsTrue(array.undo());
            Assert::AreEqual<size_t>(2, array.size());

            array.reset();
            Assert::IsTrue(array.empty());
            Assert::IsFalse(array.can_undo());
            Assert::IsFalse(array.can_redo());
        }

        TEST_METHOD(concurrent_access)
        {
            std::atomic<int> live_count = 0;
            {
                synchronized_undo_redo_collection<int*> array([&](int*& element) { delete element; live_count--; });
                auto make = [&](int value) { live_count++; return new int(value); };

                std::vector<std::thread> threads;
                for (int thread_index = 0; thread_index < 8; thread_index++) {
                    threads.emplace_back([&, thread_index] {
                        for (int index = 0; index < 1000; index++) {
                            switch ((index + thread_index) % 6) {
                            case 0: case 1: array.push_back(make(index)); break;
                            case 2:         array.erase(0); break;
                            case 3:         array.undo(); break;
                            case 4:         array.redo(); break;
                            case 5:         array.modify([&](auto& collection) { collection.push_back(make(index)); collection.push_back(make(index)); }); break;
                            }
                        }
                    });
                }
                for (auto& thread : threads)
                    thread.join();
            }
            Assert::AreEqual<int>(0, live_count);
        }
    };

}
