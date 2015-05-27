#include <gtest/gtest.h>
#include <gretl/lib/libgretl.h>

TEST(LibGretl, GretlMatrix) {
    gretl_matrix* mat = gretl_identity_matrix_new(2);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
