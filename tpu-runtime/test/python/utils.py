import os
import random
import subprocess
import threading
from collections import defaultdict
from global_def import gloabl_def

def run_cmd(cmd, right_ret = 0):
    ret = os.system(cmd)
    ret >>= 8
    if ret != right_ret: return False
    else: return True

class TestBase:
  def __init__(self, case_names, loop_num):
      self.case_names = case_names
      for case_name in case_names:
          if case_name not in gloabl_def.CONST_CASE_NAME:
              raise ValueError("unknown case name: " + case_name)
      self.arch_name = None
      self.bmodel_list = None
      self.bmodel_list_file = "bmodel_list_temp.txt"
      self.loop_num = loop_num
      self.bmrt_app = gloabl_def.G_BMRT_APP
      self.cmd = None
      self.seed = random.randint(0, 10000)
      self.runtime_mode = gloabl_def.DEF_CMODEL #todo, extend for soc
      self.memory_prealloc = False

  def set_arch_name(self, arch_name):
      if arch_name not in gloabl_def.CONST_CORE_NUM_DICT.keys():
          raise ValueError("unknown arch name: " + arch_name)
      self.arch_name = arch_name

  def set_case_names(self, case_names):
        for case_name in case_names:
            if case_name not in gloabl_def.CONST_CASE_NAME:
                raise ValueError("unknown case name: " + case_name)
        self.case_names = case_names

  def set_bmodel_list(self, bmodel_list):
      self.bmodel_list = bmodel_list

  def set_bmodel_list_file(self, bmodel_list_file):
      self.bmodel_list_file = bmodel_list_file

  def set_loop_num(self, loop_num):
    self.loop_num = loop_num

  def run_cmd_and_record(self, cmd, case_name, right_ret = 0):
    stdout_ = None
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)
    time_out = False
    try:
        stdout_, stderr = process.communicate(timeout=1200)
    except subprocess.TimeoutExpired:
        time_out = True
    if stdout_: print(stdout_.decode())
    ret = process.returncode
    if ret != right_ret:
        issue_log = stdout_.decode() if stdout_ else None
        log_info = LogInfo()
        log_info.log = issue_log
        log_info.bmodel_list = self.bmodel_list
        log_info.run_cmd = cmd
        log_info.type = gloabl_def.DEF_TIMEOUT if time_out else gloabl_def.DEF_ERROR
        gloabl_def.G_FAILED_CASES[self.arch_name + "_" +  case_name] = log_info

  def gen_run_cmd(self, case_name):
      self.cmd = self.bmrt_app + " --loopnum " + str(self.loop_num)
      if case_name != "bmrt_test":
          self.cmd += " -t " + case_name
      if len(self.bmodel_list) == 1:
          self.cmd += " --context " + self.bmodel_list[0]
          if self.memory_prealloc:
            self.cmd += " --memory_prealloc "
      else:
          random.seed(self.seed)
          random.shuffle(self.bmodel_list)
          with open(self.bmodel_list_file,'w') as file:
              for bmodel in self.bmodel_list:
                  file.writelines(bmodel + "\n")
          self.cmd += " --bmodel_list " + self.bmodel_list_file
      return self.cmd

  def run_single_mession(self):
      if self.runtime_mode == gloabl_def.DEF_CMODEL:
          bmrt_set_libcmodel(self.arch_name)
      for case_name in self.case_names:
          self.run_cmd_and_record(self.gen_run_cmd(case_name), case_name)

  def run_multi_mession(self):
      pass

  def __del__(self):
        if os.path.exists(self.bmodel_list_file):
            os.system("rm " + self.bmodel_list_file)
        if os.getenv("TPUKERNEL_FIRMWARE_PATH") != None:
            os.environ.pop("TPUKERNEL_FIRMWARE_PATH")

class TestBM1684(TestBase):
    def __init__(self, case_names, loop_num):
        super(TestBM1684, self).__init__(case_names, loop_num)
        self.set_arch_name("bm1684")

class TestBM1684X(TestBase):
    def __init__(self, case_names, loop_num):
        super(TestBM1684X, self).__init__(case_names, loop_num)
        self.set_arch_name("bm1684x")

class TestBM1688(TestBase):
    def __init__(self, case_names, loop_num):
        super(TestBM1688, self).__init__(case_names, loop_num)
        self.set_arch_name("bm1688")

    def run_multi_mession(self):
        if self.runtime_mode == gloabl_def.DEF_CMODEL:
            bmrt_set_libcmodel(self.arch_name)
        for case_name in self.case_names:
            cmd_ = self.gen_run_cmd(case_name)
            cmd_ += " --core_list 0:1"
            self.run_cmd_and_record(cmd_, case_name)

class TestBM1690(TestBase):
    def __init__(self, case_names, loop_num):
        super(TestBM1690, self).__init__(case_names, loop_num)
        self.set_arch_name("bm1690")

class BmodelInfo:
    def __init__(self):
        self.model_name = None
        self.dtype = None
        self.full_path = None
        self.core_num = None
        self.arch_name = None

    def __repr__(self) -> str:
        bmodel_info = ",".join([self.model_name, self.arch_name, self.full_path])
        return "[ " + bmodel_info + "]"

class LogInfo:
    def __init__(self):
        self.log = None
        self.bmodel_list = None
        self.run_cmd = None
        self.type = None

def get_bmodel_list(path):
    result_folders = []
    for root, dirs, files in os.walk(path):
        if "compilation.bmodel" in files:
            result_folders.append(root)
    return result_folders

def get_bmodel_list_info(bmodel_list):
    res = defaultdict(list)
    for i in range(len(bmodel_list)):
        bmodel_path = bmodel_list[i]
        bmodel_name = bmodel_path.split("/")[-1]
        name_li = bmodel_name.split("_")
        bmodel_info = BmodelInfo()
        if 'core' in name_li[-1]:
            bmodel_info.core_num = name_li[-1]
            bmodel_info.dtype = name_li[-2]
            bmodel_info.arch_name = name_li[-3]
            bmodel_info.model_name = ('_').join(name_li[:-3])
        else:
            bmodel_info.core_num = "1"
            bmodel_info.dtype = name_li[-1]
            bmodel_info.arch_name = name_li[-2]
            bmodel_info.model_name = ('_').join(name_li[:-2])
        bmodel_info.full_path = bmodel_path
        res[bmodel_info.arch_name].append(bmodel_info)
    return res

def get_run_type_forlder(path):
    res = []
    entries = os.listdir(path)
    for entry in entries:
        full_path = os.path.join(path, entry)
        if os.path.isdir(full_path): res.append(entry)
    return res

def bmrt_get_env(env_name):
    env = os.getenv(env_name)
    if not env:
        raise Exception("get env faild, name: " + env_name)
    return env

def bmrt_get_bmodel_list(path):
    bmodel_paths = get_bmodel_list(path)
    return get_bmodel_list_info(bmodel_paths)

def bmrt_set_libcmodel(arch_name):
    root_path = bmrt_get_env("BMRT_TEST_TOP")
    libcmodel_path = root_path + "/lib/libcmodel_" + arch_name + ".so"
    os.environ["TPUKERNEL_FIRMWARE_PATH"]=libcmodel_path

def bmrt_out_faild_log(failed_infos : defaultdict):
    if not failed_infos.keys(): return
    type_to_str = { gloabl_def.DEF_TIMEOUT : "Time Out",
                    gloabl_def.DEF_ERROR : "Run Error" }
    case_names = list(failed_infos.keys())
    for i in range(len(case_names)):
        log_ = failed_infos[case_names[i]].log
        if log_: print(log_)
    print("follow cases failed: \n")
    for i in range(len(case_names)):
        if "bmodel_list" in failed_infos[case_names[i]].run_cmd:
            print("[ {}, case name: {}, run cmd: {}\n bmodel list:\n{} ]\n".format(type_to_str[failed_infos[case_names[i]].type], case_names[i],  \
                                                                                failed_infos[case_names[i]].run_cmd, '\n'.join(failed_infos[case_names[i]].bmodel_list)))
        else:
             print("[ {}, case name: {}, run cmd: {} ]\n".format(type_to_str[failed_infos[case_names[i]].type], case_names[i], failed_infos[case_names[i]].run_cmd))

