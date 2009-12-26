#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <openssl/bn.h>

#define PPC_FPU64	(1<<0)

static int OPENSSL_ppccap_P = 0;

static sigset_t all_masked;

int bn_mul_mont(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp, const BN_ULONG *np, const BN_ULONG *n0, int num)
	{
	int bn_mul_mont_fpu64(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp, const BN_ULONG *np, const BN_ULONG *n0, int num);
	int bn_mul_mont_int(BN_ULONG *rp, const BN_ULONG *ap, const BN_ULONG *bp, const BN_ULONG *np, const BN_ULONG *n0, int num);

	if (sizeof(size_t)==4)
		{
#if (defined(__APPLE__) && defined(__MACH__))
		if ((OPENSSL_ppccap_P&PPC_FPU64))
			return bn_mul_mont_fpu64(rp,ap,bp,np,n0,num);
#else
		/* boundary of 32 was experimentally determined on
		   Linux 2.6.22, might have to be adjusted on AIX... */
		if ((num>=32) && (OPENSSL_ppccap_P&PPC_FPU64))
			{
			sigset_t oset;
			int ret;

			sigprocmask(SIG_SETMASK,&all_masked,&oset);
			ret=bn_mul_mont_fpu64(rp,ap,bp,np,n0,num);
			sigprocmask(SIG_SETMASK,&oset,NULL);

			return ret;
			}
#endif
		}
	else if ((OPENSSL_ppccap_P&PPC_FPU64))
		/* this is a "must" on Power 6, but run-time detection
		 * is not implemented yet... */
		return bn_mul_mont_fpu64(rp,ap,bp,np,n0,num);

	return bn_mul_mont_int(rp,ap,bp,np,n0,num);
	}

static sigjmp_buf ill_jmp;
static void ill_handler (int sig) { siglongjmp(ill_jmp,sig); }

void OPENSSL_cpuid_setup(void)
	{
	char *e;

	sigfillset(&all_masked);
	sigdelset(&all_masked,SIGSEGV);
	sigdelset(&all_masked,SIGILL);

	if ((e=getenv("OPENSSL_ppccap")))
		{
		OPENSSL_ppccap_P=strtoul(e,NULL,0);
		return;
		}

	if (sizeof(size_t)==4)
		{
		struct sigaction	ill_oact,ill_act;
		sigset_t		oset;

		memset(&ill_act,0,sizeof(ill_act));
		ill_act.sa_handler = ill_handler;
		sigfillset(&ill_act.sa_mask);
		sigdelset(&ill_act.sa_mask,SIGILL);
		sigprocmask(SIG_SETMASK,&ill_act.sa_mask,&oset);
		sigaction (SIGILL,&ill_act,&ill_oact);
		if (sigsetjmp(ill_jmp,0) == 0)
			{
			OPENSSL_ppc64_probe();
			OPENSSL_ppccap_P |= PPC_FPU64;
			}
		else
			{
			OPENSSL_ppccap_P &= ~PPC_FPU64;
			}
		sigaction (SIGILL,&ill_oact,NULL);
		sigprocmask(SIG_SETMASK,&oset,NULL);
		}
	}
