#pragma once

namespace Hermes {
    template<class Type>
    typename RawOutputSocketRange<Type>::Iterator& RawOutputSocketRange<Type>::Iterator::operator*() {
        return *this;
    }

    template<class Type>
    typename RawOutputSocketRange<Type>::Iterator& RawOutputSocketRange<Type>::Iterator::operator++() {
        if (++view->index >= view->_buffer.size())
            view->index = 0;
        return *this;
    }

    template<class Type>
    typename RawOutputSocketRange<Type>::Iterator& RawOutputSocketRange<Type>::Iterator::operator++(int) {
        return ++(*this);
    }


    template<class Type>
    typename RawOutputSocketRange<Type>::Iterator& RawOutputSocketRange<Type>::Iterator::operator=(Type value) {
        return static_cast<bool>(!view->errorStatus);
    }

    template<class Type>
    RawOutputSocketRange<Type>::RawOutputSocketRange(SOCKET socket) {
    }

    template<class Type>
    RawOutputSocketRange<Type>::~RawOutputSocketRange() {
    }

    template<class Type>
    typename RawOutputSocketRange<Type>::Iterator RawOutputSocketRange<Type>::begin() {
        return Iterator{this};
    }

    template<class Type>
    std::default_sentinel_t RawOutputSocketRange<Type>::end() {
        return {};
    }

    template<class Type>
    ConnectionResultOper RawOutputSocketRange<Type>::Flush() {


        index = 0;
    }

    template<class Type>
    ConnectionResultOper RawOutputSocketRange<Type>::OptError() const {
        return errorStatus;
    }
}
