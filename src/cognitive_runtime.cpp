#include "wds/ai/core.hpp"
#include <chrono>
#include <algorithm>
namespace wds::ai::runtime {
void Engine::configure_language(std::string mode,int calls,int tokens,float threshold) {
    require(mode=="legacy"||mode=="assisted"||mode=="strict","language mode must be legacy, assisted or strict");
    require(calls>=0&&calls<=64&&tokens>=32&&tokens<=4096&&threshold>=0&&threshold<=1,"invalid language limits");
    language_mode_=std::move(mode);language_teacher_limit_=calls;language_teacher_tokens_=tokens;language_student_threshold_=threshold;
}
Engine::Result Engine::cognitive_compute(const Work &w, Emit emit) const {
    Result r; r.state=w.request.state;
    auto start=std::chrono::steady_clock::now();
    std::string streamed_teacher;bool teacher_started=false;
    try {
        if(!w.prior_cognition.is_null()) r.cognitive=w.prior_cognition;
        else if(w.instance.contains("language_mode") && !w.request.seed.starts_with("/cognition ")) {
            json trace={{"operation","language.sequence"},{"source",w.instance.at("id")},{"encoder_calls",0},{"generation_calls",0},{"generated_tokens",0},{"claim",json::object()}};
            auto turns=w.request.relationship.value("conversation_turns",json::array());
            std::string response;bool accepted=false;
            if(w.language->sequence) {
                auto student=language::generate(*w.language->sequence,language::encode_context(turns,w.request.seed),1024);
                trace["student"]=student;trace["student_package"]=w.language->manifest.at("id");
                accepted=student.at("complete")==true && !student.at("text").get<std::string>().empty() && student.at("mean_selected_probability").get<float>()>=w.instance.at("language_student_threshold").get<float>();
                if(accepted)response=student.at("text");
            }
            if(!accepted && w.instance.at("language_mode")=="assisted") {
                require(w.instance.value("teacher_reserved",false),"Teacher budget exhausted; retry after the shared budget window. No output was silently discarded.");
                require(foundation_.name().find("demo")==std::string::npos,"Streaming teacher requires a configured GGUF; use strict mode for a local student only.");
                auto request=w.request;request.language_teacher=true;
                request.memories.erase(std::remove_if(request.memories.begin(),request.memories.end(),[](const json &m){return m.value("memory_kind",std::string{})=="reflection"||m.value("memory_kind",std::string{})=="expression_trace";}),request.memories.end());
                trace["generation_calls"]=1;trace["teacher"]={{"model",foundation_.name()},{"role","unverified language teacher"},{"generated",true}};
                std::string pending;size_t chunks=0;
                teacher_started=true;
                response=foundation_.propose(request,[&](const std::string &piece){
                    ++chunks;streamed_teacher+=piece;pending+=piece;try{(void)json(pending).dump();}catch(const json::type_error&){return;}
                    if(emit)emit({{"schema",1},{"type","token"},{"trace_id",w.instance.at("trace_id")},{"root_id",w.instance.at("root_id")},
                        {"instance_id",w.instance.at("id")},{"iteration",w.instance.at("iteration")},{"generation",w.instance.value("generation",0)},
                        {"speaker",w.instance.at("speaker")},{"source_kind","language_teacher"},{"text",pending}});pending.clear();
                });
                trace["generated_tokens"]=chunks;
                trace["lesson"]={{"schema","language.lesson/1"},{"input",w.request.seed},{"context",turns},{"response",response},{"teacher_model",foundation_.name()},
                    {"review_status","unreviewed"},{"use_for_training",false},{"source_instance",w.instance.at("id")},{"root_id",w.instance.at("root_id")}};
            } else if(!accepted) {
                response="The local language student has no accepted response for this input. Enable the optional streaming teacher, or train and explicitly activate a candidate.";
                trace["held"]=w.language->sequence?"student incomplete or below probability threshold":"no sequence Language Wind is active";
            }
            trace["renderer"]=trace.contains("teacher")?"qwen-teacher/1":"native-language-sequence/1";
            r.cognitive={{"base_version",w.cognition.at("version")},{"state",w.cognition},{"changed",false},{"text",response},{"trace",trace},{"handled",true},{"status",trace.contains("held")?"held":"completed"}};
        }
        else {
            cognition::Encode encode;
            if(foundation_.supports_representations()) encode=[&](const std::string &text){return foundation_.represent(w.instance.at("root_id"),text,512);};
            // Mesh slots are budgeted before compute. No unreserved encoding.
            if(w.instance.value("mode",std::string{})=="neural" ? w.instance.value("knowledge_reserved",0)<1 : !w.instance.value("cognitive_reserved",false)) encode={};
            r.cognitive=cognition::process(w.cognition,cognition::parse(w.request.seed),w.instance.at("id"),encode,w.language.get());
            if(!r.cognitive.at("handled").get<bool>()) {
                json a={{"input",w.request.seed},{"evidence",expression::evidence(w.request.memories,w.request.seed,true)},
                    {"conflict",false},{"unresolved",json::array()},{"contributions",json::array()}};
                std::map<std::string,std::string> claims;
                for(const auto &e:a.at("evidence")) if(e.contains("semantic") && e.at("semantic").value("exclusive",false)) {
                    const auto &v=e.at("semantic");auto key=v.at("subject").get<std::string>()+"\n"+v.at("predicate").get<std::string>();
                    auto value=v.at("object").get<std::string>();if(claims.count(key)&&claims.at(key)!=value)a["conflict"]=true;claims[key]=value;
                }
                // The existing source contract supplies grounded statements, not Qwen prose.
                auto expression=expression::compose(a,*w.package);
                r.cognitive["text"]=expression.at("text");r.cognitive["trace"]["expression"]=expression;
                if(a.at("evidence").empty() && expression::intent(w.request.seed)=="question" && w.request.seed.size()<=512)
                    r.cognitive["trace"]["knowledge_need"]={{"kind","general_knowledge"},{"query",w.request.seed},{"scope",w.instance.at("root_id")},
                        {"status","request scoped archive and bounded web review"}};
            }
        }
        r.proposal=r.cognitive.at("text");
        r.evaluation.status=heart::Status::pass;
        r.evaluation.evaluator_id="wds.ai.heart.cognition/1";
        r.evaluation.score=1;r.evaluation.feedback="bounded cognitive operation completed; predictions remain hypotheses";
        if(r.cognitive.at("trace").contains("teacher")){r.evaluation.evaluator_id="wds.ai.heart.teacher-transport/1";r.evaluation.feedback="teacher response transported; content not independently verified";}
        if(!r.cognitive.at("handled").get<bool>())
            r.evaluation=evaluators_.create(w.instance.at("evaluator"))->evaluate(r.proposal,w.context);
        r.evaluation.scope=w.instance.at("root_id");r.evaluation.snapshot_sequence=w.context.heart.sequence();
        r.neural={{"action","rest"},{"reason",r.evaluation.feedback},{"method","language-wind+cognition/1"},
            {"strength",1.0},{"margin",1.0},{"uncertain",false},{"features",r.state},
            {"knowledge_calls",r.cognitive.at("trace").at("encoder_calls")},{"cache_hits",0},
            {"knowledge",{{"generated_tokens",r.cognitive.at("trace").value("generated_tokens",0)}}},{"cognitive_plan",{{"text",r.proposal},{"trace",r.cognitive.at("trace")}}},
            {"payload",{{"schema","engram.payload/1"},{"evidence",json::array()}}}};
    } catch(const intelligence::GenerationLimit &e) {
        r.error=e.what();r.proposal=json::parse(json(e.partial).dump(-1,' ',false,json::error_handler_t::replace)).get<std::string>();
        r.cognitive=json();r.neural={{"knowledge_calls",0},{"cache_hits",0},{"generation_calls",1}};
    } catch(const std::exception &e) {r.error=e.what();r.proposal=json::parse(json(streamed_teacher).dump(-1,' ',false,json::error_handler_t::replace)).get<std::string>();r.cognitive=json();r.neural={{"knowledge_calls",0},{"cache_hits",0},{"generation_calls",teacher_started?1:0}};}
    if(!r.error.empty() && w.instance.contains("language_mode") && !w.request.seed.starts_with("/cognition ")) {
        json trace={{"renderer",teacher_started?"qwen-teacher/1":"native-language-sequence/1"},{"complete",false},{"error",r.error}};
        if(teacher_started)trace["teacher"]={{"model",foundation_.name()}};
        r.neural["language_failure"]={{"text",r.proposal.empty()?"I couldn't complete this response: "+r.error:r.proposal},{"trace",trace}};
    }
    r.elapsed_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();return r;
}
void Engine::cognitive_commit(const Work &w, Result &r, Emit emit) {
    if(r.cognitive.is_null() || !r.error.empty())return;
    const auto root=w.instance.at("root_id").get<std::string>(), id=w.instance.at("id").get<std::string>();
    const auto operation_key=w.instance.contains("mesh_pulse")?w.instance.at("mesh_pulse").at("key").get<std::string>():w.instance.value("neuron_signal_key",id);
    if(!store_.cognitive_results.count(operation_key)) { // Crash after commit: no duplicate learning.
    auto current=store_.cognitive_states.count(root)?store_.cognitive_states.at(root):cognition::empty_state(root);
    if(current.at("version")!=r.cognitive.at("base_version")) {
        r.error="Knowledge changed during this impulse; submit again against the new version. No update was applied.";
        r.neural.erase("cognitive_plan");return;
    }
    json saved=r.cognitive;saved["state"]=json(); // State is persisted once per commit.
    json packet={{"schema",1},{"intent","memory.upsert"},{"root_id",root},{"engram_id","cognitive-"+id},{"packet_id","cognitive-"+id},
        {"expected_version",0},{"hops",0},{"expires_ms",now_ms()+60000},{"text","Cognitive operation "+r.cognitive.at("trace").at("operation").get<std::string>()+" recorded its provenance."},
        {"source","cognition:"+id},{"claim_status","hypothetical"}};
    auto memory=mesh::IGF2BP2Handler(store_).prepare_rna(packet,id);
    memory["memory_kind"]="expression_trace";memory["learning_link"]={{"cognitive_instance",id},{"trace",r.cognitive.at("trace")}};
    event("cognition.committed",root,w.instance.at("trace_id"),
        {{"instance_id",id},{"trace",r.cognitive.at("trace")},
         { "maintenance", {{"memories",json::array({memory})}}},
         {"cognition",{{"instance_id",id},{"operation_key",operation_key},{"state",r.cognitive.at("state")},{"result",saved}}}},emit);
    }
    const auto need=r.cognitive.at("trace").value("knowledge_need",json::object());
    if(need.value("kind",std::string{})=="general_knowledge") {
        json request={{"schema",1},{"type","mesh.review"},{"request_id","lookup-"+checksum(operation_key)},
            {"root_id",root},{"node_id",root},{"text",need.at("query")},{"lookup",true},{"due_in_ms",0},{"response",true}};
        if(w.instance.contains("neuron_signal_key")) for(const auto &[key,chat]:store_.chats)
            if(chat.at("pending_signal")==w.instance.at("neuron_signal_key") && chat.at("project_id")==root){request["chat_id"]=key;break;}
        try{command(request,emit);}catch(const std::exception &error){event("cognition.lookup_held",root,w.instance.at("trace_id"),{{"instance_id",id},{"reason",error.what()}},emit);}
    }
}
}
