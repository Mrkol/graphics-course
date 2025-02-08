#pragma once

#include <cstdint>
#include <vector>
template<typename T>
requires requires{ T::Id::Invalid; }

class ResourceManager {
public:
    ResourceManager()  = default;
    ~ResourceManager() = default;
    
    ResourceManager           (const ResourceManager& oth) = delete;
    ResourceManager& operator=(const ResourceManager& oth) = delete;

    explicit ResourceManager(std::vector<T> v) : resources(std::move(v)) {}


    void reserve(std::size_t n) { resources.reserve(n); }

    template< class... Args >
    T::Id emplace( Args&&... args ) {
        resources.emplace_back(std::forward<Args>(args)...);
        return static_cast<T::Id>(resources.size()-1);
    }

    T::Id add(T resource) {
        resources.emplace_back(std::move(resource));
        return static_cast<T::Id>(resources.size()-1);
    }

    const T& operator[](T::Id id) const {
        assert(static_cast<uint32_t>(id) < resources.size());
        return resources[static_cast<uint32_t>(id)];
    }

    T& get(T::Id id) {
        assert(static_cast<uint32_t>(id) < resources.size());
        return resources[static_cast<uint32_t>(id)];
    }

    auto begin() const { return resources.begin(); }
    auto end  () const { return resources.end();   }

    std::size_t size() const { return resources.size(); }

    const T* data() const { return resources.data(); }
private:
    std::vector<T> resources;
};