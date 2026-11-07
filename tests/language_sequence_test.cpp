#include "wds/ai/core.hpp"
#include <fstream>
#include <iostream>
using namespace wds::ai;
struct Teacher final : intelligence::Foundation {
    int calls=0;std::vector<int> context_sizes;
    std::string name() const override{return "fixture-teacher";}
    std::string propose(const intelligence::Request &r,intelligence::Token emit) override {
        require(r.language_teacher,"teacher route was not explicit");++calls;
        context_sizes.push_back(int(r.relationship.value("conversation_turns",json::array()).size()));
        emit("A useful ");if(r.seed=="interrupt")throw intelligence::GenerationLimit("fixture output limit","A useful partial answer");
        emit("response.");return "A useful response.";
    }
};
int main(int argc,char **argv) {
    auto dir=fs::temp_directory_path()/("wds-language-sequence-"+std::to_string(now_ms()));fs::create_directories(dir);
    try {
        require(argc==2,"parent package required");json corpus={{"schema","english.conversations/1"},{"conversations",json::array()}};
        for(auto split:{"train","validation","test"})for(int i=0;i<6;++i)corpus["conversations"].push_back({{"id",std::string(split)+std::to_string(i)},{"group",split},{"split",split},
            {"turns",json::array({json{{"role","user"},{"text",std::string(split)+" example "+std::to_string(i)}},json{{"role","assistant"},{"text","The cat is here."}}})}});
        auto data=dir/"corpus.json";{std::ofstream f(data);f<<corpus;}
        auto out=dir/"candidate";json job={{"curriculum",data.string()},{"parent",argv[1]},{"output",out.string()},{"epochs",6},{"learning_rate",.1}};
        language::train(job);winds::Package p(out);
        require(p.sequence && fs::file_size(out/"policy.wind")==1152,"sequence extension broke Wind ABI");
        auto stats=p.manifest.at("training");require(stats.at("validation").at("cross_entropy").get<double>()<stats.at("initial_validation").at("cross_entropy").get<double>(),"sequence optimizer did not learn");
        language::train(job); // Exact completed checkpoint retry must be immutable.
        auto report=language::evaluate({{"package",out.string()},{"curriculum",data.string()}});
        require(report.at("predicted_bytes")>0 && report.at("activated")==false,"locked evaluation missing");
        language::Model controlled;controlled.digest="controlled";controlled.weights.output_bias['Z']=4;
        auto a=language::generate(controlled,language::encode_context(json::array(),"hello"),8);
        auto b=language::generate(controlled,language::encode_context(json::array(),"explain quantum fields"),8);
        require(a.at("text")=="ZZZZZZZZ" && a.at("text")==b.at("text") && a.at("complete")==false,"responses bypassed learned numerical model or limit was silent");
        require(a.at("weight_trace").front().at("units").size()==8,"missing bounded activation trace");
        {std::fstream f(out/"language.tensor",std::ios::binary|std::ios::in|std::ios::out);f.seekp(65);f.put('!');}
        bool rejected=false;try{winds::Package broken(out);}catch(const std::exception&){rejected=true;}require(rejected,"corrupted tensor accepted");
        Teacher teacher;winds::Package parent(argv[1]);
        {
            chain::Store store(dir/"chain");runtime::Engine e(store,parent,teacher);e.configure_language("assisted",8,512);int request=0,tokens=0;auto emit=[&](const json &v){if(v.at("type")=="token"){++tokens;require(v.at("source_kind")=="language_teacher","unattributed teacher stream");}};
            auto cmd=[&](json m){m["schema"]=1;m["request_id"]="test-"+std::to_string(++request);return e.command(m,emit);};
            auto drain=[&](){for(int i=0;i<20;++i){auto ids=e.dispatch_ready(now_ms()+i*2000,emit);for(auto &id:ids)while(e.step(id,emit)){};if(store.chats.at("chat").at("pending_signal")==""&&store.chats.at("chat").value("queued_messages",json::array()).empty())return;}throw std::runtime_error("teacher chat did not drain");};
            cmd({{"type","project.create"},{"project_id","home"},{"name","Home"},{"purpose","language learning"}});
            cmd({{"type","chat.create"},{"project_id","home"},{"chat_id","chat"},{"title","Language"}});
            cmd({{"type","chat.update"},{"chat_id","chat"},{"expected_version",1},{"mode","host"}});
            for(auto text:{"hello there","interrupt","he likes windows"}){cmd({{"type","chat.send"},{"chat_id","chat"},{"text",text}});drain();}
            require(teacher.calls==3 && tokens==5,"streaming fallback did not run on all natural input");
            require(teacher.context_sizes[0]==0 && teacher.context_sizes[1]==2 && teacher.context_sizes[2]==4,"multi-turn context lost or current turn duplicated");
            auto ids=store.messages_by_chat.at("chat");require(ids.size()==6,"reply queue wedged");
            require(store.chat_messages.at(ids[3]).at("text")=="A useful partial answer" && store.chat_messages.at(ids[3]).at("status")=="failed","partial teacher output was silently discarded");
            json lessons;cmd({{"type","language.lessons"},{"root_id","home"}});
            e.command({{"schema",1},{"type","language.lessons"},{"request_id","export"},{"root_id","home"}},[&](const json &v){lessons=v.at("data");});
            require(lessons.at("lessons").size()==2 && lessons.at("lessons").front().at("use_for_training")==false,"unreviewed teaching auto-approved");
            auto lesson_id=lessons.at("lessons").front().at("source_instance");
            cmd({{"type","language.review"},{"root_id","home"},{"source_instance",lesson_id},{"approved",true}});
            require(store.language_reviews.at(lesson_id).at("approved")==true,"explicit review was not journaled");
            bool denied=false;try{cmd({{"type","language.review"},{"root_id","foreign"},{"source_instance",lesson_id},{"approved",true}});}catch(const std::exception&){denied=true;}
            require(denied,"cross-root teacher lesson review accepted");
            require(store.active_packages.empty(),"teacher activated a package");
            e.configure_language("strict");cmd({{"type","chat.send"},{"chat_id","chat"},{"text","hello"}});drain();require(teacher.calls==3,"strict mode called teacher");
            e.configure_language("assisted",0);cmd({{"type","chat.send"},{"chat_id","chat"},{"text","another message"}});drain();require(teacher.calls==3,"teacher budget bypassed");
            // Same language path must work through the neural response assembly.
            e.configure_language("assisted",8);cmd({{"type","chat.update"},{"chat_id","chat"},{"expected_version",store.chats.at("chat").at("version")},{"mode","wind"}});
            cmd({{"type","chat.send"},{"chat_id","chat"},{"text","this is a neural conversation"}});drain();require(teacher.calls==4,"neural channel did not reach teacher");
            cmd({{"type","chat.send"},{"chat_id","chat"},{"text","interrupt"}});drain();
            const auto &partial=store.chat_messages.at(store.messages_by_chat.at("chat").back());
            require(partial.at("text")=="A useful partial answer" && partial.at("status")!="completed" && partial.at("error")=="fixture output limit","neural assembly discarded partial teacher output or its error");
            cmd({{"type","chat.send"},{"chat_id","chat"},{"text","continue after the interruption"}});drain();require(teacher.calls==6,"neural conversation wedged after a partial failure");
        }
        {chain::Store replay(dir/"chain");require(replay.language_reviews.size()==1,"operator review did not survive restart");size_t lessons=0;for(const auto &[id,r]:replay.cognitive_results)if(r.at("trace").contains("lesson"))++lessons;require(lessons==4,"teacher lesson provenance lost on restart");}
        fs::remove_all(dir);std::cout<<"Sequence learning, ABI, corruption, immutable resume, learned-only inference, streaming, budgets, failure recovery, context, neural path and replay passed\n";
    }catch(const std::exception &ex){std::cerr<<ex.what()<<" fixture="<<dir<<'\n';return 1;}
}
