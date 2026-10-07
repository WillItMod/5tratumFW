#pragma once

#include <pthread.h>
#include <string.h>

#include "esp_heap_caps.h"

#include "macros.h"
#include "mining.h"

#define MAX_ASIC_JOBS 128

class AsicJobs {
protected:
    bm_job *m_activeJobs[MAX_ASIC_JOBS];
    uint64_t m_chainGeneration = 1;
    uint64_t m_jobGenerations[MAX_ASIC_JOBS]{};
    pthread_mutex_t m_validJobsLock;

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
    }

    void storeJob(bm_job *next_job, uint8_t asic_job_id, uint64_t generation = 0) {
        PThreadGuard g(m_validJobsLock);
        if (asic_job_id >= MAX_ASIC_JOBS || (generation && generation != m_chainGeneration)) {
            free_bm_job(next_job);
            return;
        }
        // if a slot was used before free it
        if (m_activeJobs[asic_job_id]) {
            free_bm_job(m_activeJobs[asic_job_id]);
        }
        // save job into slot
        m_activeJobs[asic_job_id] = next_job;
        m_jobGenerations[asic_job_id] = m_chainGeneration;
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
