/* psprecomp ABI trap and HLE bridge to repsp runtime. */

#include "recomp_abi.h"

#include <psprecomp/cpu.h>
#include <psprecomp/hle.h>
#include <psprecomp/recomp_rt.h>
#include <repsp/platform/guarded_run.h>
#include <repsp/recomp/guest_cpu.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t g_syscalls = 0;
static uint64_t g_unimplemented = 0;
static char g_unimpl_reason[256];

/* Optional env-gated HLE call site tracer (REDMP2_TRACE_NID=0xXXXXXXXX). */
static int g_trace_nid_init = 0;
static uint32_t g_trace_nid = 0;
static uint64_t g_trace_nid_limit = 16;
static uint64_t g_trace_nid_seen = 0;

static void trace_nid_init_once(void) {
    g_trace_nid_init = 1;
    const char *env = getenv("REDMP2_TRACE_NID");
    if (!env || !env[0]) {
        return;
    }
    g_trace_nid = (uint32_t)strtoul(env, NULL, 0);
    const char *lim = getenv("REDMP2_TRACE_NID_LIMIT");
    if (lim && lim[0]) {
        unsigned long long v = strtoull(lim, NULL, 0);
        if (v > 0) {
            g_trace_nid_limit = (uint64_t)v;
        }
    }
}

void psp_hle_call(uint32_t nid) {
    if (!g_trace_nid_init) {
        trace_nid_init_once();
    }
    if (g_trace_nid != 0u && nid == g_trace_nid && g_trace_nid_seen < g_trace_nid_limit) {
        const uint64_t idx = ++g_trace_nid_seen;
        const uint32_t ra = psp_cpu.r[31];
        const uint32_t sp = psp_cpu.r[29];
        const uint32_t a0 = psp_cpu.r[4];
        const uint32_t a1 = psp_cpu.r[5];
        const uint32_t a2 = psp_cpu.r[6];
        const uint32_t a3 = psp_cpu.r[7];
        const uint32_t s0 = psp_cpu.r[16];
        const uint32_t s1 = psp_cpu.r[17];
        repsp_recomp_hle_call(nid);
        fprintf(stderr,
                "[REDMP2_TRACE_NID #%llu] nid=0x%08X ra=0x%08X sp=0x%08X a0=0x%08X a1=0x%08X a2=0x%08X a3=0x%08X s0=0x%08X s1=0x%08X -> v0=0x%08X\n",
                (unsigned long long)idx, nid, ra, sp, a0, a1, a2, a3, s0, s1, psp_cpu.r[2]);
        return;
    }
    repsp_recomp_hle_call(nid);
}

void psp_syscall(uint32_t id) {
    g_syscalls++;
    repsp_recomp_syscall(id);
}

void psp_unimplemented(uint32_t addr, const char *what) {
    g_unimplemented++;
    snprintf(g_unimpl_reason, sizeof(g_unimpl_reason),
             "Unimplemented at 0x%08X: %s", addr, what ? what : "unknown");
    fprintf(stderr, "[recomp_abi] %s\n", g_unimpl_reason);
    repsp_raise_stop(g_unimpl_reason);
}

uint64_t psp_syscall_total(void) { return g_syscalls; }
uint64_t psp_unimplemented_total(void) { return g_unimplemented; }
void psp_trap_reset(void) {
    g_syscalls = 0;
    g_unimplemented = 0;
    g_trace_nid_seen = 0;
}
