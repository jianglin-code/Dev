/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "format/binary/TableFlattener.h"

#include "android-base/stringprintf.h"
#include "androidfw/TypeWrappers.h"

#include "ResChunkPullParser.h"
#include "ResourceUtils.h"
#include "SdkConstants.h"
#include "format/binary/BinaryResourceParser.h"
#include "test/Test.h"
#include "util/Util.h"

using namespace android;

using ::testing::Gt;
using ::testing::IsNull;
using ::testing::NotNull;

namespace aapt {

class TableFlattenerTest : public ::testing::Test {
 public:
  void SetUp() override {
    context_ =
        test::ContextBuilder().SetCompilationPackage("com.app.test").SetPackageId(0x7f).Build();
  }

  ::testing::AssertionResult Flatten(IAaptContext* context, const TableFlattenerOptions& options,
                                     ResourceTable* table, std::string* out_content) {
    BigBuffer buffer(1024);
    TableFlattener flattener(options, &buffer);
    if (!flattener.Consume(context, table)) {
      return ::testing::AssertionFailure() << "failed to flatten ResourceTable";
    }
    *out_content = buffer.to_string();
    return ::testing::AssertionSuccess();
  }

  ::testing::AssertionResult Flatten(IAaptContext* context, const TableFlattenerOptions& options,
                                     ResourceTable* table, ResTable* out_table) {
    std::string content;
    auto result = Flatten(context, options, table, &content);
    if (!result) {
      return result;
    }

    if (out_table->add(content.data(), content.size(), 1, true) != NO_ERROR) {
      return ::testing::AssertionFailure() << "flattened ResTable is corrupt";
    }
    return ::testing::AssertionSuccess();
  }

  ::testing::AssertionResult Flatten(IAaptContext* context, const TableFlattenerOptions& options,
                                     ResourceTable* table, ResourceTable* out_table) {
    std::string content;
    auto result = Flatten(context, options, table, &content);
    if (!result) {
      return result;
    }

    BinaryResourceParser parser(context->GetDiagnostics(), out_table, {}, content.data(),
                                content.size());
    if (!parser.Parse()) {
      return ::testing::AssertionFailure() << "flattened ResTable is corrupt";
    }
    return ::testing::AssertionSuccess();
  }

  ::testing::AssertionResult Exists(ResTable* table, const StringPiece& expected_name,
                                    const ResourceId& expected_id,
                                    const ConfigDescription& expected_config,
                                    const uint8_t expected_data_type, const uint32_t expected_data,
                                    const uint32_t expected_spec_flags) {
    const ResourceName expected_res_name = test::ParseNameOrDie(expected_name);

    table->setParameters(&expected_config);

    ResTable_config config;
    Res_value val;
    uint32_t spec_flags;
    if (table->getResource(expected_id.id, &val, false, 0, &spec_flags, &config) < 0) {
      return ::testing::AssertionFailure() << "could not find resource with";
    }

    if (expected_data_type != val.dataType) {
      return ::testing::AssertionFailure()
             << "expected data type " << std::hex << (int)expected_data_type
             << " but got data type " << (int)val.dataType << std::dec << " instead";
    }

    if (expected_data != val.data) {
      return ::testing::AssertionFailure()
             << "expected data " << std::hex << expected_data << " but got data " << val.data
             << std::dec << " instead";
    }

    if (expected_spec_flags != spec_flags) {
      return ::testing::AssertionFailure()
             << "expected specFlags " << std::hex << expected_spec_flags << " but got specFlags "
             << spec_flags << std::dec << " instead";
    }

    ResTable::resource_name actual_name;
    if (!table->getResourceName(expected_id.id, false, &actual_name)) {
      return ::testing::AssertionFailure() << "failed to find resource name";
    }

    Maybe<ResourceName> resName = ResourceUtils::ToResourceName(actual_name);
    if (!resName) {
      return ::testing::AssertionFailure()
             << "expected name '" << expected_res_name << "' but got '"
             << StringPiece16(actual_name.package, actual_name.packageLen) << ":"
             << StringPiece16(actual_name.type, actual_name.typeLen) << "/"
             << StringPiece16(actual_name.name, actual_name.nameLen) << "'";
    }

    ResourceName actual_res_name(resName.value());

    if (expected_res_name.entry != actual_res_name.entry ||
        expected_res_name.package != actual_res_name.package ||
        expected_res_name.type != actual_res_name.type) {
      return ::testing::AssertionFailure() << "expected resource '" << expected_res_name.to_string()
                                           << "' but got '" << actual_res_name.to_string() << "'";
    }

    if (expected_config != config) {
      return ::testing::AssertionFailure() << "expected config '" << expected_config
                                           << "' but got '" << ConfigDescription(config) << "'";
    }
    return ::testing::AssertionSuccess();
  }

 protected:
  std::unique_ptr<IAaptContext> context_;
};

TEST_F(TableFlattenerTest, FlattenFullyLinkedTable) {
  std::unique_ptr<ResourceTable> table =
      test::ResourceTableBuilder()
          .SetPackageId("com.app.test", 0x7f)
          .AddSimple("com.app.test:id/one", ResourceId(0x7f020000))
          .AddSimple("com.app.test:id/two", ResourceId(0x7f020001))
          .AddValue("com.app.test:id/three", ResourceId(0x7f020002),
                    test::BuildReference("com.app.test:id/one", ResourceId(0x7f020000)))
          .AddValue("com.app.test:integer/one", ResourceId(0x7f030000),
                    util::make_unique<BinaryPrimitive>(uint8_t(Res_value::TYPE_INT_DEC), 1u))
          .AddValue("com.app.test:integer/one", test::ParseConfigOrDie("v1"),
                    ResourceId(0x7f030000),
                    util::make_unique<BinaryPrimitive>(uint8_t(Res_value::TYPE_INT_DEC), 2u))
          .AddString("com.app.test:string/test", ResourceId(0x7f040000), "foo")
          .AddString("com.app.test:layout/bar", ResourceId(0x7f050000), "res/layout/bar.xml")
          .Build();

  ResTable res_table;
  ASSERT_TRUE(Flatten(context_.get(), {}, table.get(), &res_table));

  EXPECT_TRUE(Exists(&res_table, "com.app.test:id/one", ResourceId(0x7f020000), {},
                     Res_value::TYPE_INT_BOOLEAN, 0u, 0u));

  EXPECT_TRUE(Exists(&res_table, "com.app.test:id/two", ResourceId(0x7f020001), {},
                     Res_value::TYPE_INT_BOOLEAN, 0u, 0u));

  EXPECT_TRUE(Exists(&res_table, "com.app.test:id/three", ResourceId(0x7f020002), {},
                     Res_value::TYPE_REFERENCE, 0x7f020000u, 0u));

  EXPECT_TRUE(Exists(&res_table, "com.app.test:integer/one", ResourceId(0x7f030000), {},
                     Res_value::TYPE_INT_DEC, 1u, ResTable_config::CONFIG_VERSION));

  EXPECT_TRUE(Exists(&res_table, "com.app.test:integer/one", ResourceId(0x7f030000),
                     test::ParseConfigOrDie("v1"), Res_value::TYPE_INT_DEC, 2u,
                     ResTable_config::CONFIG_VERSION));

  std::u16string foo_str = u"foo";
  ssize_t idx = res_table.getTableStringBlock(0)->indexOfString(foo_str.data(), foo_str.size());
  ASSERT_GE(idx, 0);
  EXPECT_TRUE(Exists(&res_table, "com.app.test:string/test", ResourceId(0x7f040000), {},
                     Res_value::TYPE_STRING, (uint32_t)idx, 0u));

  std::u16string bar_path = u"res/layout/bar.xml";
  idx = res_table.getTableStringBlock(0)->indexOfString(bar_path.data(), bar_path.size());
  ASSERT_GE(idx, 0);
  EXPECT_TRUE(Exists(&res_table, "com.app.test:layout/bar", ResourceId(0x7f050000), {},
                     Res_value::TYPE_STRING, (uint32_t)idx, 0u));
}

TEST_F(TableFlattenerTest, FlattenEntriesWithGapsInIds) {
  std::unique_ptr<ResourceTable> table =
      test::ResourceTableBuilder()
          .SetPackageId("com.app.test", 0x7f)
          .AddSimple("com.app.test:id/one", ResourceId(0x7f020001))
          .AddSimple("com.app.test:id/three", ResourceId(0x7f020003))
          .Build();

  ResTable res_table;
  ASSERT_TRUE(Flatten(context_.get(), {}, table.get(), &res_table));

  EXPECT_TRUE(Exists(&res_table, "com.app.test:id/one", ResourceId(0x7f020001), {},
                     Res_value::TYPE_INT_BOOLEAN, 0u, 0u));
  EXPECT_TRUE(Exists(&res_table, "com.app.test:id/three", ResourceId(0x7f020003), {},
                     Res_value::TYPE_INT_BOOLEAN, 0u, 0u));
}

TEST_F(TableFlattenerTest, FlattenMinMaxAttributes) {
  Attribute attr;
  attr.type_mask = android::ResTable_map::TYPE_INTEGER;
  attr.min_int = 10;
  attr.max_int = 23;
  std::unique_ptr<ResourceTable> table =
      test::ResourceTableBuilder()
          .SetPackageId("android", 0x01)
          .AddValue("android:attr/foo", ResourceId(0x01010000), util::make_unique<Attribute>(attr))
          .Build();

  ResourceTable result;
  ASSERT_TRUE(Flatten(context_.get(), {}, table.get(), &result));

  Attribute* actual_attr = test::GetValue<Attribute>(&result, "android:attr/foo");
  ASSERT_THAT(actual_attr, NotNull());
  EXPECT_EQ(attr.IsWeak(), actual_attr->IsWeak());
  EXPECT_EQ(attr.type_mask, actual_attr->type_mask);
  EXPECT_EQ(attr.min_int, actual_attr->min_int);
  EXPECT_EQ(attr.max_int, actual_attr->max_int);
}

TEST_F(TableFlattenerTest, FlattenArray) {
  auto array = util::make_unique<Array>();
  array->elements.push_back(util::make_unique<BinaryPrimitive>(uint8_t(Res_value::TYPE_INT_DEC),
                                                               1u));
  array->elements.push_back(util::make_unique<BinaryPrimitive>(uint8_t(Res_value::TYPE_INT_DEC),
                                                               2u));
  std::unique_ptr<ResourceTable> table =
      test::ResourceTableBuilder()
          .SetPackageId("android", 0x01)
          .AddValue("android:array/foo", ResourceId(0x01010000), std::move(array))
          .Build();

  std::string result;
  ASSERT_TRUE(Flatten(context_.get(), {}, table.get(), &result));

  // Parse the flattened resource table
  ResChunkPullParser parser(result.data(), result.size());
  ASSERT_TRUE(parser.IsGoodEvent(parser.Next()));
  ASSERT_EQ(util::DeviceToHost16(parser.chunk()->type), RES_TABLE_TYPE);

  // Retrieve the package of the entry
  ResChunkPullParser table_parser(GetChunkData(parser.chunk()), GetChunkDataLen(parser.chunk()));
  const ResChunk_header* package_chunk = nullptr;
  while (table_parser.IsGoodEvent(table_parser.Next())) {
    if (util::DeviceToHost16(table_parser.chunk()->type) == RES_TABLE_PACKAGE_TYPE) {
      package_chunk = table_parser.chunk();
      break;
    }
  }

  // Retrieve the type that proceeds the array entry
  ASSERT_NE(package_chunk, nullptr);
  ResChunkPullParser package_parser(GetChunkData(table_parser.chunk()),
                                    GetChunkDataLen(table_parser.chunk()));
  const ResChunk_header* type_chunk = nullptr;
  while (package_parser.IsGoodEvent(package_parser.Next())) {
    if (util::DeviceToHost16(package_parser.chunk()->type) == RES_TABLE_TYPE_TYPE) {
      type_chunk = package_parser.chunk();
      break;
    }
  }

  // Retrieve the array entry
  ASSERT_NE(type_chunk, nullptr);
  TypeVariant typeVariant((const ResTable_type*) type_chunk);
  auto entry = (const ResTable_map_entry*)*typeVariant.beginEntries();
  ASSERT_EQ(util::DeviceToHost16(entry->count), 2u);

  // Check that the value and name of the array entries are correct
  auto values = (const ResTable_map*)(((const uint8_t *)entry) + entry->size);
  ASSERT_EQ(values->value.data, 1u);
  ASSERT_EQ(values->name.ident, android::ResTable_map::ATTR_MIN);
  ASSERT_EQ((values+1)->value.data, 2u);
  ASSERT_EQ((values+1)->name.ident, android::ResTable_map::ATTR_MIN + 1);
}

static std::unique_ptr<ResourceTable> BuildTableWithSparseEntries(
    IAaptContext* context, const ConfigDescription& sparse_config, float load) {
  std::unique_ptr<ResourceTable> table =
      test::ResourceTableBuilder()
          .SetPackageId(context->GetCompilationPackage(), context->GetPackageId())
          .Build();

  // Add regular entries.
  int stride = static_cast<int>(1.0f / load);
  for (int i = 0; i < 100; i++) {
    const ResourceName name = test::ParseNameOrDie(
        base::StringPrintf("%s:string/foo_%d", context->GetCompilationPackage().data(), i));
    const ResourceId resid(context->GetPackageId(), 0x02, static_cast<uint16_t>(i));
    const auto value =
        util::make_unique<BinaryPrimitive>(Res_value::TYPE_INT_DEC, static_cast<uint32_t>(i));
    CHECK(table->AddResourceWithId(name, resid, ConfigDescription::DefaultConfig(), "",
                                   std::unique_ptr<Value>(value->Clone(nullptr)),
                                   context->GetDiagnostics()));

    // Every few entries, write out a sparse_config value. This will give us the desired load.
    if (i % stride == 0) {
      CHECK(table->AddResourceWithId(name, resid, sparse_config, "",
                                     std::unique_ptr<Value>(value->Clone(nullptr)),
                                     context->GetDiagnostics()));
    }
  }
  return table;
}

TEST_F(TableFlattenerTest, FlattenSparseEntryWithMinSdkO) {
  std::unique_ptr<IAaptContext> context = test::ContextBuilder()
                                              .SetCompilationPackage("android")
                                              .SetPackageId(0x01)
                                              .SetMinSdkVersion(SDK_O)
                                              .Build();

  const ConfigDescription sparse_config = test::ParseConfigOrDie("en-rGB");
  auto table_in = BuildTableWithSparseEntries(context.get(), sparse_config, 0.25f);

  TableFlattenerOptions options;
  options.use_sparse_entries = true;

  std::string no_sparse_contents;
  ASSERT_TRUE(Flatten(context.get(), {}, table_in.get(), &no_sparse_contents));

  std::string sparse_contents;
  ASSERT_TRUE(Flatten(context.get(), options, table_in.get(), &sparse_contents));

  EXPECT_GT(no_sparse_contents.size(), sparse_contents.size());

  // Attempt to parse the sparse contents.

  ResourceTable sparse_table;
  BinaryResourceParser parser(context->GetDiagnostics(), &sparse_table, Source("test.arsc"),
                              sparse_contents.data(), sparse_contents.size());
  ASSERT_TRUE(parser.Parse());

  auto value = test::GetValueForConfig<BinaryPrimitive>(&sparse_table, "android:string/foo_0",
                                                        sparse_config);
  ASSERT_THAT(value, NotNull());
  EXPECT_EQ(0u, value->value.data);

  ASSERT_THAT(test::GetValueForConfig<BinaryPrimitive>(&sparse_table, "android:string/foo_1",
                                                       sparse_config),
              IsNull());

  value = test::GetValueForConfig<BinaryPrimitive>(&sparse_table, "android:string/foo_4",
                                                   sparse_config);
  ASSERT_THAT(value, NotNull());
  EXPECT_EQ(4u, value->value.data);
}

TEST_F(TableFlattenerTest, FlattenSparseEntryWithConfigSdkVersionO) {
  std::unique_ptr<IAaptContext> context = test::ContextBuilder()
                                              .SetCompilationPackage("android")
                                              .SetPackageId(0x01)
                                              .SetMinSdkVersion(SDK_LOLLIPOP)
                                              .Build();

  const ConfigDescription sparse_config = test::ParseConfigOrDie("en-rGB-v26");
  auto table_in = BuildTableWithSparseEntries(context.get(), sparse_config, 0.25f);

  TableFlattenerOptions options;
  options.use_sparse_entries = true;

  std::string no_sparse_contents;
  ASSERT_TRUE(Flatten(context.get(), {}, table_in.get(), &no_sparse_contents));

  std::string sparse_contents;
  ASSERT_TRUE(Flatten(context.get(), options, table_in.get(), &sparse_contents));

  EXPECT_GT(no_sparse_contents.size(), sparse_contents.size());
}

TEST_F(TableFlattenerTest, DoNotUseSparseEntryForDenseConfig) {
  std::unique_ptr<IAaptContext> context = test::ContextBuilder()
                                              .SetCompilationPackage("android")
                                              .SetPackageId(0x01)
                                              .SetMinSdkVersion(SDK_O)
                                              .Build();

  const ConfigDescription sparse_config = test::ParseConfigOrDie("en-rGB");
  auto table_in = BuildTableWithSparseEntries(context.get(), sparse_config, 0.80f);

  TableFlattenerOptions options;
  options.use_sparse_entries = true;

  std::string no_sparse_contents;
  ASSERT_TRUE(Flatten(context.get(), {}, table_in.get(), &no_sparse_contents));

  std::string sparse_contents;
  ASSERT_TRUE(Flatten(context.get(), options, table_in.get(), &sparse_contents));

  EXPECT_EQ(no_sparse_contents.size(), sparse_contents.size());
}

TEST_F(TableFlattenerTest, FlattenSharedLibrary) {
  std::unique_ptr<IAaptContext> context =
      test::ContextBuilder().SetCompilationPackage("lib").SetPackageId(0x00).Build();
  std::unique_ptr<ResourceTable> table =
      test::ResourceTableBuilder()
          .SetPackageId("lib", 0x00)
          .AddValue("lib:id/foo", ResourceId(0x00010000), util::make_unique<Id>())
          .Build();
  ResourceTable result;
  ASSERT_TRUE(Flatten(context.get(), {}, table.get(), &result));

  Maybe<ResourceTable::SearchResult> search_result =
      result.FindResource(test::ParseNameOrDie("lib:id/foo"));
  ASSERT_TRUE(search_result);
  EXPECT_EQ(0x00u, search_result.value().package->id.value());

  auto iter = result.included_packages_.find(0x00);
  ASSERT_NE(result.included_packages_.end(), iter);
  EXPECT_EQ("lib", iter->second);
}

TEST_F(TableFlattenerTest, FlattenTableReferencingSharedLibraries) {
  std::unique_ptr<IAaptContext> context =
      test::ContextBuilder().SetCompilationPackage("app").SetPackageId(0x7f).Build();
  std::unique_ptr<ResourceTable> table =
      test::ResourceTableBuilder()
          .SetPackageId("app", 0x7f)
          .AddValue("app:id/foo", ResourceId(0x7f010000),
                    test::BuildReference("lib_one:id/foo", ResourceId(0x02010000)))
          .AddValue("app:id/bar", ResourceId(0x7f010001),
                    test::BuildReference("lib_two:id/bar", ResourceId(0x03010000)))
          .Build();
  table->included_packages_[0x02] = "lib_one";
  table->included_packages_[0x03] = "lib_two";

  ResTable result;
  ASSERT_TRUE(Flatten(context.get(), {}, table.get(), &result));

  const DynamicRefTable* dynamic_ref_table = result.getDynamicRefTableForCookie(1);
  ASSERT_THAT(dynamic_ref_table, NotNull());

  const KeyedVector<String16, uint8_t>& entries = dynamic_ref_table->entries();

  ssize_t idx = entries.indexOfKey(android::String16("lib_one"));
  ASSERT_GE(idx, 0);
  EXPECT_EQ(0x02u, entries.valueAt(idx));

  idx = entries.indexOfKey(android::String16("lib_two"));
  ASSERT_GE(idx, 0);
  EXPECT_EQ(0x03u, entries.valueAt(idx));
}

TEST_F(TableFlattenerTest, PackageWithNonStandardIdHasDynamicRefTable) {
  std::unique_ptr<IAaptContext> context =
      test::ContextBuilder().SetCompilationPackage("app").SetPackageId(0x80).Build();
  std::unique_ptr<ResourceTable> table = test::ResourceTableBuilder()
                                             .SetPackageId("app", 0x80)
                                             .AddSimple("app:id/foo", ResourceId(0x80010000))
                                             .Build();

  ResTable result;
  ASSERT_TRUE(Flatten(context.get(), {}, table.get(), &result));

  const DynamicRefTable* dynamic_ref_table = result.getDynamicRefTableForCookie(1);
  ASSERT_THAT(dynamic_ref_table, NotNull());

  const KeyedVector<String16, uint8_t>& entries = dynamic_ref_table->entries();
  ssize_t idx = entries.indexOfKey(android::String16("app"));
  ASSERT_GE(idx, 0);
  EXPECT_EQ(0x80u, entries.valueAt(idx));
}

TEST_F(TableFlattenerTest, LongPackageNameIsTruncated) {
  std::string kPackageName(256, 'F');

  std::unique_ptr<IAaptContext> context =
      test::ContextBuilder().SetCompilationPackage(kPackageName).SetPackageId(0x7f).Build();
  std::unique_ptr<ResourceTable> table =
      test::ResourceTableBuilder()
          .SetPackageId(kPackageName, 0x7f)
          .AddSimple(kPackageName + ":id/foo", ResourceId(0x7f010000))
          .Build();

  ResTable result;
  ASSERT_TRUE(Flatten(context.get(), {}, table.get(), &result));

  ASSERT_EQ(1u, result.getBasePackageCount());
  EXPECT_EQ(127u, result.getBasePackageName(0).size());
}

TEST_F(TableFlattenerTest, LongSharedLibraryPackageNameIsIllegal) {
  std::string kPackageName(256, 'F');

  std::unique_ptr<IAaptContext> context = test::ContextBuilder()
                                              .SetCompilationPackage(kPackageName)
                                              .SetPackageId(0x7f)
                                              .SetPackageType(PackageType::kSharedLib)
                                              .Build();
  std::unique_ptr<ResourceTable> table =
      test::ResourceTableBuilder()
          .SetPackageId(kPackageName, 0x7f)
          .AddSimple(kPackageName + ":id/foo", ResourceId(0x7f010000))
          .Build();

  ResTable result;
  ASSERT_FALSE(Flatten(context.get(), {}, table.get(), &result));
}

TEST_F(TableFlattenerTest, ObfuscatingResourceNamesNoWhitelistSucceeds) {
  std::unique_ptr<ResourceTable> table =
      test::ResourceTableBuilder()
          .SetPackageId("com.app.test", 0x7f)
          .AddSimple("com.app.test:id/one", ResourceId(0x7f020000))
          .AddSimple("com.app.test:id/two", ResourceId(0x7f020001))
          .AddValue("com.app.test:id/three", ResourceId(0x7f020002),
                    test::BuildReference("com.app.test:id/one", ResourceId(0x7f020000)))
          .AddValue("com.app.test:integer/one", ResourceId(0x7f030000),
                    util::make_unique<BinaryPrimitive>(uint8_t(Res_value::TYPE_INT_DEC), 1u))
          .AddValue("com.app.test:integer/one", test::ParseConfigOrDie("v1"),
                    ResourceId(0x7f030000),
                    util::make_unique<BinaryPrimitive>(uint8_t(Res_value::TYPE_INT_DEC), 2u))
          .AddString("com.app.test:string/test", ResourceId(0x7f040000), "foo")
          .AddString("com.app.test:layout/bar", ResourceId(0x7f050000), "res/layout/bar.xml")
          .Build();

  TableFlattenerOptions options;
  options.collapse_key_stringpool = true;

  ResTable res_table;

  ASSERT_TRUE(Flatten(context_.get(), options, table.get(), &res_table));

  EXPECT_TRUE(Exists(&res_table, "com.app.test:id/0_resource_name_obfuscated",
                     ResourceId(0x7f020000), {}, Res_value::TYPE_INT_BOOLEAN, 0u, 0u));

  EXPECT_TRUE(Exists(&res_table, "com.app.test:id/0_resource_name_obfuscated",
                     ResourceId(0x7f020001), {}, Res_value::TYPE_INT_BOOLEAN, 0u, 0u));

  EXPECT_TRUE(Exists(&res_table, "com.app.test:id/0_resource_name_obfuscated",
                     ResourceId(0x7f020002), {}, Res_value::TYPE_REFERENCE, 0x7f020000u, 0u));

  EXPECT_TRUE(Exists(&res_table, "com.app.test:integer/0_resource_name_obfuscated",
                     ResourceId(0x7f030000), {}, Res_value::TYPE_INT_DEC, 1u,
                     ResTable_config::CONFIG_VERSION));

  EXPECT_TRUE(Exists(&res_table, "com.app.test:integer/0_resource_name_obfuscated",
                     ResourceId(0x7f030000), test::ParseConfigOrDie("v1"), Res_value::TYPE_INT_DEC,
                     2u, ResTable_config::CONFIG_VERSION));

  std::u16string foo_str = u"foo";
  ssize_t idx = res_table.getTableStringBlock(0)->indexOfString(foo_str.data(), foo_str.size());
  ASSERT_GE(idx, 0);
  EXPECT_TRUE(Exists(&res_table, "com.app.test:string/0_resource_name_obfuscated",
                     ResourceId(0x7f040000), {}, Res_value::TYPE_STRING, (uint32_t)idx, 0u));

  std::u16string bar_path = u"res/layout/bar.xml";
  idx = res_table.getTableStringBlock(0)->indexOfString(bar_path.data(), bar_path.size());
  ASSERT_GE(idx, 0);
  EXPECT_TRUE(Exists(&res_table, "com.app.test:layout/0_resource_name_obfuscated",
                     ResourceId(0x7f050000), {}, Res_value::TYPE_STRING, (uint32_t)idx, 0u));
}

TEST_F(TableFlattenerTest, ObfuscatingResourceNamesWithWhitelistSucceeds) {
  std::unique_ptr<ResourceTable> table =
      test::ResourceTableBuilder()
          .SetPackageId("com.app.test", 0x7f)
          .AddSimple("com.app.test:id/one", ResourceId(0x7f020000))
          .AddSimple("com.app.test:id/two", ResourceId(0x7f020001))
          .AddValue("com.app.test:id/three", ResourceId(0x7f020002),
                    test::BuildReference("com.app.test:id/one", ResourceId(0x7f020000)))
          .AddValue("com.app.test:integer/one", ResourceId(0x7f030000),
                    util::make_unique<BinaryPrimitive>(uint8_t(Res_value::TYPE_INT_DEC), 1u))
          .AddValue("com.app.test:integer/one", test::ParseConfigOrDie("v1"),
                    ResourceId(0x7f030000),
                    util::make_unique<BinaryPrimitive>(uint8_t(Res_value::TYPE_INT_DEC), 2u))
          .AddString("com.app.test:string/test", ResourceId(0x7f040000), "foo")
          .AddString("com.app.test:layout/bar", ResourceId(0x7f050000), "res/layout/bar.xml")
          .Build();

  TableFlattenerOptions options;
  options.collapse_key_stringpool = true;
  options.whitelisted_resources.insert("test");
  options.whitelisted_resources.insert("three");
  ResTable res_table;

  ASSERT_TRUE(Flatten(context_.get(), options, table.get(), &res_table));

  EXPECT_TRUE(Exists(&res_table, "com.app.test:id/0_resource_name_obfuscated",
                     ResourceId(0x7f020000), {}, Res_value::TYPE_INT_BOOLEAN, 0u, 0u));

  EXPECT_TRUE(Exists(&res_table, "com.app.test:id/0_resource_name_obfuscated",
                     ResourceId(0x7f020001), {}, Res_value::TYPE_INT_BOOLEAN, 0u, 0u));

  EXPECT_TRUE(Exists(&res_table, "com.app.test:id/three", ResourceId(0x7f020002), {},
                     Res_value::TYPE_REFERENCE, 0x7f020000u, 0u));

  EXPECT_TRUE(Exists(&res_table, "com.app.test:integer/0_resource_name_obfuscated",
                     ResourceId(0x7f030000), {}, Res_value::TYPE_INT_DEC, 1u,
                     ResTable_config::CONFIG_VERSION));

  EXPECT_TRUE(Exists(&res_table, "com.app.test:integer/0_resource_name_obfuscated",
                     ResourceId(0x7f030000), test::ParseConfigOrDie("v1"), Res_value::TYPE_INT_DEC,
                     2u, ResTable_config::CONFIG_VERSION));

  std::u16string foo_str = u"foo";
  ssize_t idx = res_table.getTableStringBlock(0)->indexOfString(foo_str.data(), foo_str.size());
  ASSERT_GE(idx, 0);
  EXPECT_TRUE(Exists(&res_table, "com.app.test:string/test", ResourceId(0x7f040000), {},
                     Res_value::TYPE_STRING, (uint32_t)idx, 0u));

  std::u16string bar_path = u"res/layout/bar.xml";
  idx = res_table.getTableStringBlock(0)->indexOfString(bar_path.data(), bar_path.size());
  ASSERT_GE(idx, 0);
  EXPECT_TRUE(Exists(&res_table, "com.app.test:layout/0_resource_name_obfuscated",
                     ResourceId(0x7f050000), {}, Res_value::TYPE_STRING, (uint32_t)idx, 0u));
}

TEST_F(TableFlattenerTest, FlattenOverlayable) {
  OverlayableItem overlayable_item(std::make_shared<Overlayable>("TestName", "overlay://theme"));
  overlayable_item.policies |= OverlayableItem::Policy::kProduct;
  overlayable_item.policies |= OverlayableItem::Policy::kSystem;
  overlayable_item.policies |= OverlayableItem::Policy::kVendor;

  std::string name = "com.app.test:integer/overlayable";
  std::unique_ptr<ResourceTable> table =
      test::ResourceTableBuilder()
          .SetPackageId("com.app.test", 0x7f)
          .AddSimple(name, ResourceId(0x7f020000))
          .SetOverlayable(name, overlayable_item)
          .Build();

  ResourceTable output_table;
  ASSERT_TRUE(Flatten(context_.get(), {}, table.get(), &output_table));

  auto search_result = output_table.FindResource(test::ParseNameOrDie(name));
  ASSERT_TRUE(search_result);
  ASSERT_THAT(search_result.value().entry, NotNull());
  ASSERT_TRUE(search_result.value().entry->overlayable_item);
  OverlayableItem& result_overlayable_item = search_result.value().entry->overlayable_item.value();
  EXPECT_EQ(result_overlayable_item.policies, OverlayableItem::Policy::kSystem
                                         | OverlayableItem::Policy::kVendor
                                         | OverlayableItem::Policy::kProduct);
}

TEST_F(TableFlattenerTest, FlattenMultipleOverlayablePolicies) {
  auto overlayable = std::make_shared<Overlayable>("TestName", "overlay://theme");
  std::string name_zero = "com.app.test:integer/overlayable_zero_item";
  OverlayableItem overlayable_item_zero(overlayable);
  overlayable_item_zero.policies |= OverlayableItem::Policy::kProduct;
  overlayable_item_zero.policies |= OverlayableItem::Policy::kSystem;

  std::string name_one = "com.app.test:integer/overlayable_one_item";
  OverlayableItem overlayable_item_one(overlayable);
  overlayable_item_one.policies |= OverlayableItem::Policy::kPublic;

  std::string name_two = "com.app.test:integer/overlayable_two_item";
  OverlayableItem overlayable_item_two(overlayable);
  overlayable_item_two.policies |= OverlayableItem::Policy::kProduct;
  overlayable_item_two.policies |= OverlayableItem::Policy::kSystem;
  overlayable_item_two.policies |= OverlayableItem::Policy::kVendor;

  std::unique_ptr<ResourceTable> table =
      test::ResourceTableBuilder()
          .SetPackageId("com.app.test", 0x7f)
          .AddSimple(name_zero, ResourceId(0x7f020000))
          .SetOverlayable(name_zero, overlayable_item_zero)
          .AddSimple(name_one, ResourceId(0x7f020001))
          .SetOverlayable(name_one, overlayable_item_one)
          .AddSimple(name_two, ResourceId(0x7f020002))
          .SetOverlayable(name_two, overlayable_item_two)
          .Build();

  ResourceTable output_table;
  ASSERT_TRUE(Flatten(context_.get(), {}, table.get(), &output_table));

  auto search_result = output_table.FindResource(test::ParseNameOrDie(name_zero));
  ASSERT_TRUE(search_result);
  ASSERT_THAT(search_result.value().entry, NotNull());
  ASSERT_TRUE(search_result.value().entry->overlayable_item);
  OverlayableItem& overlayable_item = search_result.value().entry->overlayable_item.value();
  EXPECT_EQ(overlayable_item.policies, OverlayableItem::Policy::kSystem
                                       | OverlayableItem::Policy::kProduct);

  search_result = output_table.FindResource(test::ParseNameOrDie(name_one));
  ASSERT_TRUE(search_result);
  ASSERT_THAT(search_result.value().entry, NotNull());
  ASSERT_TRUE(search_result.value().entry->overlayable_item);
  overlayable_item = search_result.value().entry->overlayable_item.value();
  EXPECT_EQ(overlayable_item.policies, OverlayableItem::Policy::kPublic);

  search_result = output_table.FindResource(test::ParseNameOrDie(name_two));
  ASSERT_TRUE(search_result);
  ASSERT_THAT(search_result.value().entry, NotNull());
  ASSERT_TRUE(search_result.value().entry->overlayable_item);
  overlayable_item = search_result.value().entry->overlayable_item.value();
  EXPECT_EQ(overlayable_item.policies, OverlayableItem::Policy::kSystem
                                       | OverlayableItem::Policy::kProduct
                                       | OverlayableItem::Policy::kVendor);
}

TEST_F(TableFlattenerTest, FlattenMultipleOverlayable) {
  auto group = std::make_shared<Overlayable>("TestName", "overlay://theme");
  std::string name_zero = "com.app.test:integer/overlayable_zero";
  OverlayableItem overlayable_item_zero(group);
  overlayable_item_zero.policies |= OverlayableItem::Policy::kProduct;
  overlayable_item_zero.policies |= OverlayableItem::Policy::kSystem;

  auto group_one = std::make_shared<Overlayable>("OtherName", "overlay://customization");
  std::string name_one = "com.app.test:integer/overlayable_one";
  OverlayableItem overlayable_item_one(group_one);
  overlayable_item_one.policies |= OverlayableItem::Policy::kPublic;

  std::string name_two = "com.app.test:integer/overlayable_two";
  OverlayableItem overlayable_item_two(group);
  overlayable_item_two.policies |= OverlayableItem::Policy::kOdm;
  overlayable_item_two.policies |= OverlayableItem::Policy::kOem;
  overlayable_item_two.policies |= OverlayableItem::Policy::kVendor;

  std::string name_three = "com.app.test:integer/overlayable_three";
  OverlayableItem overlayable_item_three(group_one);
  overlayable_item_three.policies |= OverlayableItem::Policy::kSignature;

  std::unique_ptr<ResourceTable> table =
      test::ResourceTableBuilder()
          .SetPackageId("com.app.test", 0x7f)
          .AddSimple(name_zero, ResourceId(0x7f020000))
          .SetOverlayable(name_zero, overlayable_item_zero)
          .AddSimple(name_one, ResourceId(0x7f020001))
          .SetOverlayable(name_one, overlayable_item_one)
          .AddSimple(name_two, ResourceId(0x7f020002))
          .SetOverlayable(name_two, overlayable_item_two)
          .AddSimple(name_three, ResourceId(0x7f020003))
          .SetOverlayable(name_three, overlayable_item_three)
          .Build();

  ResourceTable output_table;
  ASSERT_TRUE(Flatten(context_.get(), {}, table.get(), &output_table));
  auto search_result = output_table.FindResource(test::ParseNameOrDie(name_zero));
  ASSERT_TRUE(search_result);
  ASSERT_THAT(search_result.value().entry, NotNull());
  ASSERT_TRUE(search_result.value().entry->overlayable_item);
  OverlayableItem& result_overlayable = search_result.value().entry->overlayable_item.value();
  EXPECT_EQ(result_overlayable.overlayable->name, "TestName");
  EXPECT_EQ(result_overlayable.overlayable->actor, "overlay://theme");
  EXPECT_EQ(result_overlayable.policies, OverlayableItem::Policy::kSystem
                                         | OverlayableItem::Policy::kProduct);

  search_result = output_table.FindResource(test::ParseNameOrDie(name_one));
  ASSERT_TRUE(search_result);
  ASSERT_THAT(search_result.value().entry, NotNull());
  ASSERT_TRUE(search_result.value().entry->overlayable_item);
  result_overlayable = search_result.value().entry->overlayable_item.value();
  EXPECT_EQ(result_overlayable.overlayable->name, "OtherName");
  EXPECT_EQ(result_overlayable.overlayable->actor, "overlay://customization");
  EXPECT_EQ(result_overlayable.policies, OverlayableItem::Policy::kPublic);

  search_result = output_table.FindResource(test::ParseNameOrDie(name_two));
  ASSERT_TRUE(search_result);
  ASSERT_THAT(search_result.value().entry, NotNull());
  ASSERT_TRUE(search_result.value().entry->overlayable_item);
  result_overlayable = search_result.value().entry->overlayable_item.value();
  EXPECT_EQ(result_overlayable.overlayable->name, "TestName");
  EXPECT_EQ(result_overlayable.overlayable->actor, "overlay://theme");
  EXPECT_EQ(result_overlayable.policies, OverlayableItem::Policy::kOdm
                                         | OverlayableItem::Policy::kOem
                                         | OverlayableItem::Policy::kVendor);

  search_result = output_table.FindResource(test::ParseNameOrDie(name_three));
  ASSERT_TRUE(search_result);
  ASSERT_THAT(search_result.value().entry, NotNull());
  ASSERT_TRUE(search_result.value().entry->overlayable_item);
  result_overlayable = search_result.value().entry->overlayable_item.value();
  EXPECT_EQ(result_overlayable.overlayable->name, "OtherName");
  EXPECT_EQ(result_overlayable.overlayable->actor, "overlay://customization");
  EXPECT_EQ(result_overlayable.policies, OverlayableItem::Policy::kSignature);
}

TEST_F(TableFlattenerTest, FlattenOverlayableNoPolicyFails) {
  auto group = std::make_shared<Overlayable>("TestName", "overlay://theme");
  std::string name_zero = "com.app.test:integer/overlayable_zero";
  OverlayableItem overlayable_item_zero(group);

  std::unique_ptr<ResourceTable> table =
      test::ResourceTableBuilder()
          .SetPackageId("com.app.test", 0x7f)
          .AddSimple(name_zero, ResourceId(0x7f020000))
          .SetOverlayable(name_zero, overlayable_item_zero)
          .Build();
  ResourceTable output_table;
  ASSERT_FALSE(Flatten(context_.get(), {}, table.get(), &output_table));
}

}  // namespace aapt
