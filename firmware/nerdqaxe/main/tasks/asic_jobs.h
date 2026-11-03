#pragma once

#include <pthread.h>
#include <string.h>

#include "esp_heap_caps.h"

#include "macros.h"
#include "mining.h"
#include "work_difficulty.h"

#define MAX_ASIC_JOBS 128

class AsicJobs {
protected:
    bm_job *m_activeJobs[MAX_ASIC_JOBS];
    uint64_t m_chainGeneration = 1;
    uint64_t m_jobGenerations[MAX_ASIC_JOBS]{};
    pthread_mutex_t m_validJobsLock;
    FiveTratumWork::Stamp m_workStamp[2]{};
    int m_difficultySlot[2]{-1,-1};

    void clearDifficultyLocked(int pool) {
        if(pool<0||pool>1) return;
        if(m_workStamp[pool].epoch<UINT64_MAX) ++m_workStamp[pool].epoch;
        m_workStamp[pool].receivedAtUs=0;m_difficultySlot[pool]=-1;
    }

    void lock() {
        pthread_mutex_lock(&m_validJobsLock);
    }

    void unlock() {
        pthread_mutex_unlock(&m_validJobsLock);
    }

    bm_job *cloneBmJob(bm_job *src)
    {
        bm_job *dst = (bm_job *) MALLOC(sizeof(bm_job));
        if (!dst) return nullptr;

        // copy all
        memcpy(dst, src, sizeof(bm_job));

        // copy strings
        dst->extranonce2 = strdup(src->extranonce2);
        dst->jobid = strdup(src->jobid);
        if (!dst->extranonce2 || !dst->jobid) { free_bm_job(dst); return nullptr; }

        return dst;
    }

public:
    AsicJobs() {
        m_validJobsLock = PTHREAD_MUTEX_INITIALIZER;
        memset(m_activeJobs, 0, sizeof(m_activeJobs));
    }

    ~AsicJobs() { resetChainGeneration(m_chainGeneration); }

    int cleanJobs(int pool) {
        PThreadGuard g(m_validJobsLock);
        int deleted = 0;
        clearDifficultyLocked(pool);
        for (int i = 0; i < MAX_ASIC_JOBS; i++) {
            if (m_activeJobs[i] && m_activeJobs[i]->pool_id == pool) {
                free_bm_job(m_activeJobs[i]);
                m_activeJobs[i] = 0;
                deleted++;
            }
        }
        return deleted;
    }

    void resetChainGeneration(uint64_t generation) {
        PThreadGuard g(m_validJobsLock);
        for (int i = 0; i < MAX_ASIC_JOBS; ++i) {
            if (m_activeJobs[i]) free_bm_job(m_activeJobs[i]);
            m_activeJobs[i] = nullptr;
            m_jobGenerations[i] = 0;
        }
        m_chainGeneration = generation;
        // A protected clock change retires ASIC slots, not the upstream job.
        // Keep its original receipt time for a subsequent verified redispatch.
        m_difficultySlot[0]=m_difficultySlot[1]=-1;
    }

    void storeJob(bm_job *next_job, uint8_t asic_job_id, uint64_t generation = 0, FiveTratumWork::Stamp stamp = {}, bool dispatchConfirmed = true) {
        PThreadGuard g(m_validJobsLock);
        if(!dispatchConfirmed) m_difficultySlot[0]=m_difficultySlot[1]=-1;
        if (asic_job_id >= MAX_ASIC_JOBS || (generation && generation != m_chainGeneration)) {
            free_bm_job(next_job);
            return;
        }
        for(int pool=0;pool<2;++pool) if(m_difficultySlot[pool]==asic_job_id) m_difficultySlot[pool]=-1;
        // if a slot was used before free it
        if (m_activeJobs[asic_job_id]) {
            free_bm_job(m_activeJobs[asic_job_id]);
        }
        // save job into slot
        m_activeJobs[asic_job_id] = next_job;
        m_jobGenerations[asic_job_id] = m_chainGeneration;
        const int pool=next_job->pool_id;
        if(dispatchConfirmed&&pool>=0&&pool<2&&stamp.epoch&&stamp.receivedAtUs&&
           stamp.epoch==m_workStamp[pool].epoch&&stamp.receivedAtUs==m_workStamp[pool].receivedAtUs)
            m_difficultySlot[pool]=asic_job_id;
    }

    // Called only for a newly accepted/activated protocol job, never for a
    // poll or a repeated extranonce dispatch. Old in-flight builds cannot
    // publish metadata after this epoch changes.
    void noteWorkReceived(int pool,uint64_t now) {
        PThreadGuard g(m_validJobsLock);clearDifficultyLocked(pool);
        if(pool>=0&&pool<2&&m_workStamp[pool].epoch<UINT64_MAX) m_workStamp[pool].receivedAtUs=now;
    }
    FiveTratumWork::Stamp workStamp(int pool) {
        PThreadGuard g(m_validJobsLock);
        return pool>=0&&pool<2?m_workStamp[pool]:FiveTratumWork::Stamp{};
    }
    uint64_t copyWorkDifficulty(FiveTratumWork::Sample out[2],uint64_t now) {
        PThreadGuard g(m_validJobsLock);
        for(int pool=0;pool<2;++pool) {
            out[pool]={};const int slot=m_difficultySlot[pool];
            if(slot<0||slot>=MAX_ASIC_JOBS||!m_workStamp[pool].receivedAtUs||now<m_workStamp[pool].receivedAtUs) continue;
            const bm_job *job=m_activeJobs[slot];
            if(!job||job->pool_id!=pool||m_jobGenerations[slot]!=m_chainGeneration||!job->pool_diff) continue;
            double network=0;if(!FiveTratumWork::targetDifficulty(job->target,network)) continue;
            out[pool]={true,static_cast<double>(job->pool_diff),network,m_workStamp[pool].receivedAtUs};
        }
        return m_chainGeneration;
    }

    bm_job *getClone(uint8_t asic_job_id, uint64_t generation = 0) {
        PThreadGuard g(m_validJobsLock);
        // check if we have a job with this job id
        if (asic_job_id >= MAX_ASIC_JOBS || !m_activeJobs[asic_job_id] ||
            m_jobGenerations[asic_job_id] != m_chainGeneration || (generation && generation != m_chainGeneration)) {
            return NULL;
        }
        // create a clone
        bm_job *job = cloneBmJob(m_activeJobs[asic_job_id]);

        // and return it
        return job;
    }

};
