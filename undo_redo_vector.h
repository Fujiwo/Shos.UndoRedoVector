#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
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
template <typename TElement, typename TCollection = std::vector<TElement>>
class undo_redo_collection
{
public:
    using value_type        = TElement;
    using size_type         = std::size_t;
    using const_reference   = const TElement&;
    using const_iterator    = typename TCollection::const_iterator;
    using iterator          = const_iterator;
    using clean_up_function = std::function<void(TElement&)>;

private:
    class undo_step
    {
    public:
        virtual ~undo_step() = default;

        virtual void undo(TCollection& collection) = 0;
        virtual void redo(TCollection& collection) = 0;

        // Passes the elements held only by this step (i.e. not in the collection) to clean_up.
        virtual void clean_up_elements(const clean_up_function& clean_up) = 0;
    };

    // Insertion and removal are inverse operations of each other.
    // While the element is held by this step, redo/undo inserts it at index;
    // otherwise redo/undo moves the element at index out of the collection into this step.
    class insert_or_remove_step : public undo_step
    {
        std::size_t             index;
        std::optional<TElement> element;

    public:
        static std::unique_ptr<undo_step> insert(std::size_t index, TElement element)
        {
            return std::make_unique<insert_or_remove_step>(index, std::optional<TElement>(std::move(element)));
        }

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
            if (element.has_value())
                clean_up(*element);
        }

    private:
        void toggle(TCollection& collection)
        {
            if (element.has_value()) {
                collection.insert(std::next(collection.begin(), index), std::move(*element));
                element.reset();
            } else {
                auto position = std::next(collection.begin(), index);
                element.emplace(std::move(*position));
                collection.erase(position);
            }
        }
    };

    // Swaps the element at index with the one held by this step.
    class update_step : public undo_step
    {
        std::size_t index;
        TElement    element;

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
            clean_up(element);
        }

    private:
        void swap_element(TCollection& collection)
        {
            using std::swap;
            swap(*std::next(collection.begin(), index), element);
        }
    };

    class group_step : public undo_step
    {
        std::vector<std::unique_ptr<undo_step>> steps;

    public:
        bool empty() const
        {
            return steps.empty();
        }

        void reserve_one_more()
        {
            steps.reserve(steps.size() + 1);
        }

        void push_back(std::unique_ptr<undo_step> step)
        {
            steps.push_back(std::move(step));
        }

        void undo(TCollection& collection) override
        {
            std::for_each(steps.rbegin(), steps.rend(), [&](const std::unique_ptr<undo_step>& step) { step->undo(collection); });
        }

        void redo(TCollection& collection) override
        {
            std::for_each(steps.begin(), steps.end(), [&](const std::unique_ptr<undo_step>& step) { step->redo(collection); });
        }

        void clean_up_elements(const clean_up_function& clean_up) override
        {
            std::for_each(steps.begin(), steps.end(), [&](const std::unique_ptr<undo_step>& step) { step->clean_up_elements(clean_up); });
        }
    };

    TCollection                             data;
    std::vector<std::unique_ptr<undo_step>> undo_steps;
    std::size_t                             undo_steps_index = 0;
    std::unique_ptr<group_step>             transaction_group;
    std::size_t                             transaction_depth = 0;
    clean_up_function                       clean_up;

public:
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

    explicit undo_redo_collection(clean_up_function function) : clean_up(std::move(function))
    {}

    undo_redo_collection(const undo_redo_collection&)            = delete;
    undo_redo_collection& operator=(const undo_redo_collection&) = delete;

    undo_redo_collection(undo_redo_collection&& other)
        : data             (std::move(other.data))
        , undo_steps       (std::move(other.undo_steps))
        , undo_steps_index (std::exchange(other.undo_steps_index, 0))
        , transaction_group(std::move(other.transaction_group))
        , transaction_depth(std::exchange(other.transaction_depth, 0))
        , clean_up         (std::exchange(other.clean_up, nullptr))
    {
        other.data.clear();
        other.undo_steps.clear();
    }

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

    const_reference operator[](std::size_t index) const
    {
        return *std::next(data.begin(), index);
    }

    const_reference at(std::size_t index) const
    {
        if (index >= data.size())
            throw std::out_of_range("undo_redo_collection::at: index out of range");
        return (*this)[index];
    }

    const_reference front() const
    {
        return data.front();
    }

    const_reference back() const
    {
        return data.back();
    }

    std::size_t size() const
    {
        return data.size();
    }

    bool empty() const
    {
        return data.empty();
    }

    const_iterator begin() const
    {
        return data.cbegin();
    }

    const_iterator end() const
    {
        return data.cend();
    }

    const_iterator cbegin() const
    {
        return data.cbegin();
    }

    const_iterator cend() const
    {
        return data.cend();
    }

    void push_back(TElement element)
    {
        execute(insert_or_remove_step::insert(data.size(), std::move(element)));
    }

    void insert(const_iterator position, TElement element)
    {
        execute(insert_or_remove_step::insert(index_of(position), std::move(element)));
    }

    void erase(const_iterator position)
    {
        execute(insert_or_remove_step::remove(index_of(position)));
    }

    void update(const_iterator position, TElement element)
    {
        execute(std::make_unique<update_step>(index_of(position), std::move(element)));
    }

    // Removes all the elements as one undo step.
    void clear()
    {
        transaction scope(*this);
        while (!data.empty())
            erase(std::prev(data.cend()));
    }

    // Removes all the elements and the undo history. This cannot be undone.
    void reset()
    {
        if (in_transaction())
            throw std::logic_error("undo_redo_collection::reset: cannot reset during a transaction");

        clean_up_all();
    }

    bool undo()
    {
        if (!can_undo())
            return false;

        undo_steps[undo_steps_index - 1]->undo(data);
        undo_steps_index--;
        return true;
    }

    bool redo()
    {
        if (!can_redo())
            return false;

        undo_steps[undo_steps_index]->redo(data);
        undo_steps_index++;
        return true;
    }

    bool can_undo() const
    {
        return !in_transaction() && undo_steps_index != 0;
    }

    bool can_redo() const
    {
        return !in_transaction() && undo_steps_index != undo_steps.size();
    }

    bool in_transaction() const
    {
        return transaction_depth != 0;
    }

private:
    std::size_t index_of(const_iterator position) const
    {
        return static_cast<std::size_t>(std::distance(data.cbegin(), position));
    }

    void begin_transaction()
    {
        if (transaction_depth == 0) {
            // Reserve here so that end_transaction, called from a destructor, never allocates.
            undo_steps.reserve(undo_steps_index + 1);
            transaction_group = std::make_unique<group_step>();
        }
        transaction_depth++;
    }

    void end_transaction() noexcept
    {
        if (transaction_depth == 0 || --transaction_depth != 0)
            return;

        auto group = std::move(transaction_group);
        if (!group->empty())
            push_to_undo_steps(std::move(group));
    }

    // Applies the step to data, then records it.
    // Space for the step is reserved first so that a failure leaves the collection unchanged.
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

    // The capacity must have been reserved beforehand.
    void push_to_undo_steps(std::unique_ptr<undo_step> step) noexcept
    {
        discard_undo_steps(undo_steps_index);
        undo_steps.push_back(std::move(step));
        undo_steps_index++;
    }

    void discard_undo_steps(std::size_t first) noexcept
    {
        if (clean_up)
            std::for_each(undo_steps.begin() + first, undo_steps.end(), [&](const std::unique_ptr<undo_step>& step) { step->clean_up_elements(clean_up); });
        undo_steps.erase(undo_steps.begin() + first, undo_steps.end());
    }

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
template <typename TElement, typename TCollection = std::vector<TElement*>>
class undo_redo_pointer_collection : public undo_redo_collection<TElement*, TCollection>
{
public:
    undo_redo_pointer_collection() : undo_redo_collection<TElement*, TCollection>([](TElement*& pointer) { delete pointer; })
    {}
};

template <typename TElement>
using undo_redo_vector = undo_redo_collection<TElement>;

template <typename TElement>
using undo_redo_pointer_vector = undo_redo_pointer_collection<TElement>;

} // namespace shos
