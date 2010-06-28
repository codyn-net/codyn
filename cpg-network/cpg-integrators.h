#ifndef __CPG_INTEGRATORS_H__
#define __CPG_INTEGRATORS_H__

#include <cpg-network/cpg-integrator-euler.h>
#include <cpg-network/cpg-integrator-predict-correct.h>
#include <cpg-network/cpg-integrator-runge-kutta.h>
#include <cpg-network/cpg-integrator-stub.h>

G_BEGIN_DECLS

GSList const *cpg_integrators_list (void);

void cpg_integrators_register (GType gtype);
void cpg_integrators_unregister (GType gtype);

GSList *cpg_integrators_create (void);

GType cpg_integrators_find (const gchar *id);

G_END_DECLS

#endif /* __CPG_INTEGRATORS_H__ */

