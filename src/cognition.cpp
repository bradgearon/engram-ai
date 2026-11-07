#include "wds/ai/cognition.hpp"
#include "wds/ai/core.hpp"
#include <cmath>
#include <sstream>
#include <algorithm>
namespace wds::ai::cognition {
namespace {
std::string trim(std::string s) {
    auto a=s.find_first_not_of(" \t\r\n"), b=s.find_last_not_of(" \t\r\n");
    return a==std::string::npos ? "" : s.substr(a,b-a+1);
}
std::string word(std::istringstream &in) { std::string s; in>>s; identifier(s); return s; }
void properties(const json &v) {
    require(v.is_object() && v.size()<=16,"properties must be an object with at most 16 fields");
    for(const auto &[k,x]:v.items()) {
        identifier(k); require(x.is_boolean() || x.is_string() || x.is_number(),"property must be a scalar");
        require(x.dump().size()<=128,"property value too long");
        if(x.is_number()) require(std::isfinite(x.get<double>()),"non-finite property");
    }
}
json fields(const std::string &text) {
    json r=json::object(); std::istringstream in(text); std::string pair;
    while(std::getline(in,pair,',')) {
        auto split=pair.find('='); require(split!=std::string::npos,"use property=value, separated by commas");
        auto key=trim(pair.substr(0,split)), value=trim(pair.substr(split+1)); identifier(key);
        require(!value.empty() && !r.contains(key),"empty or duplicate property");
        auto parsed=json::parse(value,nullptr,false); r[key]=parsed.is_discarded()?json(value):parsed;
    }
    properties(r); return r;
}
json learn(json action, const json &sample) {
    auto &samples=action["samples"];
    require(samples.size()<32,"action demonstration capacity (32); start a new versioned action");
    samples.push_back(sample);
    json common=json::object(), effects=json::object(); bool first=true; std::set<std::string> varying;
    for(const auto &s:samples) if(s.at("success")==true) {
        json changed=json::object();
        for(const auto &[k,v]:s.at("after").items())
            if(!s.at("before").contains(k) || s.at("before").at(k)!=v) changed[k]=v;
        // Missing fields are unknown, not contradictory. A later observation
        // can introduce a newly discovered prerequisite.
        for(const auto &[k,v]:s.at("before").items()) {
            if(varying.count(k))continue;
            if(common.contains(k) && common.at(k)!=v){common.erase(k);varying.insert(k);}
            else common[k]=v;
        }
        if(first) {effects=changed;first=false;}
        else {
            for(auto it=effects.begin();it!=effects.end();) {
                if(!changed.contains(it.key()) || changed.at(it.key())!=it.value()) it=effects.erase(it); else ++it;
            }
        }
    }
    action["preconditions"]=common; action["effects"]=effects;
    action["ambiguous"]=false;
    for(const auto &a:samples) for(const auto &b:samples)
        if(a.at("before")==b.at("before") && (a.at("success")!=b.at("success") || a.at("after")!=b.at("after"))) action["ambiguous"]=true;
    // Local eligibility only on demonstrated feature/outcome edges. Reward is
    // observed success/failure, never attention or the system's own prediction.
    auto &edges=action["edges"];
    for(const auto &[k,v]:sample.at("before").items()) {
        const auto id=k+"="+v.dump();
        require(edges.contains(id)||edges.size()<128,"action edge capacity reached");
        auto edge=edges.value(id,json{{"weight",0.0},{"eligibility",0.0},{"observations",0}});
        double e=std::min(2.,.8*edge.at("eligibility").get<double>()+1.);
        double w=edge.at("weight"), reward=sample.at("success")==true?1.:-1.;
        edge["weight"]=std::clamp(w+.2*reward*e-.01*w,-2.,2.);
        edge["eligibility"]=e; edge["observations"]=edge.at("observations").get<int>()+1;
        edges[id]=edge;
    }
    action["version"]=action.at("version").get<int>()+1; return action;
}
json predict(const json &action,const json &entity) {
    json unmet=json::array(), unknown=json::array();
    for(const auto &[k,v]:action.at("preconditions").items()) {
        if(!entity.at("properties").contains(k)) unknown.push_back(k);
        else if(entity.at("properties").at(k)!=v) unmet.push_back(k);
    }
    bool observed_failure=false;
    for(const auto &sample:action.at("samples")) if(sample.at("success")==false) {
        bool matches=!sample.at("before").empty();
        for(const auto &[k,v]:sample.at("before").items()) if(!entity.at("properties").contains(k)||entity.at("properties").at(k)!=v)matches=false;
        observed_failure|=matches;
    }
    // A bounded recurrent micro-network: only matching predicate edges wake.
    // Recurrence changes activation/priority, not the truth of preconditions.
    double drive=0.; json nodes=json::array();
    for(const auto &[k,v]:entity.at("properties").items()) {
        auto id=k+"="+v.dump(); if(!action.at("edges").contains(id)) continue;
        double w=action.at("edges").at(id).at("weight"); drive+=w; nodes.push_back({{"predicate",id},{"weight",w}});
    }
    double u=0.; json trajectory=json::array();
    for(int step=0;step<4;++step) {u=.5*u+.5*std::tanh(drive+.2*u);trajectory.push_back(u);}
    auto status=action.at("ambiguous")==true?"conflict": !unmet.empty() || observed_failure?"blocked":
                !unknown.empty() || action.at("effects").empty()?"unknown":"predicted";
    json after=entity.at("properties"); if(std::string(status)=="predicted") after.update(action.at("effects"));
    return {{"status",status},{"before",entity.at("properties")},{"predicted_after",after},
        {"unmet",unmet},{"unknown",unknown},{"observed_failure",observed_failure},{"activation",u},{"neurons",nodes},{"trajectory",trajectory},
        {"max_steps",4},{"action_version",action.at("version")},{"entity_version",entity.at("version")},
        {"confidence_kind","demonstration-based hypothesis; no calibrated probability"}};
}
}
std::vector<std::string> language_actions() { return {"greeting","help","inspect"}; }
json empty_state(const std::string &root) {
    return {{"schema","cognition/1"},{"root_id",root},{"version",0},{"entities",json::object()},
            {"actions",json::object()},{"language_samples",json::array()}};
}
json parse(const std::string &text) {
    const auto t=trim(text);
    if(t.starts_with("/cognition ")) {
        auto op=json::parse(t.substr(11)); require(op.is_object(),"cognition instruction must be an object"); return op;
    }
    if(t=="help" || t=="/help") return {{"op","help"}};
    if(t=="hello" || t=="hi" || t=="Hello" || t=="Hi") return {{"op","greeting"}};
    if(t=="show knowledge") return {{"op","inspect"}};
    for(auto prefix:{"entity ","teach ","predict ","try ","show "}) if(t.starts_with(prefix)) {
        std::istringstream in(t.substr(std::string(prefix).size())); auto a=word(in);
        if(std::string(prefix)=="show ") {std::string extra;require(!(in>>extra),"show expects one entity identifier");return {{"op","inspect"},{"entity",a}};}
        std::string b;in>>b;bool colon=b.ends_with(":");if(colon)b.pop_back();identifier(b);
        if(std::string(prefix)=="predict " || std::string(prefix)=="try ") {
            std::string extra;require(!(in>>extra),"predict/try expects ACTION ENTITY");
            return {{"op",std::string(prefix)=="try "?"simulate":"predict"},{"action",a},{"entity",b}};
        }
        // A colon may immediately follow the type identifier.
        std::string rest;std::getline(in,rest);rest=trim(rest);
        if(colon)rest=":"+rest;
        require(rest.starts_with(":"),"use a space before the colon: entity ID TYPE : property=value");rest=trim(rest.substr(1));
        if(std::string(prefix)=="entity ") return {{"op","entity"},{"entity",a},{"kind",b},{"properties",fields(rest)}};
        auto arrow=rest.find("=>");require(arrow!=std::string::npos,"teach needs BEFORE => AFTER or failure");
        auto before=fields(rest.substr(0,arrow)); auto tail=trim(rest.substr(arrow+2));
        auto after=before; if(tail!="failure") after.update(fields(tail));
        return {{"op","teach"},{"action",a},{"kind",b},{"before",before},{"after",after},{"success",tail!="failure"}};
    }
    return {{"op","unknown"},{"text",text}};
}
State project(const neural::Representation &r) {
    require(!r.identity.empty() && !r.values.empty() && r.values.size()<=32768,"invalid language representation");
    // Fixed signed projection; learned W remains the existing 16x16 policy ABI.
    State x{};
    for(size_t i=0;i<r.values.size();++i) {
        require(std::isfinite(r.values[i]),"non-finite encoder feature");
        auto h=hash64("language.projection/1:"+std::to_string(i)); x[h%16]+=(h&256?1.f:-1.f)*r.values[i];
    }
    float n=0;for(float v:x)n+=v*v;require(n>1e-12f,"zero language representation");
    for(float &v:x)v/=std::sqrt(n);
    return x;
}
json process(const json &snapshot,const json &operation,const std::string &source,const Encode &encode,const winds::Package *language) {
    json s=snapshot, op=operation, trace={{"grammar",grammar},{"generated_tokens",0},{"generation_calls",0},{"encoder_calls",0}};
    auto name=field(op,"op",32); bool changed=false; std::string text; json claim=json::object();
    require(s.at("schema")=="cognition/1","unsupported cognitive snapshot");
    if(name=="unknown" && encode && language && language->manifest.value("feature_schema",std::string{})==language_schema) {
        auto r=encode(field(op,"text",4096));trace["encoder_calls"]=1;
        require(r.identity==language->manifest.at("representation_id").get<std::string>(),"Language Wind representation mismatch; re-encode and retrain for this model");
        auto x=project(r);float values[3]{},sum=0;
        for(size_t o=0;o<3;++o){values[o]=language->policy.bias[o];for(size_t i=0;i<16;++i)values[o]+=language->policy.weight[o*16+i]*x[i];}
        auto high=*std::max_element(values,values+3);for(float &v:values){v=std::exp(v-high);sum+=v;}for(float &v:values)v/=sum;
        std::array<int,3> order{0,1,2};std::sort(order.begin(),order.end(),[&](int a,int b){return values[a]>values[b];});
        trace["language_readout"]={{"model",r.identity},{"features",x},{"scores",{values[0],values[1],values[2]}},{"checksum",language->digest}};
        if(values[order[0]]>=.75f && values[order[0]]-values[order[1]]>=.25f) name=language_actions()[order[0]];
        else trace["language_held"]="readout below strength/margin thresholds";
    }
    if(name=="greeting") text="Hello. I can remember scoped knowledge and learn small operations from demonstrations. Type help to see what I understand.";
    else if(name=="help") text="Language Wind v1 understands:\nentity gate door : locked=true,key=true\nteach unlock door : locked=true,key=true => locked=false\nteach unlock door : locked=true,key=false => failure\npredict unlock gate\ntry unlock gate\nshow gate\nshow knowledge\n\nTry changes a local simulation only. Teach records your reported observation. Other questions use cited project memories; unfamiliar language may need clarification.";
    else if(name=="inspect") {
        if(op.contains("entity")) {auto id=field(op,"entity",80); require(s.at("entities").contains(id),"unknown entity");claim=s.at("entities").at(id);text=id+": "+claim.at("properties").dump();}
        else {text="This scope contains "+std::to_string(s.at("entities").size())+" entities and "+std::to_string(s.at("actions").size())+" learned operations.";claim={{"entity_ids",json::array()},{"action_ids",json::array()}};for(const auto &[id,v]:s.at("entities").items())claim["entity_ids"].push_back(id);for(const auto &[id,v]:s.at("actions").items())claim["action_ids"].push_back(id);}
    } else if(name=="entity") {
        auto id=field(op,"entity",80),kind=field(op,"kind",80);identifier(id);identifier(kind);properties(op.at("properties"));
        require(s.at("entities").contains(id)||s.at("entities").size()<256,"entity capacity reached");
        auto old=s.at("entities").value(id,json{{"version",0}});
        if(op.contains("expected_version"))require(op.at("expected_version")==old.at("version"),"entity version conflict");
        claim={{"id",id},{"kind",kind},{"properties",op.at("properties")},{"version",old.at("version").get<int>()+1},{"source",source},{"claim_status","operator_reported"}};
        s["entities"][id]=claim;changed=true;text="Recorded "+id+" as "+kind+" with "+op.at("properties").dump()+".";
    } else if(name=="teach") {
        auto id=field(op,"action",80),kind=field(op,"kind",80);identifier(id);identifier(kind);
        properties(op.at("before"));properties(op.at("after"));require(op.at("success").is_boolean(),"success must be boolean");
        require(s.at("actions").contains(id)||s.at("actions").size()<64,"action capacity reached");
        auto action=s.at("actions").value(id,json{{"id",id},{"kind",kind},{"version",0},{"samples",json::array()},{"edges",json::object()}});
        require(action.at("kind")==kind,"action target kind mismatch");
        if(op.contains("expected_version")) require(op.at("expected_version")==action.at("version"),"action version conflict");
        json sample={{"before",op.at("before")},{"after",op.at("after")},{"success",op.at("success")},{"source",source},{"claim_status","operator_reported"}};
        action=learn(action,sample);s["actions"][id]=action;changed=true;
        claim={{"action",id},{"version",action.at("version")},{"preconditions",action.at("preconditions")},{"effects",action.at("effects")},{"ambiguous",action.at("ambiguous")}};
        text="Learned observation "+std::to_string(action.at("samples").size())+" for "+id+". Current hypothesized preconditions: "+action.at("preconditions").dump()+"; effects: "+action.at("effects").dump()+". These need further testing.";
    } else if(name=="predict" || name=="simulate") {
        auto a=field(op,"action",80),e=field(op,"entity",80);
        require(s.at("actions").contains(a),"unknown operation; teach demonstrations first");require(s.at("entities").contains(e),"unknown entity; describe it first");
        const auto &action=s.at("actions").at(a);auto entity=s.at("entities").at(e);require(entity.at("kind")==action.at("kind"),"operation target kind mismatch");
        claim=predict(action,entity);claim["action"]=a;claim["entity"]=e;claim["sample_sources"]=json::array();for(const auto &sample:action.at("samples"))claim["sample_sources"].push_back(sample.at("source"));
        if(claim.at("status")=="predicted") {
            text="I predict "+a+" on "+e+" would produce "+claim.at("predicted_after").dump()+" based on "+std::to_string(action.at("samples").size())+" observations.";
            if(name=="simulate") {entity["properties"]=claim.at("predicted_after");entity["version"]=entity.at("version").get<int>()+1;entity["source"]=source;entity["claim_status"]="simulated";s["entities"][e]=entity;changed=true;text+=" Applied to the local simulation; this is not a new observed outcome.";}
        } else text="I cannot predict a successful "+a+" on "+e+": "+claim.at("status").get<std::string>()+". Unmet: "+claim.at("unmet").dump()+"; unknown: "+claim.at("unknown").dump()+".";
    } else if(name=="language.sample") {
        require(bool(encode),"language sampling needs a Qwen3 representation backend");
        auto label=field(op,"label",32),t=field(op,"text",4096);auto labels=language_actions();auto it=std::find(labels.begin(),labels.end(),label);require(it!=labels.end(),"language labels: greeting, help, inspect");
        require(s.at("language_samples").size()<128,"language sample capacity reached");
        auto split=field(op,"split",16);require(split=="train"||split=="validation"||split=="test","invalid sample split");
        for(const auto &row:s.at("language_samples"))require(row.at("text")!=t,"duplicate language text; split leakage rejected");
        auto r=encode(t);trace["encoder_calls"]=1;auto x=project(r);
        claim={{"text",t},{"features",x},{"target",it-labels.begin()},{"split",split},{"representation_id",r.identity},{"source",source}};
        s["language_samples"].push_back(claim);changed=true;text="Recorded a model-bound language training example. No package was activated.";
    } else {require(name=="unknown","unknown cognitive operation");text="I don't understand that instruction yet. Type help for the current Language Wind grammar.";}
    require(s.dump().size()<=96*1024,"root cognitive state exceeds 96 KiB budget");
    if(changed)s["version"]=s.at("version").get<int>()+1;
    if(claim.value("status",std::string{})=="unknown") trace["knowledge_need"]={
        {"kind","missing_preconditions"},{"entity",claim.at("entity")},{"properties",claim.at("unknown")},
        {"scope",s.at("root_id")},{"status","awaiting attributed observation"},
        {"acceptance","operator observation or validated source; never infer private object state from a web excerpt"}};
    trace["operation"]=name;trace["claim"]=claim;trace["source"]=source;
    return {{"base_version",snapshot.at("version")},{"state",s},{"changed",changed},{"text",text},{"trace",trace},
        {"handled",name!="unknown"},{"status",claim.value("status",std::string("completed"))}};
}
}
