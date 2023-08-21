hemis Core version v5.0.1 is now available from: https://github.com/hemis-project/hemis/releases
Please report bugs using the issue tracker at github: https://github.com/hemis-project/hemis/issues

Mandatory Update
----

This is an emergency update.
A vulnerability with the budgeting system was discovered and has been fixed (PR 2137).
This release also contains a protocol upgrade (to 70921).
Users will need to update their clients before enforcement of this update goes into effect.
Update enforcement is currently scheduled to go into effect at the same block height that was scheduled for v5.0.0: 2700500

Gamemasters will need to be restarted once both the gamemaster daemon and the controller wallet have been upgraded.

Important Note
----

No user's funds are at risk, but users facing a checkpoint error after upgrade need to resync the chain.