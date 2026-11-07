#include "wds/ai/core.hpp"
#include <iostream>
#include <fstream>
using namespace wds::ai;
struct NoGeneration final : intelligence::Foundation {
    int calls=0;
    std::string name() const override {return "generation-forbidden";}
    std::string propose(const intelligence::Request &,intelligence::Token) override {++calls;throw std::runtime_error("unexpected generation");}
};
struct Encoder final : intelligence::Foundation {
    int encoded=0;
    std::string name() const override{return "synthetic-encoder";}
    bool supports_representations() const override{return true;}
    std::string propose(const intelligence::Request &,intelligence::Token) override{throw std::runtime_error("generation forbidden");}
    neural::Representation represent(const std::string &,const std::string &,int) override{++encoded;return {{1,0,0},"fixture-encoder/1",2,false};}
};
int main(int argc,char **argv) {
    auto path=fs::temp_directory_path()/("wds-cognition-"+std::to_string(now_ms()));
    try {
        require(argc==2,"package needed");winds::Package package(argv[1]);NoGeneration brain;
        std::string checksum_before=package.digest;json outcome;
        {
            chain::Store store(path);runtime::Engine engine(store,package,brain);engine.configure_language("legacy");int counter=0;
            auto emit=[](const json &){};
            auto run=[&](std::string text,const std::string &root="home") {
                auto id=engine.command({{"schema",1},{"type","seed"},{"request_id","cog-"+std::to_string(++counter)},
                    {"root_id",root},{"text",text},{"allow_upkeep",false}},emit);
                while(engine.step(id,emit)){}
                require(store.instances.at(id).at("status")!="failed",store.instances.at(id).dump());
                return store.cognitive_results.at(id);
            };
            run("teach unlock door : locked=true,key=true,color=red => locked=false");
            run("teach unlock door : locked=true,key=true,color=blue => locked=false");
            run("teach unlock door : locked=true,key=false,color=red => failure");
            run("entity cave-gate door : locked=true,key=true,color=green");
            auto prediction=run("predict unlock cave-gate");
            auto claim=prediction.at("trace").at("claim");
            require(claim.at("status")=="predicted" && claim.at("predicted_after").at("locked")==false,"held-out color/entity failed to transfer");
            require(claim.at("trajectory").size()==4 && claim.at("neurons").size()<=16,"unbounded neuronal work");
            auto samples=store.cognitive_states.at("home").at("actions").at("unlock").at("samples").size();
            run("try unlock cave-gate");
            require(store.cognitive_states.at("home").at("actions").at("unlock").at("samples").size()==samples,"simulation reinforced itself");
            run("entity sealed-gate door : locked=true,key=false,color=gold");
            require(run("predict unlock sealed-gate").at("trace").at("claim").at("status")=="blocked","missing prerequisite did not block");
            run("entity unknown-gate door : locked=true");
            require(run("predict unlock unknown-gate").at("trace").at("claim").at("status")=="unknown","missing data treated as fact");
            // Newly discovered prerequisite changes prediction for an unseen
            // entity, without merely memorizing the failed entity name.
            run("entity jammed-gate door : locked=true,key=true,color=violet,jammed=true");
            require(run("predict unlock jammed-gate").at("trace").at("claim").at("status")=="predicted","baseline prediction missing");
            run("teach unlock door : locked=true,key=true,color=violet,jammed=true => failure");
            run("teach unlock door : locked=true,key=true,color=white,jammed=false => locked=false");
            run("entity new-jammed-gate door : locked=true,key=true,color=silver,jammed=true");
            require(run("predict unlock new-jammed-gate").at("trace").at("claim").at("status")=="blocked","new prerequisite did not transfer after correction");
            // Correct a formerly successful exact situation. Contradiction is
            // retained, and the next attempt must abstain instead of guessing.
            run("teach unlock door : locked=true,key=true,color=red => failure");
            require(run("predict unlock sealed-gate").at("trace").at("claim").at("status")=="conflict","contradictory feedback ignored");
            require(store.cognitive_states.at("home").at("actions").at("unlock").at("edges").at("key=true").at("eligibility")>0,"eligibility not learned");
            auto isolated=run("show knowledge","foreign");
            require(isolated.at("trace").at("claim").at("action_ids").empty(),"cross-root knowledge leaked");
            require(store.active_packages.empty() && brain.calls==0 && package.digest==checksum_before,"generation, policy mutation or auto activation occurred");
            require(store.engrams.begin()->second.at("arc_dataset")=="astroengrams","learning trace not stored in Arc astroengrams");
            outcome=store.cognitive_states.at("home");
            bool rejected=false;
            try {cognition::process(outcome,{{"op","entity"},{"entity","bad"},{"kind","door"},{"properties",{{"nested",json::array()}}}},"invalid");}catch(const std::exception &){rejected=true;}
            require(rejected,"invalid nested property accepted");
            // Duplicate text cannot enter both training and validation.
            auto encoder=[](const std::string &){return neural::Representation{{1,0,0},"fixture",2,false};};
            json sample={{"op","language.sample"},{"text","hello there"},{"label","greeting"},{"split","train"}};
            auto learned=cognition::process(outcome,sample,"sample",encoder);sample["split"]="validation";rejected=false;
            try {cognition::process(learned.at("state"),sample,"duplicate",encoder);}catch(const std::exception &){rejected=true;}
            require(rejected,"language split leakage allowed");
        }
        {
            chain::Store store(path);runtime::Engine engine(store,package,brain);engine.configure_language("legacy");
            require(store.cognitive_states.at("home")==outcome,"cognitive state changed on replay");
            auto independent=cognition::process(outcome,{{"op","inspect"},{"entity","cave-gate"}},"offline");
            require(independent.at("trace").at("claim").at("properties").at("locked")==false && brain.calls==0,"offline learned state unavailable");
        }
        // Crash precisely after durable cognitive learning but before the mesh
        // terminal event. Recovery reuses the stable pulse key, not a new episode.
        {
            chain::Store store(path/"mesh");runtime::Engine engine(store,package,brain);engine.configure_language("legacy");auto emit=[](const json &){};
            engine.command({{"schema",1},{"type","root.select"},{"request_id","root"},{"root_id","mesh"},{"purpose","operations"}},emit);
            engine.command({{"schema",1},{"type","mesh.signal"},{"request_id","learn"},{"root_id","mesh"},{"node_id","mesh"},
                {"text","teach open box : closed=true => closed=false"},{"response",true}},emit);
            for(auto id:engine.dispatch_ready(now_ms()+1000,emit))engine.step(id,emit);
        }
        fs::create_directories(path/"crash");
        {std::ifstream input(path/"mesh"/"events.jsonl");std::ofstream output(path/"crash"/"events.jsonl");std::string line;bool found=false;
         while(std::getline(input,line)){output<<line<<'\n';if(json::parse(line).at("type")=="cognition.committed"){found=true;break;}}
         require(found,"missing crash injection point");}
        {
            chain::Store store(path/"crash");runtime::Engine engine(store,package,brain);engine.configure_language("legacy");auto emit=[](const json &){};
            for(int n=0;n<4;++n)for(auto id:engine.dispatch_ready(now_ms()+1000,emit))engine.step(id,emit);
            require(store.cognitive_states.at("mesh").at("actions").at("open").at("samples").size()==1,"crash replay duplicated learning");
            require(store.cognitive_results.size()==1 && store.mesh_running.empty(),"interrupted cognitive impulse did not settle");
            require(store.assemblies.begin()->second.at("status")=="completed","replayed cognitive reply missing");
        }
        {
            Encoder encoder;chain::Store store(path/"budget");runtime::Engine engine(store,package,encoder);engine.configure_language("legacy");auto emit=[](const json &){};
            engine.command({{"schema",1},{"type","root.select"},{"request_id","root"},{"root_id","budget"},{"purpose","language"}},emit);
            engine.command({{"schema",1},{"type","mesh.configure"},{"request_id","budget-config"},{"expected_version",0},
                {"settings",{{"max_knowledge_calls",1},{"window_ms",1000},{"refractory_ms",10}}}},emit);
            for(int i=0;i<2;++i)engine.command({{"schema",1},{"type","cognition.run"},{"request_id","sample-"+std::to_string(i)},{"root_id","budget"},
                {"operation",{{"op","language.sample"},{"text","phrase "+std::to_string(i)},{"label","greeting"},{"split","train"}}}},emit);
            const auto time=now_ms();for(auto id:engine.dispatch_ready(time,emit))engine.step(id,emit);
            for(auto id:engine.dispatch_ready(time+100,emit))engine.step(id,emit);
            require(encoder.encoded==1 && !store.mesh_due.empty(),"language training bypassed shared encoder budget");
            for(auto id:engine.dispatch_ready(time+1100,emit))engine.step(id,emit);
            require(encoder.encoded==2 && store.cognitive_states.at("budget").at("language_samples").size()==2,"deferred sample failed to resume at budget deadline");
        }
        fs::remove_all(path);std::cout<<"Native learning, held-out transfer, correction, scope, sparse bounds, ABI, no generation and restart passed\n";
    }catch(const std::exception &e){std::cerr<<e.what()<<" fixture="<<path<<'\n';return 1;}
}
