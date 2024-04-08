
#include "bm_common.h"
#include "bm_thread.h"

void hash_add_rcu(PLIST_ENTRY hash_table, PLIST_ENTRY thd_info_list, pid_t pid) {
    int key = pid % 32;
    InsertTailList(&hash_table[key], thd_info_list);
}

struct bm_thread_info *bmdrv_find_thread_info(struct bm_handle_info *h_info, pid_t pid)
{
	struct bm_thread_info *thd_info = NULL;
	int pid_found = 0;
    PLIST_ENTRY            handle_list = NULL;
    int                    key       = pid % 32;
    PLIST_ENTRY            temp_list = (h_info->api_htable) + key;

	if (h_info) {
		for (handle_list = temp_list->Flink; handle_list != temp_list; handle_list = handle_list->Flink){//list is not empty or have finished serch
			thd_info = CONTAINING_RECORD(handle_list, struct bm_thread_info, node_list);
            if (thd_info->user_pid == pid) {
                pid_found = 1;
                break;
            }
		}
	}
	if (pid_found)
		return thd_info;
	else
		return NULL;
}

struct bm_thread_info *bmdrv_create_thread_info(struct bm_handle_info *h_info, pid_t pid){
	struct bm_thread_info *thd_info;
	PHYSICAL_ADDRESS pa;
    pa.QuadPart = 0xFFFFFFFFFFFFFFFF;

	thd_info = MmAllocateContiguousMemory(sizeof(struct bm_thread_info), pa);
	if (!thd_info)
		return thd_info;
    thd_info->user_pid = pid;

	KeInitializeEvent(&thd_info->msg_doneEvent, SynchronizationEvent, FALSE);

	thd_info->last_api_seq = 0;
	thd_info->cpl_api_seq = 0;

	thd_info->profile.cdma_in_time = 0ULL;
	thd_info->profile.cdma_in_counter = 0ULL;
	thd_info->profile.cdma_out_time = 0ULL;
	thd_info->profile.cdma_out_counter = 0ULL;
	thd_info->profile.tpu_process_time = 0ULL;
	thd_info->profile.sent_api_counter = 0ULL;
	thd_info->profile.completed_api_counter = 0ULL;

	thd_info->trace_enable = 0;
	thd_info->trace_item_num = 0ULL;
    WDF_OBJECT_ATTRIBUTES attributes;
    NTSTATUS              status;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    status = WdfSpinLockCreate(&attributes, &thd_info->trace_spinlock);
    if (!NT_SUCCESS(status)) {
        return NULL;
    }

	InitializeListHead(&thd_info->trace_list);

	hash_add_rcu(h_info->api_htable, &thd_info->node_list, pid);

	return thd_info;
}

void bmdrv_delete_thread_info(struct bm_handle_info *h_info)//remove all thread_node in h_info's hash table
{
	struct bm_thread_info *thd_info;

	for (int i = 0; i < TABLE_NUM; i++) {
        PLIST_ENTRY temp_list = (h_info->api_htable)+i;
        PLIST_ENTRY handle_list = NULL;

        while (!IsListEmpty(temp_list)) {
            handle_list = RemoveHeadList(temp_list);
            thd_info = CONTAINING_RECORD(handle_list, struct bm_thread_info, node_list);
            MmFreeContiguousMemory(thd_info);
        }
	}
}
