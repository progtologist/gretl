#include <gtest/gtest.h>
#include <gretl/lib/libgretl.h>

const char * filename = "dataset.gdt";

TEST(LibGretl, LoadDataset) {
    libgretl_init();
    DATASET *dset = NULL;
    PRN *prn = NULL;
    prn = gretl_print_new(GRETL_PRINT_STDOUT, NULL);
    dset = datainfo_new();

    int error = gretl_read_native_data(filename, dset);
    EXPECT_EQ( 0, error);
    EXPECT_EQ(204, dset->n);
    EXPECT_EQ(13 , dset->v);
    EXPECT_EQ(4 ,dset->pd);

    destroy_dataset(dset);
    gretl_print_destroy(prn);
    libgretl_cleanup();
}

TEST(LibGretl, Estimation) {
    libgretl_init();
    MODEL model;
    DATASET *dset = NULL;
    PRN *prn = NULL;
    int *list = NULL;
    prn = gretl_print_new(GRETL_PRINT_STDOUT, NULL);
    dset = datainfo_new();

    int error = gretl_read_native_data(filename, dset);
    EXPECT_EQ( 0, error);
    list = gretl_list_new(dset->v);
    list[0] = dset->v;
    list[1] = 1;
    list[2] = 0;
    for (int i=3; i<=list[0]; i++) {
        list[i] = i - 1;
    }
    model = lsq(list, dset, OLS, OPT_NONE);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_A);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_B);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_C);
    EXPECT_EQ(18, model.errcode);
    model = lsq(list, dset, OLS, OPT_D);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_E);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_F);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_G);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_H);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_I);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_J);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_K);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_L);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_M);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_N);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_O);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_P);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_Q);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_R);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_S);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_T);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_U);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_V);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_W);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_X);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_Z);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_Y);
    EXPECT_EQ(0, model.errcode);
    model = lsq(list, dset, OLS, OPT_UNSET);
    EXPECT_EQ(0, model.errcode);

    clear_model(&model);
    destroy_dataset(dset);
    gretl_print_destroy(prn);
    libgretl_cleanup();
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
