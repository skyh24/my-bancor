
eosiocpp -o mybancor.wast mybancor.cpp 
eosiocpp -g mybancor.abi mybancor.cpp

cleos set contract myban1 ../mybancor

cleos create account eosio myban1 EOS6PTWVKBcpeDhAwV6csW2BdNSPJcyyAgaPGPWW71xte89GY2WXN EOS6PTWVKBcpeDhAwV6csW2BdNSPJcyyAgaPGPWW71xte89GY2WXN
cleos create account eosio creator EOS6PTWVKBcpeDhAwV6csW2BdNSPJcyyAgaPGPWW71xte89GY2WXN EOS6PTWVKBcpeDhAwV6csW2BdNSPJcyyAgaPGPWW71xte89GY2WXN

cleos  set account permission creator active '{"threshold": 1,"keys": [{"key": "EOS6PTWVKBcpeDhAwV6csW2BdNSPJcyyAgaPGPWW71xte89GY2WXN","weight": 1}],"accounts": [{"permission":{"actor":"myban1","permission":"eosio.code"},"weight":1}]}' owner -p creator
cleos  set account permission myban1 active '{"threshold": 1,"keys": [{"key": "EOS6PTWVKBcpeDhAwV6csW2BdNSPJcyyAgaPGPWW71xte89GY2WXN","weight": 1}],"accounts": [{"permission":{"actor":"myban1","permission":"eosio.code"},"weight":1}]}' owner -p myban1

cleos push action eosio.token issue '[myban1, "1000000.0000 EOS", 123]' -p eosio
cleos push action eosio.token issue '[user, "10000.0000 EOS", 123]' -p eosio
cleos push action myban1 issue '[tester, "10000.0000 CBT", 123]' -p creator
# new
cleos push action myban1 newtoken '["creator","42000.0000 EOS", "300000.0000 CBT", "1000000000.0000 CBT", 0.14]' -p myban1@active
# buy
# {
#   "rows": [{
#       "eos_supply": "42000.0000 EOS",
#       "token_supply": "300000.0000 CBT",
#       "cw": "0.14000000000000001",
#       "creator": "creator"
#     }
#   ],
#   "more": false
# }
cleos push action eosio.token transfer '[ "user", "myban1", "300.0000 EOS", "CBT-symbol" ]' -p user@active
# R:3.000000000000000e+09 * (1.000000000000000e+00 + T:3.000000000000000e+06 / C:4.230000000000000e+08) ^ F:1.400000000000000e-01 - 1.000000000000000e+00) = E: 2.969679096756828e+06 issue: 2969679
cleos push action eosio.token transfer '[ "user", "myban1", "700.0000 EOS", "CBT-symbol" ]' -p user@active
# sell
cleos push action myban1 sell '[tester, "990.0000 CBT"]' -p tester

# balance
balance myban1 user

cleos get table myban1 myban1 markets

# cleos push action myban1 issue '[user, "100000.0000 CBT", 123, 1]' -p creator
# cleos push action myban1 issue '[tester, "100000.0000 CBT", 123, 0]' -p creator

cleos get table myban1 myban1 markets
cleos get table myban1 user accounts
{
  "rows": [{
      "balance": "99909.9980 CBT",
      "lock_balance": "0.0000 CBT",
      "init_balance": "0.0000 CBT",
      "mtime": 1540700951,
      "type": 0
    }
  ],
  "more": false
}
cleos get table myban1 CBT stat
{
  "rows": [{
      "supply": "1000000000.0000 CBT",
      "max_supply": "1000000000.0000 CBT",
      "issuer": "creator"
    }
  ],
  "more": false
}