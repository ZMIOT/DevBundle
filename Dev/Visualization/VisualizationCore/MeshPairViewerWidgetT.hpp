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
bool MeshPairViewerWidgetT<M>::save_mesh(const char* _filename,Mesh& mesh_,IO::Options _opt)
{
  // load mesh
  // calculate normals
  // set scene center and radius

  mesh_.request_face_normals();
  mesh_.request_face_colors();
  mesh_.request_vertex_normals();
  mesh_.request_vertex_colors();
  mesh_.request_vertex_texcoords2D();

  std::cout << "Saving to file '" << _filename << "'\n";
  if ( IO::write_mesh(mesh_, _filename, _opt, 16 ) )
  {
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
    normal_scale_ = (bbMax-bbMin).min()*0.05f;
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
void
MeshPairViewerWidgetT<M>::draw_openmesh(MeshBundle<Mesh>& b,const std::string& _draw_mode)
{
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
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, mesh_.points());

    if ( use_color_)
    {
        if(mesh_.has_vertex_colors())
        {
            glEnableClientState(GL_COLOR_ARRAY);
            glColorPointer(3, GL_UNSIGNED_BYTE, 0, mesh_.vertex_colors());
        }else{
            glEnableClientState(GL_COLOR_ARRAY);
            glColorPointer(4, GL_UNSIGNED_BYTE, 0, color_.vertex_colors());
        }
    }
    glPointSize(4.0);
    glDrawArrays( GL_POINTS, 0, static_cast<GLsizei>(mesh_.n_vertices()) );
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
  }


}

//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------


template <typename M>
void
MeshPairViewerWidgetT<M>::draw_scene(const std::string& _draw_mode)
{

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
    glDisable(GL_LIGHTING);
    glShadeModel(GL_SMOOTH);
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
    for(vit=second_->mesh_.vertices_begin(); vit!=second_->mesh_.vertices_end(); ++vit)
    {
      glVertex( second_->mesh_ , *vit );
      glVertex( second_->mesh_.point( *vit ) + normal_scale_*second_->mesh_.normal( *vit ) );
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
    for(fit=second_->mesh_.faces_begin(); fit!=second_->mesh_.faces_end(); ++fit)
    {
      glVertex( second_->mesh_.property(fp_normal_base_, *fit) );
      glVertex( second_->mesh_.property(fp_normal_base_, *fit) +
                normal_scale_*second_->mesh_.normal( *fit ) );
    }
    glEnd();
  }
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


//-----------------------------------------------------------------------------

#define TEXMODE( Mode ) \
   tex_mode_ = Mode; std::cout << "Texture mode set to " << #Mode << std::endl

template <typename M>
void
MeshPairViewerWidgetT<M>::keyPressEvent( QKeyEvent* _event)
{
  switch( _event->key() )
  {
    case Key_D:
      if ( first_->mesh_.has_vertex_colors() &&
           second_->mesh_.has_vertex_colors() &&
           (current_draw_mode()=="Points")
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

    default:
      this->QGLViewerWidget::keyPressEvent( _event );
  }
}

#undef TEXMODE
#endif
//=============================================================================

