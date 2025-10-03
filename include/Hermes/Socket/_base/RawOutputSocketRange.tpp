#pragma once

namespace Hermes {
    template<ByteLike Type>
    typename RawOutputSocketRange<Type>::Iterator& RawOutputSocketRange<Type>::Iterator::operator*() {
        return *this;
    }

    template<ByteLike Type>
    typename RawOutputSocketRange<Type>::Iterator& RawOutputSocketRange<Type>::Iterator::operator++() {
        if (++view->index >= view->_buffer.size())
            view->index = 0;
        return *this;
    }

    template<ByteLike Type>
    typename RawOutputSocketRange<Type>::Iterator& RawOutputSocketRange<Type>::Iterator::operator++(int) {
        return ++(*this);
    }


    template<ByteLike Type>
    typename RawOutputSocketRange<Type>::Iterator& RawOutputSocketRange<Type>::Iterator::operator=(Type value) {
        return static_cast<bool>(!view->_errorStatus);
    }


    template<ByteLike Type>
    bool RawOutputSocketRange<Type>::operator==(std::default_sentinel_t) const {
        return true;
    }


    template<ByteLike Type>
    RawOutputSocketRange<Type>::RawOutputSocketRange(SOCKET socket) {
    }

    template<ByteLike Type>
    RawOutputSocketRange<Type>::~RawOutputSocketRange() {
    }

    template<ByteLike Type>
    typename RawOutputSocketRange<Type>::Iterator RawOutputSocketRange<Type>::begin() {
        return Iterator{this};
    }

    template<ByteLike Type>
    std::default_sentinel_t RawOutputSocketRange<Type>::end() {
        return {};
    }

    template<ByteLike Type>
    ConnectionResultOper RawOutputSocketRange<Type>::Flush() {
        if (_socket == macroINVALID_SOCKET)
            return unexpected{ ConnectionErrorEnum::SOCKET_NOT_OPEN };

        const int sent{ send(_socket, reinterpret_cast<const char*>(data.data()),
                        static_cast<int>(data.size()), 0) };

        if (sent == macroSOCKET_ERROR)
            return unexpected{ ConnectionErrorEnum::SEND_FAILED }; // TODO: This is disgusting... Improve error handling for send and receive

        index = 0;
    }

    template<ByteLike Type>
    ConnectionResultOper RawOutputSocketRange<Type>::OptError() const {
        return _errorStatus;
    }
}
