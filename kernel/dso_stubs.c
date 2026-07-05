/*
 * dso_stubs.c — DSO-TRON Stub System Calls
 * ==========================================
 * Заглушки для μITRON системных вызовов, которые не поддерживаются в DSO.
 * Все возвращают E_NOSPT (not supported).
 *
 * Полный список вырезанных функций μITRON:
 *   cre_sem, del_sem, sig_sem, twai_sem, ref_sem        — семафоры (не нужны в TT)
 *   cre_flg, del_flg, set_flg, clr_flg, twai_flg, ref_flg — eventflags (не нужны в TT)
 *   cre_mbx, del_mbx, snd_mbx, rcv_mbx, trcv_mbx, ref_mbx — mailboxes (заменены DTQ)
 *   cre_mpf, del_mpf, get_mpf, rel_mpf, ref_mpf          — fixed mem pool (DSO Arena)
 *   cre_mpl, del_mpl, get_mpl, rel_mpl, ref_mpl          — var mem pool (DSO Arena)
 *   cre_mtx, del_mtx, loc_mtx, unl_mtx, ref_mtx          — mutexes (не нужны в TT)
 *   cre_por, del_por, cal_por, acp_por, ...              — rendezvous (не нужны)
 *   cre_isr, del_isr, ref_isr                            — ISR (PIT-only)
 *   def_inh, def_tex                                     — exception handlers
 *   def_ovr, sta_ovr, stp_ovr, ref_ovr                   — overrun
 *   chg_ims, get_ims                                     — interrupt mask
 *   ras_tex                                              — raise task exception
 *   ref_cfg, ref_tst                                     — config/test
 */

#include "kernel.h"

/* Task management extras */
ER get_tid(ID *p_tskid)
{
    if (!p_tskid) return E_PAR;
    *p_tskid = _kernel_cur ? _kernel_cur->id : 0;
    return E_OK;
}

ER get_pri(ID tskid, PRI *p_tskpri)
{
    (void)tskid;
    if (!p_tskpri) return E_PAR;
    *p_tskpri = 1;
    return E_OK;
}

ER chg_pri(ID tskid, PRI tskpri)
{
    (void)tskid;
    (void)tskpri;
    return E_OK;
}

ER dly_tsk(TMO dlytim)
{
    (void)dlytim;
    /* DSO: delay is handled by TT schedule, not by this call */
    return E_OK;
}

/* Time */
ER set_tim(const SYSTIM *pk_tim)
{
    (void)pk_tim;
    return E_NOSPT;
}

/* System info */
ER ref_sys(void)
{
    return E_OK;
}

ER ref_ver(void)
{
    return E_OK;
}

/* ── Not supported (cut features) ──────────────────────── */

ER cre_sem(void)  { return E_NOSPT; }
ER del_sem(void)  { return E_NOSPT; }
ER sig_sem(void)  { return E_NOSPT; }
ER twai_sem(void) { return E_NOSPT; }
ER ref_sem(void)  { return E_NOSPT; }

ER cre_flg(void)  { return E_NOSPT; }
ER del_flg(void)  { return E_NOSPT; }
ER set_flg(void)  { return E_NOSPT; }
ER clr_flg(void)  { return E_NOSPT; }
ER twai_flg(void) { return E_NOSPT; }
ER ref_flg(void)  { return E_NOSPT; }

ER cre_mbx(void)  { return E_NOSPT; }
ER del_mbx(void)  { return E_NOSPT; }
ER snd_mbx(void)  { return E_NOSPT; }
ER rcv_mbx(void)  { return E_NOSPT; }
ER trcv_mbx(void) { return E_NOSPT; }
ER ref_mbx(void)  { return E_NOSPT; }

ER cre_mpf(void)  { return E_NOSPT; }
ER del_mpf(void)  { return E_NOSPT; }
ER get_mpf(void)  { return E_NOSPT; }
ER rel_mpf(void)  { return E_NOSPT; }
ER ref_mpf(void)  { return E_NOSPT; }

ER cre_mpl(void)  { return E_NOSPT; }
ER del_mpl(void)  { return E_NOSPT; }
ER get_mpl(void)  { return E_NOSPT; }
ER rel_mpl(void)  { return E_NOSPT; }
ER ref_mpl(void)  { return E_NOSPT; }

ER cre_mtx(void)  { return E_NOSPT; }
ER del_mtx(void)  { return E_NOSPT; }
ER loc_mtx(void)  { return E_NOSPT; }
ER unl_mtx(void)  { return E_NOSPT; }
ER ref_mtx(void)  { return E_NOSPT; }

ER cre_por(void)  { return E_NOSPT; }
ER del_por(void)  { return E_NOSPT; }
ER cal_por(void)  { return E_NOSPT; }
ER acp_por(void)  { return E_NOSPT; }
ER rpl_rdv(void)  { return E_NOSPT; }
ER fwd_por(void)  { return E_NOSPT; }
ER tcal_por(void) { return E_NOSPT; }
ER tacp_por(void) { return E_NOSPT; }
ER ref_por(void)  { return E_NOSPT; }

ER cre_isr(void)  { return E_NOSPT; }
ER del_isr(void)  { return E_NOSPT; }
ER ref_isr(void)  { return E_NOSPT; }

ER def_inh(void)  { return E_NOSPT; }
ER def_tex(void)  { return E_NOSPT; }
ER def_ovr(void)  { return E_NOSPT; }
ER sta_ovr(void)  { return E_NOSPT; }
ER stp_ovr(void)  { return E_NOSPT; }
ER ref_ovr(void)  { return E_NOSPT; }

ER chg_ims(void)  { return E_NOSPT; }
ER get_ims(void)  { return E_NOSPT; }
ER ras_tex(void)  { return E_NOSPT; }
ER ref_cfg(void)  { return E_NOSPT; }
ER ref_tst(void)  { return E_NOSPT; }
