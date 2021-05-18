#ifndef KERNEL_RUNTIME_SMARTPOINTERS_H
#define KERNEL_RUNTIME_SMARTPOINTERS_H

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <log.h>

namespace rt {
// forward declare weak ptr so we can befriend it in the shared ptr
template<class T> class SharedPtr;
template<class T> class SharedFromThis;
template<class T> class WeakPtr;

/**
 * Deleters are used by the smart pointers to properly discard objects we don't need anymore. This
 * is just a simple class that has a call operator implemented.
 */
template<typename T>
concept Deleter = requires(T deleter, T object) {
    { deleter(object) } -> std::same_as<void>;
};

/**
 * A class may implement the SharedFromThis trait
 */
template<typename T>
concept Shareable = requires(T ptr) {
    { ptr.sharedFromThis() } -> std::same_as<SharedPtr<T>>;
    { ptr._weakRef } -> std::same_as<WeakPtr<T>>;
};

/**
 * Private namespace to hide some implementation details of the smart pointers
 */
namespace PtrImpl {
    /**
     * Information block for tracking the number of strong and weak references to the object, as
     * well as storing the deleter object.
     *
     * This structure is references as well by the weak pointer class below.
     *
     * Note that as long as the `useCount` field is non-zero, the `weakCount` field will be one
     * greater than the actual number of weak references. This is a small optimization to
     * ensure that we don't delete the info block while the deleter is running; think of this
     * as each shared pointer instance holding a weak reference to the info block, but we
     * skip the atomic increment for for any pointer past the first.
     */
    struct InfoBlock {
        /// Number of strong references
        size_t useCount = 1;
        /// Number of weak references
        size_t weakCount = 1;

        /// Ensure the class is virtual
        virtual ~InfoBlock() = default;
        /// Called when all strong references to the object are released to delete the object
        virtual void destroy() = 0;
    };

    /**
     * Default deleter implementation for heap allocated objects
     */
    template<class U>
    struct DefaultDeleter {
        void operator()(U *obj) const {
            delete obj;
        }
    };
};


/**
 * Shared pointers implement reference counting for any object `T`, allowing multiple references
 * to the object to exist, and automatically releasing the object when all references are closed.
 */
template<class T>
class SharedPtr {
    template<typename> friend class SharedPtr;
    friend class WeakPtr<T>;

    private:
        /**
         * Actual implementation of the information block; this is what's actually allocated. It needs
         * to be done using inheritance so we can rely on virtual dispatch to invoke the correct
         * deleter method when `T` points to some subclass.
         */
        template<class U, class Deleter>
        struct InfoBlockImpl: public PtrImpl::InfoBlock {
            U *ptr;
            Deleter deleter;

            /// Create a new instance of the info block
            InfoBlockImpl(U *_ptr, Deleter _deleter) : ptr(_ptr), deleter(_deleter) {}

            /// Invokes the deleter.
            virtual void destroy() override {
                this->deleter(this->ptr);
            }
        };

    private:
        /**
         * Increments the number of strong references held to the object.
         */
        inline void incrementStrongRefs() {
            if(this->info) {
                // TODO: ensure there cannot be an overflow
                __atomic_add_fetch(&this->info->useCount, 1, __ATOMIC_RELAXED);
            }
        }

        /**
         * Decrements the number of strong references held to the object. If the reference count
         * reaches zero, the deleter is invoked.
         */
        inline void decrementStrongRefs() {
            if(this->info) {
                // decrement strong references
                if(!__atomic_sub_fetch(&this->info->useCount, 1, __ATOMIC_RELEASE)) {
                    // no strong references remain, so deallocate the pointee
                    this->info->destroy();
                    this->ptr = nullptr;

                    // if there are no weak references remaining, deallocate info block
                    if(!__atomic_sub_fetch(&this->info->weakCount, 1, __ATOMIC_RELEASE)) {
                        delete this->info;
                        this->info = nullptr;
                    }
                }
            }
        }

    protected:
        /// info block (stores reference counts)
        PtrImpl::InfoBlock *info = nullptr;
        /// owned object
        T *ptr = nullptr;

    private:
        /**
         * Creates a shared pointer, given an existing info block and pointer.
         *
         * This is used to convert a weak pointer into a shared pointer. It is required that when
         * this method is invoked, the `strongCount` in the info block has already been incremented
         * appropriately.
         */
        template<class U>
        SharedPtr(PtrImpl::InfoBlock *_info, U *_ptr) : info(_info), ptr(_ptr) {}

    public:
        /**
         * Allocate an empty shared pointer, which points to `nullptr`
         */
        SharedPtr() = default;
        /**
         * Allocate a shared pointer that points to null.
         */
        SharedPtr(std::nullptr_t) {};

        /**
         * Allocate a shared pointer that owns `obj` with a custom deleter.
         */
        template<class U, class Deleter,
            std::enable_if_t<std::negation<std::is_base_of<SharedFromThis<U>, U>>::value, bool> = true>
        SharedPtr(U *_ptr, Deleter d) : info(new InfoBlockImpl<U, Deleter>(_ptr, d)), ptr(_ptr) {}
        /**
         * Allocate a shared pointer from an object that supports converting a plain *this
         * reference to a shared pointer, using a custom deleter.
         */
        template<class U, class Deleter,
            std::enable_if_t<std::is_base_of<SharedFromThis<U>, U>::value, bool> = true>
        SharedPtr(SharedFromThis<U> *_ptr, Deleter d) {
            auto &weak = _ptr->_weakRef;

            // if weak reference is valid, copy its info block and get another strong reference
            if(weak) {
                this->info = weak.info;
                this->ptr = weak.ptr;
                this->incrementStrongRefs();
            }
            // otherwise, allocate this shared ptr as normal and create weak ptr to store in object
            else {
                this->info = new InfoBlockImpl<U, Deleter>(_ptr, d);
                this->ptr = _ptr;
                weak = rt::WeakPtr<U>(*this);
            }
        }


        /**
         * Allocate a shared pointer, with reference count 1 to the given object, with the default
         * deleter that invokes `operator delete`.
         */
        template<class U,
            std::enable_if_t<std::negation<std::is_base_of<SharedFromThis<U>, U>>::value, bool> = true>
        explicit SharedPtr(U *_ptr) : info(new InfoBlockImpl<U,
                PtrImpl::DefaultDeleter<U>>(_ptr, PtrImpl::DefaultDeleter<U>())), ptr(_ptr) {}
        /**
         * Allocate a shared pointer, from an object that supports converting a plain *this
         * reference into a smart pointer.
         */
        template<class U,
            std::enable_if_t<std::is_base_of<SharedFromThis<U>, U>::value, bool> = true>
        explicit SharedPtr(U *_ptr) {
            auto &weak = _ptr->_weakRef;

            // if weak reference exists, we can try to get the shared ptr
            if(weak) {
                this->info = weak.info;
                this->ptr = weak.ptr;
                this->incrementStrongRefs();
            }
            // otherwise, allocate this shared ptr as normal and create weak ptr to store in object
            else {
                this->info = new InfoBlockImpl<U, PtrImpl::DefaultDeleter<U>>(_ptr, 
                        PtrImpl::DefaultDeleter<U>());
                this->ptr = _ptr;
                weak = rt::WeakPtr<U>(*this);
            }
        }


        /**
         * Copies a shared pointer. The strong reference count is incremented.
         */
        SharedPtr(const SharedPtr &ptr) : info(ptr.info), ptr(ptr.ptr) {
            this->incrementStrongRefs();
        }
        /**
         * Copies a shared pointer to a derived type.
         */
        template<class U>
        SharedPtr(const SharedPtr<U> &ptr) : info(ptr.info), ptr(ptr.ptr) {
            this->incrementStrongRefs();
        }

        /**
         * Assigns the contents of one shared pointer to another.
         *
         * XXX: is there a race between decrementing the old info block's counts and taking over
         * the new one?
         */
        SharedPtr& operator=(const SharedPtr &ptr) {
            if(this != &ptr) {
                this->decrementStrongRefs();

                this->info = ptr.info;
                this->ptr = ptr.ptr;

                this->incrementStrongRefs();
            }

            return *this;
        }

        /**
         * Relinquishes control over the pointee, by decrementing the reference count. If the
         * count becomes zero, the deleter is invoked.
         */
        SharedPtr& operator=(std::nullptr_t) {
            this->decrementStrongRefs();

            this->ptr = nullptr;
            this->info = nullptr;

            return *this;
        }

        /**
         * When the shared pointer is deallocated (going out of scope, etc.) we'll decrement the
         * strong reference count, which will possibly also deallocate the pointee.
         */
        ~SharedPtr() {
            this->decrementStrongRefs();
        }

        /**
         * Shared pointers compare as equal if they point to the same object.
         */
        bool operator==(const SharedPtr &ptr) const {
            return (this->ptr == ptr.ptr);
        }

        /// Decay to a void ptr
        explicit operator void*() const {
            return this->ptr;
        }
        /// Test whether the pointee is null
        explicit operator bool() const {
            return (this->ptr != nullptr);
        }
        /// Access the pointee
        T* operator->() const {
            return this->ptr;
        }
        /// Dereference the pointee
        T& operator*() const {
            return *this->ptr;
        }
        /// Return the raw pointer
        T* get() const {
            return this->ptr;
        }



        /**
         * Returns the number of strong references to the pointee.
         *
         * @note This result may be stale when it's returned. You should not use it for any purpose
         * other than debugging.
         */
        size_t getStrongRefs() const {
            if(!this->info) return 0;
            return __atomic_load_n(&this->info->useCount, __ATOMIC_RELAXED);
        }
        /**
         * Returns the number of weak references to the pointee.
         *
         * @note This result may be stale when it's returned. You should not use it for any purpose
         * other than debugging.
         */
        size_t getWeakRefs() const {
            if(!this->info) return 0;
            return __atomic_load_n(&this->info->weakCount, __ATOMIC_RELAXED);
        }
};



/**
 * Weak pointers may be created from an existing shared pointer. They do not prevent the pointee
 * from being deleted when all strong references (from `SharedPtr`) die, but WILL keep the info
 * struct alive.
 */
template<class T>
class WeakPtr {
    friend class SharedPtr<T>;
    template<typename> friend class WeakPtr;

    private:
        /**
         * Increments the number of weak references to the info block.
         */
        inline void incrementWeakRefs() {
            if(this->info) {
                // TODO: ensure there cannot be an overflow
                __atomic_add_fetch(&this->info->weakCount, 1, __ATOMIC_RELAXED);
            }
        }

        /**
         * Decrements the number of weak references in the info block, and if the count reaches
         * zero, deletes the info block.
         *
         * We do not have to check the strong references field for nonzero, as if that's zero, we
         * know that the +1 has been removed from the weak references count. So, the only way the
         * weak count can reach zero (barring bugs, lmao) is if there's no strong references
         * remaining.
         */
        inline void decrementWeakRefs() {
            // if there are no weak references remaining, deallocate info block
            if(this->info && !__atomic_sub_fetch(&this->info->weakCount, 1, __ATOMIC_RELEASE)) {
                delete this->info;
                this->info = nullptr;
            }
        }

    private:
        /// info block (stores reference counts; copied from shared ptr)
        PtrImpl::InfoBlock *info = nullptr;
        /// owned object
        T *ptr = nullptr;

    public:
        /**
         * Allocate an empty weak pointer, which points to `nullptr`
         */
        WeakPtr() = default;

        /**
         * Creates a weak pointer from a particular shared pointer.
         */
        template<class U>
        WeakPtr(const SharedPtr<U> &s) : info(s.info), ptr(s.ptr) {
            this->incrementWeakRefs();
        }

        /**
         * Assigns the contents of one weak pointer to another.
         */
        WeakPtr& operator=(const WeakPtr &ptr) {
            if(this != &ptr) {
                this->decrementWeakRefs();

                this->info = ptr.info;
                this->ptr = ptr.ptr;

                this->incrementWeakRefs();
            }

            return *this;
        }

        /**
         * Relinquishes control over the pointee, removing a weak reference from the information
         * block. If the weak reference count becomes zero, we'll delete the info block.
         */
        WeakPtr& operator=(std::nullptr_t) {
            this->decrementWeakRefs();

            this->ptr = nullptr;
            this->info = nullptr;

            return *this;
        }

        /**
         * Releases a weak reference over the owning object.
         */
        ~WeakPtr() {
            this->decrementWeakRefs();
        }



        /**
         * Attempts to create a shared pointer to manage the pointee. This will return an empty
         * pointer if the pointee has already been deallocated.
         */
        SharedPtr<T> lock() const {
            // bail if no info struct
            if(!this->info) return SharedPtr<T>();

            // increment strong refs count if nonzero, otherwise fail
            size_t strongCount = __atomic_load_n(&this->info->useCount, __ATOMIC_ACQUIRE);

            for(;;) {
                // pointee already deallocated
                if(!strongCount) {
                    return SharedPtr<T>();
                }

                // increment it atomically
                if(__atomic_compare_exchange_n(&this->info->useCount, &strongCount,
                            (strongCount + 1), false, __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
                    return SharedPtr<T>(this->info, this->ptr);
                }
            }
        }



        /**
         * Returns the number of strong references to the pointee.
         *
         * @note This result may be stale when it's returned. You should not use it for any purpose
         * other than debugging.
         */
        size_t getStrongRefs() const {
            if(!this->info) return 0;
            return __atomic_load_n(&this->info->useCount, __ATOMIC_RELAXED);
        }
        /**
         * Returns the number of weak references to the pointee.
         *
         * @note This result may be stale when it's returned. You should not use it for any purpose
         * other than debugging.
         */
        size_t getWeakRefs() const {
            if(!this->info) return 0;
            return __atomic_load_n(&this->info->weakCount, __ATOMIC_RELAXED);
        }
        /**
         * Check whether the object is expired, i.e. the strong references have reached zero.
         *
         * @note The result may be stale; you should try to get a pointer rather than guarding it
         * with this function. Do not use this for production code.
         */
        bool expired() const {
            return (this->getStrongRefs() == 0);
        }

        /// Test whether the pointee is null
        explicit operator bool() const {
            return (this->ptr != nullptr);
        }
};



/**
 * Base class to inherit from to allow creating a shared pointer from a plain this pointer.
 *
 * This is implemented by having special versions of the SharedPtr constructor that detect this
 * class (it must be accessible to the SharedPtr class, so public inheritance must be used) and
 * store a weak reference to that first shared pointer inside the object itself. All subsequent
 * invocations of the SharedPtr destructor will lock the weak pointer and all share the same
 * allocation state.
 */
template<class T>
class SharedFromThis {
    friend class SharedPtr<T>;

    public:
        /**
         * Generates a shared pointer to reference this object.
         *
         * @note Behavior of this function is undefined if this object has never been the pointee
         * of a SharedPtr.
         */
        SharedPtr<T> sharedFromThis() {
            return this->_weakRef.lock();
        }
        SharedPtr<T const> sharedFromThis() const {
            return this->_weakRef.lock();
        }

        /**
         * Returns the underlying weak pointer that shares ownership of *this.
         */
        WeakPtr<T> weakFromThis() {
            return this->_weakRef;
        }
        WeakPtr<T const> weakFromThis() const {
            return this->_weakRef;
        }

    private:
        /**
         * Weak reference to `this`, generated by the first invocation to a SharedPtr constructor
         * and dereferenced subsequently.
         */
        WeakPtr<T> _weakRef;
};


/**
 * Creates a shared pointer.
 */
template<class T, class... Args>
SharedPtr<T> MakeShared(Args&&... args) {
    return SharedPtr<T>(new T(args...));
}
}

#endif
