/*
 * This file is part of WinUtil.
 * This software is public domain. See UNLICENSE for more information.
 * WinUtil was created by Alexander Kernozhitsky.
 */

#ifndef EVENTHANDLER_H_INCLUDED
#define EVENTHANDLER_H_INCLUDED

#include <cstdint>
#include <functional>
#include <iostream>
#include <map>

class EventId {
  int64_t id;
  template <typename FuncArgs>
  friend class EventHandler;
  friend class EventOwner;
};

class EventOwner {
 public:
  EventOwner() {}
  EventOwner(const EventOwner &) = delete;
  EventOwner(EventOwner &&) = delete;
  EventOwner &operator=(const EventOwner &) = delete;
  EventOwner &operator=(EventOwner &&) = delete;

  virtual ~EventOwner() {
    destroying_ = true;
    for (const auto &iter : destroyHooks_) {
      iter.second();
    }
  }

  EventId AddDestroyHook(std::function<void()> hook) {
    EventId id;
    id.id = lastEvent_++;
    destroyHooks_[id.id] = hook;
    return id;
  }

  void RemoveDestroyHook(EventId id) { destroyHooks_.erase(id.id); }

  template <typename FuncArgs>
  friend class EventHandler;

 private:
  std::map<int64_t, std::function<void()>> destroyHooks_;
  int64_t lastEvent_ = 0;
  bool destroying_ = false;
};

template <typename FuncArgs>
class EventHandler {
 private:
  struct Event {
    EventOwner *owner;
    std::function<FuncArgs> func;
    EventId hook;
  };

 public:
  EventHandler() {}
  EventHandler(const EventHandler &) = delete;
  EventHandler(EventHandler &&) = delete;
  EventHandler &operator=(const EventHandler &) = delete;
  EventHandler &operator=(EventHandler &&) = delete;

  ~EventHandler() {
    for (const auto &iter : events_) {
      const Event &event = iter.second;
      if (event.owner != nullptr) {
        event.owner->RemoveDestroyHook(event.hook);
      }
    }
  }

  EventId AddEvent(std::function<FuncArgs> func, EventOwner *owner = nullptr) {
    int64_t id = ++lastEvent_;
    EventId res;
    res.id = id;
    EventId hook;
    if (owner != nullptr) {
      hook = owner->AddDestroyHook([this, res]() { RemoveEvent(res); });
    }
    events_[id] = Event{owner, func, hook};
    return res;
  }

  void RemoveEvent(EventId id) {
    const Event &event = events_.at(id.id);
    if (event.owner != nullptr && !event.owner->destroying_) {
      event.owner->RemoveDestroyHook(event.hook);
    }
    events_.erase(id.id);
  }

  template <typename... Args>
  void Activate(Args... args) {
    for (const auto &iter : events_) {
      iter.second.func(args...);
    }
  }

 private:
  std::map<int64_t, Event> events_;
  int64_t lastEvent_ = 0;
};

#endif  // EVENTHANDLER_H_INCLUDED
