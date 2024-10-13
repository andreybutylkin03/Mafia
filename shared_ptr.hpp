#include <iostream>
#include <cstdlib>
#include <ctime>


template<typename T>
class Shared_ptr {
    template <typename U>
    struct ControlBlock {
        size_t count_;
        U object_;

        ControlBlock (const size_t &count, const U &object) :
            count_(count),
            object_(object)
        {}
    };

    ControlBlock<T> *cptr_ = nullptr;

    template <typename U, typename... Args>
    friend Shared_ptr<U> make_shared(Args&&... args);

    struct make_shared_t {};

    template <typename... Args>
    Shared_ptr(make_shared_t tag, ControlBlock<T> *storage_ptr):
        cptr_(storage_ptr) 
    {}

public:
    Shared_ptr() {}

    Shared_ptr(T *ptr) {
        cptr_ = new ControlBlock<T>(1, *ptr);
    }

    Shared_ptr(const Shared_ptr& other) {
        cptr_ = other.cptr_;
        if (cptr_)
            ++cptr_->count_;
    }

    Shared_ptr(Shared_ptr&& other) {
        cptr_ = other.cptr_;
        other.cptr_ = nullptr;
    }

    T& operator*() const {
        return cptr_->object_;
    }

    T* operator->() const {
        return &cptr_->object_;
    }

    Shared_ptr& operator=(const Shared_ptr& other) {
        if (this != &other) {
            if (cptr_ && --cptr_->count_ == 0) {
                delete cptr_;
            }

            cptr_ = other.cptr_;
            if (cptr_) {
                ++cptr_->count_;
            }
        }
        return *this;
    }

    Shared_ptr& operator=(Shared_ptr&& other) noexcept {
        if (this != &other) {
            if (cptr_ && --cptr_->count_ == 0) {
                delete cptr_;
            }

            cptr_ = other.cptr_;
            other.cptr_ = nullptr;
        }
        return *this;
    }

    size_t use_count() const {
        return cptr_ ? cptr_->count_ : 0;
    }

    T* get() const noexcept {
        return cptr_ ? &cptr_->object_ : nullptr;
    }

    void swap(Shared_ptr<T> &other) noexcept {
        auto vr = other.cptr_;
        other.cptr_ = cptr_;
        cptr_ = vr;
    }

    void reset(T *ptr = nullptr) {
        if (cptr_) {
            if (--cptr_->count_ == 0)
                delete cptr_;
        }
            
        if (ptr)
            cptr_ = new ControlBlock<T>(1, *ptr);
        else
            cptr_ = nullptr;
    }

    template<class U>
    friend std::strong_ordering operator<=>(const Shared_ptr<T>& lhs, const Shared_ptr<U>& rhs) noexcept {
        if (lhs.cptr_ == nullptr && rhs.cptr_ == nullptr) {
            return std::strong_ordering::equal;
        } else if (lhs.cptr_ == nullptr) {
            return std::strong_ordering::less;
        } else if (rhs.cptr_ == nullptr) {
            return std::strong_ordering::greater;
        } else {
            return lhs.cptr_->object_ <=> rhs.cptr_->object_;
        }
    }

    ~Shared_ptr() {
        if (cptr_) {
            if (cptr_->count_ > 1) {
                --cptr_->count_;
                return;
            }

            delete cptr_;
        }
    }
};

template <typename T, typename... Args>
Shared_ptr<T> make_shared(Args&&... args) {
    auto ptr = new typename Shared_ptr<T>::ControlBlock<T>{1, T{std::forward<Args>(args)...}};
    return Shared_ptr<T>(typename Shared_ptr<T>::make_shared_t(), ptr);
}
