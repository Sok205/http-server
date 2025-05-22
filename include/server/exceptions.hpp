//
// Created by Filip Sokołowski on 22/05/2025.
//

#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP
#include <stdexcept>


class CustomException : public std::runtime_error
{
    public:
        explicit CustomException(const std::string& msg) : std::runtime_error(msg) {}

        const char* what() const noexcept override
        {
            return std::runtime_error::what();
        };
};

class SocketCreationException final : CustomException
{
    public:
        explicit SocketCreationException(const std::string& msg) : CustomException(msg) {}
};

class SocketOptionSet final : public std::runtime_error
{
    public:
        explicit SocketOptionSet(const std::string& msg) : std::runtime_error(msg) {}
};

class BindException final : CustomException
{
    public:
        explicit BindException(const std::string& msg) : CustomException(msg) {}
};

class ListenException final : CustomException
{
    public:
        explicit ListenException(const std::string& msg) : CustomException(msg) {}
};

class AcceptException final : CustomException
{
    public:
        explicit AcceptException(const std::string& msg) : CustomException(msg) {}
};
#endif //EXCEPTIONS_HPP
