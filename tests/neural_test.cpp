#include "wds/ai/core.hpp"
#include <iostream>
using namespace wds::ai;
struct Knowledge : intelligence::Foundation {
    int generated = 0, evaluated = 0;
    std::map<std::string, neural::Representation> cache;
    std::string name() const override { return "fixture-representation/1"; }
    bool supports_representations() const override { return true; }
    std::string propose(const intelligence::Request &, intelligence::Token) override { ++generated; throw std::runtime_error("mesh must never generate prose"); }
    neural::Representation represent(const std::string &root, const std::string &text, int) override {
        auto key = root + "/" + text;
        if (cache.count(key)) { auto r=cache.at(key);r.cached=true;return r; }
        ++evaluated;
        return cache[key] = {text.find("quiet") != std::string::npos ? std::vector<float>{0,1} : std::vector<float>{1,0}, name(), 5, false};
    }
};
int main(int argc, char **argv) {
    auto path = fs::temp_directory_path() / ("wds-neural-" + std::to_string(now_ms()));
    try {
        require(argc == 2, "need package"); winds::Package package(argv[1]); Knowledge knowledge;
        std::string unfinished;
        {
            chain::Store store(path); runtime::Engine engine(store, package, knowledge);engine.configure_language("legacy");
            int seq=0;std::vector<json> events;json last;
            auto emit=[&](const json &e){events.push_back(e);last=e;};
            auto cmd=[&](json m){m["schema"]=1;if(!m.contains("request_id"))m["request_id"]="n-"+std::to_string(++seq);return engine.command(m,emit);};
            auto config=[&](json changes){cmd({{"type","mesh.configure"},{"expected_version",store.mesh_settings.at("version")},{"settings",changes}});};
            auto node=[&](std::string root,std::string id,json extra=json::object()){
                json m={{"type","mesh.node.register"},{"root_id",root},{"node_id",id},{"purpose","music motif"},{"expected_version",0},{"web",false}};
                m.update(extra);cmd(m);
            };
            auto signal=[&](std::string root,std::string id,std::string text,json extra=json::object()){
                json m={{"type","mesh.signal"},{"root_id",root},{"node_id",id},{"text",text}};m.update(extra);cmd(m);return last.at("data").at("pulse_key").get<std::string>();
            };
            auto drain=[&]{for(int i=0;i<16;++i){auto ids=engine.dispatch_ready(now_ms()+100000,emit);if(ids.empty())break;for(auto &id:ids)engine.step(id,emit);}};
            auto reject=[&](json m){bool bad=false;try{cmd(m);}catch(const std::exception &){bad=true;}require(bad,"invalid mesh request accepted");};
            config({{"auto_register",false},{"refractory_ms",10},{"window_ms",1000}});
            for(auto root:{"a","b"})cmd({{"type","root.select"},{"root_id",root},{"purpose","music motif"}});
            for(auto id:{"one","two","three"})node("a",id);
            node("b","one");
            for(auto edge:{std::pair{"one","two"},std::pair{"two","three"},std::pair{"three","one"}})
                cmd({{"type","mesh.route.connect"},{"root_id","a"},{"from",edge.first},{"to",edge.second},{"expected_version",0}});
            reject({{"type","mesh.route.connect"},{"root_id","a"},{"from","one"},{"to","b/one"},{"expected_version",0}});
            cmd({{"type","memory.upsert"},{"packet",{{"schema",1},{"intent","memory.upsert"},{"root_id","a"},
                {"engram_id","motif"},{"packet_id","motif"},{"expected_version",0},{"expires_ms",now_ms()+60000},
                {"hops",0},{"text","music motif resolve to the home note"},{"source","human lesson"},{"claim_status","observed"}}}});
            drain();
            require(knowledge.generated==0 && knowledge.evaluated==0,"local recall invoked knowledge model");
            require(store.mesh_due.empty() && store.mesh_running.empty(),"cycle failed to settle");
            bool routed=false,cycle=false;
            for(const auto &e:events)if(e.at("type")=="mesh.impulse"){
                cycle=cycle||e.at("data").at("routes_suppressed").get<int>()>0;
                routed=routed||e.at("data").at("node_key")=="a/three";
            }
            require(routed&&cycle,"route traversal/cycle suppression missing");
            for(const auto &[key,p]:store.mesh_pulses)require(p.at("root_id")=="a","cross-root impulse leakage");
            json examples={{{"text","share the understanding"},{"action","propagate"}},{{"text","quiet and rest"},{"action","rest"}}};
            node("a","semantic",{{"examples",examples}});node("a","semantic2",{{"examples",examples}});
            const auto first=signal("a","semantic","new concept");
            const auto second=signal("a","semantic2","new concept");drain();
            require(knowledge.evaluated==3,"same scoped input was recomputed");
            require(store.mesh_pulses.at(second).at("decision").at("cache_hits")==3,"representation sharing not visible");
            require(store.mesh_pulses.at(first).at("decision").at("method")=="qwen-representation/exemplar-readout/1","semantic readout missing");
            require(knowledge.generated==0,"semantic readout generated text");
            node("b","semantic",{{"examples",examples}});signal("b","semantic","new concept");drain();
            require(knowledge.evaluated==6,"representation cache crossed a root boundary");
            auto active=store.active_packages;
            json feedback={{"type","mesh.feedback"},{"request_id","feedback-once"},{"root_id","a"},{"pulse_key",first},{"action","propagate"}};
            cmd(feedback);auto before=store.sequence();cmd(feedback);
            require(store.sequence()==before&&store.active_packages==active,"feedback retry wrote twice or activated a policy");
            cmd({{"type","mesh.training.export"},{"root_id","a"}});
            require(last.at("data").at("samples").size()==1,"training export omitted feedback");
            config({{"max_knowledge_calls",0}});
            const auto held=signal("a","semantic","another unknown");drain();
            require(store.mesh_pulses.at(held).at("status")=="held","knowledge budget did not hold work");
            require(knowledge.evaluated==6,"zero knowledge budget was ignored");
            config({{"web_enabled",true},{"max_web_queries",2}});
            node("b","web",{{"web",true}});
            auto webpulse=signal("b","web","orbital resonance");drain();
            require(store.mesh_pulses.at(webpulse).at("status")=="waiting_web","missing web escalation");
            const auto web=store.mesh_web.rbegin()->second;
            cmd({{"type","mesh.web.claim"},{"request_id","claim-web"},{"root_id","b"},{"web_key",web.at("key")}});
            reject({{"type","mesh.web.complete"},{"root_id","b"},{"web_key",web.at("key")},{"claim_id","wrong"},{"error","bad"}});
            cmd({{"type","mesh.web.complete"},{"root_id","b"},{"web_key",web.at("key")},{"claim_id","claim-web"},
                 {"results",{{{"url","https://en.wikipedia.org/wiki/Orbital_resonance"},{"title","Orbital resonance"},{"text","Orbital resonance involves periodic gravitational influence."}}}}});
            drain();
            require(store.mesh_pulses.at(webpulse).at("decision").at("action")=="recall","web evidence did not wake local understanding");
            bool provenance=false;for(const auto &[key,m]:store.engrams)if(m.contains("web_provenance")){
                provenance=true;require(m.at("claim_status")=="hypothetical","web excerpt was automatically trusted");
            }
            require(provenance,"web source provenance missing");
            config({{"max_impulses_per_window",1},{"web_enabled",false}});
            auto one=signal("a","one","music motif"),two=signal("a","two","music motif");
            auto ids=engine.dispatch_ready(now_ms()+200000,emit);
            require(ids.size()==1,"impulse window cap ignored");engine.step(ids.front(),emit);
            require(engine.next_wake(now_ms()).has_value(),"budget deferral has no wake condition");
            config({{"enabled",false}});require(engine.dispatch_ready(now_ms()+300000,emit).empty(),"paused mesh dispatched work");
            config({{"enabled",true},{"max_impulses_per_window",64}});drain();
            unfinished=signal("b","one","music motif");
            ids=engine.dispatch_ready(now_ms()+400000,emit);require(!ids.empty(),"restart fixture not dispatched");
        }
        {
            chain::Store store(path);runtime::Engine engine(store,package,knowledge);engine.configure_language("legacy");
            require(store.mesh_pulses.at(unfinished).at("status")=="queued","unfinished impulse lost on restart");
            auto sink=[](const json&){};
            for(auto &id:engine.dispatch_ready(now_ms()+500000,sink))engine.step(id,sink);
            require(store.mesh_pulses.at(unfinished).at("status")!="running","recovered impulse did not finish");
            require(store.mesh_samples.size()==1,"feedback did not survive restart");
        }
        fs::remove_all(path);
        std::cout<<"Local decisions, routed impulses, cycle suppression, scoped representation reuse, budget deferral, web provenance, feedback and restart passed.\n";
        return 0;
    } catch(const std::exception &e) { std::cerr<<e.what()<<" (fixture "<<path<<")\n";return 1; }
}
