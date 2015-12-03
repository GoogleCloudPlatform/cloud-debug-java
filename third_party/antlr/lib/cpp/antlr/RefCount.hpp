#ifndef INC_RefCount_hpp__
#define INC_RefCount_hpp__
/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/RIGHTS.html
 *
 * $Id: //depot/code/org.antlr/release/antlr-2.7.2/lib/cpp/antlr/RefCount.hpp#1 $
 */

#include <antlr/config.hpp>

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
namespace antlr {
#endif

template<class T>
class ANTLR_API RefCount {
private:
        struct Ref {
                T* const ptr;
                int count;

                // static_ref == true will create a no-op reference counter for
                // static objects that are never supposed to be deleted.
                Ref(T* p, bool static_ref) : ptr(p), count(static_ref ? -1 : 1) {}
                ~Ref() {delete ptr;}
                Ref* increment() {if (count != -1) ++count;return this;}
                bool decrement() {return (count != -1 && --count==0);}
        private:
                Ref(const Ref&);
                Ref& operator=(const Ref&);
        }* ref;

public:
        explicit RefCount(T* p = 0, bool static_ref = false)
        : ref(p ? new Ref(p, static_ref) : 0)
        {
        }
        RefCount(const RefCount<T>& other)
        : ref(other.ref ? other.ref->increment() : 0)
        {
        }
        ~RefCount()
        {
                if (ref && ref->decrement())
                        delete ref;
        }
        RefCount<T>& operator=(const RefCount<T>& other)
        {
                Ref* tmp = other.ref ? other.ref->increment() : 0;
                if (ref && ref->decrement())
                        delete ref;
                ref = tmp;
                return *this;
        }

        operator T* () const
        {
                return ref ? ref->ptr : 0;
        }

        T* operator->() const
        {
                return ref ? ref->ptr : 0;
        }

        T* get() const
        {
                return ref ? ref->ptr : 0;
        }

        template<class newType> operator RefCount<newType>()
        {
                return RefCount<newType>(ref);
        }
};

#ifdef ANTLR_CXX_SUPPORTS_NAMESPACE
}
#endif

#endif //INC_RefCount_hpp__
