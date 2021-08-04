#ifndef PLANKTON_UTIL_SHORTCUTS_HPP
#define PLANKTON_UTIL_SHORTCUTS_HPP

#include <memory>
#include <exception>

namespace plankton {
    
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
    static inline typename T::iterator FindIf(T& container, const U& unaryPredicate) {
        return std::find_if(container.begin(), container.end(), unaryPredicate);
    }
    
    template<typename T, typename U>
    static inline std::pair<bool, const T*> IsOfType(const U& object) {
        auto result = dynamic_cast<const T*>(&object);
        if (result) return std::make_pair(true, result);
        else return std::make_pair(false, nullptr);
    }
    
    struct ExceptionWithMessage : public std::exception {
        std::string message;
        explicit ExceptionWithMessage(std::string message) : message(std::move(message)) {}
        [[nodiscard]] const char* what() const noexcept override { return message.c_str(); }
    };
    
} // namespace plankton

#endif //PLANKTON_UTIL_SHORTCUTS_HPP
