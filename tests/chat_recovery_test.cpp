#include "wds/ai/core.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace wds::ai;
struct Brain final : intelligence::Foundation {
    std::string name() const override { return "chat-test"; }
    std::string propose(const intelligence::Request &r, intelligence::Token) override {
        if (r.seed == "fail") throw intelligence::GenerationLimit("test generation bound", "partial retained");
        return "A thoughtful music composition response to " + r.seed;
    }
};
std::string bytes(const fs::path &p) { std::ifstream f(p,std::ios::binary); return {std::istreambuf_iterator<char>(f),{}}; }
int main(int argc, char **argv) {
    auto dir = fs::temp_directory_path() / ("wds-chat-recovery-" + std::to_string(now_ms()));
    try {
        require(argc==2,"package required"); winds::Package package(argv[1]); Brain brain;
        uint64_t completion=0; json stale;
        {
            chain::Store store(dir); runtime::Engine engine(store,package,brain,{}, {},"wds.ai.heart.rules/1");engine.configure_language("legacy");
            int sequence=0;
            auto emit=[&](const json &e) { if (e.at("type")=="neuron.completed" && !completion) completion=e.at("seq"); };
            auto cmd=[&](json m) { m["schema"]=1; m["request_id"]="request-"+std::to_string(++sequence); return engine.command(m,emit); };
            auto drain=[&] { for (int i=0;i<40;++i) {
                auto ready=engine.dispatch_ready(now_ms()+2000,emit);
                for(const auto &id:ready) while(engine.step(id,emit)) {}
                if (store.chats.at("chat").at("pending_signal")=="" && store.chats.at("chat").value("queued_messages",json::array()).empty()) return;
            } throw std::runtime_error("queue did not drain"); };
            cmd({{"type","project.create"},{"project_id","home"},{"name","Home"},{"purpose","music"}});
            cmd({{"type","chat.create"},{"project_id","home"},{"chat_id","chat"},{"title","Chat"}});
            cmd({{"type","chat.update"},{"chat_id","chat"},{"expected_version",1},{"mode","host"}});
            cmd({{"type","chat.send"},{"chat_id","chat"},{"text","entity one door : locked=true"}});
            auto ready=engine.dispatch_ready(now_ms(),emit);
            auto first=store.chats.at("chat").at("pending_instance").get<std::string>();
            require(!first.empty(),"first dispatch");
            for(const auto &id:ready)if(id!=first)while(engine.step(id,emit)){};
            stale=store.neurons.at("home/home");
            cmd({{"type","chat.send"},{"chat_id","chat"},{"text","entity two door : locked=true"}});
            cmd({{"type","chat.send"},{"chat_id","chat"},{"text","entity three door : locked=true"}});
            require(store.chats.at("chat").at("queued_messages").size()==2,"messages not queued");
            while(engine.step(first,emit)){}
            drain();
            require(store.messages_by_chat.at("chat").size()==6,"queued messages lost or duplicated");
            std::vector<std::string> replies;
            for(auto id:store.messages_by_chat.at("chat")) if(store.chat_messages.at(id).at("role")=="assistant") replies.push_back(store.chat_messages.at(id).at("text"));
            require(replies.size()==3 && replies[0].find("Recorded one")!=std::string::npos && replies[1].find("Recorded two")!=std::string::npos && replies[2].find("Recorded three")!=std::string::npos,"response ordering");
            cmd({{"type","chat.send"},{"chat_id","chat"},{"text","/cognition {broken"}});
            cmd({{"type","chat.send"},{"chat_id","chat"},{"text","after failure"}});drain();
            bool partial=false;for(const auto &[id,m]:store.chat_messages)if(m.at("status")=="failed")partial=true;
            require(partial && store.chats.at("chat").at("pending_signal")=="","instruction failure blocked conversation");
            cmd({{"type","chat.send"},{"chat_id","chat"},{"text","cancel"}});
            ready=engine.dispatch_ready(now_ms(),emit);
            cmd({{"type","chat.send"},{"chat_id","chat"},{"text","after cancellation"}});
            cmd({{"type","cancel"},{"instance_id",store.chats.at("chat").at("pending_instance")}});
            for(const auto &id:ready)while(engine.step(id,emit)){};drain();
            cmd({{"type","chat.send"},{"chat_id","chat"},{"text","exception"}});
            ready=engine.dispatch_ready(now_ms(),emit);
            cmd({{"type","chat.send"},{"chat_id","chat"},{"text","after exception"}});
            engine.fail(store.chats.at("chat").at("pending_instance"),"injected coordinator exception",emit);
            for(const auto &id:ready)while(engine.step(id,emit)){};drain();
            // Neural conversation remains open while a bounded impulse is pending.
            cmd({{"type","chat.update"},{"chat_id","chat"},{"expected_version",store.chats.at("chat").at("version")},{"mode","wind"}});
            cmd({{"type","chat.send"},{"chat_id","chat"},{"text","music one"}});
            cmd({{"type","chat.send"},{"chat_id","chat"},{"text","music two"}});drain();
            require(store.active_packages.empty(),"chat activated candidate");
            cmd({{"type","chat.send"},{"chat_id","chat"},{"text","budget pending"}});
            for(int i=0;i<16;++i)cmd({{"type","chat.send"},{"chat_id","chat"},{"text","queued "+std::to_string(i)}});
            auto queue_seq=store.sequence();bool full=false;
            try {cmd({{"type","chat.send"},{"chat_id","chat"},{"text","overflow"}});}catch(const std::exception &e){full=std::string(e.what()).find("queue is full")!=std::string::npos;}
            require(full && store.sequence()==queue_seq && store.chats.at("chat").at("queued_messages").size()==16,"queue limit was silent or wrote overflow");
            auto size=fs::file_size(dir/"events.jsonl"),seq=store.sequence(); bool rejected=false;
            try { store.append("invalid","home","bad",{{"mesh",{{"assemblies",{{"key","bad"}}}}}}); } catch(const std::exception &){rejected=true;}
            require(rejected && store.sequence()==seq && fs::file_size(dir/"events.jsonl")==size,"invalid collection written before validation");
        }
        // Simulate the Windows singleton-object journal and a crash after native
        // completion, before chat delivery. Rehash fixture only, never real data.
        auto source=bytes(dir/"events.jsonl"); std::istringstream input(source);std::string line,previous;json last;
        auto recovery=dir/"legacy";fs::create_directories(recovery);std::ofstream out(recovery/"events.jsonl");
        while(std::getline(input,line)) {
            auto e=json::parse(line); if(e.at("seq").get<uint64_t>()>completion)break;
            if(previous.empty())previous=e.at("prev");e["prev"]=previous;
            for(auto section:{"mesh","maintenance"})if(e["data"].contains(section))
                for(auto &[key,value]:e["data"][section].items())
                    if(value.is_array() && value.size()==1 && value.front().is_object())value=json(value.front());
            e.erase("hash");previous=checksum(e.dump());e["hash"]=previous;out<<e.dump()<<'\n';last=e;
        }
        // Later queue events in the affected runtime carried a stale active reservation.
        last["seq"]=last.at("seq").get<uint64_t>()+1;last["prev"]=previous;last["type"]="neuron.queued";
        last["data"]={{"maintenance",{{"neurons",stale}}}};last.erase("hash");last["hash"]=checksum(last.dump());out<<last.dump()<<'\n';out.close();
        auto prefix=bytes(recovery/"events.jsonl");uint64_t final_sequence=0;
        {
            chain::Store store(recovery); runtime::Engine engine(store,package,brain,{}, {},"wds.ai.heart.rules/1");engine.configure_language("legacy");
            require(store.neurons.at("home/home").at("active")=="","stale reservation not repaired");
            require(store.chats.at("chat").at("pending_signal")=="","completed host response not recovered");
            require(store.messages_by_chat.at("chat").size()==4,"completed reply lost on replay");
            for(int n=0;n<5;++n)for(const auto &id:engine.dispatch_ready(now_ms()+2000,[](const json &){}))while(engine.step(id,[](const json &){})){}
            require(store.chats.at("chat").at("queued_messages").empty() && store.messages_by_chat.at("chat").size()==6,"restart queue lost or duplicated");
            require(bytes(recovery/"events.jsonl").starts_with(prefix),"recovery rewrote history");final_sequence=store.sequence();
        }
        {chain::Store store(recovery); runtime::Engine engine(store,package,brain,{}, {},"wds.ai.heart.rules/1");engine.configure_language("legacy");require(store.sequence()==final_sequence,"recovery not idempotent");}
        fs::remove_all(dir);std::cout<<"host/neural queue, failure, cancellation, partial output, legacy replay, stale reservation, restart and history integrity passed\n";
    } catch(const std::exception &e){std::cerr<<e.what()<<'\n';fs::remove_all(dir);return 1;}
}
