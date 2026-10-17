#pragma once
#include <cstdint>
#include <atomic>

namespace Core
{
    using ComponentTypeID = uint32_t;
    
    namespace Internal
    {
        inline ComponentTypeID getNextComponentTypeID() noexcept
        {
            static std::atomic<ComponentTypeID> lastID{0};
            return lastID++;
        }
    }
    
    /// Returns a unique type ID for each component type.
    /// This is much faster than using typeid().name() for lookups.
    template<typename T>
    inline ComponentTypeID getComponentTypeID() noexcept
    {
        static const ComponentTypeID typeID = Internal::getNextComponentTypeID();
        return typeID;
    }
}
