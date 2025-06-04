#ifndef _SMP_X86_H_
#define _SMP_X86_H_

void start_aps(void);
void send_ipi(int cpu, unsigned char vector);

#endif
