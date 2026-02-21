#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include <basis/string_hash.h>

namespace basis
{
    template<typename T>
    class Cache
    {
    public:
        using Loader = std::function<std::unique_ptr<T>(std::string_view)>;

    private:
        Loader                                                m_loader;
        std::unordered_map<string_hash_t, std::unique_ptr<T>> m_assets;

    public:
        explicit Cache(Loader loader)
            : m_loader(std::move(loader))
        {}

        ~Cache()                       = default;

        Cache(const Cache&)            = delete;
        Cache& operator=(const Cache&) = delete;
        Cache(Cache&&)                 = default;
        Cache& operator=(Cache&&)      = default;

        T* GetOrLoad(std::string_view key)
        {
            const string_hash_t hash = hash_string(key);
            if (T* existing = Get(hash))
                return existing;

            return Get(Store(hash, m_loader(key)));
        }

        [[nodiscard]] const T* Get(string_hash_t key) const
        {
            auto it = m_assets.find(key);
            return it != m_assets.end() ? it->second.get() : nullptr;
        }

        [[nodiscard]] T* Get(string_hash_t key)
        {
            auto it = m_assets.find(key);
            return it != m_assets.end() ? it->second.get() : nullptr;
        }

        string_hash_t Store(string_hash_t key, std::unique_ptr<T> asset)
        {
            m_assets.insert_or_assign(key, std::move(asset));
            return key;
        }

        string_hash_t Store(std::string_view key, std::unique_ptr<T> asset)
        {
            return Store(hash_string(key), std::move(asset));
        }

        void Release(string_hash_t key)
        {
            m_assets.erase(key);
        }

        void ReleaseAll()
        {
            m_assets.clear();
        }

        [[nodiscard]] bool Contains(string_hash_t key) const
        {
            return m_assets.contains(key);
        }

        [[nodiscard]] std::size_t Size() const
        {
            return m_assets.size();
        }
    };

}
