#include "MeshColor.h"
#include <time.h>
template<typename M>
MeshColor<M>::MeshColor(const Mesh& m):Ref_(m)
{

}
template<typename M>
MeshColor<M>::MeshColor(const MeshColor& c):Ref_(c.Ref_)
{
    v_colors = c.v_colors;
}

template<typename M>
ColorArray::RGBArray MeshColor<M>::vertex_colors_array(void)
{
    return v_colors;
}

template<typename M>
void* MeshColor<M>::vertex_colors(void)
{
    std::srand(time(NULL));
    int index = std::rand() % ColorArray::DefaultColorNum_;
    uint8_t r = ColorArray::DefaultColor[index].rgba.r;
    uint8_t g = ColorArray::DefaultColor[index].rgba.g;
    uint8_t b = ColorArray::DefaultColor[index].rgba.b;
    if(!v_colors.data_)
    {
        v_colors.reset(Ref_.n_vertices(),r,g,b);
    }else if(Ref_.n_vertices()!=v_colors.size_)
    {
        v_colors.reset(Ref_.n_vertices(),r,g,b);
    }
    return (void*)v_colors.data_;
}