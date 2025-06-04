  #ifdef USING_CMODEL
  #include "bmlib_internal.h"
  #include "bmlib_memory.h"
  #include "bmlib_device.h"
  #include "api.h"
  #include <unistd.h>

  #define BM_API_QUIT                       0xffffffff
  #define BMLIB_DEVICE_TAG "bmlib_device"

  INLINE static int pointer_wrap_around(u32 cur_pointer, int step, int len_bit_width) {
  u32 max_len     = (1 << len_bit_width);
  u32 new_pointer = 0;

  new_pointer = cur_pointer + step;
  if (new_pointer >= max_len) new_pointer -= max_len;

  return (int)new_pointer;
  }

  unsigned long long get_global_mem_size()
  {
    auto env = std::getenv("CMODEL_GLOBAL_MEM_SIZE");
    if (env)
    {
        unsigned long long v;
        try {
            v = std::stoll(env);
        } catch (std::invalid_argument &) {
            printf("invalid CMODEL_GLOBAL_MEM_SIZE \"%s\"\n", env);
            throw;
        }
        printf("global mem size from env %lld\n", v);
        return v;
    } else {
        return 0x100000000;
    }
  }
  void *cmodel_so_handle_ = NULL;
  void bm_device::cmodel_setup(void)
  {
  get_global_memaddr_    =  (t_get_global_memaddr)dlsym(NULL,    "get_global_memaddr");
  cmodel_init_  =  (t_cmodel_init)dlsym(NULL,  "cmodel_init");
  set_cur_nodechip_idx_ =  (t_set_cur_nodechip_idx)dlsym(NULL, "set_cur_nodechip_idx");
  cmodel_nodechip_runtime_init_ =  (t_cmodel_nodechip_runtime_init)dlsym(NULL, "cmodel_nodechip_runtime_init");
  cmodel_nodechip_runtime_exit_   =  (t_cmodel_nodechip_runtime_exit)dlsym(NULL, "cmodel_nodechip_runtime_exit");
  cmodel_deinit_   =  (t_cmodel_deinit)dlsym(NULL, "cmodel_deinit");
  cmodel_get_share_memory_addr_   =  (t_cmodel_get_share_memory_addr)dlsym(NULL, "cmodel_get_share_memory_addr");
  cmodel_write_share_reg_   =  (t_cmodel_write_share_reg)dlsym(NULL, "cmodel_write_share_reg");
  cmodel_write_share_memory_   =  (t_cmodel_write_share_memory)dlsym(NULL, "cmodel_write_share_memory");
  cmodel_read_share_reg_   =  (t_cmodel_read_share_reg)dlsym(NULL, "cmodel_read_share_reg");
  host_dma_copy_s2d_cmodel_ = (t_host_dma_copy_s2d_cmodel)dlsym(NULL, "host_dma_copy_s2d_cmodel");
  host_dma_copy_d2s_cmodel_ = (t_host_dma_copy_d2s_cmodel)dlsym(NULL, "host_dma_copy_d2s_cmodel");
  host_dma_copy_d2s_cmodel_ = (t_host_dma_copy_d2s_cmodel)dlsym(NULL, "host_dma_copy_d2s_cmodel");

  cmodel_share_reg_message_rp_ = (t_cmodel_share_reg_func)dlsym(NULL, "cmodel_share_reg_message_rp");
  cmodel_share_reg_message_wp_ = (t_cmodel_share_reg_func)dlsym(NULL, "cmodel_share_reg_message_wp");
  cmodel_share_reg_fw_status_ = (t_cmodel_share_reg_func)dlsym(NULL, "cmodel_share_reg_fw_status");
  cmodel_wait_share_reg_equal_ = (t_cmodel_wait_share_reg_equal)dlsym(NULL, "cmodel_wait_share_reg_equal");

  cmodel_api_poll_ = (t_cmodel_api_poll)dlsym(NULL, "api_poll");
  cmodel_api_signal_ = (t_cmodel_api_poll)dlsym(NULL, "api_signal");
  cmodel_api_signal_begin_ = (t_cmodel_api_poll)dlsym(NULL, "api_signal_begin");
  cmodel_get_chip_id_ = (t_cmodel_get_chip_id)dlsym(NULL, "get_chip_id");
  cmodel_get_total_nodechip_num_ = (t_cmodel_get_total_nodechip_num)dlsym(NULL, "get_total_nodechip_num");
  cmodel_get_gmem_start_addr_ = (t_cmodel_get_gmem_start_addr)dlsym(NULL, "cmodel_get_gmem_start_addr");
  cmodel_get_last_func_id = (t_cmodel_get_last_func_id)dlsym(NULL, "cmodel_get_last_func_id");

  if (cmodel_deinit_ == NULL) {
      const char *path = getenv("TPUKERNEL_FIRMWARE_PATH");
      cmodel_so_handle_ = dlopen(path ? path : "libcmodel.so", RTLD_LAZY);
      if(!cmodel_so_handle_) {
          printf("not able to open libcmodel.so: %s\n", dlerror());
        return;
      }
      get_global_memaddr_    =  (t_get_global_memaddr)dlsym(cmodel_so_handle_,  "get_global_memaddr");
      cmodel_init_  =  (t_cmodel_init)dlsym(cmodel_so_handle_,  "cmodel_init");
      set_cur_nodechip_idx_ =  (t_set_cur_nodechip_idx)dlsym(cmodel_so_handle_, "set_cur_nodechip_idx");
      cmodel_nodechip_runtime_init_ =  (t_cmodel_nodechip_runtime_init)dlsym(cmodel_so_handle_, "cmodel_nodechip_runtime_init");
      cmodel_nodechip_runtime_exit_   =  (t_cmodel_nodechip_runtime_exit)dlsym(cmodel_so_handle_, "cmodel_nodechip_runtime_exit");
      cmodel_deinit_   =  (t_cmodel_deinit)dlsym(cmodel_so_handle_, "cmodel_deinit");
      cmodel_get_share_memory_addr_   =  (t_cmodel_get_share_memory_addr)dlsym(cmodel_so_handle_, "cmodel_get_share_memory_addr");
      cmodel_write_share_reg_   =  (t_cmodel_write_share_reg)dlsym(cmodel_so_handle_, "cmodel_write_share_reg");
      cmodel_write_share_memory_   =  (t_cmodel_write_share_memory)dlsym(cmodel_so_handle_, "cmodel_write_share_memory");
      cmodel_read_share_reg_   =  (t_cmodel_read_share_reg)dlsym(cmodel_so_handle_, "cmodel_read_share_reg");
      host_dma_copy_d2s_cmodel_ = (t_host_dma_copy_d2s_cmodel)dlsym(cmodel_so_handle_, "host_dma_copy_d2s_cmodel");
      host_dma_copy_s2d_cmodel_ = (t_host_dma_copy_s2d_cmodel)dlsym(cmodel_so_handle_, "host_dma_copy_s2d_cmodel");

      cmodel_share_reg_message_rp_ = (t_cmodel_share_reg_func)dlsym(cmodel_so_handle_, "cmodel_share_reg_message_rp");
      cmodel_share_reg_message_wp_ = (t_cmodel_share_reg_func)dlsym(cmodel_so_handle_, "cmodel_share_reg_message_wp");
      cmodel_share_reg_fw_status_ = (t_cmodel_share_reg_func)dlsym(cmodel_so_handle_, "cmodel_share_reg_fw_status");
      cmodel_wait_share_reg_equal_ = (t_cmodel_wait_share_reg_equal)dlsym(cmodel_so_handle_, "cmodel_wait_share_reg_equal");

      cmodel_api_poll_ = (t_cmodel_api_poll)dlsym(cmodel_so_handle_, "api_poll");
      cmodel_api_signal_ = (t_cmodel_api_poll)dlsym(cmodel_so_handle_, "api_signal");
      cmodel_api_signal_begin_ = (t_cmodel_api_poll)dlsym(cmodel_so_handle_, "api_signal_begin");
      cmodel_get_chip_id_ = (t_cmodel_get_chip_id)dlsym(cmodel_so_handle_, "get_chip_id");
      cmodel_get_total_nodechip_num_ = (t_cmodel_get_total_nodechip_num)dlsym(cmodel_so_handle_, "get_total_nodechip_num");
      cmodel_get_gmem_start_addr_ = (t_cmodel_get_gmem_start_addr)dlsym(cmodel_so_handle_, "cmodel_get_gmem_start_addr");
      cmodel_get_last_func_id = (t_cmodel_get_last_func_id)dlsym(cmodel_so_handle_, "cmodel_get_last_func_id");
  }
  return;
  }


  bm_device::bm_device(int _dev_id)
  : dev_id(_dev_id),
    device_mem_pool(get_global_mem_size()),
    device_sync_last(0),
    device_sync_cpl(0) {
  cmodel_setup();
  core_num = cmodel_get_total_nodechip_num_();

  printf("begin to cmodel init...core_num = %d\n", core_num);

  for (int core_idx = 0; core_idx < core_num; ++core_idx) {
    if (cmodel_init_(core_idx, get_global_mem_size()) != BM_SUCCESS) {
      printf("BM: cmodel_init (core_idx=%d) failed\n", core_idx);
      exit(-1);
    }
  }
  set_cur_nodechip_idx_(0);

  chip_id = cmodel_get_chip_id_();
  for (int core_idx = 0; core_idx < core_num; ++core_idx) {
    cmodel_nodechip_runtime_init_(core_idx);
  }
  reserved_ddr_arm = 0x1000000;
  api_message_empty_slot_num = 2;
  sharemem_size_bit = 11;
  sharemem_mask = ((1 << sharemem_size_bit) - 1);
  share_reg_message_wp = cmodel_share_reg_message_wp_();
  share_reg_message_rp = cmodel_share_reg_message_rp_();
  share_reg_fw_status = cmodel_share_reg_fw_status_();
  last_ini_reg_val = 0x76125438;


  // ctx->device_mem_size = cmodel_get_global_mem_size(devid);

  BM_CHECK_RET(bm_alloc_instr_reserved());
  BM_CHECK_RET(bm_alloc_arm_reserved());
  BM_CHECK_RET(bm_alloc_iommu_reserved());
  for (int core_idx = 0; core_idx < core_num; ++core_idx) {
    bm_wait_fwinit_done(core_idx);
  }

  thread_api_tables = new std::map<pthread_t, thread_api_info>[core_num];
  pending_api_queues = new std::queue<api_queue_entry>[core_num];

  // init sync_last and sync_cpl
  device_sync_last = new u32[core_num];
  device_sync_cpl = new std::atomic<u32>[core_num];
  for (int core_idx = 0; core_idx < core_num; ++core_idx) {
    device_sync_last[core_idx] = 0;
    device_sync_cpl[core_idx] = 0;
  }

  api_locks = new pthread_mutex_t[core_num];
  for (int core_idx = 0; core_idx < core_num; ++core_idx) {
    pthread_mutex_init(&api_locks[core_idx], nullptr);
  }
  pthread_mutex_init(&arm_reserved_lock, nullptr);
  // init msg poll thread
  msg_poll_threads = new pthread_t[core_num];
  msg_done_poll_params = new msg_done_poll_param_t[core_num];
  for (int core_idx = 0; core_idx < core_num; ++core_idx) {
    msg_done_poll_params[core_idx].dev = this;
    msg_done_poll_params[core_idx].core_idx = core_idx;
    pthread_create(&msg_poll_threads[core_idx], nullptr, bm_msg_done_poll, (void*)&msg_done_poll_params[core_idx]);
  }
  }

  bm_device::~bm_device() {
    // printf("destroy device %d\n", dev_id);

    for (int core_idx = 0; core_idx < core_num; ++core_idx) {
        if(nullptr==msg_poll_threads) continue;
        if (msg_poll_threads[core_idx] != 0) { 
            pthread_cancel(msg_poll_threads[core_idx]);
            pthread_join(msg_poll_threads[core_idx], nullptr);
        }
    }

    delete[] msg_poll_threads;
    msg_poll_threads = nullptr;

    delete[] msg_done_poll_params;
    msg_done_poll_params = nullptr;

    bm_free_arm_reserved();
    bm_free_instr_reserved();
    bm_free_iommu_reserved();

    for (int core_idx = 0; core_idx < core_num; ++core_idx) {
        bm_send_quit_message(core_idx);
    }

    for (int core_idx = 0; core_idx < core_num; ++core_idx) {
        pthread_mutex_destroy(&api_locks[core_idx]);
    }

    delete[] api_locks;
    api_locks = nullptr;

    delete[] thread_api_tables;
    thread_api_tables = nullptr;

    delete[] pending_api_queues;
    pending_api_queues = nullptr;

    delete[] device_sync_last;
    device_sync_last = nullptr;

    delete[] device_sync_cpl;
    device_sync_cpl = nullptr;

    for (int core_idx = 0; core_idx < core_num; ++core_idx) {
        cmodel_nodechip_runtime_exit_(core_idx);
    }

    for (int core_idx = 0; core_idx < core_num; ++core_idx) {
        cmodel_deinit_(core_idx);
    }

    printf("cmodel_deinit complete\n");

    if (cmodel_so_handle_) {
        dlclose(cmodel_so_handle_);
        cmodel_so_handle_ = nullptr;
    }
  }


  u64 bm_device::bm_device_alloc_mem(u64 size) {
  return device_mem_pool.bm_mem_pool_alloc(size);
  }

  void bm_device::bm_device_free_mem(u64 addr) {
  device_mem_pool.bm_mem_pool_free(addr);
  }

  void bm_device::_write_share_mem(u32 offset, u32 data, int core_idx) {
  cmodel_write_share_memory_(offset, data, core_idx);
  }

  void bm_device::_write_share_reg(u32 idx, u32 data, int core_idx) {
  cmodel_write_share_reg_(idx, data, core_idx);
  }

  u32 bm_device::_read_share_reg(u32 idx, int core_idx) {
  return cmodel_read_share_reg_(idx, core_idx);
  }

  u32 bm_device::_poll_message_fifo_cnt(int core_idx) {
  u32 wp, rp;
  wp = _read_share_reg(share_reg_message_wp, core_idx);
  rp = _read_share_reg(share_reg_message_rp, core_idx);

  u32 wp_tog = wp >> sharemem_size_bit;
  u32 rp_tog = rp >> sharemem_size_bit;
  u32 wp_offset = wp - (wp_tog << sharemem_size_bit);
  u32 rp_offset = rp - (rp_tog << sharemem_size_bit);

  if (wp_tog == rp_tog)
    return (1 << sharemem_size_bit) - wp_offset + rp_offset;
  else
    return rp_offset - wp_offset;
  }

  void bm_device::copy_message_to_sharemem(const u32 *src_msg_buf,
    u32 *wp, u32 size, u32 api_id, int core_idx) {
  // read writing pointer from the share register
  u32 cur_wp = _read_share_reg(share_reg_message_wp, core_idx);
  *wp = cur_wp;

  // copy api_id to buffer
  _write_share_mem(cur_wp & sharemem_mask, api_id, core_idx);
  u32 next_wp = pointer_wrap_around(cur_wp, 1, sharemem_size_bit) &
        sharemem_mask;

  // copy size to buffer
  _write_share_mem(next_wp & sharemem_mask, size, core_idx);

  // copy sg_api_* structure date to buffer
  for (u32 idx = 0; idx < size; idx++) {
    next_wp = pointer_wrap_around(*wp, 2 + idx, sharemem_size_bit);
    _write_share_mem(next_wp & sharemem_mask, src_msg_buf[idx], core_idx);
  }

  // write back writing pointer to the share register
  next_wp = pointer_wrap_around(*wp, size + 2, sharemem_size_bit);
  _write_share_reg(share_reg_message_wp, next_wp, core_idx);
  }

  void P_(int semid) {
    struct sembuf sb = {0, -1, 0};
    semop(semid, &sb, 1);
  }

  bm_status_t bm_device::bm_device_send_api(int api_id, const u8 *api, u32 size, int core_idx) {
  pthread_t tid=pthread_self();
  // pid_t pid = getpid();
  char proj_id[10];
  struct bm_ret bm_ret;
  std::hash<pthread_t> hasher;
  key_t sem_key = hasher(tid);

  // bmlib_log(BMLIB_DEVICE_TAG, BMLIB_LOG_INFO,
  // 							"build message to bmcpu.\n");
  // message to bmcpu
  bm_api_to_bmcpu_t bm_api;
  bm_api.api_id = api_id;
  memcpy(bm_api.api_data, api, size);
  bm_api.api_size = size;
  bm_api.sem_key = sem_key;
  bm_api.tid = tid;

  // open message queue
  // bmlib_log(BMLIB_DEVICE_TAG, BMLIB_LOG_INFO,
  // 							"open message queue.\n");
  struct mq_attr attr;
  attr.mq_flags = 0;
  attr.mq_maxmsg = 20;
  attr.mq_msgsize = sizeof(bm_api_to_bmcpu_t);
  attr.mq_curmsgs = 0;
  mqd_t mq1 = mq_open(QUEUE1_NAME, O_WRONLY | O_CREAT, 0644, &attr);
  // mqd_t mq1 = mq_open(QUEUE1_NAME, O_WRONLY);
  if (mq1 == (mqd_t)-1) {
      perror("mq_open queue");
      return BM_ERR_FAILURE;
  }

  bmlib_log(BMLIB_DEVICE_TAG, BMLIB_LOG_INFO,
                "create semget.....\n");
  int semid = semget(sem_key, 1, IPC_CREAT | IPC_EXCL | 0666);
  if (semid == -1) {
      if (errno == EEXIST) {
          semid = semget(sem_key, 1, 0666);
          if (semid == -1) {
              perror("Failed to get existing semaphore");
              return BM_ERR_FAILURE;
          }
          printf("Existing semaphore acquired, semid: %d\n", semid);
      } else {
          perror("Failed to create semaphore");
          return BM_ERR_FAILURE;
      }
  } else {
      if (semctl(semid, 0, SETVAL, 0) == -1) {
          perror("Failed to initialize semaphore");
          return BM_ERR_FAILURE;
      }
      printf("New semaphore created, semid: %d\n", semid);
  }

  bmlib_log(BMLIB_DEVICE_TAG, BMLIB_LOG_INFO,
                "send data to mquequ.....\n");
  if (mq_send(mq1, (const char*)&bm_api, sizeof(bm_api_to_bmcpu_t), 0) == -1) {
      perror("mq_send header");
      mq_close(mq1);
      return BM_ERR_FAILURE;
  }

  bmlib_log(BMLIB_DEVICE_TAG, BMLIB_LOG_INFO,
                "mq_send success! send_size %d, thread_id %d, wait for sem_key %ld \n",
                sizeof(bm_api_to_bmcpu_t), tid, sem_key);
  P_(semid);

  // get result from message queue
  bmlib_log(BMLIB_DEVICE_TAG, BMLIB_LOG_INFO,
                "start handle result from bmcpu.....\n");
  struct mq_attr attr2;
  attr2.mq_flags = 0;
  attr2.mq_maxmsg = 20;
  attr2.mq_msgsize = sizeof(bm_ret);
  attr2.mq_curmsgs = 0;
  mqd_t mq2 = mq_open(QUEUE2_NAME, O_RDONLY | O_CREAT, 0644, &attr2);
  // mqd_t mq2 = mq_open(QUEUE2_NAME, O_RDONLY);
  if (mq2 == (mqd_t)-1) {
      perror("mq_open queue2");
      mq_close(mq1);
      return BM_ERR_FAILURE;
  }

  mq_receive(mq2, (char*)&bm_ret, sizeof(struct bm_ret), NULL);
  if (mq2 == (mqd_t)-1) {
      bmlib_log(BMLIB_DEVICE_TAG, BMLIB_LOG_ERROR,"mq_receive failed....");
      return BM_ERR_FAILURE;
  }

  mq_close(mq1);
  mq_close(mq2);
  if(bm_ret.result!=0){
    bmlib_log(BMLIB_DEVICE_TAG, BMLIB_LOG_ERROR, "api result = %u\n", bm_ret.result);
    return BM_ERR_FAILURE;
  }

  if(api_id==BM_API_ID_TPUSCALER_GET_FUNC){
    int read_f_id;
    a53lite_get_func_t *api_new=(a53lite_get_func_t*)api;
    if (sscanf(bm_ret.msg, "f_id value: %d", &read_f_id) == 1) {
      bmlib_log(BMLIB_DEVICE_TAG, BMLIB_LOG_INFO, "Read f_id: %d\n", read_f_id);
    }
    api_new->f_id=read_f_id;
  }
    
  bmlib_log(BMLIB_DEVICE_TAG, BMLIB_LOG_INFO,
                "api_id 0x%x done *************\n", api_id);
  return BM_SUCCESS;
  }

  bm_status_t bm_device::bm_device_sync() {
  for (int core_idx =0; core_idx < core_num; ++core_idx) {
    pthread_mutex_lock(&api_locks[core_idx]);

    u32 dev_sync_last =  ++device_sync_last[core_idx];
    pending_api_queues[core_idx].push({DEVICE_SYNC_MARKER, 0, dev_sync_last});
    //printf("SYNC DEVICE API: device (core_idx=%d) last seq %d\n", core_idx, device_sync_last[core_idx]);
    pthread_mutex_unlock(&api_locks[core_idx]);
  }

  while (true) {
    bool success = true;
    for (int core_idx = 0; core_idx < core_num; ++core_idx) {
      if (device_sync_last[core_idx] != device_sync_cpl[core_idx]) {
        success = false;
        break;
      }
    }
    if (success) break;
  }

  for (int core_idx = 0; core_idx < core_num; ++core_idx) {
    cmodel_wait_share_reg_equal_(share_reg_message_rp,
        cmodel_read_share_reg_(share_reg_message_wp, core_idx), 32, 0, core_idx);
  }
  // while (_poll_message_fifo_cnt() != (1 << sharemem_size_bit));
  return BM_SUCCESS;
  }

  bm_status_t bm_device::bm_send_quit_message(int core_idx) {
  printf("BMLIB Send Quit Message\n");
  bm_device_send_api((sglib_api_id_t)BM_API_QUIT, nullptr, 0, core_idx);
  sleep(1);
  return BM_SUCCESS;
  }

  void bm_device::bm_wait_fwinit_done(int core_idx) {
  cmodel_wait_share_reg_equal_(share_reg_fw_status, last_ini_reg_val, 32, 0, core_idx);
  while (_read_share_reg(share_reg_fw_status, core_idx) != last_ini_reg_val) { }
  }

  bm_status_t bm_device::bm_malloc_device_dword(
    bm_device_mem_t *pmem, int cnt) {
  u32 size = cnt * FLOAT_SIZE;
  u64 addr = 0;

  addr = device_mem_pool.bm_mem_pool_alloc(size);

  pmem->u.device.device_addr = addr;
  pmem->flags.u.mem_type = BM_MEM_TYPE_DEVICE;
  pmem->size = size;
  return BM_SUCCESS;
  }

  void bm_device::bm_free_device(bm_device_mem_t mem) {
  u64 addr = (u64)bm_mem_get_device_addr(mem);
  device_mem_pool.bm_mem_pool_free(addr);
  }

  bm_status_t bm_device::bm_alloc_arm_reserved() {
  BM_CHECK_RET(bm_malloc_device_dword(&arm_reserved_dev_mem,
        reserved_ddr_arm));
  return BM_SUCCESS;
  }

  void bm_device::bm_free_arm_reserved() {
  bm_free_device(arm_reserved_dev_mem);
  }

  bm_status_t bm_device::bm_alloc_instr_reserved() {
  BM_CHECK_RET(bm_malloc_device_dword(&instr_reserved_mem,
        reserved_ddr_arm / 4));
  return BM_SUCCESS;
  }

  void bm_device::bm_free_instr_reserved() {
  bm_free_device(instr_reserved_mem);
  }

  #define SMMU_RESERVED_SIZE (0x40000 * 4)
  bm_status_t bm_device::bm_alloc_iommu_reserved() {
  BM_CHECK_RET(bm_malloc_device_dword(&iommu_reserved_dev_mem,
        SMMU_RESERVED_SIZE/sizeof(float)));
  return BM_SUCCESS;
  }

  void bm_device::bm_free_iommu_reserved() {
  bm_free_device(iommu_reserved_dev_mem);
  }


  bm_status_t bm_device::bm_device_memcpy_s2d(bm_device_mem_t dst, void *src, int core_idx) {
  u32 size_total = bm_mem_get_size(dst);
  u64 dst_addr = bm_mem_get_device_addr(dst);
  host_dma_copy_s2d_cmodel_(dst_addr, src, (u64)size_total, core_idx);
  return BM_SUCCESS;
  }

  bm_status_t bm_device::bm_device_memcpy_d2s(void *dst, bm_device_mem_t src, int core_idx) {
  u32 size_total = bm_mem_get_size(src);
  u64 src_addr = bm_mem_get_device_addr(src);

  host_dma_copy_d2s_cmodel_(dst, src_addr, (u64)size_total, core_idx);
  return BM_SUCCESS;
  }

  bm_status_t bm_device::bm_device_memcpy_s2d_u64(bm_device_mem_u64_t dst, void *src, int core_idx) {
  u64 size_total = bm_mem_get_size_u64(dst);
  u64 dst_addr = bm_mem_get_device_addr_u64(dst);
  host_dma_copy_s2d_cmodel_(dst_addr, src, (u64)size_total, core_idx);
  return BM_SUCCESS;
  }

  bm_status_t bm_device::bm_device_memcpy_d2s_u64(void *dst, bm_device_mem_u64_t src, int core_idx) {
  u64 size_total = bm_mem_get_size_u64(src);
  u64 src_addr = bm_mem_get_device_addr_u64(src);

  host_dma_copy_d2s_cmodel_(dst, src_addr, (u64)size_total, core_idx);
  return BM_SUCCESS;
  }

  bm_status_t bm_device::sg_device_memcpy_s2d(sg_device_mem_t dst, void *src, int core_idx) {
  u64 size_total = sg_mem_get_size(dst);
  u64 dst_addr = sg_mem_get_device_addr(dst);
  host_dma_copy_s2d_cmodel_(dst_addr, src, size_total, core_idx);
  return BM_SUCCESS;
  }

  bm_status_t bm_device::sg_device_memcpy_d2s(void *dst, sg_device_mem_t src, int core_idx) {
  u64 size_total = sg_mem_get_size(src);
  u64 src_addr = sg_mem_get_device_addr(src);

  host_dma_copy_d2s_cmodel_(dst, src_addr, size_total, core_idx);
  return BM_SUCCESS;
  }
  u64 bm_device::bm_device_arm_reserved_req() {
  pthread_mutex_lock(&arm_reserved_lock);
  return arm_reserved_dev_mem.u.device.device_addr;
  }
  void bm_device::bm_device_arm_reserved_rel() {
  pthread_mutex_unlock(&arm_reserved_lock);
  }

  bm_status_t bm_device::bm_device_thread_sync_from_core(int core_idx) {
  thread_api_info *thd_api_info;
  thd_api_info = bm_get_thread_api_info(pthread_self(), core_idx);
  if (!thd_api_info) {
    printf("Error: thread api info %lu is not found!\n", pthread_self());
    ASSERT(0);
    return BM_ERR_FAILURE;
  }

  pthread_mutex_lock(&thd_api_info->lock);
  while (thd_api_info->last_seq != thd_api_info->cpl_seq) {
      pthread_cond_wait(&thd_api_info->cond, &thd_api_info->lock);
  }
  pthread_mutex_unlock(&thd_api_info->lock);

  cmodel_wait_share_reg_equal_(share_reg_message_rp,
      cmodel_read_share_reg_(share_reg_message_wp, core_idx), 32, 0, core_idx);
  return BM_SUCCESS;
  }

  thread_api_info *bm_device::bm_get_thread_api_info(pthread_t thd_id, int core_idx) {
  std::map < pthread_t, thread_api_info>::iterator it;
  it = thread_api_tables[core_idx].find(thd_id);
  if (it != thread_api_tables[core_idx].end())
    return &it->second;
  else
    return nullptr;
  }

  bm_status_t bm_device::bm_add_thread_api_info(pthread_t thd_id, int core_idx) {
  thread_api_tables[core_idx].insert(std::pair<pthread_t, thread_api_info>(thd_id,
              {thd_id, 0, 0}));
  return BM_SUCCESS;
  }

  bm_status_t bm_device::bm_remove_thread_api_info(pthread_t thd_id, int core_idx) {
  std::map < pthread_t, thread_api_info>::iterator it;
  it = thread_api_tables[core_idx].find(thd_id);
  if (it != thread_api_tables[core_idx].end())
    thread_api_tables[core_idx].erase(it);
  return BM_SUCCESS;
  }

  void *bm_device::bm_msg_done_poll(void *arg) {
  msg_done_poll_param_t *_param = reinterpret_cast < msg_done_poll_param_t *>(arg);
  bm_device *bm_dev = _param->dev;
  int core_idx = _param->core_idx;
  while (1) {
    while (!bm_dev->pending_api_queues[core_idx].empty()) {
      api_queue_entry api_front = bm_dev->pending_api_queues[core_idx].front();
      if (api_front.thd_id == DEVICE_SYNC_MARKER) {
  // device sync
        bm_dev->device_sync_cpl[core_idx] = api_front.dev_seq;
        pthread_mutex_lock(&bm_dev->api_locks[core_idx]);
        bm_dev->pending_api_queues[core_idx].pop();
        pthread_mutex_unlock(&bm_dev->api_locks[core_idx]);
        pthread_yield();
      } else {
  // msg api pending
        bm_dev->cmodel_api_poll_(core_idx);

        if (api_front.thd_id != 0) {
          thread_api_info *thd_api_info = bm_dev->bm_get_thread_api_info(
              api_front.thd_id, core_idx);
          pthread_mutex_lock(&thd_api_info->lock);
          thd_api_info->cpl_seq = api_front.thd_seq;
          pthread_mutex_unlock(&thd_api_info->lock);
          pthread_cond_signal(&thd_api_info->cond);

          pthread_mutex_lock(&bm_dev->api_locks[core_idx]);
          bm_dev->pending_api_queues[core_idx].pop();
          pthread_mutex_unlock(&bm_dev->api_locks[core_idx]);
        } else {
          ASSERT(0);
        }
      }
    }
  // busy waiting sleep 200ms, reduce cpu usage
  #if defined(USING_CMODEL) && !defined(USING_MULTI_THREAD_ENGINE)
    usleep(200000);
  #endif
    pthread_testcancel();
  }
  return nullptr;
  }

  bm_device_manager::bm_device_manager(int _max_dev_cnt)
  : dev_cnt(0),
    max_dev_cnt(_max_dev_cnt),
    bm_dev_list(nullptr) {
  bm_dev_list = new bm_device *[max_dev_cnt];
  if (!bm_dev_list)
      return;
  dev_user = new int [max_dev_cnt];
  for (int i = 0; i < max_dev_cnt; i++) {
    bm_dev_list[i] = nullptr;
    dev_user[i] = 0;
  }
  }

  bm_device_manager::~bm_device_manager() {
  if (bm_dev_list) {
    for (int i = 0; i < max_dev_cnt; i++) {
      if (bm_dev_list[i]) {
        delete bm_dev_list[i];
        bm_dev_list[i] = nullptr;
      }
    }
    delete []bm_dev_list;
  }
  if (dev_user) {
    delete [] dev_user;
  }
  }

  bm_device_manager *bm_device_manager::get_dev_mgr() {
  pthread_mutex_lock(&init_lock);
  if (!bm_dev_mgr)
    bm_dev_mgr = new bm_device_manager(MAX_DEVICE_NUM);
  pthread_mutex_unlock(&init_lock);
  return bm_dev_mgr;
  }

  bm_device * bm_device_manager::get_bm_device(int dev_id) {
  ASSERT(bm_dev_list);
  ASSERT(dev_id < max_dev_cnt);
  pthread_mutex_lock(&init_lock);
  if (!bm_dev_list[dev_id]) {
    bm_dev_list[dev_id] = new bm_device(dev_id);
    dev_cnt++;
  }
  dev_user[dev_id]++;
  pthread_mutex_unlock(&init_lock);
  return bm_dev_list[dev_id];
  }

  void bm_device_manager::destroy_dev_mgr() {
  // std::cout << "bm_dev_mgr "<<bm_dev_mgr <<std::endl;
  if (bm_dev_mgr) {
    delete bm_dev_mgr;
    bm_dev_mgr = nullptr;
  }
  }

  void bm_device_manager::free_bm_device(int dev_id) {
  ASSERT(bm_dev_list);
  ASSERT(dev_id < max_dev_cnt);
  ASSERT(dev_user[dev_id] > 0);
  pthread_mutex_lock(&init_lock);
  dev_user[dev_id]--;
  if (dev_user[dev_id] == 0) {
    delete bm_dev_list[dev_id];
    bm_dev_list[dev_id] = nullptr;
  }
  pthread_mutex_unlock(&init_lock);
  }
  bm_device_manager *bm_device_manager::bm_dev_mgr = nullptr;
  pthread_mutex_t bm_device_manager::init_lock = PTHREAD_MUTEX_INITIALIZER;
  #endif


