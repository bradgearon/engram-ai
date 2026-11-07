#include "wds/ai/core.hpp"
#include "wds/ai/language.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <numeric>
#include <random>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace wds::ai::language {
namespace {
using Prob = std::array<float, vocab>;
struct Step { Hidden previous{}, state{}; Prob probability{}; unsigned input=0, target=0; };
Hidden advance(const Weights &w, const Hidden &previous, unsigned input) {
    Hidden out{};
    for(size_t i=0;i<hidden;++i) {
        float x=w.hidden_bias[i]+w.embedding[input*hidden+i];
        for(size_t j=0;j<hidden;++j)x+=w.recurrent[i*hidden+j]*previous[j];
        out[i]=std::tanh(x);
    }
    return out;
}
Prob predict(const Weights &w, const Hidden &h) {
    Prob p{};float maximum=-1e30f,sum=0;
    for(size_t o=0;o<vocab;++o){float x=w.output_bias[o];for(size_t i=0;i<hidden;++i)x+=w.output[o*hidden+i]*h[i];p[o]=x;maximum=std::max(maximum,x);}
    for(auto &v:p){v=std::exp(v-maximum);sum+=v;}for(auto &v:p)v/=sum;return p;
}
std::vector<float> flatten(const Weights &w) {
    std::vector<float> f;
    for(auto *a:{&w.embedding,&w.output})f.insert(f.end(),a->begin(),a->end());
    f.insert(f.end(),w.recurrent.begin(),w.recurrent.end());
    f.insert(f.end(),w.hidden_bias.begin(),w.hidden_bias.end());f.insert(f.end(),w.output_bias.begin(),w.output_bias.end());return f;
}
void assign(Weights &w,const std::vector<float> &f) {
    require(f.size()==vocab*hidden*2+hidden*hidden+hidden+vocab,"language tensor shape mismatch");size_t n=0;
    auto copy=[&](auto &a){for(auto &x:a)x=f[n++];};copy(w.embedding);copy(w.output);copy(w.recurrent);copy(w.hidden_bias);copy(w.output_bias);
}
std::string binary(const Weights &w) {
    const auto f=flatten(w);std::string out(64+f.size()*4,'\0');out.replace(0,8,std::string("WDSSEQ1\0",8));
    auto put=[&](size_t off,uint32_t v){for(int i=0;i<4;++i)out[off+i]=char(v>>(8*i));};
    put(8,1);put(12,hidden);put(16,vocab);put(20,uint32_t(f.size()));put(24,64);
    for(size_t i=0;i<f.size();++i)put(64+4*i,std::bit_cast<uint32_t>(f[i]));
    return out;
}
void decode(const std::string &bytes, Weights &w) {
    auto get=[&](size_t off){uint32_t v=0;for(int i=0;i<4;++i)v|=uint32_t(uint8_t(bytes.at(off+i)))<<(8*i);return v;};
    require(bytes.size()==64+4*(vocab*hidden*2+hidden*hidden+hidden+vocab) && bytes.substr(0,8)==std::string("WDSSEQ1\0",8),"invalid sequence tensor file");
    require(get(8)==1 && get(12)==hidden && get(16)==vocab && get(24)==64,"incompatible language architecture");
    std::vector<float> f;for(size_t off=64;off<bytes.size();off+=4){auto v=std::bit_cast<float>(get(off));require(std::isfinite(v)&&std::abs(v)<=4,"invalid language weight");f.push_back(v);}
    require(get(20)==f.size(),"language tensor count mismatch");assign(w,f);
}
std::string bytes(const fs::path &p,size_t limit=64*1024*1024) {
    require(fs::file_size(p)<=limit,"language input exceeds byte limit");std::ifstream f(p,std::ios::binary);require(bool(f),"cannot read language file");return {(std::istreambuf_iterator<char>(f)),{}};
}
void write(const fs::path &p,const std::string &data) {
    std::ofstream f(p,std::ios::binary|std::ios::trunc);f.write(data.data(),data.size());f.flush();require(bool(f),"language file write failed");
}
std::string conversation(const json &row) {
    require(row.at("turns").is_array() && row.at("turns").size()>=2 && row.at("turns").size()<=32,"conversation needs 2..32 turns");
    std::string s;
    for(const auto &t:row.at("turns")){auto role=field(t,"role",16);require(role=="user"||role=="assistant","invalid curriculum role");auto text=field(t,"text",8192);
        require(text.find('\x1e')==std::string::npos&&text.find('\x1f')==std::string::npos,"reserved language separator in text");
        s+='\x1e';s+=role=="user"?'U':'A';s+='\n';s+=text;s+='\x1f';}
    require(s.size()<=16384,"conversation byte limit exceeded");return s;
}
struct Row { std::string id, group, text; };
std::vector<Row> rows(const json &dataset,const std::string &split) {
    std::vector<Row> out;size_t total=0;
    for(const auto &r:dataset.at("conversations"))if(r.at("split")==split){auto s=conversation(r);total+=s.size();out.push_back({field(r,"id",128),field(r,"group",128),s});}
    require(!out.empty()&&out.size()<=16384&&total<=32*1024*1024,"invalid curriculum split size");return out;
}
void validate(const json &data) {
    require(data.at("schema")=="english.conversations/1","invalid conversation corpus schema");
    std::map<std::string,std::string> groups;std::set<std::string> ids,contents;
    for(const auto &r:data.at("conversations")){auto split=field(r,"split",16),g=field(r,"group",128);require(split=="train"||split=="validation"||split=="test","invalid split");
        require(ids.insert(field(r,"id",128)).second,"duplicate conversation id");require(contents.insert(checksum(conversation(r))).second,"duplicate conversations leak across splits");
        require(!groups.count(g)||groups.at(g)==split,"conversation family leakage across splits");groups[g]=split;}
}
json metrics(const Weights &w,const std::vector<Row> &data) {
    double loss=0;size_t tokens=0,correct=0;
    for(const auto &r:data){Hidden h{};for(size_t n=0;n+1<r.text.size();++n){h=advance(w,h,uint8_t(r.text[n]));auto p=predict(w,h);unsigned target=uint8_t(r.text[n+1]);loss-=std::log(std::max(1e-12f,p[target]));correct+=size_t(std::max_element(p.begin(),p.end())-p.begin())==target;++tokens;}}
    return {{"predicted_bytes",tokens},{"cross_entropy",loss/tokens},{"bits_per_byte",loss/tokens/std::log(2.)},{"next_byte_accuracy",double(correct)/tokens},{"scope","sequence prediction, not a test of general understanding or factual correctness"}};
}
void epoch(Weights &w,const std::vector<Row> &data,float rate,int epoch_index,std::ostream &learning_trace) {
    // The tensor is roughly 145 KiB. Keep it off the limited Windows stack,
    // and reuse one aligned heap allocation throughout the epoch.
    auto gradient=std::make_unique<Weights>();
    std::vector<size_t> order(data.size());std::iota(order.begin(),order.end(),0);std::mt19937 random(1409+epoch_index);std::shuffle(order.begin(),order.end(),random);
    size_t completed=0;
    for(auto ri:order){const auto &r=data[ri];Hidden h{};
        for(size_t start=0;start+1<r.text.size();start+=32){std::vector<Step> tape;
            for(size_t n=start;n<std::min(start+32,r.text.size()-1);++n){Step s;s.input=uint8_t(r.text[n]);s.target=uint8_t(r.text[n+1]);s.previous=h;h=advance(w,h,s.input);s.state=h;s.probability=predict(w,h);tape.push_back(s);}
            auto &grad=*gradient;grad.embedding.fill(0);grad.recurrent.fill(0);grad.output.fill(0);grad.hidden_bias.fill(0);grad.output_bias.fill(0);Hidden future{};
            for(auto it=tape.rbegin();it!=tape.rend();++it){const auto &s=*it;auto dz=s.probability;dz[s.target]-=1;Hidden dh=future;
                for(size_t o=0;o<vocab;++o){grad.output_bias[o]+=dz[o];for(size_t i=0;i<hidden;++i){grad.output[o*hidden+i]+=dz[o]*s.state[i];dh[i]+=w.output[o*hidden+i]*dz[o];}}
                Hidden local{};future.fill(0);
                for(size_t i=0;i<hidden;++i){local[i]=dh[i]*(1-s.state[i]*s.state[i]);grad.hidden_bias[i]+=local[i];grad.embedding[s.input*hidden+i]+=local[i];
                    for(size_t j=0;j<hidden;++j){grad.recurrent[i*hidden+j]+=local[i]*s.previous[j];future[j]+=w.recurrent[i*hidden+j]*local[i];}}
            }
            json touched=json::array();
            if(start==0){const auto &last=tape.back();std::vector<size_t> units(hidden);std::iota(units.begin(),units.end(),0);
                std::partial_sort(units.begin(),units.begin()+8,units.end(),[&](auto a,auto b){return std::abs(grad.output[last.target*hidden+a])>std::abs(grad.output[last.target*hidden+b]);});
                for(size_t k=0;k<8;++k){auto index=last.target*hidden+units[k];touched.push_back({{"output_weight_index",index},{"before",w.output[index]},{"gradient",grad.output[index]/float(tape.size())},{"unit",units[k]},{"activation",last.state[units[k]]}});}}
            auto apply=[&](auto &a,const auto &g){for(size_t i=0;i<a.size();++i)a[i]=std::clamp(a[i]-rate*std::clamp(g[i]/float(tape.size()),-1.f,1.f),-4.f,4.f);};
            apply(w.embedding,grad.embedding);apply(w.recurrent,grad.recurrent);apply(w.output,grad.output);apply(w.hidden_bias,grad.hidden_bias);apply(w.output_bias,grad.output_bias);
            if(start==0){for(auto &edge:touched)edge["after"]=w.output[edge.at("output_weight_index").get<size_t>()];
                learning_trace<<json({{"schema","language.weight-update/1"},{"example_id",r.id},{"group",r.group},{"epoch",epoch_index+1},{"chunk",0},{"hidden_state",tape.back().state},{"touched",touched},
                    {"sampling","first 32-symbol chunk of every conversation; 8 largest target-row gradients"}}).dump()<<'\n';}

        }
        if(++completed%32==0)std::cout<<json({{"type","language.batch"},{"epoch",epoch_index+1},{"conversations",completed},{"total",data.size()}})<<std::endl;
    }
}
}
std::shared_ptr<const Model> load(const fs::path &dir,const json &manifest) {
    if(manifest.value("feature_schema",std::string{})!=schema)return {};
    require(manifest.at("sequence").at("file")=="language.tensor","invalid language tensor path");
    require(manifest.at("sequence").at("hidden")==hidden && manifest.at("sequence").at("vocabulary")==vocab && manifest.at("sequence").at("alignment")==64,"language metadata shape mismatch");
    auto b=bytes(dir/"language.tensor",1024*1024);auto model=std::make_shared<Model>();model->digest=checksum(b);
    require(model->digest==manifest.at("sequence").at("checksum_fnv1a64").get<std::string>(),"language tensor checksum mismatch");decode(b,model->weights);model->metadata=manifest.at("sequence");return model;
}
std::string encode_context(const json &turns,const std::string &input) {
    json row={{"turns",turns}};row["turns"].push_back({{"role","user"},{"text",input}});
    // The serialization is an explicit wire format, not a natural-language classifier.
    std::string s;for(const auto &t:row.at("turns")){auto role=t.at("role").get<std::string>();if(role!="user"&&role!="assistant")continue;
        auto text=t.at("text").get<std::string>();require(text.find('\x1e')==std::string::npos&&text.find('\x1f')==std::string::npos,"reserved sequence separator in user input");
        s+='\x1e';s+=role=="user"?'U':'A';s+='\n';s+=text;s+='\x1f';}
    s+="\x1e" "A\n";require(s.size()<=16384,"language context exceeds 16 KiB; start a shorter conversation context");return s;
}
json generate(const Model &m,const std::string &context,size_t limit) {
    require(!context.empty()&&context.size()<=16384&&limit>0&&limit<=4096,"invalid language generation budget");Hidden h{};
    for(unsigned char c:context)h=advance(m.weights,h,c);
    json trajectory=json::array();std::string out;double surprise=0;bool ended=false;size_t count=0;
    for(size_t i=0;i<limit;++i){auto p=predict(m.weights,h);auto token=size_t(std::max_element(p.begin(),p.end())-p.begin());surprise-=std::log(std::max(1e-12f,p[token]));++count;
        if(i<16){json active=json::array();std::vector<size_t> order(hidden);std::iota(order.begin(),order.end(),0);std::partial_sort(order.begin(),order.begin()+8,order.end(),[&](auto a,auto b){return std::abs(h[a])>std::abs(h[b]);});
            for(size_t k=0;k<8;++k){auto n=order[k];active.push_back({{"unit",n},{"activation",h[n]},{"output_weight_index",token*hidden+n},{"weight",m.weights.output[token*hidden+n]},{"contribution",h[n]*m.weights.output[token*hidden+n]}});}
            trajectory.push_back({{"step",i},{"byte",token},{"probability",p[token]},{"units",active}});}
        if(token==31){ended=true;break;}out+=char(token);h=advance(m.weights,h,unsigned(token));
    }
    bool utf8=true;try{(void)json(out).dump();}catch(const std::exception&){utf8=false;}
    return {{"text",utf8?out:std::string{}},{"complete",ended&&utf8},{"valid_utf8",utf8},{"generated_bytes",out.size()},
        {"mean_selected_probability",std::exp(-surprise/std::max(size_t(1),count))},{"weight_trace",trajectory},{"tensor_checksum",m.digest},
        {"reason",!ended?"sequence byte budget exhausted":!utf8?"student emitted invalid UTF-8":"end-of-turn predicted"}};
}
int train(const json &job) {
    std::string phase="loading curriculum";
    auto stage=[&](const char *name){phase=name;std::cout<<json({{"type","language.stage"},{"stage",phase},{"activated",false}})<<std::endl;};
    try {
    stage("loading curriculum");
    const auto data=json::parse(bytes(field(job,"curriculum",2048)));validate(data);auto training=rows(data,"train"),validation=rows(data,"validation");
    int epochs=job.value("epochs",8);float rate=job.value("learning_rate",.08f);require(epochs>0&&epochs<=200&&rate>0&&rate<=.2f,"invalid sequence optimizer budget");
    auto fingerprint=checksum(data.dump()+job.dump());fs::path output=field(job,"output",2048),checkpoint=output;checkpoint+=".checkpoint";fs::create_directories(output.parent_path());
    auto current=std::make_unique<Weights>();auto best_model=std::make_unique<Model>();
    auto &w=*current;auto &best=best_model->weights;
    std::mt19937 random(1409);std::uniform_real_distribution<float> init(-.08f,.08f);for(auto &v:w.embedding)v=init(random);for(auto &v:w.output)v=init(random);for(size_t i=0;i<hidden;++i)w.recurrent[i*hidden+i]=.4f;
    best=w;double best_loss=1e30;int start=0,best_epoch=0;json before;
    if(fs::exists(checkpoint/"state.json")){stage("restoring checkpoint");auto c=json::parse(bytes(checkpoint/"state.json"));require(c.at("fingerprint")==fingerprint,"sequence checkpoint/config conflict");before=c.at("initial_validation");start=c.at("epoch");best_epoch=c.at("best_epoch");best_loss=c.at("best_loss");decode(bytes(checkpoint/("epoch-"+std::to_string(start)+".tensor")),w);decode(bytes(checkpoint/("epoch-"+std::to_string(best_epoch)+".tensor")),best);
        std::cout<<json({{"type","language.resumed"},{"epoch",start},{"epochs",epochs},{"checkpoint",checkpoint.string()},{"activated",false}})<<std::endl;}
    fs::create_directories(checkpoint);
    if(before.is_null())before=metrics(w,validation);
    if(start<epochs)stage("training epochs");
    for(int e=start;e<epochs;++e){std::ofstream trace(checkpoint/("epoch-"+std::to_string(e+1)+".traces.jsonl"),std::ios::binary|std::ios::trunc);epoch(w,training,rate,e,trace);trace.flush();require(bool(trace),"learning update trace write failed");auto score=metrics(w,validation);double loss=score.at("cross_entropy");require(std::isfinite(loss),"non-finite language loss");
        if(loss<best_loss){best=w;best_loss=loss;best_epoch=e+1;}write(checkpoint/("epoch-"+std::to_string(e+1)+".tensor"),binary(w));
        write(checkpoint/"state.pending",json({{"fingerprint",fingerprint},{"epoch",e+1},{"best_epoch",best_epoch},{"best_loss",best_loss},{"initial_validation",before}}).dump());
#ifdef _WIN32
        require(MoveFileExW((checkpoint/"state.pending").c_str(),(checkpoint/"state.json").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"checkpoint replacement failed");
#else
        fs::rename(checkpoint/"state.pending",checkpoint/"state.json");
#endif
        std::cout<<json({{"type","language.epoch"},{"epoch",e+1},{"epochs",epochs},{"validation",score},{"activated",false}})<<std::endl;
    }
    stage("packaging candidate");
    winds::Package parent(field(job,"parent",2048));auto manifest=parent.manifest;manifest.erase("representation_id");manifest.erase("sequence");
    auto tensor=binary(best);manifest["id"]="wds.ai.winds.english."+fingerprint;manifest["policy_version"]=parent.manifest.at("policy_version").get<unsigned>()+1;manifest["maturity"]="seeded";
    manifest["action_domain"]="language";manifest["feature_schema"]=schema;manifest["action_schema"]="language.bytes/1";manifest["foundation_contract"]="native recurrent next-byte student; Qwen teacher separate";
    manifest["sequence"]={{"file","language.tensor"},{"checksum_fnv1a64",checksum(tensor)},{"architecture","embedding+tanh-recurrence+softmax/1"},{"hidden",hidden},{"vocabulary",vocab},{"alignment",64},{"inference_backend","native-cpu"},{"experimental",true}};
    manifest["sequence"]["tensors"]=json::array({
        json{{"name","embedding"},{"shape",{vocab,hidden}},{"offset_bytes",64},{"dtype","f32-le"}},
        json{{"name","output"},{"shape",{vocab,hidden}},{"offset_bytes",64+4*vocab*hidden},{"dtype","f32-le"}},
        json{{"name","recurrent"},{"shape",{hidden,hidden}},{"offset_bytes",64+8*vocab*hidden},{"dtype","f32-le"}},
        json{{"name","hidden_bias"},{"shape",{hidden}},{"offset_bytes",64+4*(2*vocab*hidden+hidden*hidden)},{"dtype","f32-le"}},
        json{{"name","output_bias"},{"shape",{vocab}},{"offset_bytes",64+4*(2*vocab*hidden+hidden*hidden+hidden)},{"dtype","f32-le"}}});
    manifest["training"]={{"fingerprint",fingerprint},{"curriculum_checksum",checksum(data.dump())},{"backend","native-cpp-tbptt-sgd"},{"objective","next-byte cross entropy"},{"tbptt_steps",32},{"best_epoch",best_epoch},{"validation",metrics(best,validation)},{"initial_validation",before},{"activation","explicit operator only"}};
    manifest["promotion"]="experimental candidate; perplexity is not a general-understanding gate";
    if(fs::exists(output)){stage("validating existing candidate");winds::Package existing(output);require(existing.manifest==manifest,"immutable sequence candidate conflict");}
    else {auto staging=output;staging+=".pending";fs::create_directories(staging);write(staging/"language.tensor",tensor);write(staging/"policy.wind",bytes(fs::path(field(job,"parent",2048))/"policy.wind"));write(staging/"manifest.json",manifest.dump(2));
        stage("copying learning traces");std::ofstream updates(staging/"weight-updates.jsonl",std::ios::binary);
        for(int e=1;e<=epochs;++e)updates<<bytes(checkpoint/("epoch-"+std::to_string(e)+".traces.jsonl"));
        updates.close();require(bool(updates),"weight update archive failed");
        stage("generating activation traces");best_model->digest=checksum(tensor);best_model->metadata=manifest.at("sequence");std::ofstream traces(staging/"learning-traces.jsonl",std::ios::binary);
        for(const auto &r:training){auto cut=r.text.find(std::string("\x1e" "A\n"));if(cut==std::string::npos)continue;auto trace=generate(*best_model,r.text.substr(0,cut+3),16);trace.erase("text");traces<<json({{"example_id",r.id},{"group",r.group},{"source","synthetic curriculum; not user memory"},{"trace",trace}}).dump()<<'\n';}traces.close();require(bool(traces),"learning trace write failed");
        stage("validating candidate");(void)load(staging,manifest);stage("publishing candidate");fs::rename(staging,output);}
    std::cout<<json({{"type","language.finished"},{"output",output.string()},{"fingerprint",fingerprint},{"activated",false},{"validation",manifest["training"]["validation"]}})<<std::endl;return 0;
    } catch(const std::exception &error){throw std::runtime_error("Language training failed during "+phase+": "+error.what()+". Completed checkpoints are retained; retry the same job.");}
}
json evaluate(const json &job) {
    fs::path dir=field(job,"package",2048);auto manifest=json::parse(bytes(dir/"manifest.json"));auto m=load(dir,manifest);require(bool(m),"not a sequence language package");
    auto data=json::parse(bytes(field(job,"curriculum",2048)));validate(data);auto result=metrics(m->weights,rows(data,"test"));result["tensor_checksum"]=m->digest;result["activated"]=false;result["generation_samples"]=json::array();
    size_t tested=0,exact=0,complete=0,accepted=0,accepted_exact=0;std::set<std::string> shown;
    for(const auto &row:data.at("conversations"))if(row.at("split")=="test"){
        auto turns=row.at("turns");auto expected=turns.back().at("text").get<std::string>();turns.erase(turns.size()-1);
        auto input=turns.back().at("text").get<std::string>();turns.erase(turns.size()-1);
        auto prediction=generate(*m,encode_context(turns,input),256);bool correct=prediction.at("text")==expected;
        bool usable=prediction.at("complete")==true && prediction.at("mean_selected_probability").get<double>()>=.9;
        ++tested;exact+=correct;complete+=prediction.at("complete").get<bool>();accepted+=usable;accepted_exact+=usable&&correct;
        if(shown.insert(row.at("group").get<std::string>()).second){prediction.erase("weight_trace");result["generation_samples"].push_back({{"input",input},{"expected",expected},{"result",prediction}});}}
    result["conversation_completions"]={{"tested",tested},{"exact",exact},{"exact_match_rate",double(exact)/tested},{"complete",complete},{"accepted_at_default_threshold",accepted},{"accepted_exact",accepted_exact},
        {"scope","strict exact match on synthetic held-out final turns; paraphrases can be correct without matching"}};
    return result;
}
}
