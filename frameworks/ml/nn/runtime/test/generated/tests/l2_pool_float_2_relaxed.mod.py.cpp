// clang-format off
// Generated file (from: l2_pool_float_2_relaxed.mod.py). Do not edit
#include "../../TestGenerated.h"

namespace l2_pool_float_2_relaxed {
// Generated l2_pool_float_2_relaxed test
#include "generated/examples/l2_pool_float_2_relaxed.example.cpp"
// Generated model constructor
#include "generated/models/l2_pool_float_2_relaxed.model.cpp"
} // namespace l2_pool_float_2_relaxed

TEST_F(GeneratedTests, l2_pool_float_2_relaxed) {
    execute(l2_pool_float_2_relaxed::CreateModel,
            l2_pool_float_2_relaxed::is_ignored,
            l2_pool_float_2_relaxed::get_examples());
}

TEST_F(DynamicOutputShapeTest, l2_pool_float_2_relaxed_dynamic_output_shape) {
    execute(l2_pool_float_2_relaxed::CreateModel_dynamic_output_shape,
            l2_pool_float_2_relaxed::is_ignored_dynamic_output_shape,
            l2_pool_float_2_relaxed::get_examples_dynamic_output_shape());
}

