#pragma once

// undo_redo_vector.h
//
// A header-only library that provides sequence containers with built-in undo / redo.
//
//   undo_redo_collection<T, C>              : the core container (single-threaded).
//   undo_redo_pointer_collection<T, C>      : an undo_redo_collection of T* that owns and deletes the objects.
//   synchronized_undo_redo_collection<T, C> : a thread-safe wrapper of undo_redo_collection.
//
//   undo_redo_vector<T>, undo_redo_pointer_vector<T>, synchronized_undo_redo_vector<T>
//                                           : aliases of the above that use std::vector as the underlying container.
//
// Example:
//
//   shos::undo_redo_vector<int> numbers;
//   numbers.push_back(1);              // { 1 }
//   numbers.push_back(2);              // { 1, 2 }
//   numbers.undo();                    // { 1 }
//   numbers.redo();                    // { 1, 2 }
//   {
//       shos::undo_redo_vector<int>::transaction transaction(numbers);
//       numbers.push_back(3);
//       numbers.erase(numbers.begin());
//   }                                  // { 2, 3 }
//   numbers.undo();                    // { 1, 2 }  (the whole transaction is undone at once)

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace shos {

// A collection that records every modification so that it can be undone / redone.
//
// - Elements can be modified only through push_back / insert / erase / update / clear,
//   so begin() / end() return const_iterator.
// - Multiple modifications can be grouped into one undo step with transaction.
//   Transactions can be nested; only the outermost one creates the undo step.
// - undo() / redo() do nothing (return false) while a transaction is in progress.
// - If a clean_up function is given, it is called exactly once for each element
//   that leaves the collection for good (removed and its undo history discarded,
//   or the collection is reset / destroyed).
// - Not copyable. Movable, but must not be moved while a transaction is in progress.
//
// TElement    : the element type. It only needs to be movable (move-only types such as std::unique_ptr are fine).
// TCollection : the underlying sequence container (e.g. std::vector, std::list).
//               It must support begin / end / insert / erase / front / back / size / empty / clear.
//
// How the undo history works:
//
//   undo_steps holds every recorded step, oldest first, and undo_steps_index splits it into two parts.
//
//       undo_steps:  [ step0 | step1 | step2 | step3 | step4 ]
//                                          ^
//                                  undo_steps_index == 3
//
//   - Steps before undo_steps_index (step0 - step2) have been applied; undo() reverts step2 and moves the index left.
//   - Steps from undo_steps_index on (step3, step4) have been undone; redo() reapplies step3 and moves the index right.
//   - A new modification discards step3 and step4 (they can no longer be redone) and is appended at the index.
template <typename TElement, typename TCollection = std::vector<TElement>>
class undo_redo_collection
{
public:
    using value_type        = TElement;
    using size_type         = std::size_t;
    using const_reference   = const TElement&;
    using const_iterator    = typename TCollection::const_iterator;
    // Elements must not be modified directly (that would bypass the undo history), so iterator is read-only too.
    using iterator          = const_iterator;
    // Called for each element that leaves the collection for good (e.g. to delete an owned pointer).
    using clean_up_function = std::function<void(TElement&)>;

private:
    // One undoable modification of the collection.
    //
    // Each step keeps whatever is needed to revert itself, and undo / redo
    // always alternate: a step is created right after redo has been applied,
    // so undo is always called first, then redo, then undo, and so on.
    class undo_step
    {
    public:
        virtual ~undo_step() = default;

        // Reverts this step on collection.
        virtual void undo(TCollection& collection) = 0;
        // Applies (or reapplies) this step on collection.
        virtual void redo(TCollection& collection) = 0;

        // Passes the elements held only by this step (i.e. not in the collection) to clean_up.
        virtual void clean_up_elements(const clean_up_function& clean_up) = 0;
    };

    // Insertion and removal are inverse operations of each other.
    // While the element is held by this step, redo/undo inserts it at index;
    // otherwise redo/undo moves the element at index out of the collection into this step.
    //
    //   insert step: holds the element  --redo-->  element is in the collection  --undo-->  holds the element ...
    //   remove step: holds nothing      --redo-->  holds the removed element     --undo-->  holds nothing ...
    class insert_or_remove_step : public undo_step
    {
        std::size_t             index;   // Position of the element in the collection.
        std::optional<TElement> element; // The element while it is out of the collection; empty while it is in.

    public:
        // Creates a step that inserts element at index when it is redone.
        static std::unique_ptr<undo_step> insert(std::size_t index, TElement element)
        {
            return std::make_unique<insert_or_remove_step>(index, std::optional<TElement>(std::move(element)));
        }

        // Creates a step that removes the element at index when it is redone.
        static std::unique_ptr<undo_step> remove(std::size_t index)
        {
            return std::make_unique<insert_or_remove_step>(index, std::nullopt);
        }

        insert_or_remove_step(std::size_t position, std::optional<TElement> held_element)
            : index(position), element(std::move(held_element))
        {}

        void undo(TCollection& collection) override
        {
            toggle(collection);
        }

        void redo(TCollection& collection) override
        {
            toggle(collection);
        }

        void clean_up_elements(const clean_up_function& clean_up) override
        {
            // Only an element held by this step is out of the collection;
            // otherwise the collection owns it and will clean it up itself.
            if (element.has_value())
                clean_up(*element);
        }

    private:
        // Moves the element between this step and the collection, in whichever direction applies.
        void toggle(TCollection& collection)
        {
            if (element.has_value()) {
                // Held by this step: put it back into the collection.
                collection.insert(std::next(collection.begin(), index), std::move(*element));
                element.reset();
            } else {
                // In the collection: take it out and keep it here.
                auto position = std::next(collection.begin(), index);
                element.emplace(std::move(*position));
                collection.erase(position);
            }
        }
    };

    // Swaps the element at index with the one held by this step.
    // Before redo it holds the new value; after redo it holds the old value, and vice versa.
    class update_step : public undo_step
    {
        std::size_t index;   // Position of the updated element in the collection.
        TElement    element; // The value that is not currently in the collection (new or old).

    public:
        update_step(std::size_t position, TElement held_element) : index(position), element(std::move(held_element))
        {}

        void undo(TCollection& collection) override
        {
            swap_element(collection);
        }

        void redo(TCollection& collection) override
        {
            swap_element(collection);
        }

        void clean_up_elements(const clean_up_function& clean_up) override
        {
            // The held value is never in the collection.
            clean_up(element);
        }

    private:
        void swap_element(TCollection& collection)
        {
            // Allows a custom swap of TElement to be found by argument-dependent lookup.
            using std::swap;
            swap(*std::next(collection.begin(), index), element);
        }
    };

    // A sequence of steps that is undone / redone as a single step (built by a transaction).
    class group_step : public undo_step
    {
        std::vector<std::unique_ptr<undo_step>> steps; // In the order they were applied.

    public:
        bool empty() const
        {
            return steps.empty();
        }

        // Makes sure that the next push_back does not allocate (and therefore cannot throw).
        void reserve_one_more()
        {
            steps.reserve(steps.size() + 1);
        }

        void push_back(std::unique_ptr<undo_step> step)
        {
            steps.push_back(std::move(step));
        }

        // Reverts the steps in reverse order, since each step depends on the state left by the previous ones.
        void undo(TCollection& collection) override
        {
            std::for_each(steps.rbegin(), steps.rend(), [&](const std::unique_ptr<undo_step>& step) { step->undo(collection); });
        }

        // Reapplies the steps in their original order.
        void redo(TCollection& collection) override
        {
            std::for_each(steps.begin(), steps.end(), [&](const std::unique_ptr<undo_step>& step) { step->redo(collection); });
        }

        void clean_up_elements(const clean_up_function& clean_up) override
        {
            std::for_each(steps.begin(), steps.end(), [&](const std::unique_ptr<undo_step>& step) { step->clean_up_elements(clean_up); });
        }
    };

    TCollection                             data;                  // The current elements.
    std::vector<std::unique_ptr<undo_step>> undo_steps;            // The undo history, oldest first.
    std::size_t                             undo_steps_index = 0;  // Number of applied steps; undo_steps[undo_steps_index] is the next redo.
    std::unique_ptr<group_step>             transaction_group;     // Collects the steps of the transaction in progress; null otherwise.
    std::size_t                             transaction_depth = 0; // Nesting level of transactions; 0 means none is in progress.
    clean_up_function                       clean_up;              // May be empty; then nothing is called.

public:
    // An RAII scope that groups all the modifications made during its lifetime into one undo step.
    //
    //   {
    //       undo_redo_vector<int>::transaction transaction(array);
    //       array.push_back(1);
    //       array.push_back(2);
    //   }   // Both push_backs become one undo step here.
    //
    // A transaction that made no modification creates no undo step.
    // If an exception leaves the scope, the modifications made so far are still kept as one undo step.
    class transaction
    {
        undo_redo_collection& collection;

    public:
        explicit transaction(undo_redo_collection& target) : collection(target)
        {
            collection.begin_transaction();
        }

        ~transaction()
        {
            collection.end_transaction();
        }

        transaction(const transaction&)            = delete;
        transaction& operator=(const transaction&) = delete;
    };

    undo_redo_collection() = default;

    // function is called for each element that leaves the collection for good.
    explicit undo_redo_collection(clean_up_function function) : clean_up(std::move(function))
    {}

    undo_redo_collection(const undo_redo_collection&)            = delete;
    undo_redo_collection& operator=(const undo_redo_collection&) = delete;

    // Takes over the elements, the undo history and the clean_up function.
    // other is left empty without a clean_up function, so that nothing is cleaned up twice.
    undo_redo_collection(undo_redo_collection&& other)
        : data             (std::move(other.data))
        , undo_steps       (std::move(other.undo_steps))
        , undo_steps_index (std::exchange(other.undo_steps_index, 0))
        , transaction_group(std::move(other.transaction_group))
        , transaction_depth(std::exchange(other.transaction_depth, 0))
        , clean_up         (std::exchange(other.clean_up, nullptr))
    {
        // A moved-from standard container is only guaranteed to be valid, not empty.
        other.data.clear();
        other.undo_steps.clear();
    }

    // Cleans up the current elements and history first, then takes over those of other.
    undo_redo_collection& operator=(undo_redo_collection&& other)
    {
        if (this != &other) {
            clean_up_all();
            data              = std::move(other.data);
            undo_steps        = std::move(other.undo_steps);
            undo_steps_index  = std::exchange(other.undo_steps_index, 0);
            transaction_group = std::move(other.transaction_group);
            transaction_depth = std::exchange(other.transaction_depth, 0);
            clean_up          = std::exchange(other.clean_up, nullptr);
            other.data.clear();
            other.undo_steps.clear();
        }
        return *this;
    }

    virtual ~undo_redo_collection()
    {
        clean_up_all();
    }

    // ---- Element access (read-only) ----

    // No bounds checking. Uses std::next so that it also works with non-random-access containers such as std::list.
    [[nodiscard]] const_reference operator[](std::size_t index) const
    {
        return *std::next(data.begin(), index);
    }

    // Throws std::out_of_range if index >= size().
    [[nodiscard]] const_reference at(std::size_t index) const
    {
        if (index >= data.size())
            throw std::out_of_range("undo_redo_collection::at: index out of range");
        return (*this)[index];
    }

    // The collection must not be empty.
    [[nodiscard]] const_reference front() const
    {
        return data.front();
    }

    // The collection must not be empty.
    [[nodiscard]] const_reference back() const
    {
        return data.back();
    }

    [[nodiscard]] std::size_t size() const
    {
        return data.size();
    }

    [[nodiscard]] bool empty() const
    {
        return data.empty();
    }

    // ---- Iterators (always const) ----

    [[nodiscard]] const_iterator begin() const
    {
        return data.cbegin();
    }

    [[nodiscard]] const_iterator end() const
    {
        return data.cend();
    }

    [[nodiscard]] const_iterator cbegin() const
    {
        return data.cbegin();
    }

    [[nodiscard]] const_iterator cend() const
    {
        return data.cend();
    }

    // ---- Modifiers (each one is recorded as an undo step) ----
    //
    // Iterators are converted to indices, because an undo step must stay valid
    // after the iterators obtained before it have been invalidated.

    // Appends element to the end.
    void push_back(TElement element)
    {
        execute(insert_or_remove_step::insert(data.size(), std::move(element)));
    }

    // Inserts element before position.
    void insert(const_iterator position, TElement element)
    {
        execute(insert_or_remove_step::insert(index_of(position), std::move(element)));
    }

    // Removes the element at position. It is kept in the undo history, so clean_up is not called yet.
    void erase(const_iterator position)
    {
        execute(insert_or_remove_step::remove(index_of(position)));
    }

    // Replaces the element at position with element. The old value is kept in the undo history.
    void update(const_iterator position, TElement element)
    {
        execute(std::make_unique<update_step>(index_of(position), std::move(element)));
    }

    // Removes all the elements as one undo step.
    void clear()
    {
        transaction scope(*this);
        // Erases from the back, which is the cheapest for std::vector.
        while (!data.empty())
            erase(std::prev(data.cend()));
    }

    // Removes all the elements and the undo history. This cannot be undone.
    // clean_up is called for all of them. Throws std::logic_error during a transaction.
    void reset()
    {
        if (in_transaction())
            throw std::logic_error("undo_redo_collection::reset: cannot reset during a transaction");

        clean_up_all();
    }

    // ---- Undo / redo ----

    // Reverts the last applied step. Returns false if there is nothing to undo or a transaction is in progress.
    bool undo()
    {
        if (!can_undo())
            return false;

        undo_steps[undo_steps_index - 1]->undo(data);
        undo_steps_index--;
        return true;
    }

    // Reapplies the last undone step. Returns false if there is nothing to redo or a transaction is in progress.
    bool redo()
    {
        if (!can_redo())
            return false;

        undo_steps[undo_steps_index]->redo(data);
        undo_steps_index++;
        return true;
    }

    [[nodiscard]] bool can_undo() const
    {
        return !in_transaction() && undo_steps_index != 0;
    }

    [[nodiscard]] bool can_redo() const
    {
        return !in_transaction() && undo_steps_index != undo_steps.size();
    }

    [[nodiscard]] bool in_transaction() const
    {
        return transaction_depth != 0;
    }

private:
    std::size_t index_of(const_iterator position) const
    {
        return static_cast<std::size_t>(std::distance(data.cbegin(), position));
    }

    // Called by the constructor of transaction. Only the outermost transaction starts a new group.
    void begin_transaction()
    {
        if (transaction_depth == 0) {
            // Reserve here so that end_transaction, called from a destructor, never allocates.
            undo_steps.reserve(undo_steps_index + 1);
            transaction_group = std::make_unique<group_step>();
        }
        transaction_depth++;
    }

    // Called by the destructor of transaction, so it must not throw.
    // When the outermost transaction ends, its group is recorded as one undo step (unless it is empty).
    void end_transaction() noexcept
    {
        // transaction_depth can already be 0 if the collection was (wrongly) moved during the transaction.
        if (transaction_depth == 0 || --transaction_depth != 0)
            return;

        auto group = std::move(transaction_group);
        if (!group->empty())
            push_to_undo_steps(std::move(group));
    }

    // Applies the step to data, then records it.
    // Space for the step is reserved first so that a failure leaves the collection unchanged.
    // During a transaction the step is added to the transaction's group instead of the undo history.
    void execute(std::unique_ptr<undo_step> step)
    {
        if (in_transaction()) {
            transaction_group->reserve_one_more();
            step->redo(data);
            transaction_group->push_back(std::move(step));
        } else {
            undo_steps.reserve(undo_steps_index + 1);
            step->redo(data);
            push_to_undo_steps(std::move(step));
        }
    }

    // Discards the steps that can be redone, then appends step as the latest undo step.
    // The capacity must have been reserved beforehand.
    void push_to_undo_steps(std::unique_ptr<undo_step> step) noexcept
    {
        discard_undo_steps(undo_steps_index);
        undo_steps.push_back(std::move(step));
        undo_steps_index++;
    }

    // Removes undo_steps[first] and the steps after it, cleaning up the elements only they hold.
    void discard_undo_steps(std::size_t first) noexcept
    {
        if (clean_up)
            std::for_each(undo_steps.begin() + first, undo_steps.end(), [&](const std::unique_ptr<undo_step>& step) { step->clean_up_elements(clean_up); });
        undo_steps.erase(undo_steps.begin() + first, undo_steps.end());
    }

    // Discards everything: the transaction in progress, the whole undo history and the elements.
    // Every element is cleaned up exactly once, since each one is either in data or held by exactly one step.
    void clean_up_all() noexcept
    {
        if (transaction_group && clean_up)
            transaction_group->clean_up_elements(clean_up);
        transaction_group.reset();
        transaction_depth = 0;

        discard_undo_steps(0);
        undo_steps_index = 0;

        if (clean_up)
            std::for_each(data.begin(), data.end(), [&](TElement& element) { clean_up(element); });
        data.clear();
    }
};

// An undo_redo_collection of pointers that owns the pointed objects and deletes them when no longer needed.
// The pointers given to push_back / insert / update must have been allocated with new.
//
//   undo_redo_pointer_vector<foo> array;
//   array.push_back(new foo());  // Deleted by array; do not delete it yourself.
template <typename TElement, typename TCollection = std::vector<TElement*>>
class undo_redo_pointer_collection : public undo_redo_collection<TElement*, TCollection>
{
public:
    undo_redo_pointer_collection() : undo_redo_collection<TElement*, TCollection>([](TElement*& pointer) { delete pointer; })
    {}
};

// A thread-safe undo_redo_collection: every member function can be called from multiple threads at the same time.
//
// - References and iterators would outlive the lock, so elements are read by value (get / front / back / snapshot)
//   or inside read(function), and specified by index instead of iterator.
// - modify(function) applies multiple modifications as one undo step while holding the lock,
//   so modifications from other threads never get mixed into it.
// - The functions passed to read / modify and the clean_up function are called while the lock is held,
//   so they must not access this synchronized_undo_redo_collection (that would deadlock).
// - The undo history is shared by all the threads: undo() undoes the last modification made by any thread.
// - Neither copyable nor movable.
//
// Read-only member functions take a shared lock (they can run at the same time),
// and modifying ones take an exclusive lock.
template <typename TElement, typename TCollection = std::vector<TElement>>
class synchronized_undo_redo_collection
{
public:
    using collection_type   = undo_redo_collection<TElement, TCollection>;
    using value_type        = TElement;
    using size_type         = std::size_t;
    using clean_up_function = typename collection_type::clean_up_function;

private:
    mutable std::shared_mutex mutex;      // mutable so that const member functions can lock it.
    collection_type           collection; // Accessed only while mutex is held.

public:
    synchronized_undo_redo_collection() = default;

    // function is called for each element that leaves the collection for good (while the lock is held).
    explicit synchronized_undo_redo_collection(clean_up_function function) : collection(std::move(function))
    {}

    // std::shared_mutex is neither copyable nor movable, so neither is this class.
    synchronized_undo_redo_collection(const synchronized_undo_redo_collection&)            = delete;
    synchronized_undo_redo_collection& operator=(const synchronized_undo_redo_collection&) = delete;

    // ---- Element access (by value) ----
    //
    // Each returns std::nullopt instead of throwing when there is no such element,
    // because the size may change between a size() check and the access.

    // Returns a copy of the element at index, or std::nullopt if index >= size().
    [[nodiscard]] std::optional<TElement> get(std::size_t index) const
    {
        std::shared_lock lock(mutex);
        if (index >= collection.size())
            return std::nullopt;
        return collection[index];
    }

    // Returns a copy of the first element, or std::nullopt if empty.
    [[nodiscard]] std::optional<TElement> front() const
    {
        std::shared_lock lock(mutex);
        if (collection.empty())
            return std::nullopt;
        return collection.front();
    }

    // Returns a copy of the last element, or std::nullopt if empty.
    [[nodiscard]] std::optional<TElement> back() const
    {
        std::shared_lock lock(mutex);
        if (collection.empty())
            return std::nullopt;
        return collection.back();
    }

    // Returns a copy of all the elements.
    [[nodiscard]] TCollection snapshot() const
    {
        std::shared_lock lock(mutex);
        return TCollection(collection.begin(), collection.end());
    }

    [[nodiscard]] std::size_t size() const
    {
        std::shared_lock lock(mutex);
        return collection.size();
    }

    [[nodiscard]] bool empty() const
    {
        std::shared_lock lock(mutex);
        return collection.empty();
    }

    // ---- Locked access to the underlying collection ----

    // Calls function(const collection_type&) while holding a shared lock, and returns its result.
    // The result is returned by value so that no reference to the elements escapes the lock.
    template <typename TFunction>
    auto read(TFunction&& function) const
    {
        std::shared_lock lock(mutex);
        return std::invoke(std::forward<TFunction>(function), collection);
    }

    // Calls function(collection_type&) while holding an exclusive lock, and returns its result.
    // All the modifications made in function become one undo step.
    // If function throws, the modifications made so far are kept as one undo step, as with transaction.
    // function must not keep references or iterators to collection_type after it returns.
    template <typename TFunction>
    auto modify(TFunction&& function)
    {
        // The lock is declared first so that it is released after the transaction has ended.
        std::scoped_lock                      lock(mutex);
        typename collection_type::transaction scope(collection);
        return std::invoke(std::forward<TFunction>(function), collection);
    }

    // ---- Modifiers (by index; each one is recorded as an undo step) ----
    //
    // Out-of-range indices are reported by the return value instead of an exception,
    // because the size may change between a size() check and the call.

    void push_back(TElement element)
    {
        std::scoped_lock lock(mutex);
        collection.push_back(std::move(element));
    }

    // Inserts element before index. Returns false (without changing anything) if index > size().
    bool insert(std::size_t index, TElement element)
    {
        std::scoped_lock lock(mutex);
        if (index > collection.size())
            return false;
        collection.insert(std::next(collection.begin(), index), std::move(element));
        return true;
    }

    // Removes the element at index. Returns false (without changing anything) if index >= size().
    bool erase(std::size_t index)
    {
        std::scoped_lock lock(mutex);
        if (index >= collection.size())
            return false;
        collection.erase(std::next(collection.begin(), index));
        return true;
    }

    // Replaces the element at index with element. Returns false (without changing anything) if index >= size().
    bool update(std::size_t index, TElement element)
    {
        std::scoped_lock lock(mutex);
        if (index >= collection.size())
            return false;
        collection.update(std::next(collection.begin(), index), std::move(element));
        return true;
    }

    // Removes all the elements as one undo step.
    void clear()
    {
        std::scoped_lock lock(mutex);
        collection.clear();
    }

    // Removes all the elements and the undo history. This cannot be undone.
    void reset()
    {
        std::scoped_lock lock(mutex);
        collection.reset();
    }

    // ---- Undo / redo ----

    // Reverts the last modification made by any thread. Returns false if there is nothing to undo.
    bool undo()
    {
        std::scoped_lock lock(mutex);
        return collection.undo();
    }

    // Reapplies the last undone modification. Returns false if there is nothing to redo.
    bool redo()
    {
        std::scoped_lock lock(mutex);
        return collection.redo();
    }

    // The result may already be out of date when it is returned, since other threads can modify the collection.
    [[nodiscard]] bool can_undo() const
    {
        std::shared_lock lock(mutex);
        return collection.can_undo();
    }

    // The result may already be out of date when it is returned, since other threads can modify the collection.
    [[nodiscard]] bool can_redo() const
    {
        std::shared_lock lock(mutex);
        return collection.can_redo();
    }
};

// Aliases that use std::vector as the underlying container.

template <typename TElement>
using undo_redo_vector = undo_redo_collection<TElement>;

template <typename TElement>
using undo_redo_pointer_vector = undo_redo_pointer_collection<TElement>;

template <typename TElement>
using synchronized_undo_redo_vector = synchronized_undo_redo_collection<TElement>;

} // namespace shos
