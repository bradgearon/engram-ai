#include "wds/ai/core.hpp"
#include <iostream>
using namespace wds::ai;
int main(int argc,char **argv) {
    try {
        require(argc==5,"need synthetic model, pack, mode and GPU layers");
        auto model=intelligence::create(argv[1],std::stoi(argv[4]),argv[2]);
        const std::string mode=argv[3];
        if(mode=="limit") {
            intelligence::Request r;r.seed="fixture";r.max_output_tokens=3;
            try { model->propose(r,[](const std::string &){}); }
            catch(const intelligence::GenerationLimit &e) {
                require(e.partial=="xxx","partial generation lost");
                std::cout<<"explicit generation limit preserves partial output\n";return 0;
            }
            throw std::runtime_error("generation limit was silently accepted");
        }
        auto first=model->represent("root-a","motif",32);
        auto same=model->represent("root-a","motif",32);
        auto other=model->represent("root-b","motif",32);
        require(first.values.size()==4 && first.input_tokens==6,"fixture representation lost");
        require(!first.cached && same.cached && !other.cached,"cache crossed roots");
        require(first.values==other.values,"context was not reset");
        require(first.identity.find("llama.cpp/b9870:lib-fnv64:")==0,"pack provenance missing");
        bool limit=false;try{model->represent("root-a",std::string(60,'x'),8);}catch(const std::exception &){limit=true;}
        require(limit,"input budget silently truncated");
        std::cout<<"synthetic ABI fixture: batch layout, context reset, scope cache, budgets and provenance passed\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
