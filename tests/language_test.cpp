#include "wds/ai/core.hpp"
#include <iostream>
using namespace wds::ai;
int main(int argc,char **argv) {
    try {
        require(argc==2,"candidate path needed");winds::Package p(argv[1]);
        require(p.manifest.at("feature_schema")==cognition::language_schema,"bad language schema");
        for(int target=0;target<3;++target) {
            auto encode=[&](const std::string &) {
                neural::Representation r; r.identity="synthetic-language-fixture/1";r.values.resize(256);
                bool found=false;
                for(size_t i=0;i<256;++i) {auto h=hash64("language.projection/1:"+std::to_string(i));if(h%16==size_t(target)){r.values[i]=h&256?1.f:-1.f;found=true;break;}}
                require(found,"missing fixture projection lane");return r;
            };
            auto r=cognition::process(cognition::empty_state("home"),{{"op","unknown"},{"text","held-out phrase"}},"fixture",encode,&p);
            require(r.at("trace").at("operation")==cognition::language_actions()[target],"trained readout did not select intent");
            require(r.at("trace").at("encoder_calls")==1 && r.at("trace").at("generation_calls")==0,"unexpected model work");
        }
        bool rejected=false;
        try {cognition::process(cognition::empty_state("home"),{{"op","unknown"},{"text","phrase"}},"fixture",[](const std::string &){return neural::Representation{{1,0},"wrong-model",1,false};},&p);}catch(const std::exception &){rejected=true;}
        require(rejected,"mismatched representation accepted");
        auto offline=cognition::process(cognition::empty_state("home"),{{"op","help"}},"offline",{},&p);
        require(offline.at("handled")==true && offline.at("trace").at("encoder_calls")==0,"grammar depends on Qwen");
        std::cout<<"Learned language readout, model binding, no generation and encoder-independent grammar passed\n";
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
