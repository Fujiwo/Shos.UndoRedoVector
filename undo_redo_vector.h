#pragma once

#include <iterator>
#include <vector>
#include <list>
#include <algorithm>
#include <stdexcept>
#include <functional>

namespace shos {

template <typename TElement, typename TCollection = std::vector<TElement>>
class undo_redo_collection
{
    class clean_up_function
    {
        std::function<void(TElement)> clean_up;

    public:
        clean_up_function(std::function<void(TElement)> clean_up) : clean_up(clean_up)
        {}

        void operator()(TElement element) const
        {
            clean_up(element);
        }
    };

    class undo_step
    {
    public:
        enum class operation_type
        {
            add   ,
            remove,
            update,
            group
        };

    private:
        TCollection&                   collection;
        operation_type                 operation;
        std::size_t                    index;
        TElement                       element;
        const clean_up_function* const clean_up;
        bool                           hasElement;

    public:
        operation_type get_operation_type()
        {
            return operation;
        }

        TElement get_element() const
        {
            return element;
        }

        virtual ~undo_step()
        {
            if (hasElement && clean_up != nullptr)
                (*clean_up)(element);
        }
        
        static undo_step* add(TCollection& collection, TElement element, const clean_up_function* clean_up = nullptr)
        {
            collection.push_back(element);
            return new undo_step(collection, operation_type::add, collection.size() - 1, clean_up);
        }

        static undo_step* remove(TCollection& collection, std::size_t index, const clean_up_function* clean_up = nullptr)
        {
            auto element = collection[index];
            collection.erase(collection.begin() + index);
            return new undo_step(collection, operation_type::remove, index, element, clean_up);
        }

        static undo_step* update(TCollection& collection, std::size_t index, TElement element, const clean_up_function* clean_up = nullptr)
        {
            std::swap(element, collection[index]);
            return new undo_step(collection, operation_type::update, index, element, clean_up);
        }

        virtual void undo()
        {
            switch (operation) {
                case operation_type::add:
                    operation  = operation_type::remove;
                    element    = collection[index];
                    hasElement = true;
                    collection.erase(std::next(collection.begin(), index));
                    break;
                case operation_type::remove:
                    collection.insert(std::next(collection.begin(), index), element);
                    operation  = operation_type::add;
                    hasElement = false;
                    break;
                case operation_type::update:
                    std::swap(collection[index], element);
                    break;
            }
        }

        virtual void redo()
        {
            undo();
        }

        virtual const std::vector<undo_step*>* get_data() const
        {
            return nullptr;
        }

    protected:
        undo_step(TCollection& collection, operation_type operation, const clean_up_function* clean_up = nullptr)
            : collection(collection), operation(operation), index(0), element(), hasElement(false), clean_up(clean_up)
        {}

    private:
        undo_step(TCollection& collection, operation_type operation, std::size_t index, const clean_up_function* clean_up = nullptr)
            : collection(collection), operation(operation), index(index), element(), hasElement(false), clean_up(clean_up)
        {}

        undo_step(TCollection& collection, operation_type operation, std::size_t index, TElement element, const clean_up_function* clean_up = nullptr)
            : collection(collection), operation(operation), index(index), element(element), hasElement(true), clean_up(clean_up)
        {}
    };

    class undo_step_group : public undo_step
    {
        std::vector<undo_step*> undo_steps;

    public:
        using iterator       = typename std::vector<undo_step*>::iterator;
        using const_iterator = typename std::vector<undo_step*>::const_iterator;

        undo_step_group(TCollection& collection) : undo_step(collection, undo_step::operation_type::group)
        {}
        
        virtual ~undo_step_group()
        {
            for_each(undo_steps.begin(), undo_steps.end(), [](undo_step* step) { delete step; });
        }

        virtual const std::vector<undo_step*>* get_data() const override
        {
            return &undo_steps;
        }
        
        size_t size() const
        {
            return undo_steps.size();
        }

        void push_back(undo_step* step)
        {
            undo_steps.push_back(step);
        }

        iterator begin()
        {
            return undo_steps.begin();
        }

        iterator end()
        {
            return undo_steps.end();
        }

        const_iterator cbegin() const
        {
            return undo_steps.cbegin();
        }

        const_iterator cend() const
        {
            return undo_steps.cend();
        }

        virtual void undo() override
        {
            for_each(undo_steps.rbegin(), undo_steps.rend(), [](undo_step* step) { step->undo(); });
        }

        virtual void redo() override
        {
            for_each(undo_steps.begin(), undo_steps.end(), [](undo_step* step) { step->undo(); });
        }
    };

    TCollection                    data;
    size_t                         undo_steps_index;
    std::vector<undo_step*>        undo_steps;
    undo_step_group*               current_undo_step_group;
    const clean_up_function* const clean_up;

public:
    using iterator       = typename TCollection::iterator;
    using const_iterator = typename TCollection::const_iterator;

    undo_redo_collection() : undo_steps_index(0), current_undo_step_group(nullptr), clean_up(nullptr)
    {}

    undo_redo_collection(std::function<void(TElement)> clean_up)
        : undo_steps_index(0), current_undo_step_group(nullptr), clean_up(new clean_up_function(clean_up))
    {}

    virtual ~undo_redo_collection()
    {
        reset_undo_steps();
        delete clean_up;
    }

    const TElement& operator[](size_t index) const
    {
        return data[index];
    }

    size_t size() const
    {
        return data.size();
    }

    iterator begin()
    {
        return data.begin();
    }

    iterator end()
    {
        return data.end();
    }

    const_iterator cbegin() const
    {
        return data.cbegin();
    }

    const_iterator cend() const
    {
        return data.cend();
    }

    void clear()
    {
        transaction transaction(*this);
        while (data.size() > 0)
            erase(begin());
    }

    void reset()
    {
        data.clear();
        reset_undo_steps();
    }

    void push_back(TElement element)
    {
        auto step = undo_step::add(data, element, clean_up);
        push(step);
    }

    void erase(iterator iterator)
    {
        auto step = undo_step::remove(data, std::distance(data.begin(), iterator), clean_up);
        push(step);
    }

    void update(iterator iterator, TElement element)
    {
        auto step = undo_step::update(data, std::distance(data.begin(), iterator), element, clean_up);
        push(step);
    }

    bool undo()
    {
        if (undo_steps_index == 0)
            return false;

        undo_steps[undo_steps_index - 1]->undo();
        undo_steps_index--;
        return true;
    }

    bool redo()
    {
        if (undo_steps_index == undo_steps.size())
            return false;

        undo_steps[undo_steps_index]->redo();
        undo_steps_index++;
        return true;
    }

    bool can_undo() const
    {
        return undo_steps_index != 0;
    }

    bool can_redo() const
    {
        return undo_steps_index != undo_steps.size();
    }

    class transaction
    {
        undo_redo_collection<TElement, TCollection>& collection;
        
    public:
        transaction(undo_redo_collection<TElement, TCollection>& collection) : collection(collection)
        {
            collection.begin_transaction();
        }

        virtual ~transaction()
        {
            collection.end_transaction();
        }
    };
    
private:
    void begin_transaction()
    {
        if (current_undo_step_group != nullptr)
            throw std::logic_error("an exception occurred");

        current_undo_step_group = new undo_step_group(data);
    }

    void end_transaction()
    {
        if (current_undo_step_group == nullptr)
            throw std::logic_error("an exception occurred");

        if (current_undo_step_group->size() == 0)
            delete current_undo_step_group;
        else
            push_to_steps(current_undo_step_group);
        current_undo_step_group = nullptr;
    }
    
    void push(undo_step* step)
    {
        if (current_undo_step_group == nullptr)
            push_to_steps(step);
        else
            push_to_group(step);
    }

    void push_to_steps(undo_step* step)
    {
        if (undo_steps_index != undo_steps.size()) {
            for_each(undo_steps.begin() + undo_steps_index, undo_steps.end(), [](undo_step* step) { delete step; });
            undo_steps.erase(undo_steps.begin() + undo_steps_index, undo_steps.end());
        }

        undo_steps.push_back(step);
        undo_steps_index++;
    }

    void push_to_group(undo_step* step)
    {
        if (current_undo_step_group == nullptr)
            throw std::logic_error("an exception occurred");

        current_undo_step_group->push_back(step);
    }

    static void undo_data(const std::vector<undo_step*> undo_steps, std::vector<TElement>& undoes)
    {
        for (auto step : undo_steps) {
            switch (step->get_operation_type()) {
            case undo_step::operation_type::remove:
            case undo_step::operation_type::update:
                undoes.push_back(step->get_element());
                break;
            case undo_step::operation_type::group:
                if (step->get_data() != nullptr)
                    undo_data(*step->get_data(), undoes);
                break;
            }
        }
    }

    void reset_undo_steps()
    {
        for_each(undo_steps.begin(), undo_steps.end(), [](undo_step* step) { delete step; });
        undo_steps.clear();

        delete current_undo_step_group;
        current_undo_step_group = nullptr;
        undo_steps_index        = 0;
    }
};

template <typename TElement, typename TCollection = std::vector<TElement*>>
class undo_redo_pointer_collection : public undo_redo_collection<TElement*, TCollection>
{
public:
    undo_redo_pointer_collection() : undo_redo_collection<TElement*, TCollection>([](TElement* pointer) { delete pointer; })
    {}

    virtual ~undo_redo_pointer_collection()
    {
        std::for_each(this->begin(), this->end(), [](TElement* pointer) { delete pointer; });
    }
};

template <typename TElement>
using undo_redo_vector = undo_redo_collection<TElement>;

template <typename TElement>
using undo_redo_pointer_vector = undo_redo_pointer_collection<TElement>;

} // namespace shos
