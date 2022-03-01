#ifndef KERNEL_RUNTIME_REFCOUNTABLE_H
#define KERNEL_RUNTIME_REFCOUNTABLE_H

#include <stddef.h>

/**
 * @brief Data structures and other runtime support
 */
namespace Kernel::Runtime {
/**
 * @brief Generic reference counting implementation
 *
 * Any class that wants to implement reference counting should inherit from this template. It will
 * then receive the retain() and release() methods, as well as a built-in reference counter.
 *
 * @tparam T Typename of the class to be ref counted
 *
 * @todo Handle overflow/underflow for retain/release codepaths
 */
template<typename T>
class RefCountable {
    public:
        /**
         * @brief Increment the reference count of the object.
         *
         * @return Pointer to the object
         */
        T *retain() {
            __atomic_add_fetch(&this->refCount, 1, __ATOMIC_ACQUIRE);
            return static_cast<T *>(this);
        }

        /**
         * @brief Decrement the reference count of the object, deleting it if it reaches zero.
         *
         * @return Pointer to the object, or `nullptr` if it was deleted
         */
        T *release() {
            if(!__atomic_sub_fetch(&this->refCount, 1, __ATOMIC_RELEASE)) {
                delete this;
                return nullptr;
            }
            return static_cast<T *>(this);
        }

        /**
         * @brief Get the current reference count.
         *
         * @remark You should not rely on this value to do any memory management. It's intended as
         *         a diagnostic aid. Use retain() and release() for memory management, since these
         *         calls are guaranteed to be atomic.
         *
         * @return Number of references outstanding to this object
         */
        constexpr size_t inline getRefCount() const {
            return this->refCount;
        }

    protected:
        /**
         * @brief Current reference count
         *
         * All objects are created with their reference count initially set to 1.
         */
        size_t refCount{1};
};
}

#endif
