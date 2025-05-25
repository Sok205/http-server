//
// Created by Filip Sokołowski on 22/05/2025.
//

#ifndef EXCEPTIONS_HPP
#define EXCEPTIONS_HPP
#include <stdexcept>


class CustomException : public std::runtime_error
{
    public:
        using runtime_error::runtime_error;
};

class SocketCreationException final : public CustomException
{
    public:
        using CustomException::CustomException;
};

class SocketOptionSet final : public std::runtime_error
{
    public:
        using runtime_error::runtime_error;
};

class BindException final : public CustomException
{
    public:
        using CustomException::CustomException;
};

class ListenException final : public CustomException
{
    public:
        using CustomException::CustomException;
};

class AcceptException final : public CustomException
{
    public:
        using CustomException::CustomException;
};

class HandlerException final : public CustomException
{
    public:
        using CustomException::CustomException;
};
#endif //EXCEPTIONS_HPP
