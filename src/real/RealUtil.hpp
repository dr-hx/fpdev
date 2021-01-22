#ifndef REAL_UTIL_HPP
#define REAL_UTIL_HPP
#include <stdlib.h>
#include <array>
#include <vector>
#include <list>
#include <stack>
#include <unordered_map>
#include <sys/time.h>
#include <iostream>
#include <assert.h>
#include "RealConfigure.h"

#define real_likely(x) __builtin_expect((x), 1)
#define real_unlikely(x) __builtin_expect((x), 0)
typedef unsigned long uint64;

#define PUSH_HEAD(nh, h) \
    nh->next = h;        \
    h = nh;
#define POP_HEAD(h) h = h->next;

#define KEY_SHIFT(k) (((uint64)k) >> 3)

namespace real
{
    namespace util
    {
        

        template <typename V>
        class SlotValueInitializer
        {
        public:
            inline void construct(V &v) {}
            inline void destruct(V &v) {}
        };

        template <typename V, int batchSize = 128, typename _Alloc = SlotValueInitializer<V>>
        class ValuePool
        {
        public:
            typedef V *value_ptr;
            static ValuePool INSTANCE;

        private:
            _Alloc initializer;
            class Slot
            {
            public:
                value_ptr value;
                Slot *next;
                Slot(value_ptr v) : value(v) {}
            };
            Slot *availableHead;
            Slot *usedHead;
            Slot *memoryHead;

            void expand()
            {
                value_ptr m = new V[batchSize];
                Slot *mh = new Slot(m);
                PUSH_HEAD(mh, memoryHead)

                for (int i = 0; i < batchSize; i++)
                {
                    if (std::is_same<decltype(initializer), SlotValueInitializer<V>>::value == false)
                    {
                        initializer.construct(m[i]);
                    }
                    Slot *fh = new Slot(&m[i]);
                    PUSH_HEAD(fh, availableHead)
                }
            }

        public:
            value_ptr get()
            {
                Slot *slot;
                if (real_unlikely(availableHead == nullptr))
                {
                    expand();
                }
                slot = availableHead;
                POP_HEAD(availableHead)
                PUSH_HEAD(slot, usedHead)
                return slot->value;
            }

            void put(value_ptr pt)
            {
                Slot *slot;
                if (real_unlikely(usedHead == nullptr))
                {
                    slot = new Slot(pt);
                }
                else
                {
                    slot = usedHead;
                    POP_HEAD(usedHead)
                    slot->value = pt;
                }
                PUSH_HEAD(slot, availableHead)
            }
            ValuePool() : availableHead(nullptr), usedHead(nullptr), memoryHead(nullptr)
            {
                expand();
            }
            ~ValuePool()
            {
                while (availableHead != nullptr)
                {
                    Slot *n = availableHead;
                    POP_HEAD(availableHead)
                    delete n;
                }
                while (usedHead != nullptr)
                {
                    Slot *n = usedHead;
                    POP_HEAD(usedHead)
                    delete n;
                }
                while (memoryHead != nullptr)
                {
                    Slot *n = memoryHead;
                    POP_HEAD(memoryHead)
                    if (std::is_same<decltype(initializer), SlotValueInitializer<V>>::value == false)
                    {
                        for (int i = 0; i < batchSize; i++)
                        {
                            initializer.destruct((n->value)[i]);
                        }
                    }
                    delete[] n->value;
                    delete n;
                }
            }
        };
        template <typename V, int batchSize, typename A>
        ValuePool<V, batchSize, A> ValuePool<V, batchSize, A>::INSTANCE;

        template <typename RealType>
        using RealPool = util::ValuePool<RealType>;

        template <typename Key, typename RealType, int cacheSize = 256, int mask = cacheSize - 1>
        class VariableMap
        {
            using __value_ptr = RealType *;
#if DELEGATE_TO_POOL
            using __value_type = __value_ptr;
#else
            using __value_type = RealType;
#endif
        public:
            static VariableMap<Key, RealType, cacheSize, mask> INSTANCE;

        private:
            std::unordered_map<Key, __value_type> map;

            struct RealCache
            {
                Key address;
                __value_ptr real_ptr;
                RealCache()
                {
                    address = nullptr;
                    real_ptr = nullptr;
                }
            };
            RealCache cache[cacheSize];

        public:
            RealType &getOrInit(const double *address)
            {
                int index = KEY_SHIFT(address) & mask;
                RealCache &c = cache[index];
                if (c.address != address)
                {
#if DELEGATE_TO_POOL
                    RealType *&ptr = map[(Key)KEY_SHIFT(address)];
                    if(ptr==NULL)
                    {
                        ptr = RealPool<RealType>::INSTANCE.get();
                        *ptr = *address; // init
                    }
#else
                    auto it = map.find((Key)KEY_SHIFT(address));
                    RealType *ptr = NULL;
                    if(it==map.end())
                    {
                        ptr = &(map[(Key)KEY_SHIFT(address)] = *address); // init
                    }
                    else ptr = *it;
#endif
                    c.address = address;
                    c.real_ptr = ptr;
                }
                return *c.real_ptr;
            }

            RealType &operator[](Key address)
            {
                int index = KEY_SHIFT(address) & mask;
                RealCache &c = cache[index];
                if (c.address != address)
                {
#if DELEGATE_TO_POOL
                    RealType *&ptr = map[(Key)KEY_SHIFT(address)];
                    if(ptr==NULL)
                    {
                        ptr = RealPool<RealType>::INSTANCE.get();
                    }
#else
                    RealType *ptr = &map[(Key)KEY_SHIFT(address)];
#endif
                    c.address = address;
                    c.real_ptr = ptr;
                }
                return *c.real_ptr;
            }
            RealType &def(Key address)
            {
                RealType *ptr = NULL;
#if DELEGATE_TO_POOL
                __value_ptr vp = RealPool<RealType>::INSTANCE.get();
                map[(Key)KEY_SHIFT(address)] = vp;
                ptr = vp;
#else
                ptr = &map[(Key)KEY_SHIFT(address)]; // create a real with default constructor
#endif
                return *ptr;
            }
            void undef(Key address)
            {
                int index = KEY_SHIFT(address) & mask;
                RealCache &c = cache[index];
                if (c.address == address)
                {
                    c.address = nullptr;
                    c.real_ptr = nullptr;
                }
#if DELEGATE_TO_POOL
                auto it = map.find((Key)KEY_SHIFT(address));
                RealPool<RealType>::INSTANCE.put(it->second);
                map.erase(it);
#else
                map.erase((Key)KEY_SHIFT(address));
#endif
            }
        };
        template <typename K, typename T, int c, int m>
        VariableMap<K, T, c, m> VariableMap<K, T, c, m>::INSTANCE;
    }; // namespace util
};     // namespace real

#endif