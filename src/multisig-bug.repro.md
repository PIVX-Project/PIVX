## Expected functionality
### CLI
    $ createmultisig 2 '["pub1", "pub2"]'
     {
        "address": "msAddr",
        "redeemScript": "redeem"
     }
### GUI
file->multisignature creation
paste redeem into import box and click import
success message and instructions

##Repro steps
###CLI
#### 1. get two pubkeys from separate wallets
pub1: 0320d53a000000000000000000000000000000000000000000000000000c4b0d34
pub2: 03a924950000000000000000000000000000000000000000000000000005a6798b

#### 2. create multisig
createmultisig 2 '["0320d53a000000000000000000000000000000000000000000000000000c4b0d34", "03a924950000000000000000000000000000000000000000000000000005a6798b"]'

{
  "address": "8gLENYHjFcgXttU2wpmn6sMfVEsk51NYKV",
  "redeemScript":
}

#### 3. Open GUI
file->Multisignature Creation
paste redeemScript into import box click import
CRASH




2 XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
2 ["XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX", "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"]

