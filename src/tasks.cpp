#include "wds/ai/core.hpp"
#include "wds/ai/tasks.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
namespace wds::ai::tasks {
namespace {
int integer(const json &v, int lo, int hi) {
    require(v.is_number_integer() && v >= lo && v <= hi, "task integer out of bounds");
    return v.get<int>();
}
int pitch_class(int p) { return (p % 12 + 12) % 12; }
int nearest(int pc, int around, int lo, int hi) {
    int best = -1;
    for (int p = lo; p <= hi; ++p)
        if (pitch_class(p) == pitch_class(pc) && (best < 0 || std::abs(p-around) < std::abs(best-around))) best = p;
    require(best >= 0, "pitch class unavailable in range");
    return best;
}
double mean(const std::map<std::string,double> &c) {
    double value = 0; for (const auto &kv:c) value += kv.second;
    return value / double(c.size());
}
void flags(const json &r, std::initializer_list<const char *> names) {
    for (auto k:names) if(r.contains(k)) require(r.at(k).is_boolean(), "task flag must be boolean");
}
}
std::vector<std::string> actions(const std::string &d) {
    if (d == "music") return {"transpose_into_range", "fit_duration", "return_home", "answer_interval",
        "introduce_divergence", "repeat_motif", "stop"};
    if (d == "architecture") return {"introduce_port", "isolate_ownership", "deduplicate_events",
        "bound_calls", "introduce_async_boundary", "add_supervision", "stop"};
    throw std::runtime_error("unknown task domain");
}
void validate(const json &t) {
    require(t.is_object() && t.dump().size() <= 10000 && t.at("schema") == 1, "invalid task size/schema");
    auto domain=field(t,"domain",32); actions(domain);
    identifier(field(t,"id",80));
    const auto &a=t.at("artifact"), &r=t.at("requirements");
    require(a.is_object() && r.is_object(), "invalid task artifact/requirements");
    if(domain=="music") {
        const auto &notes=a.at("notes");
        require(notes.is_array() && notes.size()>=4 && notes.size()<=16 && notes.size()%2==0, "music requires 4..16 even notes");
        for(const auto &n:notes) {integer(n.at("pitch"),0,127);integer(n.at("duration"),1,256);}
        int lo=integer(r.at("pitch_min"),0,115), hi=integer(r.at("pitch_max"),12,127);
        require(hi-lo>=12, "music range must span an octave");
        integer(r.at("target_ticks"),int(notes.size()),256);
        integer(r.at("home_pc"),0,11);
        flags(r,{"return_home","repeat_motif"});
        if(r.contains("divergence_pc")) integer(r.at("divergence_pc"),0,11);
        if(r.contains("answer_interval")) {
            integer(r.at("answer_interval"),-12,12);
            const auto &anchors=r.at("anchors");
            require(anchors.is_array() && anchors.size()==notes.size(),"anchor count mismatch");
            for(const auto &p:anchors) integer(p,0,127);
        }
    } else {
        const auto &nodes=a.at("components"), &edges=a.at("edges");
        require(nodes.is_array() && !nodes.empty() && nodes.size()<=12 && edges.is_array() && edges.size()<=24,"architecture size limit");
        std::set<std::string> ids;
        for(const auto &n:nodes) {
            auto id=field(n,"id",40); identifier(id); require(ids.insert(id).second,"duplicate component");
            auto layer=field(n,"layer",16);require(layer=="domain" || layer=="app" || layer=="adapter","invalid layer");
            require(n.at("writes").is_array() && n.at("writes").size()<=4,"invalid stores");
            for(const auto &db:n.at("writes")) {require(db.is_string(),"store ID required");identifier(db.get<std::string>());}
            require(n.at("stateful").is_boolean() && n.at("supervised").is_boolean(),"invalid state flags");
        }
        for(const auto &e:edges) {
            require(ids.count(field(e,"from",40)) && ids.count(field(e,"to",40)),"unknown graph endpoint");
            auto kind=field(e,"kind",16);require(kind=="call" || kind=="event","invalid edge kind");
            require(e.at("via_port").is_boolean() && e.at("idempotent").is_boolean(),"invalid edge flags");
            integer(e.at("timeout_ms"),0,60000);
        }
        flags(r,{"domain_isolation","private_ownership","event_idempotency","bounded_calls","async_boundary","supervision"});
        integer(r.at("latency_budget_ms"),1,60000);
    }
}
std::map<std::string,double> checks(const json &t) {
    validate(t);
    const auto &a=t.at("artifact"), &r=t.at("requirements");
    std::map<std::string,double> c;
    if(t.at("domain")=="music") {
        const auto &ns=a.at("notes"); int lo=r.at("pitch_min"),hi=r.at("pitch_max"),ticks=0;
        bool range=true,answer=true,divergence=!r.contains("divergence_pc"),repeat=true;
        for(size_t i=0;i<ns.size();++i) {
            int p=ns[i].at("pitch"); ticks+=ns[i].at("duration").get<int>(); range &= p>=lo && p<=hi;
            if(r.contains("answer_interval")) answer &= pitch_class(p-r.at("anchors")[i].get<int>())==pitch_class(r.at("answer_interval"));
            if(r.contains("divergence_pc")) divergence |= pitch_class(p)==r.at("divergence_pc").get<int>();
            if(i>=ns.size()/2) repeat &= ns[i]==ns[i-ns.size()/2];
        }
        c={{"pitch_range",range?1.:0.},{"duration",ticks==r.at("target_ticks").get<int>()?1.:0.}};
        if(r.value("return_home",false)) c["home_return"]=pitch_class(ns.back().at("pitch"))==r.at("home_pc").get<int>()?1.:0.;
        if(r.contains("answer_interval")) c["positional_interval"]=answer?1.:0.;
        if(r.contains("divergence_pc")) c["divergent_pitch"]=divergence?1.:0.;
        if(r.value("repeat_motif",false)) c["motif_repetition"]=repeat?1.:0.;
    } else {
        const auto &nodes=a.at("components"),&edges=a.at("edges");
        std::map<std::string,std::string> layer,owner;
        bool ports=true,private_db=true,dedup=true,bounded=true,async=true,supervised=true;
        for(const auto &n:nodes) {
            auto id=n.at("id").get<std::string>();layer[id]=n.at("layer");
            for(const auto &db:n.at("writes")) {auto key=db.get<std::string>();if(owner.count(key) && owner[key]!=id)private_db=false;owner[key]=id;}
            if(n.at("stateful")==true && n.at("supervised")==false) supervised=false;
        }
        for(const auto &e:edges) {
            auto from=e.at("from").get<std::string>(),to=e.at("to").get<std::string>();
            if(layer[from]=="domain" && layer[to]=="adapter" && e.at("via_port")==false)ports=false;
            if(e.at("kind")=="event" && e.at("idempotent")==false)dedup=false;
            if(e.at("kind")=="call") {
                int timeout=e.at("timeout_ms");bounded &= timeout>0 && timeout<=r.at("latency_budget_ms").get<int>();
                if(layer[from]!=layer[to])async=false;
            }
        }
        c["schema"]=1;
        for(const auto &kv:std::map<std::string,bool>{{"domain_isolation",ports},{"private_ownership",private_db},{"event_idempotency",dedup},
            {"bounded_calls",bounded},{"async_boundary",async},{"supervision",supervised}})
            if(r.value(kv.first,false))c[kv.first]=kv.second?1.:0.;
    }
    return c;
}
Embedding observe(const json &t) {
    auto c=checks(t);Embedding f{};
    // Stable contract: logical deficits, not hidden target labels. Remaining lanes are reserved.
    std::vector<std::string> order=t.at("domain")=="music" ?
        std::vector<std::string>{"home_return","pitch_range","motif_repetition","duration","divergent_pitch","positional_interval"} :
        std::vector<std::string>{"event_idempotency","supervision","domain_isolation","async_boundary","private_ownership","bounded_calls"};
    for(size_t i=0;i<order.size();++i)f[i]=c.count(order[i])?float(1-c.at(order[i])):0.f;
    f[6]=float(1-mean(c));f[7]=1;
    return f;
}
json apply(const json &t,size_t action) {
    validate(t); auto names=actions(t.at("domain"));require(action<names.size(),"unknown action");
    json out=t;auto &a=out["artifact"];const auto &r=t.at("requirements");
    if(t.at("domain")=="music") {
        auto &ns=a["notes"];int lo=r.at("pitch_min"),hi=r.at("pitch_max");
        if(action==0)for(auto &n:ns)n["pitch"]=nearest(pitch_class(n.at("pitch")),n.at("pitch"),lo,hi);
        if(action==1) {
            int ticks=r.at("target_ticks"), n=int(ns.size());
            for(int i=0;i<n;++i)ns[i]["duration"]=ticks/n+(i<ticks%n?1:0);
        }
        if(action==2)ns.back()["pitch"]=nearest(r.at("home_pc"),ns.back().at("pitch"),lo,hi);
        if(action==3 && r.contains("answer_interval"))for(size_t i=0;i<ns.size();++i)
            ns[i]["pitch"]=nearest(r.at("anchors")[i].get<int>()+r.at("answer_interval").get<int>(),ns[i].at("pitch"),lo,hi);
        if(action==4 && r.contains("divergence_pc"))ns[1]["pitch"]=nearest(r.at("divergence_pc"),ns[1].at("pitch"),lo,hi);
        if(action==5)for(size_t i=ns.size()/2;i<ns.size();++i)ns[i]=ns[i-ns.size()/2];
    } else {
        auto &nodes=a["components"],&edges=a["edges"];
        std::map<std::string,std::string> layers;for(const auto &n:nodes)layers[n.at("id")]=n.at("layer");
        if(action==0)for(auto &e:edges)if(layers[e.at("from")]=="domain" && layers[e.at("to")]=="adapter")e["via_port"]=true;
        if(action==1)for(auto &n:nodes)for(size_t i=0;i<n["writes"].size();++i)n["writes"][i]=n.at("id").get<std::string>()+"-owned-"+std::to_string(i);
        if(action==2)for(auto &e:edges)if(e.at("kind")=="event")e["idempotent"]=true;
        if(action==3)for(auto &e:edges)if(e.at("kind")=="call")e["timeout_ms"]=r.at("latency_budget_ms");
        if(action==4)for(auto &e:edges)if(layers[e.at("from")]!=layers[e.at("to")])e["kind"]="event";
        if(action==5)for(auto &n:nodes)if(n.at("stateful")==true)n["supervised"]=true;
    }
    validate(out);return out;
}
json assess(const json &t) {
    auto c=checks(t);json options=json::array();auto names=actions(t.at("domain"));
    double best=mean(c);size_t selected=names.size()-1;
    for(size_t i=0;i<names.size()-1;++i) {
        auto next=apply(t,i);double score=mean(checks(next));
        options.push_back({{"action",i},{"name",names[i]},{"score",score}});
        if(score>best+1e-9){best=score;selected=i;}
    }
    return {{"features",observe(t)},{"checks",c},{"score",mean(c)},{"options",options},{"target_action",selected},
        {"next_task",apply(t,selected)},{"feature_schema",t.at("domain").get<std::string>()+".deficits/1"},
        {"evaluator_id","wds.ai.heart."+t.at("domain").get<std::string>()+"/1"}};
}
heart::Evaluation evaluate(const json &t,const heart::EvaluationContext &ctx) {
    heart::Evaluation e;e.metrics=checks(t);e.score=float(mean(e.metrics));
    e.status=e.score>=1.f?heart::Status::pass:heart::Status::fail;
    auto domain=t.at("domain").get<std::string>();e.evaluator_id="wds.ai.heart."+domain;e.aggregation_policy="all_declared_task_constraints/1";
    e.scope=domain=="music"?"pitch/rhythm and declared symbolic motif constraints; no emotional or aesthetic judgment":
        "declared graph/ownership/delivery fixtures; no production latency or overall design-quality claim";
    e.feedback=e.passed()?"All declared task constraints passed.":"Failed checks:";
    for(const auto &kv:e.metrics)if(kv.second<1)e.feedback+=" "+kv.first;
    e.snapshot_sequence=ctx.heart.sequence();e.represented_souls={ctx.heart.root_id()};e.considered_souls=e.represented_souls;
    e.evidence={{"task",t.at("id"),checksum(t.dump()),1,ctx.heart.sequence()},
        {"requirements",t.at("id"),checksum(t.at("requirements").dump()),1,ctx.heart.sequence()}};
    return e;
}
heart::Evaluation DomainEvaluator::evaluate(const std::string &proposal,const heart::EvaluationContext &ctx) const {
    auto task=ctx.task;
    require(task.is_object() && task.at("domain").get<std::string>()==domain_,"domain evaluator requires matching task context");
    task["artifact"]=json::parse(proposal);
    return tasks::evaluate(task,ctx);
}
Decision FieldPolicy::decide(const json &t,const winds::Package &p) const {
    auto features=observe(t);auto state=p.step(State{},features,0);
    auto count=actions(t.at("domain")).size();size_t action=0;
    for(size_t i=1;i<count;++i)if(state[i]>state[action])action=i;
    return {action,state};
}
} // namespace wds::ai::tasks
