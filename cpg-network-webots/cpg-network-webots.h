#ifndef __CPG_NETWORK_WEBOTS_H__
#define __CPG_NETWORK_WEBOTS_H__

#include <cpg-network/cpg-network.h>

typedef struct _CpgNetworkWebots CpgNetworkWebots;

CpgNetworkWebots *cpg_network_webots_new(CpgNetwork *network);
void cpg_network_webots_free(CpgNetworkWebots *webots);

void cpg_network_webots_enable(CpgNetworkWebots *webots, unsigned ms);
void cpg_network_webots_disable(CpgNetworkWebots *webots);

void cpg_network_webots_update(CpgNetworkWebots *webots, float timestep);

#endif /* __CPG_NETWORK_WEBOTS_H__ */

