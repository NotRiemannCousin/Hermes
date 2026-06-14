#include <Hermes/_base/ConnectionErrorEnum.hpp>
#include <format>

namespace Hermes{

    const char* ConnectionErrorCategory::name() const noexcept {
        return "ConnectionErrorCategory";
    }

    std::string ConnectionErrorCategory::message(int ev) const {
        return std::format("{:v}", static_cast<ConnectionErrorEnum>(ev));
    }


    static const ConnectionErrorCategory& S_GetConnectionErrorCategory() noexcept {
        static ConnectionErrorCategory instance;
        return instance;
    }

    std::error_code make_error_code(ConnectionErrorEnum e) noexcept {
        return std::error_code{
            static_cast<int>(e),
            S_GetConnectionErrorCategory()
        };
    }




    TransferError TransferError::Accumulate(const TransferError other) const noexcept {
        return { bytesTransferred + other.bytesTransferred, other.error };
    }

    TransferError TransferError::Substitute(const ConnectionErrorEnum err) const noexcept {
        return { bytesTransferred, err };
    }
}