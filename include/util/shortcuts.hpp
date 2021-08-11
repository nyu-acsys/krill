#pragma once
#ifndef PLANKTON_UTIL_SHORTCUTS_HPP
#define PLANKTON_UTIL_SHORTCUTS_HPP

#include <memory>
#include <exception>
#include <algorithm>
#include <type_traits>

namespace plankton {
    
    template<typename Base, typename Sub>
    using EnableIfBaseOf = typename std::enable_if_t<std::is_base_of_v<Base, Sub>>;
    
    template<typename T, typename U>
    static inline std::pair<bool, const T*> IsOfType(const U& object) {
        auto result = dynamic_cast<const T*>(&object);
        if (result) return std::make_pair(true, result);
        else return std::make_pair(false, nullptr);
    }
    
    template<typename T, typename = void>
    std::unique_ptr<T> Copy(const T& object);
    
    template<typename T>
    static inline T CopyAll(const T& container) {
        T result;
        for (const auto& elem : container) result.push_back(plankton::Copy(*elem));
        return result;
    }
    
    template<typename T, typename U>
    static inline void MoveInto(T&& srcContainer, U& dstContainer) {
        std::move(srcContainer.begin(), srcContainer.end(), std::back_inserter(dstContainer));
    }
    
    template<typename T, typename U>
    static inline void InsertInto(T&& srcContainer, U& dstContainer) {
        dstContainer.insert(srcContainer.begin(), srcContainer.end());
    }
    
    template<typename T, typename U>
    static inline void RemoveIf(T& container, const U& unaryPredicate) {
        container.erase(std::remove_if(container.begin(), container.end(), unaryPredicate), container.end());
    }
    
    template<typename T, typename U>
    static inline void DiscardIf(T& container, const U& unaryPredicate) {
        for (auto it = container.begin(), end = container.end(); it != end; ) {
            if (unaryPredicate(*it)) it = container.erase(it);
            else ++it;
        }
    }
    
    template<typename T, typename U>
    static inline typename T::iterator FindIf(T& container, const U& unaryPredicate) {
        return std::find_if(container.begin(), container.end(), unaryPredicate);
    }
    
    template<typename T, typename U>
    static inline bool ContainsIf(T& container, const U& unaryPredicate) {
        return std::find_if(container.begin(), container.end(), unaryPredicate) != container.end();
    }
    
    template<typename T, typename U>
    static inline bool Any(const T& container, const U& unaryPredicate) {
        return std::any_of(container.begin(), container.end(), unaryPredicate);
    }
    
    template<typename T, typename U>
    static inline bool All(const T& container, const U& unaryPredicate) {
        return std::all_of(container.begin(), container.end(), unaryPredicate);
    }
    
    template<typename T, typename U>
    static inline bool Membership(const T& container, const U& element) {
        return std::find(container.begin(), container.end(), element) != container.end();
    }
    
    struct ExceptionWithMessage : public std::exception {
        std::string message;
        explicit ExceptionWithMessage(std::string message) : message(std::move(message)) {}
        [[nodiscard]] const char* what() const noexcept override { return message.c_str(); }
    };
    
} // namespace plankton

#endif //PLANKTON_UTIL_SHORTCUTS_HPP
