/*
 * Vector-backed slot container with a free list
 */

#ifndef AMETHYST__FREE_LIST_H
#define AMETHYST__FREE_LIST_H

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace Amethyst {

/**
 * @brief Vector-backed container with a free list: stable ids, no shifting on erase.
 *
 * insert() reuses a freed slot before growing and returns its id; erase() frees a slot in
 * place so every other id stays valid. Iterate with slotCount() + tryGet(), which skips freed
 * slots. The same idea as a slot map or C++26 std::hive, minus generations.
 */
template <typename T> class FreeList {
  public:
    static constexpr uint32_t INVALID = UINT32_MAX;

    /**
     * @brief Store a value and return its stable id.
     * @param value Value to store; moved into the slot
     * @return Id usable with tryGet() and erase() until erased
     */
    uint32_t insert(T value)
    {
        if (!m_free.empty()) {
            uint32_t id = m_free.back();
            m_free.pop_back();
            m_slots[id].emplace(std::move(value));
            ++m_count;
            return id;
        }
        m_slots.emplace_back(std::move(value));
        ++m_count;
        return static_cast<uint32_t>(m_slots.size() - 1);
    }

    /**
     * @brief Free the slot for an id; a no-op if the id is already free or out of range.
     * @param id Id returned by a prior insert()
     */
    void erase(uint32_t id)
    {
        if (id < m_slots.size() && m_slots[id].has_value()) {
            m_slots[id].reset();
            m_free.push_back(id);
            --m_count;
        }
    }

    /**
     * @brief Access the value for an id.
     * @param id Id to look up
     * @return Pointer to the value, or nullptr if the slot is free or out of range
     */
    T *tryGet(uint32_t id)
    {
        if (id < m_slots.size() && m_slots[id].has_value()) {
            return &*m_slots[id];
        }
        return nullptr;
    }

    uint32_t slotCount() const { return static_cast<uint32_t>(m_slots.size()); }
    size_t size() const { return m_count; }
    bool empty() const { return m_count == 0; }

  private:
    std::vector<std::optional<T>> m_slots;
    std::vector<uint32_t> m_free;
    size_t m_count = 0;
};

} // namespace Amethyst

#endif // AMETHYST__FREE_LIST_H
