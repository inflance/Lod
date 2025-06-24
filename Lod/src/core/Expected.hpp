#pragma once
#include <variant>
#include <string>

namespace lod {

template<typename E>
class unexpected {
private:
    E error_value;
    
public:
    unexpected(const E& e) : error_value(e) {}
    unexpected(E&& e) : error_value(std::move(e)) {}
    
    const E& value() const & { return error_value; }
    E& value() & { return error_value; }
    E&& value() && { return std::move(error_value); }
};

template<typename T, typename E = std::string>
class expected {
private:
    std::variant<T, E> data;
    
public:
    expected(const T& value) : data(value) {}
    expected(T&& value) : data(std::move(value)) {}
    expected(const unexpected<E>& error) : data(error.value()) {}
    
    bool has_value() const noexcept {
        return std::holds_alternative<T>(data);
    }
    
    explicit operator bool() const noexcept {
        return has_value();
    }
    
    const T& value() const & {
        return std::get<T>(data);
    }
    
    T& value() & {
        return std::get<T>(data);
    }
    
    T&& value() && {
        return std::get<T>(std::move(data));
    }
    
    const T& operator*() const & {
        return value();
    }
    
    T& operator*() & {
        return value();
    }
    
    T&& operator*() && {
        return std::move(*this).value();
    }
    
    const T* operator->() const {
        return &value();
    }
    
    T* operator->() {
        return &value();
    }
    
    const E& error() const & {
        return std::get<E>(data);
    }
    
    E& error() & {
        return std::get<E>(data);
    }
    
    E&& error() && {
        return std::get<E>(std::move(data));
    }
};

} // namespace lod 