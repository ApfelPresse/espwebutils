#pragma once
#include "../test_helpers.h"
#include "../../src/model/ModelSerializer.h"
#include "../../src/model/types/ModelTypeList.h"
#include "../../src/model/types/ModelTypePrimitive.h"
#include "../../src/model/ModelVar.h"
#include <Preferences.h>

namespace ListTest {

void testListBasics() {
  TEST_START("List Basic Operations");
  
  List<int, 5> list;
  CUSTOM_ASSERT(list.size() == 0, "Initial size should be 0");
  CUSTOM_ASSERT(list.capacity() == 5, "Capacity should be 5");
  CUSTOM_ASSERT(!list.isFull(), "Should not be full");
  
  // Add items
  CUSTOM_ASSERT(list.add(10), "Should add first item");
  CUSTOM_ASSERT(list.size() == 1, "Size should be 1");
  CUSTOM_ASSERT(list[0] == 10, "First item should be 10");
  
  list.add(20);
  list.add(30);
  CUSTOM_ASSERT(list.size() == 3, "Size should be 3");
  CUSTOM_ASSERT(list[1] == 20, "Second item should be 20");
  CUSTOM_ASSERT(list[2] == 30, "Third item should be 30");
  
  // Fill to capacity
  list.add(40);
  list.add(50);
  CUSTOM_ASSERT(list.isFull(), "Should be full");
  CUSTOM_ASSERT(!list.add(60), "Should not add when full");
  
  // Clear
  list.clear();
  CUSTOM_ASSERT(list.size() == 0, "Size should be 0 after clear");
  CUSTOM_ASSERT(!list.isFull(), "Should not be full after clear");
  
  TEST_END();
}

void testListIterator() {
  TEST_START("List Iterator");
  
  List<int, 5> list;
  list.add(1);
  list.add(2);
  list.add(3);
  
  int sum = 0;
  for (int val : list) {
    sum += val;
  }
  CUSTOM_ASSERT(sum == 6, "Sum should be 6 (1+2+3)");
  
  // Test const iterator
  const List<int, 5>& constList = list;
  int count = 0;
  for (auto val : constList) {
    count++;
  }
  CUSTOM_ASSERT(count == 3, "Should iterate over 3 items");
  
  TEST_END();
}

void testListWithStaticString() {
  TEST_START("List with String<N>");
  
  List<StringBuffer<20>, 3> list;
  
  StringBuffer<20> str1, str2, str3;
  str1.set("Alpha");
  str2.set("Beta");
  str3.set("Gamma");
  
  list.add(str1);
  list.add(str2);
  list.add(str3);
  
  CUSTOM_ASSERT(list.size() == 3, "Should have 3 items");
  CUSTOM_ASSERT(strcmp(list[0].c_str(), "Alpha") == 0, "First should be Alpha");
  CUSTOM_ASSERT(strcmp(list[1].c_str(), "Beta") == 0, "Second should be Beta");
  CUSTOM_ASSERT(strcmp(list[2].c_str(), "Gamma") == 0, "Third should be Gamma");
  
  TEST_END();
}

void testListSerialization() {
  TEST_START("List JSON Serialization");
  
  List<StringBuffer<32>, 5> list;
  StringBuffer<32> s1, s2, s3;
  s1.set("Network1");
  s2.set("Network2");
  s3.set("Network3");
  
  list.add(s1);
  list.add(s2);
  list.add(s3);
  
  // Serialize to JSON
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  fj::TypeAdapter<List<StringBuffer<32>, 5>>::write_ws(list, root);
  
  String json;
  serializeJson(root, json);
  Serial.println("List JSON: " + json);
  
  CUSTOM_ASSERT(root["type"] == "list", "Type should be 'list'");
  CUSTOM_ASSERT(root["count"] == 3, "Count should be 3");
  CUSTOM_ASSERT(root["capacity"] == 5, "Capacity should be 5");
  CUSTOM_ASSERT(root["items"].is<JsonArray>(), "Items should be an array");
  
  JsonArray items = root["items"];
  CUSTOM_ASSERT(items.size() == 3, "Items array should have 3 elements");
  CUSTOM_ASSERT(strcmp(items[0], "Network1") == 0, "First item should be Network1");
  CUSTOM_ASSERT(strcmp(items[1], "Network2") == 0, "Second item should be Network2");
  CUSTOM_ASSERT(strcmp(items[2], "Network3") == 0, "Third item should be Network3");
  
  TEST_END();
}

void testListDeserialization() {
  TEST_START("List JSON Deserialization");
  
  const char* jsonStr = R"({"items":["WiFi-A","WiFi-B","WiFi-C"]})";
  
  StaticJsonDocument<512> doc;
  deserializeJson(doc, jsonStr);
  JsonObject root = doc.as<JsonObject>();
  
  List<StringBuffer<32>, 10> list;
  bool success = fj::TypeAdapter<List<StringBuffer<32>, 10>>::read(list, root, false);
  
  CUSTOM_ASSERT(success, "Deserialization should succeed");
  CUSTOM_ASSERT(list.size() == 3, "Should have 3 items");
  CUSTOM_ASSERT(strcmp(list[0].c_str(), "WiFi-A") == 0, "First should be WiFi-A");
  CUSTOM_ASSERT(strcmp(list[1].c_str(), "WiFi-B") == 0, "Second should be WiFi-B");
  CUSTOM_ASSERT(strcmp(list[2].c_str(), "WiFi-C") == 0, "Third should be WiFi-C");
  
  TEST_END();
}

void testListInVar() {
  TEST_START("List in Var<> wrapper");
  
  fj::VarWsRo<List<StringBuffer<32>, 5>> availableNetworks;
  
  // Add networks
  StringBuffer<32> net1, net2;
  net1.set("Home-WiFi");
  net2.set("Office-WiFi");
  
  availableNetworks.get().add(net1);
  availableNetworks.get().add(net2);
  
  CUSTOM_ASSERT(availableNetworks.get().size() == 2, "Should have 2 networks");
  
  // Serialize as if via WebSocket
  StaticJsonDocument<512> doc;
  JsonObject root = doc.to<JsonObject>();
  
  // Simulate field serialization
  JsonObject nested = root.createNestedObject("available_networks");
  fj::TypeAdapter<List<StringBuffer<32>, 5>>::write_ws(availableNetworks.get(), nested);
  
  String json;
  serializeJson(root, json);
  Serial.println("Var<List> JSON: " + json);
  
  CUSTOM_ASSERT(nested["items"].size() == 2, "Should have 2 items in JSON");
  
  TEST_END();
}

void testListVarReadArrayShortcut() {
  TEST_START("Var<List> read array shortcut");

  struct Wrapper {
    fj::VarWsRo<List<StringBuffer<32>, 5>> available_networks;

    typedef fj::Schema<Wrapper,
                       fj::Field<Wrapper, decltype(available_networks)>>
        SchemaType;

    static const SchemaType& schema() {
      static const SchemaType s = fj::makeSchema<Wrapper>(
          fj::Field<Wrapper, decltype(available_networks)>{"available_networks", &Wrapper::available_networks});
      return s;
    }
  } w;

  // ReadDispatch::read_var_value supports array -> wraps into {items:[...]}
  const char* jsonStr = R"({\"available_networks\":[\"A\",\"B\"]})";
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  CUSTOM_ASSERT(!err, "JSON parse should succeed");

  bool ok = fj::readFieldsTolerant(w, Wrapper::schema(), doc.as<JsonObject>());
  CUSTOM_ASSERT(ok, "readFieldsTolerant should succeed");
  CUSTOM_ASSERT(w.available_networks.get().size() == 2, "List should contain two items");
  CUSTOM_ASSERT(strcmp(w.available_networks.get()[0].c_str(), "A") == 0, "First item A");
  CUSTOM_ASSERT(strcmp(w.available_networks.get()[1].c_str(), "B") == 0, "Second item B");

  TEST_END();
}

void runAllTests() {
  Serial.println("\n===== LIST TESTS =====\n");
  
  testListBasics();
  testListIterator();
  testListWithStaticString();
  testListSerialization();
  testListDeserialization();
  testListInVar();
  testListVarReadArrayShortcut();
  
  Serial.println("\n===== LIST TESTS COMPLETE =====\n");
}

} // namespace ListTest
