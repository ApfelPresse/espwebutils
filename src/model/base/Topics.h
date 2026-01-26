#pragma once

// Included by src/model/ModelBase.h

#include <type_traits>
#include <functional>

template <typename U>
struct ModelBase::has_setSaveCallback {
  template <typename V>
  static auto test(int)
      -> decltype(std::declval<V>().setSaveCallback(std::declval<std::function<void()>>()), std::true_type());
  template <typename>
  static std::false_type test(...);
  using type = decltype(test<U>(0));
  static const bool value = type::value;
};

template <typename T>
inline void ModelBase::registerTopic(const char* topic, T& obj) {
  addEntry<T>(topic, obj, fj::TypeAdapter<T>::defaultPersist(), fj::TypeAdapter<T>::defaultWsSend());
}

template <typename T>
inline void ModelBase::registerTopic(const char* topic, T& obj, bool persist, bool wsSend) {
  addEntry<T>(topic, obj, persist, wsSend);
}

template <typename T>
inline void ModelBase::addEntry(const char* topic, T& obj, bool persist, bool wsSend) {
  if (entryCount_ >= MAX_TOPICS) return;

  Entry& e = entries_[entryCount_++];
  e.topic = topic;
  e.objPtr = (void*)&obj;
  e.persist = persist;
  e.ws_send = wsSend;

  e.makeWsJson = &makeWsJsonImpl<T>;
  e.makePrefsJson = &makePrefsJsonImpl<T>;
  e.applyUpdate = &applyUpdateImpl<T>;

  // If the topic type exposes setSaveCallback(std::function<void()>), hook it to persist this entry on changes.
  {
    Entry* ep = &e;
    maybeAttachSaveCallback<T>(obj, ep);
  }
}

template <typename T>
inline typename std::enable_if<ModelBase::has_setSaveCallback<T>::value, void>::type
ModelBase::maybeAttachSaveCallback(T& obj, Entry* ep) {
  obj.setSaveCallback([this, ep]() { this->saveEntry(*ep); });
}

template <typename T>
inline typename std::enable_if<!ModelBase::has_setSaveCallback<T>::value, void>::type
ModelBase::maybeAttachSaveCallback(T&, Entry*) {}

inline ModelBase::Entry* ModelBase::find(const char* topic) {
  for (size_t i = 0; i < entryCount_; ++i) {
    if (strcmp(entries_[i].topic, topic) == 0) return &entries_[i];
  }
  return nullptr;
}
