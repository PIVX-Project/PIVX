
#include "consensus/params.h"
#include "consensus/upgrades.h"

namespace Consensus {

bool Params::NetworkUpgradeActive(int nHeight, Consensus::UpgradeIndex idx) const
{
        return NetworkUpgradeState(nHeight, *this, idx) == UPGRADE_ACTIVE;
}

} // End consensus namespace