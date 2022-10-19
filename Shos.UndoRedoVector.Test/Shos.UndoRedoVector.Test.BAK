#include "pch.h"
#include "CppUnitTest.h"
#include "..\undo_redo_vector.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace ShosUndoRedoVectorTest
{
	using namespace shos;

	TEST_CLASS(undo_redo_vector_test)
	{
	public:
		TEST_METHOD(check)
		{
			std::vector<int> array = { 0, 1, 1, 2, 3, 5, 8, 13 };
			std::vector<int>::iterator iterator = array.begin();

			for (; iterator != array.begin(); iterator++)
			{
				Assert::AreEqual(*iterator, array[iterator - array.begin()]);
			}
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
			Assert::AreEqual<size_t>(array[0], 100);

			array.push_back(300);

			Assert::AreEqual<size_t>(array.size(), 2UL);
			Assert::AreEqual<size_t>(array[0], 100);
			Assert::AreEqual<size_t>(array[1], 300);
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

			array.erase(array.begin() + 1);
			Assert::AreEqual<size_t>(array.size(), 3UL);
			Assert::AreEqual<size_t>(array[0], 400);
			Assert::AreEqual<size_t>(array[1], 600);
			Assert::AreEqual<size_t>(array[2], 700);

			array.erase(array.begin() + 2);
			Assert::AreEqual<size_t>(array.size(), 2UL);
			Assert::AreEqual<size_t>(array[0], 400);
			Assert::AreEqual<size_t>(array[1], 600);

			array.erase(array.begin());
			Assert::AreEqual<size_t>(array.size(), 1UL);
			Assert::AreEqual<size_t>(array[0], 600);

			array.erase(array.begin());
			Assert::AreEqual<size_t>(array.size(), 0UL);
		}

		TEST_METHOD(update)
		{
			undo_redo_vector<int> array;
			array.push_back(100);
			array.push_back(200);
			array.push_back(400);

			array.update(array.begin() + 1, 1200);
			Assert::AreEqual<size_t>(array.size(), 3UL);
			Assert::AreEqual<size_t>(array[0], 100);
			Assert::AreEqual<size_t>(array[1], 1200);
			Assert::AreEqual<size_t>(array[2], 400);
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
			Assert::AreEqual<size_t>(array[0], 400);
			Assert::AreEqual<size_t>(array[1], 500);

			Assert::IsTrue(array.undo());
			Assert::AreEqual<size_t>(array.size(), 1UL);
			Assert::AreEqual<size_t>(array[0], 400);

			Assert::IsTrue(array.undo());
			Assert::AreEqual<size_t>(array.size(), 0UL);

			Assert::IsFalse(array.can_undo());
			Assert::IsFalse(array.undo());

			array.push_back(1000);
			array.push_back(1100);
			Assert::AreEqual<size_t>(array.size(), 2UL);
			Assert::AreEqual<size_t>(array[0], 1000);
			Assert::AreEqual<size_t>(array[1], 1100);
		}

		TEST_METHOD(undo_erase)
		{
			undo_redo_vector<int> array;
			array.push_back(100);
			array.push_back(200);
			array.push_back(400);

			array.erase(array.begin() + 1);
			Assert::IsTrue(array.undo());

			Assert::AreEqual<size_t>(array.size(), 3UL);
			Assert::AreEqual<size_t>(array[0], 100);
			Assert::AreEqual<size_t>(array[1], 200);
			Assert::AreEqual<size_t>(array[2], 400);

			array.erase(array.begin() + 2);
			array.erase(array.begin() + 1);
			array.erase(array.begin());

			Assert::IsTrue(array.can_undo());
			Assert::IsTrue(array.undo());
			Assert::AreEqual<size_t>(array.size(), 1UL);
			Assert::AreEqual<size_t>(array[0], 100);

			Assert::IsTrue(array.undo());
			Assert::AreEqual<size_t>(array.size(), 2UL);
			Assert::AreEqual<size_t>(array[0], 100);
			Assert::AreEqual<size_t>(array[1], 200);

			Assert::IsTrue(array.undo());
			Assert::AreEqual<size_t>(array.size(), 3UL);
			Assert::AreEqual<size_t>(array[0], 100);
			Assert::AreEqual<size_t>(array[1], 200);
			Assert::AreEqual<size_t>(array[2], 400);

			Assert::IsTrue(array.undo());
			Assert::AreEqual<size_t>(array.size(), 2UL);
			Assert::AreEqual<size_t>(array[0], 100);
			Assert::AreEqual<size_t>(array[1], 200);
		}

		TEST_METHOD(undo_update)
		{
			undo_redo_vector<int> array;
			array.push_back(100);
			array.push_back(200);
			array.push_back(400);

			array.update(array.begin() + 1, 1200);
			Assert::AreEqual<size_t>(array.size(), 3UL);
			Assert::AreEqual<size_t>(array[0], 100);
			Assert::AreEqual<size_t>(array[1], 1200);
			Assert::AreEqual<size_t>(array[2], 400);

			Assert::IsTrue(array.can_undo());
			Assert::IsTrue(array.undo());
			Assert::AreEqual<size_t>(array.size(), 3UL);
			Assert::AreEqual<size_t>(array[0], 100);
			Assert::AreEqual<size_t>(array[1], 200);
			Assert::AreEqual<size_t>(array[2], 400);
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
			Assert::AreEqual<size_t>(array[0], 300);
			Assert::AreEqual<size_t>(array[1], 600);
			Assert::AreEqual<size_t>(array[2], 800);

			array.erase(array.begin() + 1);
			array.update(array.begin(), 1200);
			array.erase(array.begin());
			array.push_back(1400);

			Assert::AreEqual<size_t>(array.size(), 2UL);
			Assert::AreEqual<size_t>(array[0], 800);
			Assert::AreEqual<size_t>(array[1], 1400);

			array.undo();
			Assert::AreEqual<size_t>(array.size(), 1UL);
			Assert::AreEqual<size_t>(array[0], 800);

			array.undo();
			Assert::AreEqual<size_t>(array.size(), 2UL);
			Assert::AreEqual<size_t>(array[0], 1200);
			Assert::AreEqual<size_t>(array[1], 800);

			array.undo();
			Assert::AreEqual<size_t>(array.size(), 2UL);
			Assert::AreEqual<size_t>(array[0], 300);
			Assert::AreEqual<size_t>(array[1], 800);

			Assert::IsTrue(array.can_redo());
			Assert::IsTrue(array.redo());
			Assert::AreEqual<size_t>(array.size(), 2UL);
			Assert::AreEqual<size_t>(array[0], 1200);
			Assert::AreEqual<size_t>(array[1], 800);

			Assert::IsTrue(array.redo());
			Assert::AreEqual<size_t>(array.size(), 1UL);
			Assert::AreEqual<size_t>(array[0], 800);

			Assert::IsTrue(array.redo());
			Assert::AreEqual<size_t>(array.size(), 2UL);
			Assert::AreEqual<size_t>(array[0], 800);
			Assert::AreEqual<size_t>(array[1], 1400);

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
			Assert::AreEqual<size_t>(array[0], 400);

			Assert::IsTrue(array.undo());
			Assert::AreEqual<size_t>(array.size(), 2UL);
			Assert::AreEqual<size_t>(array[0], 300);
			Assert::AreEqual<size_t>(array[1], 400);

			Assert::IsTrue(array.undo());
			Assert::AreEqual<size_t>(array.size(), 1UL);
			Assert::AreEqual<size_t>(array[0], 100);

			Assert::IsTrue(array.undo());
			Assert::AreEqual<size_t>(array.size(), 0UL);
			Assert::IsFalse(array.can_undo());

			Assert::IsTrue(array.redo());
			Assert::AreEqual<size_t>(array.size(), 1UL);
			Assert::AreEqual<size_t>(array[0], 100);

			Assert::IsTrue(array.redo());
			Assert::AreEqual<size_t>(array.size(), 2UL);
			Assert::AreEqual<size_t>(array[0], 300);
			Assert::AreEqual<size_t>(array[1], 400);

			Assert::IsTrue(array.redo());
			Assert::AreEqual<size_t>(array.size(), 1UL);
			Assert::AreEqual<size_t>(array[0], 400);

			Assert::IsFalse(array.can_redo());

			Assert::IsTrue(array.undo());
			Assert::AreEqual<size_t>(array.size(), 2UL);
			Assert::AreEqual<size_t>(array[0], 300);
			Assert::AreEqual<size_t>(array[1], 400);
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
			Assert::AreEqual<size_t>(array[0], 100);
			Assert::AreEqual<size_t>(array[1], 200);

			Assert::IsTrue(array.redo());
			Assert::AreEqual<size_t>(array.size(), 0UL);

			Assert::IsTrue(array.undo());
			Assert::AreEqual<size_t>(array.size(), 2UL);
			Assert::AreEqual<size_t>(array[0], 100);
			Assert::AreEqual<size_t>(array[1], 200);
		}
	};
}
