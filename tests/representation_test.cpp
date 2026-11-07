#include "wds/ai/core.hpp"
#include <iostream>
#include <cmath>
using namespace wds::ai;
int main(int argc,char **argv) {
    try {
        require(argc==2,"need GGUF fixture");
        auto model=intelligence::create(argv[1],0);
        require(model->supports_representations(),"Qwen architecture not recognized");
        auto first=model->represent("one","music motif",128);
        auto again=model->represent("one","music motif",128);
        auto other=model->represent("two","music motif",128);
        require(!first.values.empty() && first.input_tokens>0,"missing native hidden state");
        require(!first.cached && again.cached && !other.cached,"native cache lost scope identity");
        require(first.values==again.values && first.identity==other.identity,"native representation not reusable");
        require(std::abs(neural::cosine(first.values,other.values)-1)<0.001,"context reuse leaked prior state");
        bool rejected=false;try{model->represent("one",std::string(2000,'a'),8);}catch(const std::exception&){rejected=true;}
        require(rejected,"oversized representation was silently truncated");
        std::cout<<"Native Qwen graph, pooled hidden state, scoped cache, context reset and explicit token-limit rejection passed. No sampled tokens.\n";
        return 0;
    }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
