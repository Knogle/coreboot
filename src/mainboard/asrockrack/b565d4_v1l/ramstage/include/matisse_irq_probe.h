/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef B565D4_MATISSE_IRQ_PROBE_H
#define B565D4_MATISSE_IRQ_PROBE_H

void b565d4_matisse_irq_probe(unsigned int phase);
void b565d4_matisse_vendor_msr_trial(unsigned int phase);
void b565d4_matisse_vendor_msr_cpu_trial(void);

#endif
