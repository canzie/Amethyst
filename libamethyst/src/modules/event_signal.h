#ifndef AMETHYST__EVENT_SIGNAL_H
#define AMETHYST__EVENT_SIGNAL_H

#include <cstdint>
#include <functional>
#include <map>

#include "utils/am_assert.h"

namespace Amethyst {

template <typename> class EventSignal;

class EventSignalBase {
  public:
    virtual ~EventSignalBase() = default;

    /**
     * @brief Removes a connection by its ID.
     * @param id The connection ID to remove.
     */
    virtual void removeConnection(uint32_t id) = 0;

  protected:
    bool m_firing = false;
};

class EventConnection {
    template <typename> friend class EventSignal;

  public:
    EventConnection() = default;

    /**
     * @brief Destructor automatically disconnects from the
     * signal if still connected.
     */
    ~EventConnection() { disconnect(); }

    EventConnection(EventConnection &&other) noexcept
        : m_signal(other.m_signal), m_id(other.m_id) {
        other.m_signal = nullptr;
    }

    EventConnection &operator=(EventConnection &&other) noexcept {
        if (this != &other) {
            disconnect();
            m_signal = other.m_signal;
            m_id = other.m_id;
            other.m_signal = nullptr;
        }
        return *this;
    }

    EventConnection(const EventConnection &) = delete;
    EventConnection &operator=(const EventConnection &) = delete;

    /**
     * @brief Disconnects from the signal.
     * Safe to call multiple times.
     */
    void disconnect();

    /**
     * @brief Returns whether this connection is still active.
     * @return True if connected, false otherwise.
     */
    bool connected() const { return m_signal != nullptr; }

  private:
    EventConnection(EventSignalBase *signal, uint32_t id)
        : m_signal(signal), m_id(id) {}

    EventSignalBase *m_signal = nullptr;
    uint32_t m_id = 0;
};

inline void EventConnection::disconnect() {
    if (m_signal) {
        m_signal->removeConnection(m_id);
        m_signal = nullptr;
    }
}

template <typename sig> class EventSignal;

template <typename... Args> class EventSignal<void(Args...)> : public EventSignalBase {
  public:
    /**
     * @brief Subscribes a callback to this signal.
     * Returns a scoped connection that automatically
     * disconnects on destruction.
     * @param func The callback to subscribe.
     * @return An EventConnection that manages the subscription lifetime.
     */
    EventConnection connect(std::function<void(Args...)> func) {
        uint32_t id = m_nextId++;
        m_slots.emplace(id, Slot{std::move(func), false});
        return EventConnection(this, id);
    }

    /**
     * @brief Subscribes a callback that will be automatically
     * removed after the first fire.
     * @param func The callback to subscribe.
     * @return An EventConnection that manages the subscription lifetime.
     */
    EventConnection once(std::function<void(Args...)> func) {
        uint32_t id = m_nextId++;
        m_slots.emplace(id, Slot{std::move(func), true});
        return EventConnection(this, id);
    }

    /**
     * @brief Invokes all connected callbacks with the given arguments.
     * Once-connections are removed after all callbacks have been called.
     * @param args The arguments to forward to each callback.
     */
    void fire(Args... args) {
        m_firing = true;
        for (auto &[id, slot] : m_slots)
            slot.func(args...);
        std::erase_if(m_slots, [](const auto &pair) { return pair.second.once; });
        m_firing = false;
    }

    /**
     * @brief Removes a connection by its ID.
     * @param id The connection ID to remove.
     */
    void removeConnection(uint32_t id) override {
        AM_ASSERT(!m_firing, "Cannot disconnect from a signal while it is firing");
        m_slots.erase(id);
    }

  private:
    struct Slot {
        std::function<void(Args...)> func;
        bool once;
    };

    std::map<uint32_t, Slot> m_slots;
    uint32_t m_nextId = 0;
};

} // namespace Amethyst

#endif // AMETHYST__EVENT_SIGNAL_H
