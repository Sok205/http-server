//
// Created by Filip Sokołowski on 22/05/2025.
//

#pragma once
#include <stdexcept>

namespace exceptions
{
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
}//namespace exceptions

