#pragma once
#include <string>
#include <array>
#include <cstddef>

namespace detail {

    constexpr char xor_key(size_t index) {
        constexpr char keys[] = { 0x4F, 0x2A, 0x7D, 0x13, 0x5E, 0x69, 0x3B, 0x0C };
        return keys[index % sizeof(keys)];
    }

    template<size_t N>
    struct XorString {
        std::array<char, N> encrypted;

        constexpr XorString(const char (&str)[N]) : encrypted{} {
            for (size_t i = 0; i < N; ++i) {
                encrypted[i] = str[i] ^ xor_key(i);
            }
        }

        std::string decrypt() const {
            std::string result(N - 1, '\0');
            for (size_t i = 0; i < N - 1; ++i) {
                result[i] = encrypted[i] ^ xor_key(i);
            }
            return result;
        }
    };

    template<size_t N>
    struct XorWString {
        std::array<wchar_t, N> encrypted;

        constexpr XorWString(const wchar_t (&str)[N]) : encrypted{} {
            for (size_t i = 0; i < N; ++i) {
                encrypted[i] = str[i] ^ static_cast<wchar_t>(xor_key(i));
            }
        }

        std::wstring decrypt() const {
            std::wstring result(N - 1, L'\0');
            for (size_t i = 0; i < N - 1; ++i) {
                result[i] = encrypted[i] ^ static_cast<wchar_t>(xor_key(i));
            }
            return result;
        }
    };

} // namespace detail

#define XOR_STR(s) ([]() -> std::string { \
    constexpr auto xored = ::detail::XorString(s); \
    return xored.decrypt(); \
}())

#define XOR_WSTR(s) ([]() -> std::wstring { \
    constexpr auto xored = ::detail::XorWString(L##s); \
    return xored.decrypt(); \
}())
