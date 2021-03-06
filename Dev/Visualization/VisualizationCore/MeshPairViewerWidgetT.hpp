#ifndef  MESHPAIRVIEWERWIDGETT_HPP
#define MESHPAIRVIEWERWIDGETT_HPP

#include <iostream>
#include <fstream>
// --------------------
#include <QImage>
#include <QFileInfo>
#include <QKeyEvent>
// --------------------
#include <OpenMesh/Core/Utils/vector_cast.hh>
#include <OpenMesh/Tools/Utils/Timer.hh>
#include "MeshPairViewerWidgetT.h"

using namespace OpenMesh;
using namespace Qt;

#if defined(_MSC_VER)
#  undef min
#  undef max
#endif

using namespace Qt;
//== IMPLEMENTATION ==========================================================


template <typename M>
bool
MeshPairViewerWidgetT<M>::open_mesh(const char* _filename,Mesh& mesh_,Stripifier& strips_ ,IO::Options _opt)
{
  // load mesh
  // calculate normals
  // set scene center and radius

  mesh_.request_face_normals();
  mesh_.request_face_colors();
  mesh_.request_vertex_normals();
  mesh_.request_vertex_colors();
  mesh_.request_vertex_texcoords2D();

  std::cout << "Loading from file '" << _filename << "'\n";
  if ( IO::read_mesh(mesh_, _filename, _opt ) )
  {
    // store read option
    opt_ = _opt;

    // update face and vertex normals
    if ( ! opt_.check( IO::Options::FaceNormal ) )
      mesh_.update_face_normals();
    else
      std::cout << "File provides face normals\n";

    if ( ! opt_.check( IO::Options::VertexNormal ) )
      mesh_.update_vertex_normals();
    else
      std::cout << "File provides vertex normals\n";


    // check for possible color information
    if ( opt_.check( IO::Options::VertexColor ) )
    {
      std::cout << "File provides vertex colors\n";
      add_draw_mode("Colored Vertices");
    }
    else
      mesh_.release_vertex_colors();

    if ( _opt.check( IO::Options::FaceColor ) )
    {
      std::cout << "File provides face colors\n";
      add_draw_mode("Solid Colored Faces");
      add_draw_mode("Smooth Colored Faces");
    }
    else
      mesh_.release_face_colors();

    if ( _opt.check( IO::Options::VertexTexCoord ) )
      std::cout << "File provides texture coordinates\n";

    // info
    std::clog << mesh_.n_vertices() << " vertices, "
          << mesh_.n_edges()    << " edge, "
          << mesh_.n_faces()    << " faces\n";

    // base point for displaying face normals
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
bool MeshPairViewerWidgetT<M>::save_mesh(const std::string& _filename,Mesh& mesh_,IO::Options _opt)
{
  // load mesh
  // calculate normals
  // set scene center and radius

//  mesh_.request_face_normals();
//  mesh_.request_face_colors();
  if(!mesh_.has_vertex_normals())mesh_.request_vertex_normals();
  if(!mesh_.has_vertex_colors())mesh_.request_vertex_colors();
//  mesh_.request_vertex_texcoords2D();

  std::cerr << "Saving to file '" << _filename << "'\n";
  if ( IO::write_mesh(mesh_, _filename, _opt, 10 ) )
  {
      std::cerr << "Saved to file '" << _filename << "'\n";
      return true;
  }
  return false;
}

template <typename M>
void MeshPairViewerWidgetT<M>::set_center_at_mesh(const Mesh& mesh_)
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
    normal_scale_ = (bbMax-bbMin).mean()*0.05f;
}

//-----------------------------------------------------------------------------

template <typename M>
bool MeshPairViewerWidgetT<M>::open_texture( const char *_filename )
{
   QImage texsrc;
   QString fname = _filename;

   if (texsrc.load( fname ))
   {
     return set_texture( texsrc );
   }
   return false;
}


//-----------------------------------------------------------------------------

template <typename M>
bool MeshPairViewerWidgetT<M>::set_texture( QImage& _texsrc )
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
void MeshPairViewerWidgetT<M>::draw_selected()
{
    if(first_selected_.empty())return;
    glDisable(GL_LIGHTING);
    glPointSize(2.0*point_size_);
    glBegin(GL_POINTS);
    glColor3f(1.0f,0.0f,0.0f); // red
    float* p = (float*)first_->mesh_.points();
    std::vector<arma::uword>::iterator iter;
    for(iter=first_selected_.begin();iter!=first_selected_.end();++iter)
    {
        glVertex3fv(&p[3*(*iter)]);
    }
    glEnd();
}

template <typename M>
void
MeshPairViewerWidgetT<M>::draw_openmesh(MeshBundle<Mesh>& b,const std::string& _draw_mode)
{
//    std::cout<<"draw_openmesh"<<std::endl;
//    std::cout<<"draw_mode_:"<<_draw_mode<<std::endl;
  Mesh& mesh_ = b.mesh_;
  Stripifier& strips_ = b.strips_;
  MeshColor<Mesh>& color_ = b.custom_color_;

  typename Mesh::ConstFaceIter    fIt(mesh_.faces_begin()),
                                  fEnd(mesh_.faces_end());

  typename Mesh::ConstFaceVertexIter fvIt;
  typename Mesh::ConstVertexIter vIt(mesh_.vertices_begin()),vEnd(mesh_.vertices_end());

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
     for (; fIt!=fEnd; ++fIt)
     {
        glLineWidth(2.0);
        glBegin(GL_POLYGON);
        glColor3f(1.0,1.0,1.0);
        for(fvIt=mesh_.cfv_begin(*fIt);fvIt!=mesh_.cfv_end(*fIt);++fvIt)
        {
            glVertex3fv( &mesh_.point(*fvIt)[0] );
        }
        glEnd();
     }
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
    glEnable( GL_POINT_SMOOTH );
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, mesh_.points());

    glEnableClientState(GL_NORMAL_ARRAY);
    glNormalPointer(GL_FLOAT, 0, mesh_.vertex_normals());

    if ( mesh_.has_vertex_colors() )
    {
      glEnableClientState( GL_COLOR_ARRAY );
      glColorPointer(3, GL_UNSIGNED_BYTE, 0,mesh_.vertex_colors());
    }

    glPointSize(point_size_);
    glBegin(GL_POINTS);
    for (  ; vIt!=vEnd; ++vIt)
    {
      glMaterial(mesh_,*vIt);
      glMaterial(mesh_,*vIt,GL_FRONT_AND_BACK,GL_SPECULAR);
      glArrayElement(vIt->idx());
    }
    glEnd();

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisable( GL_POINT_SMOOTH );
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

    glBegin(GL_TRIANGLES);
    for (; fIt!=fEnd; ++fIt)
    {
      glColor( mesh_,*fIt );

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

    if(mesh_.has_vertex_normals())
    {
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(GL_FLOAT, 0, mesh_.vertex_normals());
    }

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
    glDisableClientState(GL_NORMAL_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisable( GL_POINT_SMOOTH );
  }else if( _draw_mode == "VoxelGraph" )
  {
      glLineWidth(2.0);
      glDisable(GL_LIGHTING);
      if( !b.graph_.voxel_edge_colors.empty() && ( b.graph_.voxel_neighbors.n_cols == b.graph_.voxel_edge_colors.n_cols ) )
      {
          GLubyte* c_ptr = (GLubyte*)b.graph_.voxel_edge_colors.memptr();
          arma::Mat<uint16_t>::iterator iter;
          glBegin(GL_LINES);
          for(iter=b.graph_.voxel_neighbors.begin();iter!=b.graph_.voxel_neighbors.end();++(++iter))
          {
              if(b.graph_.voxel_edge_colors.n_rows==4){
                  glColor4ubv(c_ptr);
              }
              if(b.graph_.voxel_edge_colors.n_rows==3){
                  glColor3ubv(c_ptr);
              }
              glVertex3fv((GLfloat*)b.graph_.voxel_centers.colptr(*iter));
              if(b.graph_.voxel_edge_colors.n_rows==4){
                  glColor4ubv(c_ptr);
              }
              if(b.graph_.voxel_edge_colors.n_rows==3){
                  glColor3ubv(c_ptr);
              }
              glVertex3fv((GLfloat*)b.graph_.voxel_centers.colptr(*(iter+1)));
              if(b.graph_.voxel_edge_colors.n_rows==4){
                  c_ptr+=4;
              }
              if(b.graph_.voxel_edge_colors.n_rows==3){
                  c_ptr+=3;
              }
          }
          glEnd();
      }else
      {
          glEnableClientState(GL_VERTEX_ARRAY);
          glVertexPointer(3,GL_FLOAT,0,static_cast<GLfloat*>(b.graph_.voxel_centers.memptr()));
          glDrawElements(GL_LINES,b.graph_.voxel_neighbors.size(),GL_UNSIGNED_SHORT,static_cast<GLushort*>(b.graph_.voxel_neighbors.memptr()));
          glDisableClientState(GL_VERTEX_ARRAY);
      }
  }
}

//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------


template <typename M>
void
MeshPairViewerWidgetT<M>::draw_scene(const std::string& _draw_mode)
{
//    std::cout<<"draw_scene_:"<<_draw_mode<<std::endl;

  if ( (!first_->mesh_.n_vertices()) && (!second_->mesh_.n_vertices()) )
    return;

#if defined(OM_USE_OSG) && OM_USE_OSG
  else if ( _draw_mode == "OpenSG Indices")
  {
    glEnable(GL_LIGHTING);
    glShadeModel(GL_SMOOTH);
    if(0<first_->mesh_.n_vertices())draw_openmesh( first_->mesh_ , _draw_mode );
    if(0<second_->mesh_.n_vertices())draw_openmesh( second_->mesh_ , _draw_mode );
  }
  else
#endif
  if ( _draw_mode == "Points" )
  {
    glDisable(GL_LIGHTING);
    if(0<first_->mesh_.n_vertices())draw_openmesh( *first_ , _draw_mode );
    if(0<second_->mesh_.n_vertices())draw_openmesh( *second_ , _draw_mode );
  }
  else if (_draw_mode == "Wireframe")
  {
    glDisable(GL_LIGHTING);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    if(0<first_->mesh_.n_vertices())draw_openmesh( *first_, _draw_mode );
    if(0<second_->mesh_.n_vertices())draw_openmesh( *second_, _draw_mode );
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }

  else if ( _draw_mode == "Hidden-Line" )
  {
    glDisable(GL_LIGHTING);
    glShadeModel(GL_FLAT);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glColor4f( 0.0f, 0.0f, 0.0f, 1.0f );
    glDepthRange(0.01, 1.0);
    if(0<first_->mesh_.n_vertices())draw_openmesh( *first_, "Wireframe" );
    if(0<second_->mesh_.n_vertices())draw_openmesh( *second_, "Wireframe" );

    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE);
    glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
    glDepthRange( 0.0, 1.0 );
    if(0<first_->mesh_.n_vertices())draw_openmesh( *first_, "Wireframe" );
    if(0<second_->mesh_.n_vertices())draw_openmesh( *second_, "Wireframe" );

    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL);
  }

  else if ( _draw_mode == "Octree" )
  {
    glDisable(GL_LIGHTING);
    if(0<first_->mesh_.n_vertices())draw_openmesh( *first_ , "Points" );

    glPolygonMode( GL_FRONT_AND_BACK, GL_LINE);
    glColor4f( 112.0f/255.0f, 170.0f/255.0f, 57.0/255.0f, 1.0f );
    glDepthRange( 0.0, 1.0 );
    if(0<second_->mesh_.n_vertices())draw_openmesh( *second_, "Wireframe" );

    glPolygonMode( GL_FRONT_AND_BACK, GL_FILL);
  }else if( _draw_mode == "VoxelGraph")
  {
      glDisable(GL_LIGHTING);
      if(0<first_->mesh_.n_vertices())draw_openmesh( *first_ , "Points" );

      glPolygonMode( GL_FRONT_AND_BACK, GL_LINE);
      glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );
      glDepthRange( 0.0, 1.0 );
      if(0<first_->graph_.voxel_neighbors.n_cols)draw_openmesh( *first_, "VoxelGraph" );
  }
  else if( _draw_mode == "Plate")
  {
      glEnable(GL_LIGHTING);
      glShadeModel(GL_SMOOTH);
      if(0<first_->mesh_.n_vertices())draw_openmesh( *first_ , "Colored Vertices" );
      glEnable(GL_LIGHTING);
      glShadeModel(GL_FLAT);
      if(0<second_->mesh_.n_vertices())draw_openmesh( *second_, "Flat Colored Vertices" );
  }

  else if (_draw_mode == "Solid Flat")
  {
    glEnable(GL_LIGHTING);
    glShadeModel(GL_FLAT);
    if(0<first_->mesh_.n_vertices())draw_openmesh( *first_, _draw_mode );
    if(0<second_->mesh_.n_vertices())draw_openmesh( *second_, _draw_mode );
  }

  else if (_draw_mode == "Solid Smooth"        ||
       _draw_mode == "Strips'n VertexArrays" )
  {
    glEnable(GL_LIGHTING);
    glShadeModel(GL_SMOOTH);
    if(0<first_->mesh_.n_vertices())draw_openmesh( *first_, _draw_mode );
    if(0<second_->mesh_.n_vertices())draw_openmesh( *second_, _draw_mode );
  }

  else if (_draw_mode == "Show Strips")
  {
    glDisable(GL_LIGHTING);
    if(0<first_->mesh_.n_vertices())draw_openmesh( *first_, _draw_mode );
    if(0<second_->mesh_.n_vertices())draw_openmesh( *second_, _draw_mode );
  }

  else if (_draw_mode == "Colored Vertices" )
  {
    glEnable(GL_LIGHTING);
    glShadeModel(GL_SMOOTH);
    if(0<first_->mesh_.n_vertices())draw_openmesh( *first_, _draw_mode );
    if(0<second_->mesh_.n_vertices())draw_openmesh( *second_, _draw_mode );
  }

  else if (_draw_mode == "Flat Colored Vertices" )
  {
    glEnable(GL_LIGHTING);
    glShadeModel(GL_FLAT);
    if(0<first_->mesh_.n_vertices())draw_openmesh( *first_, _draw_mode );
    if(0<second_->mesh_.n_vertices())draw_openmesh( *second_, _draw_mode );
  }

  else if (_draw_mode == "Solid Colored Faces")
  {
    glDisable(GL_LIGHTING);
    glShadeModel(GL_FLAT);
    if(0<first_->mesh_.n_vertices())draw_openmesh( *first_, _draw_mode );
    if(0<second_->mesh_.n_vertices())draw_openmesh( *second_, _draw_mode );
    setDefaultMaterial();
  }

  else if (_draw_mode == "Smooth Colored Faces" )
  {
    glEnable(GL_LIGHTING);
    glShadeModel(GL_SMOOTH);
    if(0<first_->mesh_.n_vertices())draw_openmesh( *first_, _draw_mode );
    if(0<second_->mesh_.n_vertices())draw_openmesh( *second_, _draw_mode );
    setDefaultMaterial();
  }

  if (show_vnormals_)
  {
    typename Mesh::VertexIter vit;
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);
    glColor3f(1.000f, 0.803f, 0.027f); // orange
    for(vit=first_->mesh_.vertices_begin(); vit!=first_->mesh_.vertices_end(); ++vit)
    {
      glVertex( first_->mesh_ , *vit );
      glVertex( first_->mesh_.point( *vit ) + normal_scale_*first_->mesh_.normal( *vit ) );
    }
    glEnd();
  }
  if (show_fnormals_)
  {
    typename Mesh::FaceIter fit;
    glDisable(GL_LIGHTING);
    glBegin(GL_LINES);
    glColor3f(0.705f, 0.976f, 0.270f); // greenish
    for(fit=first_->mesh_.faces_begin(); fit!=first_->mesh_.faces_end(); ++fit)
    {
      glVertex( first_->mesh_.property(fp_normal_base_, *fit) );
      glVertex( first_->mesh_.property(fp_normal_base_, *fit) +
                normal_scale_*first_->mesh_.normal( *fit ) );
    }
    glEnd();
  }
  draw_selected();
}


//-----------------------------------------------------------------------------

template <typename M>
void
MeshPairViewerWidgetT<M>::enable_strips()
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
MeshPairViewerWidgetT<M>::disable_strips()
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
MeshPairViewerWidgetT<M>::processSelections()
{
    arma::uvec new_selected;
    selections_.selectAll<M>(first_->mesh_,new_selected,radius_);
    for(size_t i=0;i<new_selected.size();++i)
    {
        first_selected_.push_back(new_selected(i));
    }
}

//-----------------------------------------------------------------------------

#define TEXMODE( Mode ) \
   tex_mode_ = Mode; std::cout << "Texture mode set to " << #Mode << std::endl

template <typename M>
void
MeshPairViewerWidgetT<M>::keyPressEvent( QKeyEvent* _event)
{

  switch( _event->key() )
  {
    case Key_C:
      if ( first_->mesh_.has_vertex_colors() &&
           second_->mesh_.has_vertex_colors() &&
           (current_draw_mode()=="Points") && ( _event->modifiers() & AltModifier )
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
      std::cout << "\n# Vertices     : " << first_->mesh_.n_vertices() << std::endl;
      std::cout << "# Edges        : " << first_->mesh_.n_edges()    << std::endl;
      std::cout << "# Faces        : " << first_->mesh_.n_faces()    << std::endl;
      std::cout << "\n# Vertices     : " << second_->mesh_.n_vertices() << std::endl;
      std::cout << "# Edges        : " << second_->mesh_.n_edges()    << std::endl;
      std::cout << "# Faces        : " << second_->mesh_.n_faces()    << std::endl;
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

    // for box opertation:
    case Key_1:
      if(cube_flag_)set_box_dim(0);
      break;

    case Key_2:
      if(cube_flag_)set_box_dim(1);
      break;

    case Key_3:
      if(cube_flag_)set_box_dim(2);
      break;

    case Key_A:
    case Key_D:
    case Key_W:
    case Key_S:
    case Key_Q:
    case Key_E:
    case Key_Up:
    case Key_Down:
      transform_box(_event->key());
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
  case Key_Delete:
      if(!first_selected_.empty())
          first_selected_.pop_back();
      updateGL();
      break;
    default:
      this->QGLViewerWidget::keyPressEvent( _event );
  }
}

template <typename M>
void MeshPairViewerWidgetT<M>::wheelEvent(QWheelEvent* _event)
{
    if( _event->modifiers() & ControlModifier )
    {
        if(cube_flag_)
        {
            float d = -(float)_event->delta() / 120.0 * 0.02;
            scale_box(d);
        }
        _event->accept();
    }else
    this->QGLViewerWidget::wheelEvent(_event);
}

template <typename M>
Common::Cube::PtrLst MeshPairViewerWidgetT<M>::boxes(void)
{
    Common::Cube::PtrLst r;
    r.reserve(cube_index_.size());
    for(std::vector<uint32_t>::iterator iter=cube_index_.begin();iter!=cube_index_.end();++iter)
    {
        r.push_back(cube_lst_[*iter]);
    }
    return r;
}

template <typename M>
void MeshPairViewerWidgetT<M>::scale_box(float s)
{
    if(cube_iter_==cube_index_.end())return;
    Cube& cube = *cube_lst_[*cube_iter_];
    arma::fvec scale = cube.size();
    scale( cube_dim_ ) += s;
    cube.scaleTo(scale);
}

template <typename M>
void MeshPairViewerWidgetT<M>::transform_box(int key)
{
    if(cube_iter_==cube_index_.end())return;
    Cube& cube = *cube_lst_[*cube_iter_];
    float dtheta;
    arma::fvec t;
    arma::fmat R;
    switch(key)
    {
    case Key_W:
        t = {0.01,0,0};
        cube.translate(t,cube);
        break;
    case Key_S:
        t = {-0.01,0,0};
        cube.translate(t,cube);
        break;
    case Key_A:
        t = {0,0.01,0};
        cube.translate(t,cube);
        break;
    case Key_D:
        t = {0,-0.01,0};
        cube.translate(t,cube);
        break;
    case Key_Up:
        t = {0,0,0.01};
        cube.translate(t,cube);
        break;
    case Key_Down:
        t = {0,0,-0.01};
        cube.translate(t,cube);
        break;
    case Key_Q:
        t = {0,0,0};
        dtheta =  - M_PI / 180.0 * 5.0;
        R = {{std::cos(dtheta),std::sin(dtheta),0},{-std::sin(dtheta),std::cos(dtheta),0},{0,0,1}};
        cube.rotate(R,cube);
        break;
    case Key_E:
        t = {0,0,0};
        dtheta = M_PI / 180.0 * 5.0;
        R = {{std::cos(dtheta),std::sin(dtheta),0},{-std::sin(dtheta),std::cos(dtheta),0},{0,0,1}};
        cube.rotate(R,cube);
        break;
    }
}

template <typename M>
void MeshPairViewerWidgetT<M>::add_sub_box(void)
{
    if(!cube_flag_)return;
    if(cube_index_.size()>=cube_max_num_)return;
    arma::uword oldlabel = 0;
    if(!cube_index_.empty()){
        Cube& cubeold = *cube_lst_[*cube_iter_];
        cubeold.colorByLabel(cubeold.label_);
        oldlabel = cubeold.label_;
    }
    cube_index_.push_back(cube_index_.back()+1);
    cube_iter_ = cube_index_.end() - 1;
    Cube& cubenew = *cube_lst_[*cube_iter_];
    if(oldlabel!=0)cubenew.label_ = oldlabel;
    else cubenew.label_ = 1;
    arma::fvec t0(3,arma::fill::zeros);
    Cube::newCube()->translate(t0,cubenew);
    cubenew.colorByLabel(0);
}

template <typename M>
void
MeshPairViewerWidgetT<M>::add_box(void)
{
    if(!cube_flag_)return;
    if(cube_index_.size()>=cube_max_num_)return;
    arma::uword oldlabel = 0;
    if(!cube_index_.empty()){
        Cube& cubeold = *cube_lst_[*cube_iter_];
        cubeold.colorByLabel(cubeold.label_);
        oldlabel = cubeold.label_;
    }
    cube_index_.push_back(cube_index_.back()+1);
    cube_iter_ = cube_index_.end() - 1;
    Cube& cubenew = *cube_lst_[*cube_iter_];
    cubenew.label_ = oldlabel + 1;
    arma::fvec t0(3,arma::fill::zeros);
    Cube::newCube()->translate(t0,cubenew);
    cubenew.colorByLabel(0);
}

template <typename M>
void
MeshPairViewerWidgetT<M>::del_box(void)
{
    if(!cube_flag_)return;
    arma::fvec s0(3,arma::fill::zeros);
    uint32_t tmp = *cube_iter_;
    *cube_iter_ = cube_index_.back();
    cube_index_.back() = tmp;
    cube_index_.pop_back();
    Cube& cube = *cube_lst_[tmp];
    cube.scaleTo(s0);
}

template <typename M>
bool
MeshPairViewerWidgetT<M>::mod_box(void)
{
    arma::fvec s(3,arma::fill::zeros);
    if( !cube_flag_ ){
        cube_lst_ = Cube::newCubes(second_->mesh_,20);
        cube_index_.reserve(cube_max_num_);
        cube_index_.push_back(0);
        cube_iter_ = cube_index_.begin();
        for(Cube::PtrLst::iterator iter=cube_lst_.begin();iter!=cube_lst_.end();++iter)
        {
            Cube& cube = **iter;
            cube.scaleTo(s);
        }
        Cube& cubenew = *cube_lst_[*cube_iter_];
        arma::fvec t0(3,arma::fill::zeros);
        Cube::newCube()->translate(t0,cubenew);
        cubenew.label_ = 1;
        cubenew.colorByLabel(0);
        cube_flag_ = true;
    }
    else if(cube_flag_){
        cube_lst_.clear();
        cube_index_.clear();
        second_->mesh_.clear();
        cube_flag_ = false;
    }
    return cube_flag_;
}

template <typename M>
void
MeshPairViewerWidgetT<M>::next_box(void)
{
    if(!cube_flag_)return;
    if( cube_iter_ != cube_index_.end() )
    {
        Cube& cubeold = *cube_lst_[*cube_iter_];
        cubeold.colorByLabel( cubeold.label_ );
        ++cube_iter_;
    }else{
        cube_iter_ = cube_index_.begin();
    }
    if( cube_iter_ != cube_index_.end() )
    {
        Cube& cubenew = *cube_lst_[*cube_iter_];
        cubenew.colorByLabel(0);
    }
}

#undef TEXMODE
#endif
//=============================================================================

