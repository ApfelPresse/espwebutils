#pragma once

#include "model/ModelBase.h"
#include "model/ModelVar.h"
#include "model/types/ModelTypePrimitive.h"

// Minimal test model to validate that multiple models can coexist
// without overwriting each other's Preferences and without sharing the same WS endpoint.
class Model : public ModelBase {
public:
  struct TestTopic {
    fj::VarWsPrefsRw<int> value;

    typedef fj::Schema<TestTopic,
                      fj::Field<TestTopic, decltype(value)>>
        SchemaType;

    static const SchemaType& schema() {
      static const SchemaType s = fj::makeSchema<TestTopic>(
          fj::Field<TestTopic, decltype(value)>{"value", &TestTopic::value});
      return s;
    }

    void setSaveCallback(std::function<void()> cb) {
      value.setOnChange(cb);
    }
  } test;

  Model() : ModelBase(80, "/ws2", "model2") {
    test.value = 1;
    registerTopic("test", test);
  }
};
