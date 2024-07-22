from utils import *

def basic_test_func(bmodels : defaultdict, runtime_arch : str, loop_num : int):
    case = TestBase(["bmrt_test"], loop_num)
    case.set_arch_name(runtime_arch)

    for run_type in bmodels.keys():
        bmodel_list = [info.full_path for info in bmodels[run_type][runtime_arch]]
        if not bmodel_list: continue
        case.set_bmodel_list(bmodel_list)
        case.run_single_mession()

def test_memeory_prealloc_func(bmodels : defaultdict, runtime_arch : str, loop_num : int):
    case = TestBase(["bmrt_test"], loop_num)
    case.set_arch_name(runtime_arch)

    for run_type in bmodels.keys():
        bmodel_list = [info.full_path for info in bmodels[run_type][runtime_arch]]
        if not bmodel_list: continue
        case.set_bmodel_list(bmodel_list)
        case.memory_prealloc = True
        case.run_single_mession()

def test_api_func(bmodels : defaultdict, runtime_arch : str, loop_num : int):
    common_cases = ["bmrt_load_bmodel_data",
                    "bmrt_load_context",
                    "bmrt_launch_data",
                    "bmrt_simple_api",
                    "bmrt_multi_thread",
                    "bmcpp_load_bmodel",
                    "bmcpp_load_bmodel_data",
                    "bmcpp_reshape",
                    "bmcpp_multi_thread",
                    "bmtap2_register_bmodel",
                    "bmtap2_register_data",
                    "bmtap2_multi_thread",
                    "bmtap2cpp_load_bmodel",
                    "bmtap2cpp_multi_thread"]
    case = TestBase(common_cases, loop_num)
    case.set_arch_name(runtime_arch)
    bmodel_list= []
    for info in bmodels["static"][runtime_arch]:
        if info.core_num == "1": bmodel_list.append(info.full_path)
    case.set_bmodel_list(bmodel_list)
    case.run_single_mession()

    if runtime_arch == "bm1688" or runtime_arch == "bm1684x":
        test_cases = ["bmrt_get_bmodel_api", "bmrt_get_bmodel_api_c"]
        case.set_case_names(test_cases)
        case.run_single_mession()

    if runtime_arch == "bm1688":
        case.set_case_names(["bmmc_multi_mession"])
        case.run_multi_mession()

def test_multi_core_mession_func(bmodels : defaultdict, runtime_arch : str, loop_num : int):
    assert runtime_arch == "bm1688"
    test_case = TestBM1688(["bmmc_multi_mession"], loop_num)
    bmodel_list = []
    for run_type in bmodels.keys():
        for info in bmodels[run_type][runtime_arch]:
            if info.core_num == "1":
                bmodel_list.append(info.full_path)
    test_case.set_bmodel_list(bmodel_list)
    test_case.run_multi_mession()

    test_case.set_case_names(["bmrt_test"])
    test_case.run_multi_mession()

