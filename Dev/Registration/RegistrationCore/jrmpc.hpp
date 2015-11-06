#ifndef JRMPC_HPP
#define JRMPC_HPP
#include <memory>
#include <strstream>
#include "jrmpc.h"
#include "common.h"
namespace Registration {
template<typename M>
JRMPC<M>::JRMPC():RegistrationBase(),
    count(0)
{

}

template<typename M>
bool JRMPC<M>::configure(Info&)
{
    return true;
}

template<typename M>
bool JRMPC<M>::initForThread(void *meshlistptr)
{
    MeshList* list = reinterpret_cast<MeshList*>(meshlistptr);

    if(!list){
        error_string_ = "Can not locate the inputs";
        return false;
    }
    if(list->size()<2){
        error_string_ = "This algorithm is designed for two multiple point clouds";
        return false;
    }

    std::shared_ptr<Info> info(new Info);
    if(!configure(*info))
    {
        error_string_ = "With no proper configure";
        return false;
    }

    typename MeshList::iterator meshIter;
    std::vector<std::shared_ptr<arma::fmat>> source;
    for(meshIter=list->begin();meshIter!=list->end();++meshIter)
    {
        float*data = (float*)(*meshIter) -> mesh_.points();
        int N = (*meshIter) -> mesh_.n_vertices();
        source.push_back(
                    std::shared_ptr<arma::fmat>(new arma::fmat(data,3,N,false,true))
                    );
    }

    initK(source,info->k);
    list->push_back(typename MeshBundle<M>::Ptr(new MeshBundle<M>()));

    while( info->k > list->back()->mesh_.n_vertices() )
    {
        list->back()->mesh_.add_vertex(typename M::Point(0,0,0));
    }

    float* target_data = (float*)list->back()->mesh_.points();
    int target_N = list->back()->mesh_.n_vertices();

    if(target_N!=info->k)
    {
        std::stringstream ss;
        ss<<"Unable to add "<<info->k<<" points to target";
        error_string_ = ss.str();
        return false;
    }

    arma::fmat target(target_data,3,target_N,false,true);
    initX(source,target);
    reset(source,target,info);
    setVarColor((uint32_t*)list->back()->custom_color_.vertex_colors(),list->back()->mesh_.n_vertices());
    return true;
}

template<typename M>
void JRMPC<M>::reset(
        const std::vector<std::shared_ptr<arma::fmat>>&source,
        const arma::fmat&target,
        InfoPtr&info
        )
{
    count = 0;
    float* target_data = (float*)target.memptr();
    int r = target.n_rows;
    int c = target.n_cols;
    X_ptr = std::shared_ptr<arma::fmat>(
                new arma::fmat(
                    target_data,
                    r,
                    c,
                    false,
                    true
                    )
            );
    V_ptrs = source;
    info_ptr = info;
    P_ = arma::fvec(c);
    P_.fill(1.0/float(c));
    //initialize R and t
    res_ptr = ResPtr(new Result);
    std::vector<std::shared_ptr<arma::fmat>>::iterator VIter;
    arma::fvec meanX = arma::mean(*X_ptr,1);
    for( VIter = V_ptrs.begin() ; VIter != V_ptrs.end() ; ++VIter )
    {
        arma::fmat&v = **VIter;
        arma::fvec meanV = arma::mean(v,1);
        res_ptr->Rs.push_back(std::make_shared<arma::fmat>(3,3,arma::fill::eye));
        res_ptr->ts.push_back(std::make_shared<arma::fvec>(meanX-meanV));
        v.each_col() += *(res_ptr->ts.back());
    }
    info_ptr ->result = (void*)(res_ptr.get());

    arma::fvec maxAllXYZ,minAllXYZ;

    for( VIter = V_ptrs.begin() ; VIter != V_ptrs.end() ; ++VIter )
    {
        arma::fmat&v = **VIter;
        arma::fvec maxVXYZ = arma::max(v,1);
        arma::fvec minVXYZ = arma::min(v,1);
        if(VIter==V_ptrs.begin())
        {
            maxAllXYZ = maxVXYZ;
            minAllXYZ = minVXYZ;
        }else{
            maxAllXYZ = arma::max(maxVXYZ,maxAllXYZ);
            minAllXYZ = arma::min(minVXYZ,minAllXYZ);
        }
    }

    float maxvar = arma::accu(arma::square(maxAllXYZ-minAllXYZ));

    (*X_ptr)*=(0.5*std::sqrt(maxvar));

    var = arma::fvec(X_ptr->n_cols);
    var.fill(1.0/maxvar);

    float h;
    h = 2 / arma::mean(var);
    float gamma = info_ptr->gamma;
    beta = gamma/(h*(gamma+1));
}

template<typename M>
void JRMPC<M>::initK(
        const std::vector<std::shared_ptr<arma::fmat>>&source,
        int&k
        )
{
    //if the k is already set during configure
    if(0!=k)return;
    std::vector<std::shared_ptr<arma::fmat>>::const_iterator iter;
    arma::fvec nviews(source.size());
    int idx = 0;
    for( iter = source.begin() ; iter != source.end()  ; ++iter )
    {
        nviews(idx) = (*iter)->n_cols;
        ++idx;
    }
    k = int(2.0*arma::median(nviews));
    k = k > 12 ? k : 12 ;
}

template<typename M>
void JRMPC<M>::initX(const std::vector<std::shared_ptr<arma::fmat>>&source,
        arma::fmat&target
        )
{
    int k = target.n_cols;
    arma::frowvec az = arma::randu<arma::frowvec>(k);
    arma::frowvec el = arma::randu<arma::frowvec>(k);
    az*=2*M_PI;
    el*=2*M_PI;
    target.row(0) = arma::cos(az)%arma::cos(el);
    target.row(1) = arma::sin(el);
    target.row(2) = arma::sin(az)%arma::cos(el);

}

template<typename M>
void JRMPC<M>::stepE()
{
    arma::fmat& X_ = *X_ptr;
    std::vector<MatPtr>::iterator VIter;
    int idx = 0;
    for(VIter=V_ptrs.begin();VIter!=V_ptrs.end();++VIter)
    {
        arma::fmat& V_ = **VIter;
        while(alpha_ptrs.size()<=idx)
        {
            alpha_ptrs.push_back(std::make_shared<arma::fmat>(V_.n_cols,X_.n_cols));
        }
        arma::fmat& alpha = *alpha_ptrs[idx];
        for(int r=0;r<alpha.n_rows;++r)
        {
            arma::fmat tmpm = X_.each_col() - V_.col(r);
            alpha.row(r) = arma::sum(arma::square(tmpm));
        }
        alpha.each_row()%=(-0.5*var.t());
        alpha = arma::exp(alpha);
        alpha.each_row()%=arma::pow(var.t(),1.5);
        alpha.each_row()%=P_.t();
        arma::fvec alpha_rowsum = arma::sum(alpha,1)+beta;
        alpha.each_col()/=alpha_rowsum;
        ++idx;
    }
}

template<typename M>
void JRMPC<M>::stepMa()
{
    arma::fmat& X_ = *X_ptr;
    std::vector<MatPtr>::iterator VIter;
    int idx = 0;
    for(VIter=V_ptrs.begin();VIter!=V_ptrs.end();++VIter)
    {
        arma::fmat& V_ = **VIter;
        arma::fmat& R = *(res_ptr->Rs[idx]);
        arma::fmat dR;
        arma::fvec& t = *(res_ptr->ts[idx]);
        arma::fvec dt;
        arma::fmat& alpha = *alpha_ptrs[idx];
        arma::frowvec alpha_colsum = arma::sum(alpha);
        arma::frowvec square_lambda = (var.t())%alpha_colsum;
        arma::fmat W = V_*alpha;
        W.each_row()/=alpha_colsum;
        arma::frowvec p(square_lambda.n_cols,arma::fill::ones);
        arma::frowvec square_norm_lambda = square_lambda / arma::accu(square_lambda);
        p -= square_norm_lambda;
        arma::fmat tmp = X_;
        tmp.each_row() %= p;
        tmp.each_row() %= square_lambda;
        arma::fmat A = tmp*(W.t());
        arma::fmat U,V;
        arma::fvec s;
        if(!arma::svd(U,s,V,A,"std"))
        {
            A += 1e-6*arma::fmat(3,3,arma::fill::eye);
            if(!arma::svd(U,s,V,A))
            {
                U = arma::fmat(3,3,arma::fill::eye);
                V = U;
                s = arma::fvec(3,arma::fill::ones);
                end_ = true;
            }
        }
        arma::fmat C(3,3,arma::fill::eye);
        C(2,2) = arma::det( U * V.t() )>=0 ? 1.0 : -1.0;
        dR = U*C*(V.t());
        R = dR*R;
        tmp = X_ - dR*W;
        tmp.each_row()%=square_norm_lambda;
        dt = arma::sum(tmp,1);
        t = dR*t + dt;
        V_ = dR*V_;
        V_.each_col() += dt;
        ++idx;
    }
}

template<typename M>
void JRMPC<M>::stepMbc()
{
    arma::fmat& X_ = *X_ptr;
    arma::fmat X_sum(X_.n_rows,X_.n_cols,arma::fill::zeros);
    arma::frowvec var_sum(X_.n_cols,arma::fill::zeros);
    arma::frowvec alpha_sum(X_.n_cols,arma::fill::zeros);
    std::vector<MatPtr>::iterator VIter;
    int idx = 0;
    for(VIter=V_ptrs.begin();VIter!=V_ptrs.end();++VIter)
    {
        arma::fmat& V_ = **VIter;
        arma::fmat& alpha = *alpha_ptrs[idx];
        arma::frowvec alpha_colsum = arma::sum(alpha);
        alpha_sum += alpha_colsum;
        arma::fmat tmpx =  V_*alpha;
        X_sum += tmpx;
        arma::fmat alpha_2(alpha.n_rows,alpha.n_cols);
        for(int r=0;r<alpha_2.n_rows;++r)
        {
            arma::fmat tmpm = X_.each_col() - V_.col(r);
            alpha_2.row(r) = arma::sum(arma::square(tmpm));
        }
        arma::frowvec tmpvar = arma::sum(alpha_2%alpha);
        var_sum += tmpvar;
        ++idx;
    }
    X_ = X_sum.each_row() / alpha_sum;
    var = ( var_sum / (3.0*alpha_sum) ).t();
    var += 1e-6;
    var = 1.0/var;
}


template<typename M>
void JRMPC<M>::stepMd()
{
    float mu = 0.0;
    arma::frowvec alpha_sumij(X_ptr->n_cols,arma::fill::zeros);
    std::vector<MatPtr>::iterator AIter;
    for(AIter=alpha_ptrs.begin();AIter!=alpha_ptrs.end();++AIter)
    {
        arma::fmat& alpha = **AIter;
        alpha_sumij += arma::sum(alpha);
        mu+=arma::sum(alpha_sumij);
    }
    mu*=(info_ptr->gamma+1.0);
    P_ = alpha_sumij.t();
    P_ /= mu;
}

template<typename M>
void JRMPC<M>::computeOnce(void)
{
    arma::fmat& X_ = *X_ptr;
    arma::fmat X_sum(X_.n_rows,X_.n_cols,arma::fill::zeros);
    arma::frowvec var_sum(X_.n_cols,arma::fill::zeros);
    arma::frowvec alpha_sum(X_.n_cols,arma::fill::zeros);
    float mu = 0.0;
    arma::frowvec alpha_sumij(X_ptr->n_cols,arma::fill::zeros);
    std::vector<MatPtr>::iterator VIter;
    int idx = 0;
    for(VIter=V_ptrs.begin();VIter!=V_ptrs.end();++VIter)
    {
        arma::fmat& V_ = **VIter;
        //allocate alpha
        while(alpha_ptrs.size()<=idx)
        {
            alpha_ptrs.push_back(std::make_shared<arma::fmat>(V_.n_cols,X_.n_cols));
        }
        //compute alpha(step-E)
        arma::fmat& alpha = *alpha_ptrs[idx];
        for(int r=0;r<alpha.n_rows;++r)
        {
            arma::fmat tmpm = X_.each_col() - V_.col(r);
            alpha.row(r) = arma::sum(arma::square(tmpm));
        }
        alpha.each_row()%=(-0.5*var.t());
        alpha = arma::exp(alpha);
        alpha.each_row()%=arma::pow(var.t(),1.5);
        alpha.each_row()%=P_.t();
        arma::fvec alpha_rowsum = arma::sum(alpha,1)+beta;
        alpha.each_col()/=alpha_rowsum;

        //update R t
        arma::fmat& R = *(res_ptr->Rs[idx]);
        arma::fmat dR;
        arma::fvec& t = *(res_ptr->ts[idx]);
        arma::fvec dt;
        arma::frowvec alpha_colsum = arma::sum(alpha);
        arma::frowvec square_lambda = (var.t())%alpha_colsum;
        arma::fmat W = V_*alpha;
        W.each_row()/=alpha_colsum;
        arma::frowvec p(square_lambda.n_cols,arma::fill::ones);
        arma::frowvec square_norm_lambda = square_lambda / arma::accu(square_lambda);
        p -= square_norm_lambda;
        arma::fmat tmp = X_;
        tmp.each_row() %= p;
        tmp.each_row() %= square_lambda;
        arma::fmat A = tmp*(W.t());
        arma::fmat U,V;
        arma::fvec s;
        if(!arma::svd(U,s,V,A,"std"))
        {
            A += 1e-6*arma::fmat(3,3,arma::fill::eye);
            if(!arma::svd(U,s,V,A))
            {
                U = arma::fmat(3,3,arma::fill::eye);
                V = U;
                s = arma::fvec(3,arma::fill::ones);
                end_ = true;
                return;
            }
        }
        arma::fmat C(3,3,arma::fill::eye);
        C(2,2) = arma::det( U * V.t() )>=0 ? 1.0 : -1.0;
        dR = U*C*(V.t());
        R = dR*R;
        tmp = X_ - dR*W;
        tmp.each_row()%=square_norm_lambda;
        dt = arma::sum(tmp,1);
        t = dR*t + dt;

        //update V
        V_ = dR*V_;
        V_.each_col() += dt;

        //update X var
        alpha_sum += alpha_colsum;
        arma::fmat tmpx =  V_*alpha;
        X_sum += tmpx;
        arma::fmat alpha_2(alpha.n_rows,alpha.n_cols);
        for(int r=0;r<alpha_2.n_rows;++r)
        {
            arma::fmat tmpm = X_.each_col() - V_.col(r);
            alpha_2.row(r) = arma::sum(arma::square(tmpm));
        }
        arma::frowvec tmpvar = arma::sum(alpha_2%alpha);
        var_sum += tmpvar;

        //
        alpha_sumij += alpha_colsum;
        mu+=arma::accu(alpha_sumij);
        ++idx;
    }
    X_ = X_sum.each_row() / alpha_sum;
    var = ( var_sum / (3.0*alpha_sum) ).t();
    var += 1e-6;
    var = 1.0/var;
    mu*=(info_ptr->gamma+1.0);
    P_ = alpha_sumij.t();
    P_ /= mu;
}

template<typename M>
void JRMPC<M>::setVarColor(uint32_t* color,int k)
{
    var_color = std::make_shared<arma::Col<uint32_t>>(color,k,false,true);
    varToColor();
}

template<typename M>
void JRMPC<M>::varToColor()
{
    if(var_color->size()==var.size())
    {
        float max_var = arma::max(var);
        float min_var = arma::min(var);
        float h;
        ColorArray::RGB32 tmp;
        int idx;
        for(idx=0;idx<var_color->size();++idx)
        {
            h = std::sqrt( ( var(idx) - min_var ) / ( max_var - min_var ) );
            ColorArray::hsv2rgb(h*270.0,0.5,1.0,tmp);
            (*var_color)(idx) = tmp.color;
        }
    }else{
        std::cerr<<"failed to trans color"<<std::endl;
    }
}

template<typename M>
bool JRMPC<M>::isEnd()
{
    if( count >= info_ptr->max_iter )
    {
        info_ptr->mode = MaxIter;
        return true;
    }
    if( end_ )
    {
        info_ptr->mode = Force;
        return true;
    }
    return false;
}

}
#endif
