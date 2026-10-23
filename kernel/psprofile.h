/*
 * psprofile.h - maxbatt DTIM skipping: swap two instructions in SceWlanBt's
 * join-time PS_MODE_ENH builder so it sends multiple_dtims = 3 instead of 1.
 * Takes effect at the next join. See psprofile.c.
 */
#ifndef WIFITURBO_PSPROFILE_H
#define WIFITURBO_PSPROFILE_H

int psprofile_dtim_set(int on);   /* 1 = patch in, 0 = release; WQ_OK or WQ_ERR_* */
int psprofile_dtim_active(void);

int psprofile_ps_get(void);       /* driver PS flag: 1 on, 0 off, <0 error   */
int psprofile_ps_set(int on);     /* set via FUN_81009628(6|7,1); returns new flag */

#endif /* WIFITURBO_PSPROFILE_H */
