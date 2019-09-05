#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/action.hpp>
#include <eosiolib/symbol.hpp>
#include <eosiolib/singleton.hpp>

#include <cmath>

using namespace eosio;
using namespace std;

using eosio::asset;
using eosio::symbol_type;
typedef double real_type;

#define SYM S(4, CBT)
#define EOS S(4, EOS)

// token 发EOS和CBT币
#define TOKEN N(eosio.token)
#define CREATOR N(creator)

const uint32_t TIME_EXP = 3600ul * 24 * 30;

// creator mywallet 必须权限
class mybancor : public eosio::contract {
  public:
    using contract::contract;
    mybancor(account_name self) : contract(self) {}

    /// @abi action
    void hi(account_name user) {
      print( "Hello, ", name{user} );
    }

    /// @abi action
    void newtoken(account_name creator, asset eos_supply, asset token_supply, asset maximum_supply, double cw) { // core_symbol
      print(" >>>newtoken creator: ", creator, " eos_supply:", eos_supply, " token_supply:", token_supply);
      require_auth(_self);

      eosio_assert(eos_supply.amount > 0, "invalid eos_supply amount");
      eosio_assert(eos_supply.symbol == EOS, "eos_supply symbol only support EOS");
      eosio_assert(token_supply.amount > 0, "invalid token_supply amount");
      eosio_assert(token_supply.symbol.is_valid(), "invalid token_supply symbol");
      eosio_assert(token_supply.symbol != EOS, "token_supply symbol cannot be EOS");

      markets _market(_self, _self);
      auto itr = _market.find(token_supply.symbol);

      if( itr == _market.end() ) {
        SEND_INLINE_ACTION(*this, create, {_self, N(active)}, {creator, maximum_supply});
        SEND_INLINE_ACTION(*this, issue, {creator, N(active)}, { _self, token_supply, string("create cbt"), 0}); // 其实也可以不在self转token的
        print(" ...transfer token to _self");

        _market.emplace( _self, [&]( auto& m ) {
            m.eos_supply.amount = eos_supply.amount; // 貌似没用, 可以去掉core_symbol么
            m.eos_supply.symbol = eos_supply.symbol;
            m.token_supply.amount = token_supply.amount;
            m.token_supply.symbol = token_supply.symbol;
            m.cw = cw;
            
            m.creator = creator;
        });

        print(" ...newtoken create emplace:", SYM);
      } else {
        // add token 
        eosio_assert(creator == itr->creator, "creator must be the same");
        _market.modify( itr, 0, [&]( auto& m ) {
            m.eos_supply.amount = eos_supply.amount; // 貌似没用, 可以去掉core_symbol么
            m.eos_supply.symbol = eos_supply.symbol;
            m.token_supply.amount = token_supply.amount;
            m.token_supply.symbol = token_supply.symbol;
            m.cw = cw;
            
            m.creator = creator;
        });

        print(" ...newtoken exist modify:", SYM);
      }
    }
    
    /// @abi action
    void buy(account_name from, account_name to, asset quantity, string memo) {
      print(" >>>buy from:", name{from}, " to:", name{to}, " quantity:", quantity, " memo:", memo);
      if ((from == _self) || (to != _self) || (from == N(eosio))) {
        return;
      }

      string sym_str = "";
      if (memo.find("-symbol") != string::npos) {
          auto pos = memo.find("-symbol");
          sym_str = memo.substr(0, pos);
          print(sym_str);
      }
      eosio_assert(sym_str.length() > 0, "memo has to be CBT-symbol");
      symbol_type sym(string_to_symbol(4, sym_str.c_str()));

      eosio_assert(quantity.amount > 0, "must purchase a positive amount" );
      eosio_assert(quantity.symbol == S(4, EOS), "eos_quant symbol must be EOS");
      eosio_assert(quantity.is_valid(), "invalid token_symbol");

      markets _market(_self, _self);
      auto itr = _market.find(sym);

      if( itr != _market.end() ) {
        asset token_out;
        _market.modify( itr, 0, [&]( auto& es ) {
            token_out = es.convert_to_exchange( quantity );
        });
        print(" ...token_out: ", token_out);
        eosio_assert(token_out.amount > 0, "token_out must a positive amount" );
       
        string token_memo =name{from}.to_string() + " buy EOS:" + to_string(quantity.amount) + " => CBT:" + to_string(token_out.amount); 
        print(token_memo);

        SEND_INLINE_ACTION(*this, transfer, {_self, N(active)}, {_self, from, token_out, token_memo});
        print(" ...transfer CBT: ", token_out, " to:", name{from});

      } else {
        eosio_assert(itr == _market.end(), "token not exist" );
      }
    }

    /// @abi action
    void sell(account_name from, asset token_quant) {
      print(" >>>sell from:", name{from}, " eos_quant:", token_quant);
      require_auth( from );

      eosio_assert(token_quant.amount > 0, "must purchase a positive amount" );
      eosio_assert(token_quant.symbol != S(4, EOS), "eos_quant symbol must not be EOS");
      eosio_assert(token_quant.is_valid(), "invalid token_symbol");

      markets _market(_self, _self);
      auto itr = _market.find(token_quant.symbol);

      if( itr != _market.end() ) {
        asset eos_out;
        _market.modify( itr, 0, [&]( auto& es ) {
            eos_out = es.convert_from_exchange( token_quant );
        });
        print(" ...eos_out: ", eos_out);
        eosio_assert(eos_out.amount > 0, "eos_out must a positive amount" );

        string token_memo = name{from}.to_string() + " sell CBT:" + to_string(token_quant.amount) + " => EOS:" + to_string(eos_out.amount);
        print(token_memo);
        SEND_INLINE_ACTION(*this, transfer, {from, N(active)}, {from, _self, token_quant, token_memo});
        print(" ...transfer EOS: ", eos_out, " to:", name{from});

        //交易所账户转出EOS
        action(
          permission_level{ _self, N(active) },
          TOKEN, N(transfer),
          std::make_tuple(_self, from, eos_out, std::string(name{from}.to_string() + "-out"))
        ).send();
      } else {
        eosio_assert(itr == _market.end(), "no CBTCORE");
      }
    }

    /// @abi action
    void create( account_name issuer,
                 asset        maximum_supply ) {
        require_auth( _self );

        auto sym = maximum_supply.symbol;
        eosio_assert( sym.is_valid(), "invalid symbol name" );
        eosio_assert( maximum_supply.is_valid(), "invalid supply");
        eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

        stats statstable( _self, sym.name() );
        auto existing = statstable.find( sym.name() );
        eosio_assert( existing == statstable.end(), "token with symbol already exists" );

        statstable.emplace( _self, [&]( auto& s ) {
          s.supply.symbol = maximum_supply.symbol;
          s.max_supply    = maximum_supply;
          s.issuer        = issuer;
        });

        print(">>>token::create ", maximum_supply);
    }

    /// @abi action
    void issue( account_name to, asset quantity, string memo, uint64_t type = 0 ) {
        print(">>>token::issue ", quantity);
        auto sym = quantity.symbol;
        eosio_assert( sym.is_valid(), "invalid symbol name" );
        eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

        auto sym_name = sym.name();
        print(" ...sym_name: ", sym_name);

        stats statstable( _self, sym_name );
        auto existing = statstable.find( sym_name );
        eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
        const auto& st = *existing;

        print(" ...issuer: ", st.issuer);
        require_auth( st.issuer );
        eosio_assert( quantity.is_valid(), "invalid quantity" );
        eosio_assert( quantity.amount > 0, "must issue positive quantity" );
        eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
        eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

        statstable.modify( st, 0, [&]( auto& s ) {
          s.supply += quantity;
        });

        require_recipient( to );
        if (type == 0) {
            add_balance(to, quantity, st.issuer);
        } else {
            add_lock_balance(to, quantity, st.issuer, type);
        }

        // add_balance( st.issuer, quantity, st.issuer );

        // if( to != st.issuer ) {
        //    SEND_INLINE_ACTION( *this, transfer, {st.issuer,N(active)}, {st.issuer, to, quantity, memo} );
        // }
    }

    /// @abi action
    void transfer( account_name from,
                      account_name to,
                      asset        quantity,
                      string       memo ) {
        print(">>>token::transfer ", quantity);
        eosio_assert( from != to, "cannot transfer to self" );
        require_auth( from );
        eosio_assert( is_account( to ), "to account does not exist");
        auto sym = quantity.symbol.name();
        stats statstable( _self, sym );
        const auto& st = statstable.get( sym );

        require_recipient( from );
        require_recipient( to );

        eosio_assert( quantity.is_valid(), "invalid quantity" );
        eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
        eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
        eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

        sub_balance( from, quantity );
        add_balance( to, quantity, from );
    }


    private:
      void try_unlock(account_name owner, symbol_type sym);
      void add_lock_balance(account_name owner, asset value, account_name ram_player, uint64_t type);
      void sub_balance( account_name owner, asset value );
      void add_balance( account_name owner, asset value, account_name ram_payer );

      // @abi table accounts i64
      struct account {
        asset    balance;
        asset    lock_balance;
        asset    init_balance;
        uint64_t mtime;
        uint64_t type; // 0, 1, 2, 3

        uint64_t primary_key()const { return balance.symbol.name(); }
        EOSLIB_SERIALIZE( account, (balance)(lock_balance)(init_balance)(mtime)(type) )
      };
  
      // @abi table stat i64
      struct curr_stat {
        asset          supply;
        asset          max_supply;
        account_name   issuer;

        uint64_t primary_key()const { return supply.symbol.name(); }
        EOSLIB_SERIALIZE( curr_stat, (supply)(max_supply)(issuer) )
      };

      typedef eosio::multi_index<N(accounts), account> accounts;
      typedef eosio::multi_index<N(stat), curr_stat> stats;

      // @abi table markets i64
      struct exchange_state {
          asset    eos_supply;
          asset    token_supply;
         
          real_type cw;
          account_name  creator;
          uint64_t primary_key()const { return token_supply.symbol;}

          asset convert_to_exchange( asset in ){
            print(" ...convert_to_exchange in:", in);
            eosio_assert( in.symbol== eos_supply.symbol, "unexpected asset symbol input" );

            real_type R(token_supply.amount);
            real_type C(eos_supply.amount);// + in.amount);
            real_type F(cw);
            real_type T(in.amount);
            real_type ONE(1.0);

            real_type E = -R * (ONE - std::pow( ONE + T / C, F) );
            int64_t issued = int64_t(E);
            print( " ...func R:", R, " * (", ONE,  " + T:", T, " / C:", C, ") ^ F:", F, " - ", ONE, ")");
            print( " = E: ", E, " issue: ", issued);

            token_supply.amount += issued;
            eos_supply.amount += in.amount;

            return asset( issued, token_supply.symbol );
          }


          asset convert_from_exchange( asset in ){
            print(" ...convert_from_exchange in:", in);
            eosio_assert( in.symbol == token_supply.symbol, "unexpected asset symbol input" );

            real_type R(token_supply.amount);// - in.amount);
            real_type C(eos_supply.amount);
            real_type F(1.0/cw);
            real_type E(-in.amount);
            real_type ONE(1.0);
            
            real_type T = C * (std::pow( ONE + E/R, F) - ONE);
            int64_t out = int64_t(T);
            print( " ...func C:", C, " * (", ONE,  " + E:", E, " / R:", R, ") ^ F:", F, " - ", ONE, ")");
            print( " = T: ", T, " out: ", out);

            out = std::abs(out);
            eosio_assert( token_supply.amount > in.amount, "token_supply not enough" );
            eosio_assert( eos_supply.amount > out, "eos_supply not enough" );
            token_supply.amount -= in.amount;
            eos_supply.amount -= out;

            return asset( out, eos_supply.symbol );
          }
          

          EOSLIB_SERIALIZE( exchange_state, (eos_supply)(token_supply)(cw)(creator) )
      };

      typedef eosio::multi_index<N(markets), exchange_state> markets;
};

void mybancor::try_unlock( account_name owner, symbol_type sym ) {
    print(" ...try_unlock ", sym);
    accounts lock_acnts( _self, owner );
    const auto& acc = lock_acnts.get( sym.name(), "no balance object found" );
    print(" lock_balance:", acc.lock_balance);

    if (acc.lock_balance.amount > 0) {
        uint64_t time_exp = 0;
        // type = 1 分5期, type = 2 分10期
        time_exp = now() - acc.mtime;
        print(" time_exp:", time_exp);
        if (time_exp > TIME_EXP) {
            uint64_t times = time_exp / TIME_EXP;
            print(" times:", times);
            if (acc.type == 1) {
                lock_acnts.modify( acc, owner, [&]( auto& a ) {
                    uint64_t amt = a.lock_balance.amount - a.init_balance.amount * times / (uint64_t)5;
                    print(" amount:", amt, "=", a.lock_balance.amount, "-", a.init_balance.amount * times / (uint64_t)5);
                    a.lock_balance.amount = (amt > 0 ? amt : 0);
                    a.mtime = now();
                });
            } else if (acc.type == 2) {
                lock_acnts.modify( acc, owner, [&]( auto& a ) {
                    uint64_t amt = a.lock_balance.amount - a.init_balance.amount * times / (uint64_t)10;
                    print(" amount:", amt, "=", a.lock_balance.amount, "-", a.init_balance.amount * times / (uint64_t)10);

                    a.lock_balance.amount = (amt > 0 ? amt : 0);
                    a.mtime = now();
                });
            }
        }
    } else {
        print(" lock_balance is 0" );
    }
}

void mybancor::add_lock_balance(account_name owner, asset value, account_name ram_payer, uint64_t type ) {
    print(" ...add_lock_balance: ", type);
    accounts to_acnts( _self, owner );
    if (type != 0) {
        // 1, 基石轮, 2.天使论 
        auto to = to_acnts.find( value.symbol.name() );
        if( to == to_acnts.end() ) {
            to_acnts.emplace( ram_payer, [&]( auto& a ){
                a.balance = value;
                a.lock_balance = value;
                a.init_balance = value;
                a.mtime = now();
                a.type = type;
            });
        } else {
            // 应该不会有之前的
            to_acnts.modify( to, 0, [&]( auto& a ) {
                a.balance += value;
                a.lock_balance += value;
                a.init_balance += value;
                a.mtime = now();
                a.type = type;
            });
        }
    } 
}

void mybancor::add_balance( account_name owner, asset value, account_name ram_payer) {
   print(" ...add_balance ", value);
   accounts to_acnts( _self, owner );
   
   auto to = to_acnts.find( value.symbol.name() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
        a.lock_balance = asset(0, value.symbol);
        a.init_balance = asset(0, value.symbol);
        a.mtime = now();
        a.type = 0;
      });
   } else {
      to_acnts.modify( to, 0, [&]( auto& a ) {
        a.balance += value;
      });
   }
}


void mybancor::sub_balance( account_name owner, asset value ) {
   print(" ...sub_balance ", value);
   try_unlock(owner, value.symbol);
   accounts from_acnts( _self, owner );

   const auto& from = from_acnts.get( value.symbol.name(), "no balance object found" );
   eosio_assert( (from.balance.amount-from.lock_balance.amount) >= value.amount, "overdrawn balance" );

   if( from.balance.amount == value.amount ) {
      from_acnts.erase( from );
   } else {
      from_acnts.modify( from, owner, [&]( auto& a ) {
          a.balance -= value;
      });
   }
}



extern "C" {
    void apply( uint64_t receiver, uint64_t code, uint64_t action ) {
        mybancor thiscontract(receiver);

        if((code == N(eosio.token)) && (action == N(transfer))) {
            execute_action(&thiscontract, &mybancor::buy);
            return;
        }

        if (code != receiver) return;

        switch (action) {
            EOSIO_API(mybancor, (create)(issue)(transfer)(sell)(newtoken))
        };
        eosio_exit(0);
    }
}

// EOSIO_ABI( mybancor, (hi)(newToken)(buy)(sell)(create)(issue)(transfer) )


