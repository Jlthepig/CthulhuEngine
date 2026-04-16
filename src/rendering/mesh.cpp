#include "mesh.h"


namespace Cthulhu::Rendering
{
    void Mesh::setup(const std::vector<float>& vertices, 
        const std::vector<unsigned int>& indices,
        const std::vector<vertexAttribute>& attributes,
        unsigned int stride
    )
    {

        vertexData = vertices;
        indexData = indices;

        glGenVertexArrays(1,&VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1,&EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER,VBO);
        glBufferData(GL_ARRAY_BUFFER,vertices.size() * sizeof(float),vertices.data(),GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,indices.size() * sizeof(unsigned int),indices.data(),GL_STATIC_DRAW);

        for (const auto& attr : attributes)
        {
            glVertexAttribPointer(
                attr.location,
                attr.componentCount,
                GL_FLOAT,
                GL_FALSE,
                stride,
                (void*)attr.offset
            );
            glEnableVertexAttribArray(attr.location);
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0); 
        glBindVertexArray(0); 
    }
    
    size_t Mesh::getIndexCount() const
    {
        return indexData.size();
    }
    
    size_t Mesh::getVertexDataSize() const
    {
        return vertexData.size();
    }
    void Mesh::draw()
    {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)indexData.size(), GL_UNSIGNED_INT,0);
        glBindVertexArray(0); 
    }
    
    void Mesh::destroy()
    {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }
}