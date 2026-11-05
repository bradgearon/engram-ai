#include "wds/ai/core.hpp"
#include <future>
#include <fstream>
#include <iostream>
using namespace wds::ai;
namespace {
void rejected(std::function<void()> fn,const std::string &label){bool fail=false;try{fn();}catch(const std::exception&){fail=true;}require(fail,label);}
json fixture() {
    return json::parse(R"({"schema":1,"id":"motif","domain":"music",
      "artifact":{"notes":[{"pitch":36,"duration":1},{"pitch":38,"duration":1},{"pitch":40,"duration":1},{"pitch":38,"duration":1}]},
      "requirements":{"pitch_min":60,"pitch_max":84,"target_ticks":8,"home_pc":0,"return_home":true}})");
}
class Blocked final:public intelligence::Foundation {
  public:
    std::promise<void> entered,release;std::shared_future<void> released=release.get_future().share();
    std::string name()const override{return "blocked-fixture/1";}
    std::string propose(const intelligence::Request &,intelligence::Token token)override{
        entered.set_value();released.wait();token("old generation");return "A deliberately delayed proposal for cancellation.";
    }
};
async::Task<async::Unit> advance(runtime::Engine &e,std::string id,async::Executor &cpu,async::Executor &owner,std::promise<void> &done){
    co_await e.advance(id,[](const json&){},cpu,owner);done.set_value();co_return async::Unit{};
}
}
int main(int argc,char **argv){
    auto dir=fs::temp_directory_path()/("wds-workshop-"+std::to_string(now_ms()));
    try{
        require(argc==2,"package argument");winds::Package baseline(argv[1]);
        understanding::Signal signal;signal.root_id="scope";
        auto impulse=understanding::Neuron().receive(signal,baseline);
        require(impulse.focus.candidates.empty(),"empty signal invented memory");
        signal.engrams={{{"root_id","foreign"}}};
        rejected([&]{understanding::Neuron().receive(signal,baseline);},"neuron accepted cross-scope signal");
        auto t=fixture();require(tasks::checks(t).at("pitch_range")==0,"fixture must violate range");
        t=tasks::apply(t,0);t=tasks::apply(t,1);t=tasks::apply(t,2);
        for(auto &kv:tasks::checks(t))require(kv.second==1,"repair did not satisfy constraint");
        auto bad=t;bad["artifact"]["notes"][0]["duration"]=-1;rejected([&]{tasks::validate(bad);},"invalid artifact accepted");
        fs::create_directories(dir/"packages"/"candidate");fs::copy_file(fs::path(argv[1])/"policy.wind",dir/"packages"/"candidate"/"policy.wind");
        auto manifest=baseline.manifest;manifest["id"]="wds.ai.winds.music";manifest["policy_version"]=2;manifest["action_domain"]="music";
        {std::ofstream out(dir/"packages"/"candidate"/"manifest.json");out<<manifest;}
        std::string candidate_key;
        {
            chain::Store store(dir/"data");auto foundation=intelligence::create("",0);
            runtime::Engine engine(store,baseline,*foundation,{}, {},"wds.ai.heart.composite/1",dir/"packages");
            auto sink=[](const json&){};json ack;
            engine.command({{"schema",1},{"type","package.register"},{"request_id","register"},{"path","candidate"},{"checksum",baseline.digest}},[&](const json &e){if(e.at("type")=="package.available")candidate_key=e.at("data").at("key");});
            require(store.active_packages.empty(),"registration activated a candidate");
            auto id=engine.command({{"schema",1},{"type","task.run"},{"request_id","task"},{"root_id","music"},{"task",t},{"max_steps",1}},sink);
            auto initial=store.instances.at(id).at("package_key");
            json activation={{"schema",1},{"type","wind.activate"},{"request_id","activate"},{"domain","music"},{"package_key",candidate_key},{"expected_active_key",initial}};
            rejected([&]{engine.command(activation,sink);},"activation interrupted an episode");
            engine.command({{"schema",1},{"type","pause"},{"request_id","pause"},{"instance_id",id}},sink);
            rejected([&]{engine.command(activation,sink);},"paused episode treated as boundary");
            engine.command({{"schema",1},{"type","cancel"},{"request_id","cancel"},{"instance_id",id}},sink);
            engine.command(activation,[&](const json &e){ack=e;});auto seq=store.sequence();
            engine.command(activation,[&](const json &e){require(e==ack,"activation retry differs");});require(store.sequence()==seq,"activation retry wrote twice");
            auto next=engine.command({{"schema",1},{"type","task.run"},{"request_id","after-activation"},{"root_id","music"},{"task",t},{"max_steps",1}},sink);
            require(store.instances.at(next).at("package_key")==candidate_key,"new episode did not pin candidate");engine.step(next,sink);
            require(store.instances.at(id).at("package_key")==initial,"old episode pin mutated");
            activation["request_id"]="stale";rejected([&]{engine.command(activation,sink);},"stale activation accepted");
        }
        {
            chain::Store store(dir/"data");auto f=intelligence::create("",0);runtime::Engine e(store,baseline,*f,{}, {},"wds.ai.heart.composite/1",dir/"packages");
            require(store.active_packages.at("music").at("key")==candidate_key,"activation lost after replay");
        }
        {
            chain::Store store(dir/"async");Blocked foundation;runtime::Engine engine(store,baseline,foundation);
            async::Executor owner(128),cpu(16);std::promise<void> finished,paused;
            std::string id;
            owner.post([&]{id=engine.command({{"schema",1},{"type","seed"},{"request_id","blocked"},{"root_id","async"},{"text","test responsive controls"}},[](const json&){});
                async::launch(advance(engine,id,cpu,owner,finished),[&](std::exception_ptr e){finished.set_exception(e);});});
            auto entered=foundation.entered.get_future();require(entered.wait_for(std::chrono::seconds(5))==std::future_status::ready,"compute not entered");
            owner.post([&]{engine.command({{"schema",1},{"type","cancel"},{"request_id","cancel-blocked"},{"instance_id",id}},[](const json&){});paused.set_value();});
            auto pause=paused.get_future();bool responsive=pause.wait_for(std::chrono::seconds(2))==std::future_status::ready;
            foundation.release.set_value();auto end=finished.get_future();require(end.wait_for(std::chrono::seconds(5))==std::future_status::ready,"coroutine did not finish");end.get();
            cpu.shutdown();owner.shutdown();require(responsive,"coordinator blocked on inference");require(store.instances.at(id).at("status")=="cancelled","late result overwrote cancellation");
        }
        fs::remove_all(dir);std::cout<<"task actions, explicit activation, ABI and coroutine cancellation passed\n";return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';fs::remove_all(dir);return 1;}
}
