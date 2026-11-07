#include "wds/ai/core.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#ifdef WDS_GGML_TRAIN
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include "ggml-opt.h"
#endif
using namespace wds::ai;
namespace {
struct Row { State features; int target; };
json read(const fs::path &path,size_t limit=4*1024*1024) {
    require(fs::file_size(path)<=limit,"training input exceeds size limit");std::ifstream file(path);json value;file>>value;return value;
}
void atomic_json(const fs::path &path,const json &value) {
    auto tmp=path;tmp+=".tmp";{std::ofstream out(tmp,std::ios::binary|std::ios::trunc);out<<value.dump(2)<<'\n';out.flush();require(bool(out),"checkpoint write failed");}
#ifdef _WIN32
    require(MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,"checkpoint replacement failed");
#else
    fs::rename(tmp,path);
#endif
}
std::vector<Row> rows(const json &values) {
    require(values.is_array() && values.size()>=2 && values.size()<=4096,"need 2..4096 practice rows per split");
    std::vector<Row> out;
    for(const auto &v:values) {
        require(v.at("features").is_array() && v.at("features").size()==16,"expected 16 features");
        Row r{v.at("features").get<State>(),v.at("target").get<int>()};
        require(v.at("target").is_number_integer() && r.target>=0 && r.target<7,"invalid target action");
        for(float x:r.features)require(std::isfinite(x) && std::abs(x)<=1,"invalid feature");out.push_back(r);
    }
    return out;
}
winds::Policy weights(const json &j) {
    require(j.at("weight").size()==256 && j.at("bias").size()==16,"invalid weight shape");
    winds::Policy p{j.at("weight").get<std::array<float,256>>(),j.at("bias").get<State>()};
    for(float x:p.weight)require(std::isfinite(x) && std::abs(x)<=4,"invalid weight bound");
    for(float x:p.bias)require(std::isfinite(x) && std::abs(x)<=4,"invalid bias bound");return p;
}
json weight_record(const winds::Policy &p){return {{"weight",p.weight},{"bias",p.bias}};}
State predict(const winds::Policy &p,const State &x) {
    State logits{};for(size_t o=0;o<16;++o){logits[o]=p.bias[o];for(size_t i=0;i<16;++i)logits[o]+=p.weight[o*16+i]*x[i];}return logits;
}
double loss(const winds::Policy &p,const std::vector<Row> &data) {
    double sum=0;for(const auto &r:data){auto z=predict(p,r.features);double max=*std::max_element(z.begin(),z.end()),denom=0;
        for(float v:z)denom+=std::exp(v-max);sum+=std::log(denom)+max-z[r.target];}return sum/data.size();
}
void clip(winds::Policy &p){for(auto &w:p.weight)w=std::clamp(w,-4.f,4.f);for(auto &b:p.bias)b=std::clamp(b,-4.f,4.f);}
void scalar_epoch(winds::Policy &p,const std::vector<Row> &data,float rate) {
    for(const auto &r:data){auto z=predict(p,r.features);float max=*std::max_element(z.begin(),z.end()),sum=0;
        for(float &v:z){v=std::exp(v-max);sum+=v;}
        for(size_t o=0;o<16;++o){float gradient=z[o]/sum-(int(o)==r.target?1.f:0.f);
            p.bias[o]-=rate*gradient;for(size_t i=0;i<16;++i)p.weight[o*16+i]-=rate*gradient*r.features[i];}}
    clip(p);
}
#ifdef WDS_GGML_TRAIN
class Ggml {
    ggml_backend_t backend_=nullptr;
    ggml_backend_sched_t scheduler_=nullptr;
    ggml_context *persistent_=nullptr,*compute_=nullptr;
    ggml_backend_buffer_t buffer_=nullptr;
    ggml_opt_dataset_t dataset_=nullptr;
    ggml_opt_context_t optimizer_=nullptr;
    ggml_tensor *w_=nullptr,*b_=nullptr;
    ggml_opt_optimizer_params settings_{};
    size_t count_;
  public:
    Ggml(const winds::Policy &p,const std::vector<Row> &data,float rate):count_(data.size()) {
        backend_=ggml_backend_cpu_init();require(backend_,"ggml CPU backend unavailable");ggml_backend_cpu_set_n_threads(backend_,1);
        scheduler_=ggml_backend_sched_new(&backend_,nullptr,1,GGML_DEFAULT_GRAPH_SIZE,false,true);
        persistent_=ggml_init({8*ggml_tensor_overhead(),nullptr,true});
        compute_=ggml_init({GGML_DEFAULT_GRAPH_SIZE*ggml_tensor_overhead()+3*ggml_graph_overhead(),nullptr,true});
        auto input=ggml_new_tensor_2d(persistent_,GGML_TYPE_F32,16,1);ggml_set_name(input,"task_features");
        w_=ggml_new_tensor_2d(persistent_,GGML_TYPE_F32,16,16);ggml_set_name(w_,"wind_weight");ggml_set_param(w_);
        b_=ggml_new_tensor_1d(persistent_,GGML_TYPE_F32,16);ggml_set_name(b_,"wind_bias");ggml_set_param(b_);
        auto logits=ggml_add(compute_,ggml_mul_mat(compute_,w_,input),b_);ggml_set_name(logits,"action_logits");
        buffer_=ggml_backend_alloc_ctx_tensors(persistent_,backend_);require(buffer_,"ggml allocation failed");set(p);
        dataset_=ggml_opt_dataset_init(GGML_TYPE_F32,GGML_TYPE_F32,16,16,int64_t(data.size()),1);
        float *features=ggml_get_data_f32(ggml_opt_dataset_data(dataset_)),*labels=ggml_get_data_f32(ggml_opt_dataset_labels(dataset_));
        for(size_t n=0;n<data.size();++n){for(size_t i=0;i<16;++i){features[n*16+i]=data[n].features[i];labels[n*16+i]=int(i)==data[n].target?1.f:0.f;}}
        settings_=ggml_opt_get_default_optimizer_params(nullptr);settings_.sgd.alpha=rate;settings_.sgd.wd=0;
        auto options=ggml_opt_default_params(scheduler_,GGML_OPT_LOSS_TYPE_CROSS_ENTROPY);
        options.ctx_compute=compute_;options.inputs=input;options.outputs=logits;options.opt_period=1;
        options.optimizer=GGML_OPT_OPTIMIZER_TYPE_SGD;options.get_opt_pars=ggml_opt_get_constant_optimizer_params;options.get_opt_pars_ud=&settings_;
        optimizer_=ggml_opt_init(options);
    }
    void set(const winds::Policy &p){ggml_backend_tensor_set(w_,p.weight.data(),0,sizeof(p.weight));ggml_backend_tensor_set(b_,p.bias.data(),0,sizeof(p.bias));}
    void epoch(winds::Policy &p){ggml_opt_epoch(optimizer_,dataset_,nullptr,nullptr,int64_t(count_),nullptr,nullptr);
        ggml_backend_tensor_get(w_,p.weight.data(),0,sizeof(p.weight));ggml_backend_tensor_get(b_,p.bias.data(),0,sizeof(p.bias));clip(p);set(p);}
    ~Ggml(){if(optimizer_)ggml_opt_free(optimizer_);if(dataset_)ggml_opt_dataset_free(dataset_);if(scheduler_)ggml_backend_sched_free(scheduler_);
        if(buffer_)ggml_backend_buffer_free(buffer_);if(compute_)ggml_free(compute_);if(persistent_)ggml_free(persistent_);if(backend_)ggml_backend_free(backend_);}
};
#endif
std::string binary(const winds::Policy &p) {
    std::string data(1152,'\0');data.replace(0,8,std::string("WDSWND1\0",8));
    auto write=[&](size_t off,uint32_t value){for(int i=0;i<4;++i)data[off+i]=char((value>>(8*i))&255);};
    write(8,1);write(12,16);write(16,64);write(20,272);
    for(size_t i=0;i<256;++i)write(64+4*i,std::bit_cast<uint32_t>(p.weight[i]));
    for(size_t i=0;i<16;++i)write(1088+4*i,std::bit_cast<uint32_t>(p.bias[i]));return data;
}
}
int main(int argc,char **argv) {
    try {
        if(argc==3 && std::string(argv[1])=="--language-job") return language::train(read(argv[2]));
        if(argc==3 && std::string(argv[1])=="--language-evaluate") {std::cout<<language::evaluate(read(argv[2]))<<'\n';return 0;}
        if(argc==3 && std::string(argv[1])=="--evaluate") {
            const auto job=read(argv[2]);winds::Package package(field(job,"package",2048));
            const bool language_package=package.manifest.value("feature_schema",std::string{})==cognition::language_schema;
            const bool expression_package=package.manifest.value("feature_schema",std::string{})==expression::feature_schema;
            require(language_package || expression_package || package.manifest.value("feature_schema",std::string{})==neural::feature_schema,"evaluation requires a neural or expression readout package");
            const auto data=rows(job.at("rows"));size_t correct=0,accepted=0,accepted_correct=0;
            const auto names=language_package?cognition::language_actions():expression_package?expression::actions():neural::actions();
            json classes=json::object();for(const auto &name:names)classes[name]={{"rows",0},{"correct",0},{"accepted",0}};
            json node={{"minimum_strength",job.value("minimum_strength",0.7)},{"minimum_margin",job.value("minimum_margin",0.1)}};
            for(const auto &row:data){
                auto logits=predict(package.policy,row.features);auto best=std::max_element(logits.begin(),logits.begin()+names.size())-logits.begin();
                float maximum=logits[best],sum=0,second=0;
                for(size_t i=0;i<names.size();++i){logits[i]=std::exp(logits[i]-maximum);sum+=logits[i];}
                for(size_t i=0;i<names.size();++i)if(int(i)!=best)second=std::max(second,logits[i]/sum);
                const bool uncertain=logits[best]/sum<node.at("minimum_strength").get<float>() || logits[best]/sum-second<node.at("minimum_margin").get<float>();
                require(row.target<int(names.size()),"neural target outside action schema");
                auto &c=classes[names.at(row.target)];c["rows"]=c.at("rows").get<int>()+1;
                if(best==row.target){++correct;c["correct"]=c.at("correct").get<int>()+1;}
                if(!uncertain){++accepted;c["accepted"]=c.at("accepted").get<int>()+1;if(best==row.target)++accepted_correct;}}
            std::cout<<json({{"rows",data.size()},{"correct",correct},{"accuracy",double(correct)/data.size()},
                {"accepted",accepted},{"accepted_correct",accepted_correct},{"coverage",double(accepted)/data.size()},
                {"classes",classes},{"feature_schema",package.manifest.at("feature_schema")},{"scope","readout classification; expression source contract is checked separately at runtime"},{"generated_tokens",0},{"knowledge_calls",0},{"activated",false}})<<'\n';return 0;
        }
        if(argc==2 && std::string(argv[1])=="--capabilities") {
#ifdef WDS_GGML_TRAIN
            std::cout<<json({{"default_backend","ggml-sgd"},{"backends",{"ggml-sgd","scalar-sgd"}},{"abi",1}})<<'\n';
#else
            std::cout<<json({{"default_backend","scalar-sgd"},{"backends",{"scalar-sgd"}},{"abi",1}})<<'\n';
#endif
            return 0;
        }
        require(argc==3 && std::string(argv[1])=="--job","usage: wds-train --job job.json");
        auto job=read(argv[2]);auto train=rows(job.at("train")),validation=rows(job.at("validation"));
        auto domain=field(job,"domain",32);auto action_names=domain=="language"?cognition::language_actions():domain=="neural"?neural::actions():domain=="expression"?expression::actions():tasks::actions(domain);
        if(domain=="language") {
            field(job,"representation_id",256);
            require(job.value("feature_schema",std::string{})==cognition::language_schema,"language feature schema mismatch");
            std::set<std::string> texts;
            for(auto split:{"train","validation"}) for(const auto &row:job.at(split)) {
                require(row.at("representation_id")==job.at("representation_id"),"mixed model representations");
                require(texts.insert(field(row,"text",4096)).second,"duplicate language text across splits");
                require(row.at("target").get<int>()<3,"invalid language label");
            }
        }
        int epochs=job.value("epochs",40);float rate=job.value("learning_rate",0.05f);
        require(epochs>=1 && epochs<=200 && std::isfinite(rate) && rate>0 && rate<=0.2,"invalid optimizer budget");
        winds::Package parent(field(job,"parent",2048));
        auto state=parent.policy,best=state;double best_loss=loss(state,validation);int start=0,best_epoch=0;
        fs::path output=field(job,"output",2048);fs::create_directories(output.parent_path());auto checkpoint=output;checkpoint+=".checkpoint.json";
#ifdef WDS_GGML_TRAIN
        auto backend=job.value("backend",std::string("ggml-sgd"));
#else
        auto backend=job.value("backend",std::string("scalar-sgd"));
#endif
        require(backend=="scalar-sgd" || backend=="ggml-sgd","unknown optimizer backend");
#ifndef WDS_GGML_TRAIN
        require(backend=="scalar-sgd","ggml trainer requires WDS_GGML_TRAIN=ON or WDS_LLAMA=ON");
#endif
        const auto fingerprint=checksum(job.dump()+parent.digest+backend);
        if(fs::exists(checkpoint)) {
            auto cp=read(checkpoint);require(cp.at("fingerprint")==fingerprint,"training checkpoint/config conflict");
            state=weights(cp.at("weights"));best=weights(cp.at("best_weights"));best_loss=cp.at("best_loss");start=cp.at("epoch");best_epoch=cp.at("best_epoch");
        }
#ifdef WDS_GGML_TRAIN
        std::unique_ptr<Ggml> ggml;if(backend=="ggml-sgd")ggml=std::make_unique<Ggml>(state,train,rate);
#endif
        for(int epoch=start;epoch<epochs;++epoch) {
#ifdef WDS_GGML_TRAIN
            if(ggml)ggml->epoch(state);else
#endif
                scalar_epoch(state,train,rate);
            double value=loss(state,validation);require(std::isfinite(value),"non-finite loss");
            if(value<best_loss){best=state;best_loss=value;best_epoch=epoch+1;}
            atomic_json(checkpoint,{{"fingerprint",fingerprint},{"epoch",epoch+1},{"weights",weight_record(state)},
                {"best_weights",weight_record(best)},{"best_loss",best_loss},{"best_epoch",best_epoch},{"backend",backend}});
            std::cout<<json({{"type","training.progress"},{"epoch",epoch+1},{"epochs",epochs},{"validation_cross_entropy",value},{"backend",backend}})<<std::endl;
        }
        auto bytes=binary(best);auto manifest=parent.manifest;
        manifest["id"]="wds.ai.winds."+domain+"."+fingerprint;manifest["policy_version"]=parent.manifest.at("policy_version").get<int>()+1;
        manifest["training_version"]=manifest["policy_version"];manifest["checksum_fnv1a64"]=checksum(bytes);manifest["maturity"]="seeded";
        manifest["action_domain"]=domain;manifest["feature_schema"]=domain=="neural"?neural::feature_schema:domain+".deficits/1";manifest["action_schema"]=domain=="neural"?"neural.impulses/1":domain+".repairs/1";
        manifest["actions"]=action_names;manifest["evaluator_version"]="wds.ai.heart."+domain+"/1";
        manifest["action_decoder"]="argmax(first 7 outputs of step(z=0, features, feedback=0)); reset state per decision";
        if(domain=="neural")manifest["action_decoder"]="linear logits W*x+b; softmax first 7; explicit strength and margin thresholds; no activation";
        if(domain=="language") {
            manifest["feature_schema"]=cognition::language_schema;manifest["action_schema"]="language.intents/1";
            manifest["representation_id"]=job.at("representation_id");
            manifest["action_decoder"]="signed projection/1; softmax first 3; min strength .75 and margin .25; read-only intents";
            manifest["foundation_contract"]="frozen Qwen3 representation only; no generation";
        }
        if(domain=="expression") {
            manifest["feature_schema"]=expression::feature_schema;manifest["action_schema"]="expression.plans/1";
            manifest["action_decoder"]="linear logits W*x+b; softmax first 7; source contract gates realization; no activation";
        }
        manifest["training"]={{"backend",backend},{"objective","cross entropy on graded one-step action imitation"},{"fingerprint",fingerprint},
            {"parent_checksum",parent.digest},{"best_epoch",best_epoch},{"validation_cross_entropy",best_loss},{"activation","explicit command only"}};
        manifest["promotion"]="candidate only; benchmark gates and activation are separate";
        if(fs::exists(output)) {
            winds::Package existing(output);require(existing.manifest==manifest && existing.digest==checksum(bytes),"immutable candidate conflict");
        } else {
            auto staging=output;staging+=".pending";
            // Only this deterministic output's temporary staging directory is recoverable.
            if(fs::exists(staging))fs::remove_all(staging);fs::create_directories(staging);
            {std::ofstream file(staging/"policy.wind",std::ios::binary);file.write(bytes.data(),bytes.size());file.flush();require(bool(file),"policy write failed");}
            atomic_json(staging/"weights.json",weight_record(best));atomic_json(staging/"manifest.json",manifest);
            winds::Package verify(staging);require(verify.digest==checksum(bytes),"candidate validation failed");fs::rename(staging,output);
        }
        std::cout<<json({{"type","training.finished"},{"output",output.string()},{"checksum",checksum(bytes)},{"backend",backend},
            {"best_epoch",best_epoch},{"validation_cross_entropy",best_loss},{"activated",false}})<<std::endl;return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
