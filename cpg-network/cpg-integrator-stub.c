#include "cpg-integrator-stub.h"
#include "cpg-network.h"

#define CPG_INTEGRATOR_STUB_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE((object), CPG_TYPE_INTEGRATOR_STUB, CpgIntegratorStubPrivate))

/*struct _CpgIntegratorStubPrivate
{
};*/

G_DEFINE_TYPE (CpgIntegratorStub, cpg_integrator_stub, CPG_TYPE_INTEGRATOR)

static void
cpg_integrator_stub_finalize (GObject *object)
{
	G_OBJECT_CLASS (cpg_integrator_stub_parent_class)->finalize (object);
}

static void
stub_update (GSList const *properties)
{
	while (properties)
	{
		CpgProperty *property = properties->data;

		cpg_property_set_value (property,
		                        cpg_property_get_update (property));

		properties = g_slist_next (properties);
	}
}

void
cpg_integrator_stub_update (CpgIntegratorStub *stub,
                            gdouble            t,
                            gdouble            dt,
                            gboolean           integrate)
{
	g_return_if_fail (CPG_IS_INTEGRATOR_STUB (stub));

	/* First restore network state from the 'update' value of the cpg states */
	CpgIntegratorState *state = cpg_integrator_get_state (CPG_INTEGRATOR (stub));

	stub_update (cpg_integrator_state_integrated_properties (state));
	stub_update (cpg_integrator_state_direct_properties (state));

	/* Then calculate the differential equations in the network like any other
	   integrator */
	if (integrate)
	{
		cpg_integrator_evaluate (CPG_INTEGRATOR (stub), t, dt);
	}
}

static gdouble
cpg_integrator_stub_step_impl (CpgIntegrator *integrator,
                               gdouble        t,
                               gdouble        timestep)
{
	cpg_integrator_stub_update (CPG_INTEGRATOR_STUB (integrator),
	                            t,
	                            timestep,
	                            FALSE);

	/* Chain up to emit 'step' */
	CPG_INTEGRATOR_CLASS (cpg_integrator_stub_parent_class)->step (integrator,
	                                                               t,
	                                                               timestep);

	return timestep;
}

static gchar const *
cpg_integrator_stub_get_name_impl (CpgIntegrator *integrator)
{
	return "Stub";
}

static void
cpg_integrator_stub_class_init (CpgIntegratorStubClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CpgIntegratorClass *integrator_class = CPG_INTEGRATOR_CLASS (klass);

	object_class->finalize = cpg_integrator_stub_finalize;

	integrator_class->step = cpg_integrator_stub_step_impl;
	integrator_class->get_name = cpg_integrator_stub_get_name_impl;

	integrator_class->integrator_id = "stub";

	//g_type_class_add_private (object_class, sizeof(CpgIntegratorStubPrivate));
}

static void
cpg_integrator_stub_init (CpgIntegratorStub *self)
{
	//self->priv = CPG_INTEGRATOR_STUB_GET_PRIVATE (self);
}

/**
 * cpg_integrator_stub_new:
 * 
 * Create a new Stub integrator.
 *
 * Returns: A #CpgIntegratorStub
 *
 **/
CpgIntegratorStub *
cpg_integrator_stub_new (void)
{
	return g_object_new (CPG_TYPE_INTEGRATOR_STUB, NULL);
}
