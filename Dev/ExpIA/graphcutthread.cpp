#include "graphcutthread.h"
arma::sp_mat GraphCutThread::smooth_;

bool GraphCutThread::configure(Config::Ptr config)
{
    config_ = config;
    if(!config_->has("GC_iter_num"))return false;
    if(!config_->has("GC_data_weight"))return false;
    return true;
}

void GraphCutThread::run(void)
{
    Segmentation::GraphCut gc;
    current_frame_ = 0;
    while( current_frame_ < meshes_.size() )
    {
        MeshBundle<DefaultMesh>& m = *meshes_[current_frame_];
        label_number_ = 1 + objects_.size();
        pix_number_ = m.graph_.voxel_centers.n_cols;
        gc.setLabelNumber( label_number_ );
        gc.setPixelNumber( pix_number_ );
        if(!prepareDataTerm())
        {

        }
        if(!current_data_||0==current_data_.use_count())std::logic_error("!current_data_||0==current_data_.use_count()");
        gc.inputDataTerm(current_data_);
        if(!prepareSmoothTerm(gc))
        {

        }
        gc.inputSmoothTerm(current_smooth_);
        std::cerr<<"gc.init"<<std::endl;
        gc.init(Segmentation::GraphCut::EXPANSION);
        std::cerr<<"prepareNeighbors"<<std::endl;
        if(!prepareNeighbors(gc))
        {
            std::cerr<<"Failed in prepareNeighbors"<<std::endl;
        }
        std::cerr<<"Graphcut Update Info"<<std::endl;
        gc.updateInfo();
        float t;
        std::cerr<<"message before optimize"<<std::endl;
        emit message(QString::fromStdString(gc.info()),0);
        gc.optimize(config_->getInt("GC_iter_num"),t);
        std::cerr<<"message after optimize"<<std::endl;
        emit message(QString::fromStdString(gc.info()),0);
        arma::uvec sv_label;
        gc.getAnswer(sv_label);
        m.graph_.sv2pix(sv_label,outputs_[current_frame_]);
        m.custom_color_.fromlabel(outputs_[current_frame_]);
        ++current_frame_;
    }
}

bool GraphCutThread::prepareDataTerm()
{
    MeshBundle<DefaultMesh>& m = *meshes_[current_frame_];
    data_.reset(new double[label_number_*pix_number_]);
    arma::mat data_mat(data_.get(),label_number_,pix_number_,false,true);
    std::vector<ObjModel::Ptr>::iterator oiter;
    uint32_t obj_index = 0;
    for(oiter=objects_.begin();oiter!=objects_.end();++oiter)
    {
        ObjModel& model = **oiter;
        DefaultMesh obj_mesh;
        if(model.transform(obj_mesh,current_frame_))
        {
            prepareDataForLabel(1+obj_index,m.graph_,obj_mesh,model.GeoP_,model.ColorP_);
        }else{
            data_mat.row(1+obj_index).fill(0.0);
        }
        ++obj_index;
    }
    prepareDataForUnknown();
    current_data_.reset(new DataCost(data_.get()));
    return true;
}

void GraphCutThread::prepareDataForLabel(uint32_t l,
        VoxelGraph<DefaultMesh>& graph,
        DefaultMesh& obj,
        std::vector<float> &geo_score,
        std::vector<float> &color_score)
{
    double* data = data_.get();
    arma::mat data_mat(data,label_number_,pix_number_,false,true);
    arma::vec score;
    graph.match(obj,geo_score,color_score,score);
    data_mat.row(l) = score.t();
}

void GraphCutThread::prepareDataForUnknown()
{
    arma::mat data((double*)data_.get(),label_number_,pix_number_,false,true);
    data.row(0).fill(0.5);
    arma::rowvec label_sum = arma::sum(data);
    data.each_row() -= label_sum;
    data *= -1.0*config_->getDouble("GC_data_weight");
}

bool GraphCutThread::prepareSmoothTerm(Segmentation::GraphCut& gc)
{
    MeshBundle<DefaultMesh>& m = *meshes_[current_frame_];
    smooth_ = arma::sp_mat(pix_number_,pix_number_);
    for( size_t idx = 0 ; idx < m.graph_.voxel_neighbors.n_cols ; ++idx )
    {
        size_t pix1 = m.graph_.voxel_neighbors(0,idx);
        size_t pix2 = m.graph_.voxel_neighbors(1,idx);
        if(pix1>=pix_number_)std::logic_error("pix1>=pix_number_");
        if(pix2>=pix_number_)std::logic_error("pix2>=pix_number_");
        smooth_(pix1,pix2) = m.graph_.voxel_similarity(pix1,pix2);
    }
    current_smooth_.reset(new SmoothnessCost(GraphCutThread::fnCost));
    return true;
}

MRF::CostVal GraphCutThread::fnCost(int pix1,int pix2,MRF::Label i,MRF::Label j)
{
    if(i==j)return 0.0;
    else return smooth_(pix1,pix2);
}

bool GraphCutThread::prepareNeighbors(Segmentation::GraphCut& gc)
{
    MeshBundle<DefaultMesh>& m = *meshes_[current_frame_];
    double w_eps = 1.0 / double( m.mesh_.n_vertices() );
    for( size_t idx = 0 ; idx < m.graph_.voxel_neighbors.n_cols ; ++idx )
    {
        size_t pix1 = m.graph_.voxel_neighbors(0,idx);
        size_t pix2 = m.graph_.voxel_neighbors(1,idx);
        if(pix1>=pix_number_)std::logic_error("pix1>=pix_number_");
        if(pix2>=pix_number_)std::logic_error("pix2>=pix_number_");
        double w = w_eps * ( m.graph_.voxel_size(pix1) + m.graph_.voxel_size(pix2) );
        if(!gc.setNeighbors(pix1,pix2,w))return false;
    }
    return true;
}
