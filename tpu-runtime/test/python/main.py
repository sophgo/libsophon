from utils import *
from test_case import *
import sys
import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Script description")
    parser.add_argument("--use_soc", action="store_true", help="Choose CModel or SOC (default: CModel)")
    parser.add_argument("--root_path", type=str, default="./", help="Root path (default: ./)")
    parser.add_argument("--archs", type=str, help="Test architecture, exp.'bm1684,bm1688...'")

    args = parser.parse_args()
    use_cmodel = not args.use_soc
    root_path = args.root_path
    test_archs = args.archs.split(",")
    if use_cmodel:
        root_path = bmrt_get_env("BMRT_TEST_TOP")
        gloabl_def.G_BMRT_APP = root_path + "/../build/bmrt_test"
    else:
        raise Exception("not implemented")
    ret = run_cmd(gloabl_def.G_BMRT_APP + " --help > /dev/null", 255)
    if not ret:
        raise Exception("check bmrt test failed\n")

    bmodels = defaultdict(defaultdict)
    regression_model_path = root_path + "/../bmodel-zoo/regression_models/"
    run_types = get_run_type_forlder(regression_model_path)
    for run_type in run_types:
        bmodels[run_type] = bmrt_get_bmodel_list(regression_model_path + run_type)

    for test_arch in test_archs:
        #basic bmrt_test (static, dynamic, multi_subnet)
        basic_test_func(bmodels, test_arch, 1)

        # memory prealloc test
        test_memeory_prealloc_func(bmodels, test_arch, 1)

        #bmrt api test "bmrt_load_bmodel_data","bmmc_multi_mession", ...
        test_api_func(bmodels, test_arch, 1)

        #multi_core_multi_mession test
        if test_arch == "bm1688":
            #multi_core_multi_mession test
            test_multi_core_mession_func(bmodels, test_arch, 2)

    ret = 0
    if gloabl_def.G_FAILED_CASES.keys():
        bmrt_out_faild_log(gloabl_def.G_FAILED_CASES)
        ret = -1
    sys.exit(ret)
