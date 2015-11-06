/* ========================================================================= *
 *                                                                           *
 *                               OpenMesh                                    *
 *           Copyright (c) 2001-2015, RWTH-Aachen University                 *
 *           Department of Computer Graphics and Multimedia                  *
 *                          All rights reserved.                             *
 *                            www.openmesh.org                               *
 *                                                                           *
 *---------------------------------------------------------------------------*
 * This file is part of OpenMesh.                                            *
 *---------------------------------------------------------------------------*
 *                                                                           *
 * Redistribution and use in source and binary forms, with or without        *
 * modification, are permitted provided that the following conditions        *
 * are met:                                                                  *
 *                                                                           *
 * 1. Redistributions of source code must retain the above copyright notice, *
 *    this list of conditions and the following disclaimer.                  *
 *                                                                           *
 * 2. Redistributions in binary form must reproduce the above copyright      *
 *    notice, this list of conditions and the following disclaimer in the    *
 *    documentation and/or other materials provided with the distribution.   *
 *                                                                           *
 * 3. Neither the name of the copyright holder nor the names of its          *
 *    contributors may be used to endorse or promote products derived from   *
 *    this software without specific prior written permission.               *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED *
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A           *
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER *
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,  *
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,       *
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR        *
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    *
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      *
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        *
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              *
 *                                                                           *
 * ========================================================================= */

/*===========================================================================*\
 *                                                                           *             
 *   $Revision: 1258 $                                                         *
 *   $Date: 2015-04-28 15:07:46 +0200 (Di, 28 Apr 2015) $                   *
 *                                                                           *
\*===========================================================================*/

#ifndef MESHVIEWERWIDGET_H
#define MESHVIEWERWIDGET_H

//== INCLUDES =================================================================
#include <iostream>
#include <visualizationcore_global.h>
#include <QWidget>
#include <QString>
#include <QMessageBox>
#include <QFileDialog>
#include <OpenMesh/Tools/Utils/getopt.h>
#include <OpenMesh/Tools/Utils/Timer.hh>
#include "MeshType.h"
#include "MeshViewerWidgetT.h"

//== CLASS DEFINITION =========================================================





//== CLASS DEFINITION =========================================================

class VISUALIZATIONCORESHARED_EXPORT MeshViewerWidget : public MeshViewerWidgetT<DefaultMesh>
{
    Q_OBJECT
public:
    /// default constructor
    MeshViewerWidget(QWidget* parent=0) : MeshViewerWidgetT<DefaultMesh>(parent)
    {}
    OpenMesh::IO::Options& options() { return _options; }
    const OpenMesh::IO::Options& options() const { return _options; }
    void setOptions(const OpenMesh::IO::Options& opts) { _options = opts; }

    void open_mesh_gui(QString fname)
    {
        OpenMesh::Utils::Timer t;
        t.start();
        if ( fname.isEmpty() || !open_mesh( fname.toStdString().c_str(), _options) )
        {
            QString msg = "Cannot read mesh from file:\n '";
            msg += fname;
            msg += "'";
            QMessageBox::critical( NULL, windowTitle(), msg);
        }
        t.stop();
        std::cout << "Loaded mesh in ~" << t.as_string() << std::endl;
    }

    void save_mesh_gui(QString fname)
    {
        OpenMesh::Utils::Timer t;
        t.start();
        if ( fname.isEmpty() || !save_mesh( fname.toStdString().c_str(), _options) )
        {
            QString msg = "Cannot save mesh to file:\n '";
            msg += fname;
            msg += "'";
            QMessageBox::critical( NULL, windowTitle(), msg);
        }
        t.stop();
        std::cout << "Saved mesh to ~" << t.as_string() << std::endl;
    }

    void open_texture_gui(QString fname)
    {
        if ( fname.isEmpty() || !open_texture( fname.toStdString().c_str() ) )
        {
            QString msg = "Cannot load texture image from file:\n '";
            msg += fname;
            msg += "'\n\nPossible reasons:\n";
            msg += "- Mesh file didn't provide texture coordinates\n";
            msg += "- Texture file does not exist\n";
            msg += "- Texture file is not accessible.\n";
            QMessageBox::warning( NULL, windowTitle(), msg );
        }
    }

public slots:
    void query_open_mesh_file() {
        QString fileName = QFileDialog::getOpenFileName(this,
            tr("Open mesh file"),
            tr("../../Dev_Data/"),
            tr("OBJ Files (*.obj);;"
            "OFF Files (*.off);;"
            "STL Files (*.stl);;"
            "PLY Files (*.ply);;"
            "All Files (*)"));
        if (!fileName.isEmpty())
            open_mesh_gui(fileName);
    }
    void query_save_mesh_file() {
        QString fileName = QFileDialog::getSaveFileName(this,
            tr("Save mesh file"),
            tr("../../Dev_Data/"),
            tr("OBJ Files (*.obj);;"
            "OFF Files (*.off);;"
            "STL Files (*.stl);;"
            "PLY Files (*.ply);;"
            "All Files (*)"));
        if (!fileName.isEmpty())
            save_mesh_gui(fileName);
    }
    void query_open_texture_file() {
        QString fileName = QFileDialog::getOpenFileName(this,
            tr("Open texture file"),
            tr("../../Dev_Data/"),
            tr("PNG Files (*.png);;"
            "BMP Files (*.bmp);;"
            "GIF Files (*.gif);;"
            "JPEG Files (*.jpg);;"
            "TIFF Files (*.tif);;"
            "All Files (*)"));
        if (!fileName.isEmpty())
            open_texture_gui(fileName);
    }
private:
    OpenMesh::IO::Options _options;
};


#endif