/**
 *
 * @file token_generator.cpp
 * @author Gaspard Kirira
 * @brief Implementation of Vix Realtime opaque token generation.
 *
 * Copyright 2026, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/realtime/internal/token_generator.hpp>

#include <cctype>
#include <exception>
#include <random>
#include <utility>
#include <vector>

#include <vix/realtime/errors.hpp>

namespace vix::realtime::internal
{
  namespace
  {
    /** @brief Base64 URL alphabet without padding. */
    constexpr std::string_view base64UrlAlphabet{
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789-_"};

    /**
     * @brief Return whether a prefix character is valid.
     */
    [[nodiscard]] bool valid_prefix_character(
        unsigned char value) noexcept
    {
      return std::isalnum(value) != 0 ||
             value == '-' ||
             value == '_';
    }

    /**
     * @brief Return whether a Base64 URL payload character is valid.
     */
    [[nodiscard]] bool valid_payload_character(
        unsigned char value) noexcept
    {
      return std::isalnum(value) != 0 ||
             value == '-' ||
             value == '_';
    }

    /**
     * @brief Encode random bytes using unpadded Base64 URL.
     */
    [[nodiscard]] std::string encode_base64_url(
        const std::vector<std::uint8_t> &bytes)
    {
      std::string encoded;

      encoded.reserve(
          (bytes.size() / 3) * 4 +
          (bytes.size() % 3 == 0
               ? 0
               : bytes.size() % 3 + 1));

      std::size_t index = 0;

      while (index + 3 <= bytes.size())
      {
        const std::uint32_t value =
            (static_cast<std::uint32_t>(
                 bytes[index])
             << 16) |
            (static_cast<std::uint32_t>(
                 bytes[index + 1])
             << 8) |
            static_cast<std::uint32_t>(
                bytes[index + 2]);

        encoded.push_back(
            base64UrlAlphabet[(value >> 18) & 0x3FU]);

        encoded.push_back(
            base64UrlAlphabet[(value >> 12) & 0x3FU]);

        encoded.push_back(
            base64UrlAlphabet[(value >> 6) & 0x3FU]);

        encoded.push_back(
            base64UrlAlphabet[value & 0x3FU]);

        index += 3;
      }

      const std::size_t remaining =
          bytes.size() - index;

      if (remaining == 1)
      {
        const std::uint32_t value =
            static_cast<std::uint32_t>(
                bytes[index])
            << 16;

        encoded.push_back(
            base64UrlAlphabet[(value >> 18) & 0x3FU]);

        encoded.push_back(
            base64UrlAlphabet[(value >> 12) & 0x3FU]);
      }
      else if (remaining == 2)
      {
        const std::uint32_t value =
            (static_cast<std::uint32_t>(
                 bytes[index])
             << 16) |
            (static_cast<std::uint32_t>(
                 bytes[index + 1])
             << 8);

        encoded.push_back(
            base64UrlAlphabet[(value >> 18) & 0x3FU]);

        encoded.push_back(
            base64UrlAlphabet[(value >> 12) & 0x3FU]);

        encoded.push_back(
            base64UrlAlphabet[(value >> 6) & 0x3FU]);
      }

      return encoded;
    }

    /**
     * @brief Fill a buffer using the standard random device.
     */
    void fill_with_random_device(
        std::uint8_t *data,
        std::size_t size)
    {
      std::random_device device;
      std::uniform_int_distribution<unsigned int> distribution{
          0,
          255};

      for (std::size_t index = 0;
           index < size;
           ++index)
      {
        data[index] =
            static_cast<std::uint8_t>(
                distribution(device));
      }
    }

  } // namespace

  TokenGenerator::TokenGenerator(
      std::size_t entropyBytes,
      std::string prefix)
      : entropyBytes_(entropyBytes),
        prefix_(std::move(prefix)),
        randomSource_()
  {
    validate();
  }

  TokenGenerator::TokenGenerator(
      RandomSource randomSource,
      std::size_t entropyBytes,
      std::string prefix)
      : entropyBytes_(entropyBytes),
        prefix_(std::move(prefix)),
        randomSource_(std::move(randomSource))
  {
    if (!randomSource_)
    {
      throw Error{
          ErrorCode::MissingDependency,
          "token generator requires a random source"};
    }

    validate();
  }

  ResumeToken TokenGenerator::generate() const
  {
    std::vector<std::uint8_t> entropy(
        entropyBytes_);

    try
    {
      std::lock_guard<std::mutex> lock{mutex_};

      if (randomSource_)
      {
        randomSource_(
            entropy.data(),
            entropy.size());
      }
      else
      {
        fill_with_random_device(
            entropy.data(),
            entropy.size());
      }
    }
    catch (const Error &)
    {
      throw;
    }
    catch (const std::exception &error)
    {
      throw Error{
          ErrorCode::InternalError,
          std::string{
              "failed to generate random token entropy: "} +
              error.what()};
    }
    catch (...)
    {
      throw Error{
          ErrorCode::InternalError,
          "failed to generate random token entropy"};
    }

    std::string encoded =
        encode_base64_url(entropy);

    if (prefix_.empty())
    {
      return encoded;
    }

    ResumeToken token;
    token.reserve(
        prefix_.size() +
        1 +
        encoded.size());

    token.append(prefix_);
    token.push_back('.');
    token.append(encoded);

    return token;
  }

  bool TokenGenerator::accepts(
      std::string_view token) const noexcept
  {
    if (token.size() != token_size())
    {
      return false;
    }

    std::string_view payload =
        token;

    if (!prefix_.empty())
    {
      if (!token.starts_with(prefix_) ||
          token[prefix_.size()] != '.')
      {
        return false;
      }

      payload.remove_prefix(
          prefix_.size() + 1);
    }

    if (payload.size() !=
        encoded_size(entropyBytes_))
    {
      return false;
    }

    for (const unsigned char character : payload)
    {
      if (!valid_payload_character(character))
      {
        return false;
      }
    }

    return true;
  }

  bool TokenGenerator::is_valid(
      std::string_view token) noexcept
  {
    if (token.empty())
    {
      return false;
    }

    bool separatorSeen = false;
    std::size_t separatorPosition = 0;

    for (std::size_t index = 0;
         index < token.size();
         ++index)
    {
      const unsigned char character =
          static_cast<unsigned char>(
              token[index]);

      if (character == '.')
      {
        if (separatorSeen ||
            index == 0 ||
            index + 1 == token.size())
        {
          return false;
        }

        separatorSeen = true;
        separatorPosition = index;
        continue;
      }

      if (!valid_payload_character(character))
      {
        return false;
      }
    }

    if (separatorSeen)
    {
      if (separatorPosition >
          maximumPrefixSize)
      {
        return false;
      }

      for (std::size_t index = 0;
           index < separatorPosition;
           ++index)
      {
        if (!valid_prefix_character(
                static_cast<unsigned char>(
                    token[index])))
        {
          return false;
        }
      }
    }

    return true;
  }

  std::size_t
  TokenGenerator::entropy_bytes() const noexcept
  {
    return entropyBytes_;
  }

  const std::string &
  TokenGenerator::prefix() const noexcept
  {
    return prefix_;
  }

  std::size_t
  TokenGenerator::token_size() const noexcept
  {
    const std::size_t payloadSize =
        encoded_size(entropyBytes_);

    if (prefix_.empty())
    {
      return payloadSize;
    }

    return prefix_.size() +
           1 +
           payloadSize;
  }

  void TokenGenerator::validate() const
  {
    if (entropyBytes_ <
            minimumEntropyBytes ||
        entropyBytes_ >
            maximumEntropyBytes)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "token entropy size must be between 16 and 128 bytes"};
    }

    if (prefix_.size() >
        maximumPrefixSize)
    {
      throw Error{
          ErrorCode::InvalidConfiguration,
          "token prefix exceeds 32 characters"};
    }

    for (const unsigned char character : prefix_)
    {
      if (!valid_prefix_character(character))
      {
        throw Error{
            ErrorCode::InvalidConfiguration,
            "token prefix contains invalid characters"};
      }
    }
  }

} // namespace vix::realtime::internal
