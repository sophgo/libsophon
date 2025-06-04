from collections import defaultdict
class GlobalDefine:
    def __init__(self) -> None:
        self.CONST_CORE_NUM_DICT = {"bm1684" : 1, "bm1684x" : 1, "bm1688" : 2, "bm1690": 8}
        self.CONST_CASE_NAME = [ "bmrt_test",
                                "bmrt_load_bmodel_data",
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
                                "bmtap2cpp_multi_thread",
                                "bmrt_get_bmodel_api",
                                "bmrt_get_bmodel_api_c",
                                "bmmc_multi_mession"]
        self.G_BMRT_APP = "bmrt_test"
        self.DEF_ERROR = -1
        self.DEF_TIMEOUT = -2
        self.DEF_CMODEL = 0
        self.DEF_SOC = 1
        self.G_FAILED_CASES = defaultdict(str)

gloabl_def = GlobalDefine()