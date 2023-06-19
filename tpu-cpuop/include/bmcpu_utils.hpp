#ifndef BMCPU_UTILS_HPP
#define BMCPU_UTILS_HPP

#include<thread>
#include<vector>
#include<atomic>
#include<queue>

#include "bmcpu_common.h"

namespace bmcpu {

int cpu_get_type_len(CPU_DATA_TYPE_T dtype);
int using_thread_num(int N);
struct BlockExecutor {
    template<typename Func, typename... ArgTypes>
    static void run(Func functor, const int N, ArgTypes... args){
        int nthreads = using_thread_num(N);
        int nblocks = (N+nthreads-1)/nthreads;

        for(int b=0; b<nblocks; b++){
            std::vector<std::thread> threads;
            for(int i=0; i<nthreads; i++){
                int idx = b*nthreads + i;
                if(idx<N){
                    threads.emplace_back(functor,idx, args...);
                }
            }
            for(auto& t: threads){
                t.join();
            }
        }
    }
};

}


#endif // BMCPU_UTILS_HPP
