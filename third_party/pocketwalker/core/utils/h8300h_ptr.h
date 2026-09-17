#pragma once
#include <bit>
#include <cstring>
#include <concepts>
#include <cstddef>
#include <cstdint>

// provides an h8/300h big endian byte swap ptr for u16 and u32
// holy moly this is ugly

template <std::integral T>
class h8300h_ptr
{
public:
    struct ref
    {
        h8300h_ptr* p;

        operator T() const { return p->Load(); }

        ref& operator=(T val)
        {
            *p = val;
            return *this;
        }

        ref& operator++()
        {
            *p = (T)(p->Load() + 1);
            return *this;
        }

        T operator++(int)
        {
            T old = p->Load();
            ++*this;
            return old;
        }

        ref& operator--()
        {
            *p = (T)(p->Load() - 1);
            return *this;
        }

        T operator--(int)
        {
            T old = p->Load();
            --*this;
            return old;
        }
    };

    h8300h_ptr() : ptr(nullptr)
    {
    }

    h8300h_ptr(std::nullptr_t) : ptr(nullptr)
    {
    }

    explicit h8300h_ptr(T* ptr) : ptr(reinterpret_cast<uint8_t*>(ptr))
    {
    }

    ref operator*() { return ref{this}; }
    T operator*() const { return Load(); }

    h8300h_ptr& operator=(T val)
    {
        if constexpr (std::endian::native == std::endian::little) val = std::byteswap(val);
        std::memcpy(ptr, &val, sizeof(val));
        return *this;
    }

    h8300h_ptr& operator+=(T val)
    {
        *this = **this + val;
        return *this;
    }

    h8300h_ptr& operator-=(T val)
    {
        *this = **this - val;
        return *this;
    }

    h8300h_ptr& operator++()
    {
        *this = **this + 1;
        return *this;
    }

    bool operator==(T val) const { return **this == val; }
    bool operator>=(T val) const { return **this >= val; }
    bool operator<=(T val) const { return **this <= val; }
    bool operator>(T val) const { return **this > val; }
    bool operator<(T val) const { return **this < val; }

    T Load() const {
        T value;
        std::memcpy(&value, ptr, sizeof(value));
        if constexpr (std::endian::native == std::endian::little) value = std::byteswap(value);
        return value;
    }

    uint8_t* ptr;
};
