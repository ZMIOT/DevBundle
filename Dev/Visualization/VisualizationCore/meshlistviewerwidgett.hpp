#ifndef MESHLISTVIEWERWIDGETT_HPP
#define MESHLISTVIEWERWIDGETT_HPP
#include "MeshListViewerWidgetT.h"
#include <QKeyEvent>
#include <QString>
#include <armadillo>
using namespace OpenMesh;
using namespace Qt;

template <typename M>
bool
MeshListViewerWidgetT<M>::open_mesh(const char* _filename,Mesh& mesh_,Stripifier& strips_ ,IO::Options _opt)
{
  // load mesh
  // calculate normals
  // set scene center and radius
  QString fname(_filename);
  bool open_arma = false;
  if(fname.endsWith("mat.arma"))
  {
      arma::fmat pts;
      pts.load(_filename);
//      std::cerr<<"pts("<<pts.n_rows<<","<<pts.n_cols<<")"<<std::endl;
      for( int i=0 ; i < pts.n_cols ; ++i )
      {
          mesh_.add_vertex(typename M::Point(pts(0,i),pts(1,i),pts(2,i)));
      }
//      std::cerr<<"mesh_.n_vertices():"<<mesh_.n_vertices()<<std::endl;
      if(0<mesh_.n_vertices())open_arma=true;
  }

  mesh_.request_face_normals();
  mesh_.request_face_colors();
  mesh_.request_vertex_normals();
  mesh_.request_vertex_colors();
  mesh_.request_vertex_texcoords2D();
  std::cout << "Loading from file '" << _filename << "'\n";
  if ( open_arma || IO::read_mesh(mesh_, _filename, _opt )  )
  {
      if(open_arma)
      {
          _opt -= OpenMesh::IO::Options::VertexColor;
          _opt -= OpenMesh::IO::Options::VertexNormal;
          _opt -= OpenMesh::IO::Options::VertexTexCoord;
          _opt -= OpenMesh::IO::Options::FaceColor;
          _opt -= OpenMesh::IO::Options::FaceNormal;
          _opt -= OpenMesh::IO::Options::FaceTexCoord;
          mesh_.release_face_normals();
          mesh_.release_face_colors();
          mesh_.release_vertex_normals();
          mesh_.release_vertex_colors();
          mesh_.release_vertex_texcoords2D();
      }
    // store read option
    opt_ = _opt;
    // update face and vertex normals
    if ( ! opt_.check( IO::Options::FaceNormal ) && !open_arma )
      mesh_.update_face_normals();
    else if(!open_arma)
      std::cout << "File provides face normals\n";

    if ( ! opt_.check( IO::Options::VertexNormal ) && !open_arma )
      mesh_.update_vertex_normals();
    else if(!open_arma)
      std::cout << "File provides vertex normals\n";


    // check for possible color information
    if ( opt_.check( IO::Options::VertexColor ) && !open_arma )
    {
      std::cout << "File provides vertex colors\n";
      if(!findAction("Colored Vertices"))add_draw_mode("Colored Vertices");
    }
    else if(!open_arma)
      mesh_.release_vertex_colors();

    if ( _opt.check( IO::Options::FaceColor ) && !open_arma )
    {
      std::cout << "File provides face colors\n";
      add_draw_mode("Solid Colored Faces");
      add_draw_mode("Smooth Colored Faces");
    }
    else if(!open_arma)
      mesh_.release_face_colors();

    if ( _opt.check( IO::Options::VertexTexCoord ) && !open_arma )
      std::cout << "File provides texture coordinates\n";

    // info
    std::clog << mesh_.n_vertices() << " vertices, "
          << mesh_.n_edges()    << " edge, "
          << mesh_.n_faces()    << " faces\n";
    // base point for displaying face normals
    if(mesh_.has_face_normals())
    {
      OpenMesh::Utils::Timer t;
      t.start();
      mesh_.add_property( fp_normal_base_ );
      typename M::FaceIter f_it = mesh_.faces_begin();
      typename M::FaceVertexIter fv_it;
      for (;f_it != mesh_.faces_end(); ++f_it)
      {
        typename Mesh::Point v(0,0,0);
        for( fv_it=mesh_.fv_iter(*f_it); fv_it.is_valid(); ++fv_it)
          v += OpenMesh::vector_cast<typename Mesh::Normal>(mesh_.point(*fv_it));
        v *= 1.0f/3.0f;
        mesh_.property( fp_normal_base_, *f_it ) = v;
      }
      t.stop();
      std::clog << "Computed base point for displaying face normals ["
                << t.as_string() << "]" << std::endl;
    }
    //
    {
      std::clog << "Computing strips.." << std::flush;
      OpenMesh::Utils::Timer t;
      t.start();
      compute_strips(strips_);
      t.stop();
      std::clog << "done [" << strips_.n_strips()
        << " strips created in " << t.as_string() << "]\n";
    }
    //
#if defined(WIN32)
    updateGL();
#endif
//    setWindowTitle(QFileInfo(_filename).fileName());
    // loading done
    return true;
  }
  return false;
}

template <typename M>
bool MeshListViewerWidgetT<M>::save_mesh(const char* _filename,Mesh& mesh_,IO::Options _opt)
{
  // load mesh
  // calculate normals
  // set scene center and radius

//  mesh_.request_face_normals();
//  mesh_.request_face_colors();
  mesh_.request_vertex_normals();
  mesh_.request_vertex_colors();
//  mesh_.request_vertex_texcoords2D();

  std::cout << "Saving to file '" << _filename << "'\n";
  if ( IO::write_mesh(mesh_, _filename, _opt, 16 ) )
  {
      return true;
  }
  return false;
}

template <typename M>
bool MeshListViewerWidgetT<M>::set_texture( QImage& _texsrc )
{
  if ( !opt_.vertex_has_texcoord() )
    return false;

  {
    // adjust texture size: 2^k * 2^l
    int tex_w, w( _texsrc.width()  );
    int tex_h, h( _texsrc.height() );

    for (tex_w=1; tex_w <= w; tex_w <<= 1) {};
    for (tex_h=1; tex_h <= h; tex_h <<= 1) {};
    tex_w >>= 1;
    tex_h >>= 1;
    _texsrc = _texsrc.scaled( tex_w, tex_h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
  }

  QImage texture( QGLWidget::convertToGLFormat ( _texsrc ) );

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_SKIP_ROWS,   0);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
  glPixelStorei(GL_UNPACK_ALIGNMENT,   1);
  glPixelStorei(GL_PACK_ROW_LENGTH,    0);
  glPixelStorei(GL_PACK_SKIP_ROWS,     0);
  glPixelStorei(GL_PACK_SKIP_PIXELS,   0);
  glPixelStorei(GL_PACK_ALIGNMENT,     1);

  if ( tex_id_ > 0 )
  {
    glDeleteTextures(1, &tex_id_);
  }
  glGenTextures(1, &tex_id_);
  glBindTexture(GL_TEXTURE_2D, tex_id_);

  // glTexGenfv( GL_S, GL_SPHERE_MAP, 0 );
  // glTexGenfv( GL_T, GL_SPHERE_MAP, 0 );

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glTexImage2D(GL_TEXTURE_2D,       // target
           0,                   // level
           GL_RGBA,             // internal format
           texture.width(),     // width  (2^n)
           texture.height(),    // height (2^m)
           0,                   // border
           GL_RGBA,             // format
           GL_UNSIGNED_BYTE,    // type
           texture.bits() );    // pointer to pixels

  std::cout << "Texture loaded\n";
  return true;
}


//-----------------------------------------------------------------------------

template <typename M>
void
MeshListViewerWidgetT<M>::draw_openmesh(MeshBundle<Mesh>& b,const std::string& _draw_mode)
{
//    std::cout<<"draw_openmesh"<<std::endl;
//    std::cout<<"draw_mode_:"<<_draw_mode<<std::endl;
    Mesh& mesh_ = b.mesh_;
    Stripifier& strips_ = b.strips_;
    MeshColor<Mesh>& color_ = b.custom_color_;

    typename Mesh::ConstFaceIter    fIt(mesh_.faces_begin()),
            fEnd(mesh_.faces_end());

    typename Mesh::ConstFaceVertexIter fvIt;

#if defined(OM_USE_OSG) && OM_USE_OSG
  if (_draw_mode == "OpenSG Indices") // --------------------------------------
  {
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, mesh_.points());

    glEnableClientState(GL_NORMAL_ARRAY);
    glNormalPointer(GL_FLOAT, 0, mesh_.vertex_normals());

    if ( tex_id_ && mesh_.has_vertex_texcoords2D() )
    {
      glEnableClientState(GL_TEXTURE_COORD_ARRAY);
      glTexCoordPointer(2, GL_FLOAT, 0, mesh_.texcoords2D());
      glEnable(GL_TEXTURE_2D);
      glBindTexture(GL_TEXTURE_2D, tex_id_);
      glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, tex_mode_);
    }

    glDrawElements(GL_TRIANGLES,
           mesh_.osg_indices()->size(),
           GL_UNSIGNED_INT,
           &mesh_.osg_indices()->getField()[0] );

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  }
  else
#endif

      if (_draw_mode == "Wireframe") // -------------------------------------------
      {
          glBegin(GL_TRIANGLES);
          glColor3f(1.0,1.0,1.0);
          for (; fIt!=fEnd; ++fIt)
          {
              fvIt = mesh_.cfv_iter(*fIt);
              glVertex3fv( &mesh_.point(*fvIt)[0] );
              ++fvIt;
              glVertex3fv( &mesh_.point(*fvIt)[0] );
              ++fvIt;
              glVertex3fv( &mesh_.point(*fvIt)[0] );
          }
          glEnd();
      }

      else if (_draw_mode == "Solid Flat") // -------------------------------------
      {
          glBegin(GL_TRIANGLES);
          for (; fIt!=fEnd; ++fIt)
          {
              glNormal3fv( &mesh_.normal(*fIt)[0] );
              fvIt = mesh_.cfv_iter(*fIt);
              glVertex3fv( &mesh_.point(*fvIt)[0] );
              ++fvIt;
              glVertex3fv( &mesh_.point(*fvIt)[0] );
              ++fvIt;
              glVertex3fv( &mesh_.point(*fvIt)[0] );
          }
          glEnd();

      }


      else if (_draw_mode == "Solid Smooth") // -----------------------------------
      {
          glEnableClientState(GL_VERTEX_ARRAY);
          glVertexPointer(3, GL_FLOAT, 0, mesh_.points());

          glEnableClientState(GL_NORMAL_ARRAY);
          glNormalPointer(GL_FLOAT, 0, mesh_.vertex_normals());

          if ( tex_id_ && mesh_.has_vertex_texcoords2D() )
          {
              glEnableClientState(GL_TEXTURE_COORD_ARRAY);
              glTexCoordPointer(2, GL_FLOAT, 0, mesh_.texcoords2D());
              glEnable(GL_TEXTURE_2D);
              glBindTexture(GL_TEXTURE_2D, tex_id_);
              glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, tex_mode_);
          }

          glBegin(GL_TRIANGLES);
          for (; fIt!=fEnd; ++fIt)
          {
              fvIt = mesh_.cfv_iter(*fIt);
              glArrayElement(fvIt->idx());
              ++fvIt;
              glArrayElement(fvIt->idx());
              ++fvIt;
              glArrayElement(fvIt->idx());
          }
          glEnd();

          glDisableClientState(GL_VERTEX_ARRAY);
          glDisableClientState(GL_NORMAL_ARRAY);
          glDisableClientState(GL_TEXTURE_COORD_ARRAY);

          if ( tex_id_ && mesh_.has_vertex_texcoords2D() )
          {
              glDisable(GL_TEXTURE_2D);
          }
      }

      else if (_draw_mode == "Colored Vertices") // --------------------------------
      {
          glEnableClientState(GL_VERTEX_ARRAY);
          glVertexPointer(3, GL_FLOAT, 0, mesh_.points());

          glEnableClientState(GL_NORMAL_ARRAY);
          glNormalPointer(GL_FLOAT, 0, mesh_.vertex_normals());

          if ( mesh_.has_vertex_colors() )
          {
              glEnableClientState( GL_COLOR_ARRAY );
              glColorPointer(3, GL_UNSIGNED_BYTE, 0,mesh_.vertex_colors());
          }

          glBegin(GL_TRIANGLES);
          for (; fIt!=fEnd; ++fIt)
          {
              fvIt = mesh_.cfv_begin(*fIt);
              while(fvIt!=mesh_.cfv_end(*fIt))
              {
                    glArrayElement(fvIt->idx());
                    ++fvIt;
              }
          }
          glEnd();

          glDisableClientState(GL_VERTEX_ARRAY);
          glDisableClientState(GL_NORMAL_ARRAY);
          glDisableClientState(GL_COLOR_ARRAY);
      }

      else if (_draw_mode == "Flat Colored Vertices") // --------------------------------
      {
          glEnableClientState(GL_VERTEX_ARRAY);
          glVertexPointer(3, GL_FLOAT, 0, mesh_.points());

          glEnableClientState(GL_NORMAL_ARRAY);
          glNormalPointer(GL_FLOAT, 0, mesh_.vertex_normals());

          if ( mesh_.has_vertex_colors() )
          {
              glEnableClientState( GL_COLOR_ARRAY );
              glColorPointer(3, GL_UNSIGNED_BYTE, 0,mesh_.vertex_colors());
          }

          glBegin(GL_TRIANGLES);
          for (; fIt!=fEnd; ++fIt)
          {
              fvIt = mesh_.cfv_begin(*fIt);
              glMaterial(mesh_,*fvIt);
              glMaterial(mesh_,*fvIt,GL_FRONT_AND_BACK,GL_SPECULAR);
              while(fvIt!=mesh_.cfv_end(*fIt))
              {
                    glArrayElement(fvIt->idx());
                    ++fvIt;
              }
          }
          glEnd();

          glDisableClientState(GL_VERTEX_ARRAY);
          glDisableClientState(GL_NORMAL_ARRAY);
          glDisableClientState(GL_COLOR_ARRAY);
      }


      else if (_draw_mode == "Solid Colored Faces") // -----------------------------
      {
          glEnableClientState(GL_VERTEX_ARRAY);
          glVertexPointer(3, GL_FLOAT, 0, mesh_.points());

          glEnableClientState(GL_NORMAL_ARRAY);
          glNormalPointer(GL_FLOAT, 0, mesh_.vertex_normals());

          for (; fIt!=fEnd; ++fIt)
          {
              glBegin(GL_POLYGON);
              glColor( mesh_,*fIt );
              fvIt = mesh_.cfv_begin(*fIt);
              while(fvIt!=mesh_.cfv_end(*fIt))
              {
                    glArrayElement(fvIt->idx());
                    ++fvIt;
              }
              glEnd();
          }

          glDisableClientState(GL_VERTEX_ARRAY);
          glDisableClientState(GL_NORMAL_ARRAY);
      }


      else if (_draw_mode == "Smooth Colored Faces") // ---------------------------
      {
          glEnableClientState(GL_VERTEX_ARRAY);
          glVertexPointer(3, GL_FLOAT, 0, mesh_.points());

          glEnableClientState(GL_NORMAL_ARRAY);
          glNormalPointer(GL_FLOAT, 0, mesh_.vertex_normals());

          glBegin(GL_TRIANGLES);
          for (; fIt!=fEnd; ++fIt)
          {
              glMaterial( mesh_,*fIt );
              fvIt = mesh_.cfv_iter(*fIt);
              glArrayElement(fvIt->idx());
              ++fvIt;
              glArrayElement(fvIt->idx());
              ++fvIt;
              glArrayElement(fvIt->idx());
          }
          glEnd();

          glDisableClientState(GL_VERTEX_ARRAY);
          glDisableClientState(GL_NORMAL_ARRAY);
      }


      else if ( _draw_mode == "Strips'n VertexArrays" ) // ------------------------
      {
          glEnableClientState(GL_VERTEX_ARRAY);
          glVertexPointer(3, GL_FLOAT, 0, mesh_.points());

          glEnableClientState(GL_NORMAL_ARRAY);
          glNormalPointer(GL_FLOAT, 0, mesh_.vertex_normals());

          if ( tex_id_ && mesh_.has_vertex_texcoords2D() )
          {
              glEnableClientState(GL_TEXTURE_COORD_ARRAY);
              glTexCoordPointer(2, GL_FLOAT, 0, mesh_.texcoords2D());
              glEnable(GL_TEXTURE_2D);
              glBindTexture(GL_TEXTURE_2D, tex_id_);
              glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, tex_mode_);
          }

          typename Stripifier::StripsIterator strip_it = strips_.begin();
          typename Stripifier::StripsIterator strip_last = strips_.end();

          // Draw all strips
          for (; strip_it!=strip_last; ++strip_it)
          {
              glDrawElements(GL_TRIANGLE_STRIP,
                             static_cast<GLsizei>(strip_it->size()), GL_UNSIGNED_INT, &(*strip_it)[0] );
          }

          glDisableClientState(GL_VERTEX_ARRAY);
          glDisableClientState(GL_NORMAL_ARRAY);
          glDisableClientState(GL_TEXTURE_COORD_ARRAY);
      }
      else if (_draw_mode == "Show Strips" && strips_.is_valid() ) // -------------
      {
          typename Stripifier::StripsIterator strip_it = strips_.begin();
          typename Stripifier::StripsIterator strip_last = strips_.end();

          float cmax  = 256.0f;
          int   range = 220;
          int   base  = (int)cmax-range;
          int   drcol  = 13;
          int   dgcol  = 31;
          int   dbcol  = 17;

          int rcol=0, gcol=dgcol, bcol=dbcol+dbcol;

          // Draw all strips
          for (; strip_it!=strip_last; ++strip_it)
          {
              typename Stripifier::IndexIterator idx_it   = strip_it->begin();
              typename Stripifier::IndexIterator idx_last = strip_it->end();

              rcol = (rcol+drcol) % range;
              gcol = (gcol+dgcol) % range;
              bcol = (bcol+dbcol) % range;

              glBegin(GL_TRIANGLE_STRIP);
              glColor3f((rcol+base)/cmax, (gcol+base)/cmax, (bcol+base)/cmax);
              for ( ;idx_it != idx_last; ++idx_it )
                  glVertex3fv(&mesh_.point( OM_TYPENAME Mesh::VertexHandle(*idx_it))[0]);
              glEnd();
          }
          glColor3f(1.0, 1.0, 1.0);
      }
      else if( _draw_mode == "Points" ) // -----------------------------------------
      {
          glEnable( GL_POINT_SMOOTH );
          glEnableClientState(GL_VERTEX_ARRAY);
          glVertexPointer(3, GL_FLOAT, 0, mesh_.points());
          if ( use_color_)
          {
              if( mesh_.has_vertex_colors() && !custom_color_)
              {
                  glEnableClientState(GL_COLOR_ARRAY);
                  glColorPointer(3, GL_UNSIGNED_BYTE, 0, mesh_.vertex_colors());
              }else{
                  glEnableClientState(GL_COLOR_ARRAY);
                  glColorPointer(4, GL_UNSIGNED_BYTE, 0, color_.vertex_colors());
              }
          }
          glPointSize(point_size_);
          glDrawArrays( GL_POINTS, 0, static_cast<GLsizei>(mesh_.n_vertices()) );
          glDisableClientState(GL_VERTEX_ARRAY);
          glDisableClientState(GL_COLOR_ARRAY);
          glDisable( GL_POINT_SMOOTH );
      }
}

//-----------------------------------------------------------------------------
template <typename M>
void MeshListViewerWidgetT<M>::draw_selected()
{
    if(current_selected_.empty())return;
    glDisable(GL_LIGHTING);
    glPointSize(2.0*point_size_);
    glBegin(GL_POINTS);
    glColor3f(1.0f,0.0f,0.0f); // greenish
    float* p = (float*)mesh_list_[current_mesh_start_]->mesh_.points();
    std::vector<arma::uword>::iterator iter;
    for(iter=current_selected_.begin();iter!=current_selected_.end();++iter)
    {
        glVertex3fv(&p[3*(*iter)]);
    }
    glEnd();
}

//-----------------------------------------------------------------------------


template <typename M>
void
MeshListViewerWidgetT<M>::draw_scene(const std::string& _draw_mode)
{

  if ( mesh_list_.empty() )
    return;
  M& mesh_ = (*(mesh_list_.begin() + current_mesh_start_))->mesh_;

//#if defined(OM_USE_OSG) && OM_USE_OSG
//  else if ( _draw_mode == "OpenSG Indices")
//  {
//    glEnable(GL_LIGHTING);
//    glShadeModel(GL_SMOOTH);
//    if(0<first_->mesh_.n_vertices())draw_openmesh( first_->mesh_ , _draw_mode );
//    if(0<second_->mesh_.n_vertices())draw_openmesh( second_->mesh_ , _draw_mode );
//  }
//  else
//#endif
 if (_draw_mode == "Wireframe")
  {
    glDisable(GL_LIGHTING);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    typename std::vector<typename MeshBundle<Mesh>::Ptr>::iterator iter;
    int cnt = 0;
    for( iter = ( mesh_list_.begin() + current_mesh_start_ ); cnt < current_visible_num_ ;  )
    {
        if((*iter)->mesh_.n_vertices())draw_openmesh( **iter , _draw_mode );
        ++cnt;
        ++iter;
        if(iter==mesh_list_.end())iter=mesh_list_.begin();
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }

//  else if ( _draw_mode == "Hidden-Line" )
//  {
//    glDisable(GL_LIGHTING);
//    glShadeModel(GL_FLAT);
//    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
//    glColor4f( 0.0f, 0.0f, 0.0f, 1.0f );
//    glDepthRange(0.01, 1.0);
//    if(0<first_->mesh_.n_vertices())draw_openmesh( *first_, "Wireframe" );
//    if(0<second_->mesh_.n_vertices())draw_openmesh( *second_, "Wireframe" );

//    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE);
//    glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
//    glDepthRange( 0.0, 1.0 );
//    if(0<first_->mesh_.n_vertices())draw_openmesh( *first_, "Wireframe" );
//    if(0<second_->mesh_.n_vertices())draw_openmesh( *second_, "Wireframe" );

//    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL);
//  }

  else if (_draw_mode == "Solid Flat")
  {
    glEnable(GL_LIGHTING);
    glShadeModel(GL_FLAT);
    typename std::vector<typename MeshBundle<Mesh>::Ptr>::iterator iter;
    int cnt = 0;
    for( iter = ( mesh_list_.begin() + current_mesh_start_ ); cnt < current_visible_num_ ;  )
    {
        if((*iter)->mesh_.n_vertices())draw_openmesh( **iter , _draw_mode );
        ++cnt;
        ++iter;
        if(iter==mesh_list_.end())iter=mesh_list_.begin();
    }
  }

  else if (_draw_mode == "Solid Smooth"        ||
       _draw_mode == "Strips'n VertexArrays" )
  {
    glEnable(GL_LIGHTING);
    glShadeModel(GL_SMOOTH);
    typename std::vector<typename MeshBundle<Mesh>::Ptr>::iterator iter;
    int cnt = 0;
    for( iter = ( mesh_list_.begin() + current_mesh_start_ ); cnt < current_visible_num_ ;  )
    {
        if((*iter)->mesh_.n_vertices())draw_openmesh( **iter , _draw_mode );
        ++cnt;
        ++iter;
        if(iter==mesh_list_.end())iter=mesh_list_.begin();
    }
  }

//  else if (_draw_mode == "Show Strips")
//  {
//    glDisable(GL_LIGHTING);
//    if(0<first_->mesh_.n_vertices())draw_openmesh( *first_, _draw_mode );
//    if(0<second_->mesh_.n_vertices())draw_openmesh( *second_, _draw_mode );
//  }

  else if (_draw_mode == "Colored Vertices" )
  {
    glDisable(GL_LIGHTING);
    glShadeModel(GL_FLAT);
    typename std::vector<typename MeshBundle<Mesh>::Ptr>::iterator iter;
    int cnt = 0;
    for( iter = ( mesh_list_.begin() + current_mesh_start_ ); cnt < current_visible_num_ ;  )
    {
        if((*iter)->mesh_.n_vertices())draw_openmesh( **iter , _draw_mode );
        ++cnt;
        ++iter;
        if(iter==mesh_list_.end())iter=mesh_list_.begin();
    }
  }

  else if (_draw_mode == "Flat Colored Vertices" )
  {
    glEnable(GL_LIGHTING);
    glShadeModel(GL_FLAT);
    typename std::vector<typename MeshBundle<Mesh>::Ptr>::iterator iter;
    int cnt = 0;
    for( iter = ( mesh_list_.begin() + current_mesh_start_ ); cnt < current_visible_num_ ;  )
    {
        if((*iter)->mesh_.n_vertices())draw_openmesh( **iter , _draw_mode );
        ++cnt;
        ++iter;
        if(iter==mesh_list_.end())iter=mesh_list_.begin();
    }
    setDefaultMaterial();
  }

  else if (_draw_mode == "Solid Colored Faces")
  {
    glDisable(GL_LIGHTING);
    glShadeModel(GL_FLAT);
    typename std::vector<typename MeshBundle<Mesh>::Ptr>::iterator iter;
    int cnt = 0;
    for( iter = ( mesh_list_.begin() + current_mesh_start_ ); cnt < current_visible_num_ ;  )
    {
        if((*iter)->mesh_.n_vertices())draw_openmesh( **iter , _draw_mode );
        ++cnt;
        ++iter;
        if(iter==mesh_list_.end())iter=mesh_list_.begin();
    }
    setDefaultMaterial();
  }

  else if (_draw_mode == "Smooth Colored Faces" )
  {
    glEnable(GL_LIGHTING);
    glShadeModel(GL_SMOOTH);
    typename std::vector<typename MeshBundle<Mesh>::Ptr>::iterator iter;
    int cnt = 0;
    for( iter = ( mesh_list_.begin() + current_mesh_start_ ); cnt < current_visible_num_ ;  )
    {
        if((*iter)->mesh_.n_vertices())draw_openmesh( **iter , _draw_mode );
        ++cnt;
        ++iter;
        if(iter==mesh_list_.end())iter=mesh_list_.begin();
    }
    setDefaultMaterial();
  }else if (  _draw_mode == "Points"  )
 {
     glDisable(GL_LIGHTING);
     typename std::vector<typename MeshBundle<Mesh>::Ptr>::iterator iter;
     int cnt = 0;
     for( iter = ( mesh_list_.begin() + current_mesh_start_ ); cnt < current_visible_num_ ;  )
     {
         if((*iter)->mesh_.n_vertices())draw_openmesh( **iter , _draw_mode );
         ++cnt;
         ++iter;
         if(iter==mesh_list_.end())iter=mesh_list_.begin();
     }
 }

 if( _draw_mode != "Points" &&  mesh_.n_faces()*5 < mesh_.n_vertices())
 {
     draw_scene("Points");
 }


  if (show_vnormals_)
  {
    typename Mesh::VertexIter vit;
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);
    glColor3f(1.000f, 0.803f, 0.027f); // orange
    typename std::vector<typename MeshBundle<Mesh>::Ptr>::iterator iter;
    int cnt = 0;
    for( iter = ( mesh_list_.begin() + current_mesh_start_ ); cnt < current_visible_num_ ;  )
    {
        if((*iter)->mesh_.n_vertices())
        for(vit=(*iter)->mesh_.vertices_begin(); vit!=(*iter)->mesh_.vertices_end(); ++vit)
        {
          glVertex( (*iter)->mesh_ , *vit );
          glVertex( (*iter)->mesh_.point( *vit ) + normal_scale_*(*iter)->mesh_.normal( *vit ) );
        }
        ++cnt;
        ++iter;
        if(iter==mesh_list_.end())iter=mesh_list_.begin();
    }
    glEnd();
  }

//  if (show_fnormals_)
//  {
//    typename Mesh::FaceIter fit;
//    glDisable(GL_LIGHTING);
//    glBegin(GL_LINES);
//    glColor3f(0.705f, 0.976f, 0.270f); // greenish
//    for(fit=first_->mesh_.faces_begin(); fit!=first_->mesh_.faces_end(); ++fit)
//    {
//      glVertex( first_->mesh_.property(fp_normal_base_, *fit) );
//      glVertex( first_->mesh_.property(fp_normal_base_, *fit) +
//                normal_scale_*first_->mesh_.normal( *fit ) );
//    }
//    for(fit=second_->mesh_.faces_begin(); fit!=second_->mesh_.faces_end(); ++fit)
//    {
//      glVertex( second_->mesh_.property(fp_normal_base_, *fit) );
//      glVertex( second_->mesh_.property(fp_normal_base_, *fit) +
//                normal_scale_*second_->mesh_.normal( *fit ) );
//    }
//    glEnd();
//  }
    draw_selected();
}


//-----------------------------------------------------------------------------

template <typename M>
void
MeshListViewerWidgetT<M>::enable_strips()
{
  if (!f_strips_)
  {
    f_strips_ = true;
    add_draw_mode("Strips'n VertexArrays");
    add_draw_mode("Show Strips");
  }
}

//-----------------------------------------------------------------------------

template <typename M>
void
MeshListViewerWidgetT<M>::disable_strips()
{
  if (f_strips_)
  {
    f_strips_ = false;
    del_draw_mode("Show Strips");
    del_draw_mode("Strip'n VertexArrays");
  }
}

template <typename M>
void
MeshListViewerWidgetT<M>::processSelections()
{
    arma::uvec new_selected;
    selections_.selectAll<M>(mesh_list_[current_mesh_start_]->mesh_,new_selected,radius_);
    for(size_t i=0;i<new_selected.size();++i)
    {
        current_selected_.push_back(new_selected(i));
    }
}
//-----------------------------------------------------------------------------

#define TEXMODE( Mode ) \
   tex_mode_ = Mode; std::cout << "Texture mode set to " << #Mode << std::endl

template <typename M>
void
MeshListViewerWidgetT<M>::keyPressEvent( QKeyEvent* _event)
{
  switch( _event->key() )
  {
    case Key_C:
      if ( current_draw_mode()=="Points" && ( _event->modifiers() & AltModifier )
           )
      {
        use_color_ = !use_color_;
        std::cout << "use color: " << (use_color_?"yes\n":"no\n");
        if (!use_color_)
          glColor3f(0.60048044f, 0.7001344f, 0.79978836f);
        updateGL();
      }
      break;

    case Key_N:
      if ( _event->modifiers() & ShiftModifier )
      {
        show_fnormals_ = !show_fnormals_;
        std::cout << "show face normals: " << (show_fnormals_?"yes\n":"no\n");
      }
      else
      {
        show_vnormals_ = !show_vnormals_;
        std::cout << "show vertex normals: " << (show_vnormals_?"yes\n":"no\n");
      }
      updateGL();
      break;

    case Key_I:
      std::cout << "mesh number    : " << ( mesh_list_.size() - 1 ) << std::endl;
      std::cout << "binary  input  : " << opt_.check(opt_.Binary) << std::endl;
      std::cout << "swapped input  : " << opt_.check(opt_.Swap) << std::endl;
      std::cout << "vertex normal  : "
                << opt_.check(opt_.VertexNormal) << std::endl;
      std::cout << "vertex texcoord: "
                << opt_.check(opt_.VertexTexCoord) << std::endl;
      std::cout << "vertex color   : "
                << opt_.check(opt_.VertexColor) << std::endl;
      std::cout << "face normal    : "
                << opt_.check(opt_.FaceNormal) << std::endl;
      std::cout << "face color     : "
                << opt_.check(opt_.FaceColor) << std::endl;
      std::flush(std::cout);
      this->QGLViewerWidget::keyPressEvent( _event );
      break;

    case Key_T:
      switch( tex_mode_ )
      {
        case GL_MODULATE: TEXMODE(GL_DECAL); break;
        case GL_DECAL:    TEXMODE(GL_BLEND); break;
        case GL_BLEND:    TEXMODE(GL_REPLACE); break;
        case GL_REPLACE:  TEXMODE(GL_MODULATE); break;
      }
      updateGL();
      break;
  case Key_Up:
      if(!mesh_list_.empty())
      {
          if((current_visible_num_<32)&&(current_visible_num_<mesh_list_.size()))
          {
              current_visible_num_++;
              updateGL();
          }
      }
      break;
  case Key_Down:
      if(!mesh_list_.empty())
      {
          if( current_visible_num_ > 1 )
          {
              current_visible_num_--;
              updateGL();
          }
      }
      break;
  case Key_Left:
      if(!mesh_list_.empty())
      {
          if( current_mesh_start_==0 )current_mesh_start_= mesh_list_.size() - 1 ;
          else current_mesh_start_ -- ;
          current_selected_.clear();
          updateGL();
      }
      break;
  case Key_Right:
      if(!mesh_list_.empty())
      {
          if(current_mesh_start_ == mesh_list_.size()-1)current_mesh_start_=0;
          else current_mesh_start_ ++ ;
          current_selected_.clear();
          updateGL();
      }
      break;
  case Key_Delete:
      current_selected_.pop_back();
      updateGL();
      break;

  case Key_1:
    set_scale_dim(0);
    break;

  case Key_2:
    set_scale_dim(1);
    break;

  case Key_3:
    set_scale_dim(2);
    break;

  case Key_A:
  case Key_D:
  case Key_W:
  case Key_S:
  case Key_Q:
  case Key_E:
  case Key_R:
  case Key_F:
    transform_current(_event->key());
    updateGL();
    break;
    default:
      this->QGLViewerWidget::keyPressEvent( _event );
  }
}
#undef TEXMODE

template <typename M>
void MeshListViewerWidgetT<M>::transform_current(int key)
{
    float dtheta;
    arma::fvec t(3,arma::fill::zeros);
    arma::fmat R(3,3,arma::fill::eye);
    M& mesh = mesh_list_[current_mesh_start_]->mesh_;
    arma::fmat vv((float*)mesh.points(),3,mesh.n_vertices(),false,true);
    arma::fmat vn((float*)mesh.vertex_normals(),3,mesh.n_vertices(),false,true);
    switch(key)
    {
    case Key_W:
        t = {0.01,0,0};
        vv.each_col() += t;
        break;
    case Key_S:
        t = {-0.01,0,0};
        vv.each_col() += t;
        break;
    case Key_A:
        t = {0,0.01,0};
        vv.each_col() += t;
        break;
    case Key_D:
        t = {0,-0.01,0};
        vv.each_col() += t;
        break;
    case Key_R:
        t = {0,0,0.01};
        vv.each_col() += t;
        break;
    case Key_F:
        t = {0,0,-0.01};
        vv.each_col() += t;
        break;
    case Key_Q:
        t = arma::mean(vv,1);
        dtheta =  - M_PI / 180.0 * 5.0;
        R = {{std::cos(dtheta),std::sin(dtheta),0},{-std::sin(dtheta),std::cos(dtheta),0},{0,0,1}};
        vv.each_col() -= t;
        vv = R*vv;
        vv.each_col() += t;
        vn = R*vn;
        t = {0,0,0};
        break;
    case Key_E:
        t = arma::mean(vv,1);
        dtheta = M_PI / 180.0 * 5.0;
        R = {{std::cos(dtheta),std::sin(dtheta),0},{-std::sin(dtheta),std::cos(dtheta),0},{0,0,1}};
        vv.each_col() -= t;
        vv = R*vv;
        vv.each_col() += t;
        vn = R*vn;
        t = {0,0,0};
        break;
    }
    this->QGLViewerWidget::notify_been_transfomed(R,t,int(current_mesh_start_));
}

template <typename M>
void MeshListViewerWidgetT<M>::scale_current(float s)
{
    M& mesh = mesh_list_[current_mesh_start_]->mesh_;
    arma::fmat vv((float*)mesh.points(),3,mesh.n_vertices(),false,true);
    arma::fvec t = arma::mean(vv,1);
    vv.each_col() -= t;
    vv.row(scale_dim_) *= (1.0+s);
    vv.each_col() += t;
}

template <typename M>
void MeshListViewerWidgetT<M>::wheelEvent(QWheelEvent* _event)
{
    if( _event->modifiers() & ControlModifier )
    {
        float d = -(float)_event->delta() / 120.0 * 0.05;
        scale_current(d);
        _event->accept();
        updateGL();
    }else
    this->QGLViewerWidget::wheelEvent(_event);
}

template <typename M>
bool MeshListViewerWidgetT<M>::open_texture( const char *_filename )
{
   QImage texsrc;
   QString fname = _filename;

   if (texsrc.load( fname ))
   {
     return set_texture( texsrc );
   }

   return false;
}

template <typename M>
void MeshListViewerWidgetT<M>::set_center_at_mesh(const Mesh& mesh_)
{
    // bounding box
    typename Mesh::ConstVertexIter vIt(mesh_.vertices_begin());
    typename Mesh::ConstVertexIter vEnd(mesh_.vertices_end());

    typedef typename Mesh::Point Point;
    using OpenMesh::Vec3f;

    Vec3f bbMin, bbMax;

    bbMin = bbMax = OpenMesh::vector_cast<Vec3f>(mesh_.point(*vIt));

    for (size_t count=0; vIt!=vEnd; ++vIt, ++count)
    {
        bbMin.minimize( OpenMesh::vector_cast<Vec3f>(mesh_.point(*vIt)));
        bbMax.maximize( OpenMesh::vector_cast<Vec3f>(mesh_.point(*vIt)));
    }

    std::clog<<"Bounding Box("<<bbMin<<")-("<<bbMax<<")"<<std::endl;
    // set center and radius
    set_scene_pos( (bbMin+bbMax)*0.5, (bbMin-bbMax).norm()*0.5 );
    // for normal display
    normal_scale_ = (bbMax-bbMin).min()*0.05f;
    std::cerr<<"normal scale:"<<normal_scale_<<std::endl;
}

#endif // MESHLISTVIEWERWIDGETT_HPP

